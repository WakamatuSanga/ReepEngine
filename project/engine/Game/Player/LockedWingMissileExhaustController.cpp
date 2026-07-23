#include "LockedWingMissileExhaustController.h"

#include "Engine/Core/DirectXCommon.h"
#include "Engine/Core/SrvManager.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Particle/GpuParticleEffectSerializer.h"
#include "Engine/Graphics/Particle/GpuParticleSystem.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace {
constexpr float kMinimumVectorLength = 0.00001f;
constexpr float kPlayerJetCoreStartScaleReference = 0.055f;

Vector3 Add(const Vector3& lhs, const Vector3& rhs) {
    return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

Vector3 Scale(const Vector3& value, float scale) {
    return { value.x * scale, value.y * scale, value.z * scale };
}

float Length(const Vector3& value) {
    return std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z);
}

bool IsFinite(const Vector3& value) {
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

bool TryNormalize(const Vector3& value, Vector3& normalized) {
    const float length = Length(value);
    if (!IsFinite(value) || !std::isfinite(length)
        || length <= kMinimumVectorLength) {
        return false;
    }
    normalized = Scale(value, 1.0f / length);
    return IsFinite(normalized);
}
}

LockedWingMissileExhaustController::LockedWingMissileExhaustController() =
    default;
LockedWingMissileExhaustController::~LockedWingMissileExhaustController() =
    default;

bool LockedWingMissileExhaustController::Initialize(
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    Camera* camera) {
    Finalize();
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    camera_ = camera;
    if (!dxCommon_ || !srvManager_ || !camera_) {
        loadStatus_ = "必要な初期化参照が不足しています";
        return false;
    }

    particleSystem_ = std::make_unique<GpuParticleSystem>();
    if (!particleSystem_->Initialize(dxCommon_, srvManager_)) {
        loadStatus_ = "GPU Particle Systemの初期化に失敗しました";
        particleSystem_.reset();
        return false;
    }

    initialized_ = LoadPreset();
    if (!initialized_) {
        particleSystem_.reset();
        return false;
    }
    return true;
}

void LockedWingMissileExhaustController::Finalize() {
    if (particleSystem_) {
        Reset(true);
    }
    particleSystem_.reset();
    emitterTemplates_.clear();
    particleTypeTemplates_.clear();
    dxCommon_ = nullptr;
    srvManager_ = nullptr;
    camera_ = nullptr;
    emittersPerSlot_ = 0;
    lastHandle_ = 0;
    lastSlotState_ = SlotState::Free;
    lastExhaustPosition_ = {};
    lastExhaustDirection_ = { 0.0f, 0.0f, -1.0f };
    initialized_ = false;
    loadStatus_ = "終了済み";
}

bool LockedWingMissileExhaustController::LoadPreset() {
    if (!particleSystem_) {
        loadStatus_ = "GPU Particle Systemがありません";
        return false;
    }

    GpuParticle::ParticleEffectData data;
    if (!std::filesystem::exists(std::filesystem::path(presetPath_))) {
        loadStatus_ = "Missile Exhaust JSONが見つかりません";
        return false;
    }
    if (!GpuParticle::GpuParticleEffectSerializer::Load(presetPath_, data)
        || data.emitters.empty() || data.particleTypes.empty()) {
        loadStatus_ = "Missile Exhaust JSONの読み込みに失敗しました";
        return false;
    }

    emitterTemplates_ = data.emitters;
    particleTypeTemplates_ = data.particleTypes;
    emittersPerSlot_ = static_cast<uint32_t>(emitterTemplates_.size());
    jsonBaseScale_ = particleTypeTemplates_.front().startScale
        / kPlayerJetCoreStartScaleReference;
    maxParticleLifetime_ = 0.01f;
    for (const GpuParticle::ParticleType& type : particleTypeTemplates_) {
        maxParticleLifetime_ =
            (std::max)(maxParticleLifetime_, type.lifeTimeMax);
    }

    data.emitters.clear();
    data.emitters.reserve(
        static_cast<size_t>(kHardSlotCapacity) * emittersPerSlot_);
    for (uint32_t slotIndex = 0; slotIndex < kHardSlotCapacity; ++slotIndex) {
        for (uint32_t templateIndex = 0;
             templateIndex < emittersPerSlot_;
             ++templateIndex) {
            GpuParticle::Emitter emitter = emitterTemplates_[templateIndex];
            emitter.enabled = false;
            emitter.position = {};
            emitter.direction = { 0.0f, 0.0f, -1.0f };
            emitter.randomSeed += slotIndex * 97u + templateIndex * 17u;
            emitter.emitTimer = 0.0f;
            emitter.emissionAccumulator = 0.0f;
            emitter.pendingEmitCount = 0;
            emitter.pendingEmit = false;
            data.emitters.push_back(emitter);
        }
    }
    data.runtime.useFreeListEmit = true;
    data.runtime.generateUnusedList = true;
    data.runtime.useDeadList = true;
    data.runtime.autoRecycleDeadList = true;
    data.runtime.autoReuseDeadParticles = true;
    data.runtime.updateEnabled = true;
    GpuParticle::NormalizeParticleEffectData(data);
    particleSystem_->ApplyEffectData(data);

    for (Slot& slot : slots_) {
        slot.state = SlotState::Free;
        slot.drainRemaining = 0.0f;
        slot.position = {};
        slot.direction = { 0.0f, 0.0f, -1.0f };
        if (slot.generation == 0) {
            slot.generation = 1;
        }
    }
    appliedRuntimeScaleMultiplier_ = -1.0f;
    ApplyRuntimeScale();
    loadStatus_ = "Missile Exhaust JSONを読み込みました";
    return true;
}

uint64_t LockedWingMissileExhaustController::Start(
    uint64_t existingHandle,
    const Vector3& projectilePosition,
    const Vector3& flightDirection) {
    uint32_t existingSlot = 0;
    if (existingHandle != 0 && DecodeHandle(existingHandle, existingSlot)) {
        ++doubleStartPreventionCount_;
        return slots_[existingSlot].state == SlotState::Emitting
            ? existingHandle
            : 0;
    }

    Vector3 normalizedFlight{};
    if (!initialized_ || !IsFinite(projectilePosition)
        || !TryNormalize(flightDirection, normalizedFlight)) {
        ++invalidTransformStopCount_;
        return 0;
    }

    const int activeLimit =
        std::clamp(maxEmitterCount_, 1, static_cast<int>(kHardSlotCapacity));
    uint32_t slotIndex = kHardSlotCapacity;
    for (int index = 0; index < activeLimit; ++index) {
        if (slots_[index].state == SlotState::Free) {
            slotIndex = static_cast<uint32_t>(index);
            break;
        }
    }
    if (slotIndex >= kHardSlotCapacity) {
        ++emitterShortageCount_;
        loadStatus_ = "空きEmitter Slotがありません";
        return 0;
    }

    Slot& slot = slots_[slotIndex];
    slot.state = SlotState::Emitting;
    slot.drainRemaining = 0.0f;
    const uint64_t handle = MakeHandle(slotIndex);
    if (!UpdateMissile(handle, projectilePosition, normalizedFlight)) {
        slot.state = SlotState::Free;
        ++emitterShortageCount_;
        return 0;
    }

    ++emitterAcquireSuccessCount_;
    lastHandle_ = handle;
    lastSlotState_ = SlotState::Emitting;
    loadStatus_ = "Missile Exhaustを点火しました";
    return handle;
}

bool LockedWingMissileExhaustController::UpdateMissile(
    uint64_t handle,
    const Vector3& projectilePosition,
    const Vector3& flightDirection) {
    uint32_t slotIndex = 0;
    if (!DecodeHandle(handle, slotIndex)
        || slots_[slotIndex].state != SlotState::Emitting) {
        return false;
    }

    Vector3 normalizedFlight{};
    if (!IsFinite(projectilePosition)
        || !TryNormalize(flightDirection, normalizedFlight)) {
        ++invalidTransformStopCount_;
        Stop(handle);
        loadStatus_ = "不正な位置または方向のためEmissionを停止しました";
        return false;
    }

    const Vector3 exhaustPosition = Add(
        projectilePosition,
        Scale(normalizedFlight, -exhaustRearOffset_));
    const Vector3 exhaustDirection = Scale(normalizedFlight, -1.0f);
    if (!IsFinite(exhaustPosition) || !IsFinite(exhaustDirection)
        || !SetSlotEmitters(
            slotIndex, true, exhaustPosition, exhaustDirection)) {
        ++invalidTransformStopCount_;
        Stop(handle);
        loadStatus_ = "Emitter更新失敗のためEmissionを停止しました";
        return false;
    }

    Slot& slot = slots_[slotIndex];
    slot.position = exhaustPosition;
    slot.direction = exhaustDirection;
    lastHandle_ = handle;
    lastSlotState_ = SlotState::Emitting;
    lastExhaustPosition_ = exhaustPosition;
    lastExhaustDirection_ = exhaustDirection;
    return true;
}

void LockedWingMissileExhaustController::Stop(uint64_t handle) {
    uint32_t slotIndex = 0;
    if (!DecodeHandle(handle, slotIndex)
        || slots_[slotIndex].state != SlotState::Emitting) {
        if (handle != 0) {
            ++doubleReleasePreventionCount_;
        }
        return;
    }

    Slot& slot = slots_[slotIndex];
    SetSlotEmitters(slotIndex, false, slot.position, slot.direction);
    slot.state = SlotState::Draining;
    slot.drainRemaining = (std::max)(maxParticleLifetime_, 0.01f);
    ++emitterStopCount_;
    ++drainingStartCount_;
    lastHandle_ = handle;
    lastSlotState_ = SlotState::Draining;
    loadStatus_ = "Emissionを停止し、自然消滅を待っています";
}

void LockedWingMissileExhaustController::Update(float scaledDeltaTime) {
    if (!initialized_ || !particleSystem_ || !camera_) {
        return;
    }
    const float safeDeltaTime = std::isfinite(scaledDeltaTime)
        ? std::clamp(scaledDeltaTime, 0.0f, 1.0f / 15.0f)
        : 0.0f;
    ReleaseDrainedSlots(safeDeltaTime);
    ApplyRuntimeScale();
    particleSystem_->SetDeltaTime(safeDeltaTime);
    particleSystem_->SetParticleInfluenceEnabled(false);
    particleSystem_->SetRailParticleFlow(
        false,
        camera_->GetTranslate(),
        { 0.0f, 0.0f, -1.0f },
        0.0f,
        0.0f,
        24.0f,
        8.0f);
    particleSystem_->Update(camera_);
}

void LockedWingMissileExhaustController::Draw() {
    if (initialized_ && particleSystem_) {
        particleSystem_->Draw();
    }
}

void LockedWingMissileExhaustController::Reset(bool resetStatistics) {
    for (uint32_t index = 0; index < kHardSlotCapacity; ++index) {
        Slot& slot = slots_[index];
        if (particleSystem_ && emittersPerSlot_ > 0) {
            SetSlotEmitters(index, false, slot.position, slot.direction);
        }
        slot.state = SlotState::Free;
        slot.drainRemaining = 0.0f;
        ++slot.generation;
        if (slot.generation == 0) {
            slot.generation = 1;
        }
    }
    if (particleSystem_) {
        particleSystem_->ResetParticlePool();
    }
    lastHandle_ = 0;
    lastSlotState_ = SlotState::Free;
    lastExhaustPosition_ = {};
    lastExhaustDirection_ = { 0.0f, 0.0f, -1.0f };
    if (resetStatistics) {
        emitterAcquireSuccessCount_ = 0;
        emitterShortageCount_ = 0;
        emitterStopCount_ = 0;
        drainingStartCount_ = 0;
        slotReleaseCount_ = 0;
        doubleStartPreventionCount_ = 0;
        doubleReleasePreventionCount_ = 0;
        invalidTransformStopCount_ = 0;
    }
}

bool LockedWingMissileExhaustController::DecodeHandle(
    uint64_t handle, uint32_t& slotIndex) const {
    const uint32_t encodedSlot = static_cast<uint32_t>(handle & 0xffffffffu);
    const uint32_t generation = static_cast<uint32_t>(handle >> 32u);
    if (encodedSlot == 0 || encodedSlot > kHardSlotCapacity) {
        return false;
    }
    slotIndex = encodedSlot - 1u;
    return generation != 0
        && slots_[slotIndex].generation == generation
        && slots_[slotIndex].state != SlotState::Free;
}

uint64_t LockedWingMissileExhaustController::MakeHandle(
    uint32_t slotIndex) const {
    return (static_cast<uint64_t>(slots_[slotIndex].generation) << 32u)
        | static_cast<uint64_t>(slotIndex + 1u);
}

bool LockedWingMissileExhaustController::SetSlotEmitters(
    uint32_t slotIndex,
    bool enabled,
    const Vector3& position,
    const Vector3& direction) {
    if (!particleSystem_ || slotIndex >= kHardSlotCapacity
        || emittersPerSlot_ == 0) {
        return false;
    }

    bool success = true;
    for (uint32_t templateIndex = 0;
         templateIndex < emittersPerSlot_;
         ++templateIndex) {
        GpuParticle::Emitter emitter = emitterTemplates_[templateIndex];
        emitter.enabled = enabled;
        emitter.position = position;
        emitter.direction = direction;
        emitter.randomSeed += slotIndex * 97u + templateIndex * 17u;
        const size_t runtimeIndex =
            static_cast<size_t>(slotIndex) * emittersPerSlot_
            + templateIndex;
        success = particleSystem_->SetEmitterRuntime(runtimeIndex, emitter)
            && success;
    }
    return success;
}

void LockedWingMissileExhaustController::ApplyRuntimeScale() {
    if (!particleSystem_ || particleTypeTemplates_.empty()) {
        return;
    }
    runtimeScaleMultiplier_ =
        std::clamp(runtimeScaleMultiplier_, 0.10f, 3.0f);
    finalAppliedScale_ = jsonBaseScale_ * runtimeScaleMultiplier_;
    if (std::fabs(
            runtimeScaleMultiplier_ - appliedRuntimeScaleMultiplier_)
        <= 0.0001f) {
        return;
    }

    for (size_t index = 0; index < particleTypeTemplates_.size(); ++index) {
        GpuParticle::ParticleType type = particleTypeTemplates_[index];
        type.startScale *= runtimeScaleMultiplier_;
        type.endScale *= runtimeScaleMultiplier_;
        particleSystem_->SetParticleTypeRuntime(index, type);
    }
    appliedRuntimeScaleMultiplier_ = runtimeScaleMultiplier_;
}

void LockedWingMissileExhaustController::ReleaseDrainedSlots(
    float scaledDeltaTime) {
    for (uint32_t index = 0; index < kHardSlotCapacity; ++index) {
        Slot& slot = slots_[index];
        if (slot.state != SlotState::Draining) {
            continue;
        }
        slot.drainRemaining =
            (std::max)(slot.drainRemaining - scaledDeltaTime, 0.0f);
        if (slot.drainRemaining > 0.0f) {
            continue;
        }

        const uint64_t releasedHandle = MakeHandle(index);
        slot.state = SlotState::Free;
        ++slot.generation;
        if (slot.generation == 0) {
            slot.generation = 1;
        }
        ++slotReleaseCount_;
        if (lastHandle_ == releasedHandle) {
            lastSlotState_ = SlotState::Free;
        }
    }
}