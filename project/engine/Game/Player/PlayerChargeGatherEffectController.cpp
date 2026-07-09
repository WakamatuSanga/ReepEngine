#include "PlayerChargeGatherEffectController.h"

#include "Player.h"
#include "PlayerBulletManager.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Particle/GpuParticleEffectData.h"
#include "Engine/Graphics/Particle/GpuParticleEffectSerializer.h"
#include "Engine/Graphics/Particle/GpuParticleSystem.h"
#include "Engine/Core/SrvManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <cmath>

namespace {
    constexpr float kMinVectorLength = 0.00001f;

    Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 ScaleVector3(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    float Length(const Vector3& value) {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    }

    Vector3 Normalize(const Vector3& value, const Vector3& fallback) {
        const float length = Length(value);
        if (length <= kMinVectorLength || !std::isfinite(length)) {
            return fallback;
        }
        return { value.x / length, value.y / length, value.z / length };
    }

    float Lerp(float start, float end, float t) {
        return start + (end - start) * std::clamp(t, 0.0f, 1.0f);
    }

    GpuParticle::ParticleEffectData MakeFallbackEffect(const char* name, bool gather) {
        GpuParticle::ParticleEffectData effect;
        GpuParticle::Emitter emitter;
        emitter.enabled = true;
        emitter.radius = gather ? 1.5f : 0.18f;
        emitter.emitCount = gather ? 12u : 8u;
        emitter.emitInterval = 0.0f;
        emitter.emissionRate = gather ? 80.0f : 28.0f;
        emitter.randomSeed = gather ? 331u : 733u;
        effect.emitters.push_back(emitter);

        GpuParticle::ParticleType type = GpuParticle::MakeDefaultParticleEffectType(0);
        type.name = name;
        type.texturePath = "resources/particle/circle2.png";
        type.affectedByInfluenceField = false;
        type.affectedByRailFlow = false;
        type.affectedByChargeGather = gather;
        type.scaleByChargeRate = gather;
        type.brightnessByChargeRate = gather;
        type.emissionByChargeRate = gather;
        type.chargeGatherStrength = gather ? 6.0f : 0.0f;
        type.chargeGatherSwirlStrength = gather ? 1.2f : 0.0f;
        type.chargeGatherKillRadius = 0.15f;
        type.chargeGatherResponseScale = 1.0f;
        type.startScale = gather ? 0.055f : 0.12f;
        type.endScale = gather ? 0.018f : 0.045f;
        type.lifeTimeMin = gather ? 0.35f : 0.18f;
        type.lifeTimeMax = gather ? 0.72f : 0.34f;
        type.speedMin = gather ? 0.05f : 0.02f;
        type.speedMax = gather ? 0.32f : 0.12f;
        type.gravity = 0.0f;
        type.drag = gather ? 0.20f : 0.35f;
        type.baseColor = gather ? Vector4{0.45f, 0.82f, 1.0f, 0.85f} : Vector4{0.70f, 0.95f, 1.0f, 0.92f};
        type.startColor = gather ? Vector4{0.68f, 0.92f, 1.0f, 0.80f} : Vector4{0.92f, 0.98f, 1.0f, 0.95f};
        type.endColor = gather ? Vector4{0.95f, 1.0f, 1.0f, 0.0f} : Vector4{0.35f, 0.72f, 1.0f, 0.0f};
        effect.particleTypes.push_back(type);
        effect.runtime.randomEnabled = true;
        effect.runtime.generateUnusedList = true;
        effect.runtime.useFreeListEmit = true;
        effect.runtime.useDeadList = true;
        effect.runtime.autoRecycleDeadList = true;
        effect.runtime.autoReuseDeadParticles = true;
        effect.runtime.maxActiveParticles = gather ? 512u : 256u;
        effect.runtime.maxEmitPerFrame = gather ? 48u : 24u;
        GpuParticle::NormalizeParticleEffectData(effect);
        return effect;
    }
}

PlayerChargeGatherEffectController::PlayerChargeGatherEffectController() = default;
PlayerChargeGatherEffectController::~PlayerChargeGatherEffectController() = default;

bool PlayerChargeGatherEffectController::Initialize(
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    Camera* camera,
    Player* player,
    PlayerBulletManager* bulletManager) {
    Finalize();
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    camera_ = camera;
    player_ = player;
    bulletManager_ = bulletManager;
    SyncPathBuffers();
    if (!dxCommon_ || !srvManager_ || !camera_ || !player_ || !bulletManager_) {
        return false;
    }

    gatherSystem_ = std::make_unique<GpuParticleSystem>();
    maxCoreSystem_ = std::make_unique<GpuParticleSystem>();
    if (!gatherSystem_ || !maxCoreSystem_ || !gatherSystem_->Initialize(dxCommon_, srvManager_) || !maxCoreSystem_->Initialize(dxCommon_, srvManager_)) {
        Finalize();
        return false;
    }

    effectsLoaded_ = LoadEffects();
    initialized_ = true;
    return true;
}

void PlayerChargeGatherEffectController::Finalize() {
    initialized_ = false;
    effectsLoaded_ = false;
    gatherSystem_.reset();
    maxCoreSystem_.reset();
    gatherEffect_.reset();
    maxCoreEffect_.reset();
    dxCommon_ = nullptr;
    srvManager_ = nullptr;
    camera_ = nullptr;
    player_ = nullptr;
    bulletManager_ = nullptr;
}

bool PlayerChargeGatherEffectController::LoadEffects() {
    gatherEffect_ = std::make_unique<GpuParticle::ParticleEffectData>();
    maxCoreEffect_ = std::make_unique<GpuParticle::ParticleEffectData>();
    bool loaded = true;
    if (!GpuParticle::GpuParticleEffectSerializer::Load(gatherEffectPath_, *gatherEffect_)) {
        *gatherEffect_ = MakeFallbackEffect("PlayerChargeGatherFallback", true);
        loaded = false;
    }
    if (!GpuParticle::GpuParticleEffectSerializer::Load(maxCoreEffectPath_, *maxCoreEffect_)) {
        *maxCoreEffect_ = MakeFallbackEffect("PlayerChargeMaxCoreFallback", false);
        loaded = false;
    }
    gatherSystem_->ApplyEffectData(*gatherEffect_);
    maxCoreSystem_->ApplyEffectData(*maxCoreEffect_);
    gatherSystem_->SetCounterReadbackEnabled(true);
    maxCoreSystem_->SetCounterReadbackEnabled(false);
    return loaded;
}

void PlayerChargeGatherEffectController::ReloadEffects() {
    if (!initialized_) {
        return;
    }
    gatherEffectPath_ = gatherEffectPathBuffer_.data();
    maxCoreEffectPath_ = maxCoreEffectPathBuffer_.data();
    effectsLoaded_ = LoadEffects();
}

void PlayerChargeGatherEffectController::Update(float deltaTime) {
    if (!initialized_) {
        return;
    }

    ClampSettings();
    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    const bool held = bulletManager_ && bulletManager_->IsChargeInputHeld();
    rawChargeTimer_ = bulletManager_ ? bulletManager_->GetChargeTime() : 0.0f;
    const float maxChargeTime = (std::max)(bulletManager_ ? bulletManager_->GetMaxChargeTime() : 1.2f, 0.01f);
    rawChargeRate_ = std::clamp(rawChargeTimer_ / maxChargeTime, 0.0f, 1.0f);
    const bool debugGather = forceStrongGather_ || testGather_;
    const bool actualChargeMax = bulletManager_ && bulletManager_->IsChargeMax();
    isChargeMax_ = testMaxCore_ || actualChargeMax;
    chargeVisualActive_ = debugGather || isChargeMax_ || rawChargeTimer_ >= chargeGatherStartDelay_;
    const float visualChargeDuration = (std::max)(maxChargeTime - chargeGatherStartDelay_, 0.01f);
    visualChargeRate_ = chargeVisualActive_ ? std::clamp((rawChargeTimer_ - chargeGatherStartDelay_) / visualChargeDuration, 0.0f, 1.0f) : 0.0f;
    if (debugGather) {
        visualChargeRate_ = (std::max)(visualChargeRate_, testChargeRate_);
    }
    visualChargeRate_ = std::clamp(visualChargeRate_, 0.0f, 1.0f);
    chargeRate_ = isChargeMax_ ? 1.0f : visualChargeRate_;
    shouldEmitGather_ = debugGather || (held && chargeVisualActive_ && !isChargeMax_ && rawChargeRate_ > 0.0f);
    isCharging_ = debugGather || (held && chargeVisualActive_ && !isChargeMax_);
    playerPosition_ = player_ ? player_->GetWorldPosition() : Vector3{0.0f, 0.0f, 0.0f};
    targetPosition_ = ResolveTargetPosition();
    const Vector3 toTarget{
        targetPosition_.x - playerPosition_.x,
        targetPosition_.y - playerPosition_.y,
        targetPosition_.z - playerPosition_.z,
    };
    distanceTargetFromPlayer_ = Length(toTarget);
    gatherActive_ = enableChargeGather_ && shouldEmitGather_ && !isChargeMax_;
    maxCoreActive_ = enableMaxCore_ && isChargeMax_;

    ApplyGatherRuntime(safeDeltaTime);
    ApplyMaxCoreRuntime(safeDeltaTime);
}
void PlayerChargeGatherEffectController::ApplyGatherRuntime(float deltaTime) {
    if (!gatherSystem_ || !gatherEffect_ || gatherEffect_->emitters.empty() || gatherEffect_->particleTypes.empty()) {
        return;
    }

    GpuParticle::Emitter emitter = gatherEffect_->emitters.front();
    emitter.enabled = gatherActive_;
    emitter.position = playerPosition_;
    emitter.radius = Lerp(gatherRadiusMin_, gatherRadiusMax_, chargeRate_);
    currentEmissionRate_ = gatherEffect_->particleTypes.front().emissionByChargeRate
        ? Lerp(gatherEmissionRateMin_, gatherEmissionRateMax_, chargeRate_)
        : gatherEffect_->emitters.front().emissionRate;
    emitter.emissionRate = currentEmissionRate_;
    gatherSystem_->SetEmitterRuntime(0, emitter);

    GpuParticle::ParticleType type = gatherEffect_->particleTypes.front();
    currentGatherStrength_ = forceStrongGather_ ? forceGatherStrength_ : Lerp(gatherStrengthMin_, gatherStrengthMax_, chargeRate_);
    currentSwirlStrength_ = disableSwirlForTest_ ? 0.0f : Lerp(gatherSwirlMin_, gatherSwirlMax_, chargeRate_);
    type.chargeGatherStrength = currentGatherStrength_;
    type.chargeGatherSwirlStrength = currentSwirlStrength_;
    if (disableInitialVelocityForTest_) {
        type.speedMin = 0.0f;
        type.speedMax = 0.0f;
    }
    gatherSystem_->SetParticleTypeRuntime(0, type);
    const float strengthScale = forceStrongGather_ ? 1.35f : 1.0f;
    gatherSystem_->SetChargeGather(gatherActive_, targetPosition_, chargeRate_, strengthScale, 1.0f, gatherBrightnessScale_);
    gatherSystem_->SetDeltaTime(deltaTime);
    gatherSystem_->Update(camera_);

    const GpuParticle::State& gatherState = gatherSystem_->GetState();
    emittedCount_ += gatherState.lastActualEmitCount;
    droppedEmitCount_ += gatherState.lastSkippedEmitCount;
    gatherDeadCountEstimate_ = gatherState.isCounterReadbackValid ? gatherState.actualDeadListCount : gatherState.deadListCountEstimate;
    killedAtTargetCountEstimate_ += gatherState.lastRecycleDispatchCount;
    averageDistancePrevious_ = averageDistanceCurrent_;
    averageDistanceCurrent_ = gatherActive_ ? Lerp(emitter.radius, type.chargeGatherKillRadius, chargeRate_) : 0.0f;
}
void PlayerChargeGatherEffectController::ApplyMaxCoreRuntime(float deltaTime) {
    if (!maxCoreSystem_ || !maxCoreEffect_ || maxCoreEffect_->emitters.empty()) {
        return;
    }

    GpuParticle::Emitter emitter = maxCoreEffect_->emitters.front();
    emitter.enabled = maxCoreActive_ || (showTargetDebug_ && gatherActive_);
    emitter.position = targetPosition_;
    emitter.radius = maxCoreRadius_;
    emitter.emissionRate = maxCoreActive_ ? maxCoreEmissionRate_ : 8.0f;
    maxCoreSystem_->SetEmitterRuntime(0, emitter);
    maxCoreSystem_->SetChargeGather(false, targetPosition_, chargeRate_, 0.0f, 0.0f, 1.0f);
    maxCoreSystem_->SetDeltaTime(deltaTime);
    maxCoreSystem_->Update(camera_);
    emittedCount_ += maxCoreSystem_->GetState().lastActualEmitCount;
    droppedEmitCount_ += maxCoreSystem_->GetState().lastSkippedEmitCount;
}

void PlayerChargeGatherEffectController::Draw() {
    if (!initialized_) {
        return;
    }
    if (gatherSystem_) {
        gatherSystem_->Draw();
    }
    if (maxCoreSystem_) {
        maxCoreSystem_->Draw();
    }
}

void PlayerChargeGatherEffectController::DrawImGui() {
#ifdef USE_IMGUI
    if (!showDebugWindow_) {
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(430.0f, 460.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Player Charge Gather GPU Particle Debug")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Enable Charge Gather", &enableChargeGather_);
    ImGui::Checkbox("Enable Max Core", &enableMaxCore_);
    ImGui::Checkbox("Test Gather", &testGather_);
    ImGui::SameLine();
    ImGui::Checkbox("Test Max Core", &testMaxCore_);
    ImGui::Checkbox("Force Strong Gather", &forceStrongGather_);
    ImGui::SameLine();
    ImGui::Checkbox("Show Target Debug", &showTargetDebug_);
    ImGui::Checkbox("Disable Swirl For Test", &disableSwirlForTest_);
    ImGui::SameLine();
    ImGui::Checkbox("Disable Initial Velocity For Test", &disableInitialVelocityForTest_);
    if (ImGui::Button("Reload Charge GPU Effects")) {
        ReloadEffects();
    }
    ImGui::Text("Effects Loaded: %s", effectsLoaded_ ? "true" : "fallback/partial");
    ImGui::InputText("Gather JSON", gatherEffectPathBuffer_.data(), gatherEffectPathBuffer_.size());
    ImGui::InputText("Max Core JSON", maxCoreEffectPathBuffer_.data(), maxCoreEffectPathBuffer_.size());

    ImGui::SeparatorText("Charge State");
    ImGui::DragFloat("Gather Start Delay", &chargeGatherStartDelay_, 0.01f, 0.0f, 0.5f, "%.2f");
    ImGui::Text("Raw Charge Timer: %.2f", rawChargeTimer_);
    ImGui::Text("Raw Charge Rate: %.2f", rawChargeRate_);
    ImGui::Text("Visual Charge Rate: %.2f", visualChargeRate_);
    ImGui::Text("Charge Rate: %.2f", chargeRate_);
    ImGui::Text("Charge Visual Active: %s", chargeVisualActive_ ? "true" : "false");
    ImGui::Text("Should Emit Gather: %s", shouldEmitGather_ ? "true" : "false");
    ImGui::Text("Charging: %s", isCharging_ ? "true" : "false");
    ImGui::Text("Charge Max: %s", isChargeMax_ ? "true" : "false");
    ImGui::Text("Gather Active: %s", gatherActive_ ? "true" : "false");
    ImGui::Text("Max Core Active: %s", maxCoreActive_ ? "true" : "false");
    ImGui::Text("Target: %.2f, %.2f, %.2f", targetPosition_.x, targetPosition_.y, targetPosition_.z);
    ImGui::Text("Player: %.2f, %.2f, %.2f", playerPosition_.x, playerPosition_.y, playerPosition_.z);
    ImGui::Text("Core Offset Distance: %.2f", distanceTargetFromPlayer_);
    ImGui::Text("Current Emission Rate: %.1f", currentEmissionRate_);
    ImGui::Text("Current Gather Strength: %.1f", currentGatherStrength_);
    ImGui::Text("Current Swirl Strength: %.1f", currentSwirlStrength_);

    ImGui::SeparatorText("Gather Tuning");
    ImGui::DragFloat("Test Charge Rate", &testChargeRate_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Force Gather Strength", &forceGatherStrength_, 0.5f, 0.0f, 100.0f, "%.1f");
    ImGui::DragFloat("Front Offset", &frontOffset_, 0.01f, 0.0f, 5.0f, "%.2f");
    ImGui::DragFloat("Gather Radius Min", &gatherRadiusMin_, 0.01f, 0.0f, 10.0f, "%.2f");
    ImGui::DragFloat("Gather Radius Max", &gatherRadiusMax_, 0.01f, 0.0f, 10.0f, "%.2f");
    ImGui::DragFloat("Emission Rate Min", &gatherEmissionRateMin_, 1.0f, 0.0f, 1000.0f, "%.0f");
    ImGui::DragFloat("Emission Rate Max", &gatherEmissionRateMax_, 1.0f, 0.0f, 1000.0f, "%.0f");
    ImGui::DragFloat("Gather Strength Min", &gatherStrengthMin_, 0.1f, 0.0f, 100.0f, "%.1f");
    ImGui::DragFloat("Gather Strength Max", &gatherStrengthMax_, 0.1f, 0.0f, 100.0f, "%.1f");
    ImGui::DragFloat("Swirl Min", &gatherSwirlMin_, 0.1f, 0.0f, 100.0f, "%.1f");
    ImGui::DragFloat("Swirl Max", &gatherSwirlMax_, 0.1f, 0.0f, 100.0f, "%.1f");
    ImGui::DragFloat("Brightness Scale", &gatherBrightnessScale_, 0.01f, 1.0f, 10.0f, "%.2f");
    ImGui::DragFloat("Max Core Radius", &maxCoreRadius_, 0.005f, 0.01f, 2.0f, "%.3f");
    ImGui::DragFloat("Max Core Emission Rate", &maxCoreEmissionRate_, 1.0f, 0.0f, 1000.0f, "%.0f");

    ImGui::SeparatorText("Runtime Counters");
    ImGui::Text("Emitted Count: %u", emittedCount_);
    ImGui::Text("Dropped Emit Count: %u", droppedEmitCount_);
    ImGui::Text("Gather Dead/Kill Estimate: %u", gatherDeadCountEstimate_);
    ImGui::Text("Killed At Target Count Estimate: %u", killedAtTargetCountEstimate_);
    ImGui::Text("Average Distance Estimate Prev/Current: %.2f / %.2f", averageDistancePrevious_, averageDistanceCurrent_);
    if (ImGui::Button("Reset Counters")) {
        emittedCount_ = 0;
        droppedEmitCount_ = 0;
        gatherDeadCountEstimate_ = 0;
        killedAtTargetCountEstimate_ = 0;
        averageDistancePrevious_ = 0.0f;
        averageDistanceCurrent_ = 0.0f;
    }
    ImGui::End();
#endif
}

Vector3 PlayerChargeGatherEffectController::ResolvePlayerForward() const {
    if (player_) {
        return Normalize(player_->GetBaseForward(), {0.0f, 0.0f, 1.0f});
    }
    return {0.0f, 0.0f, 1.0f};
}

Vector3 PlayerChargeGatherEffectController::ResolveTargetPosition() const {
    const Vector3 playerPosition = player_ ? player_->GetWorldPosition() : Vector3{0.0f, 0.0f, 0.0f};
    return AddVector3(playerPosition, ScaleVector3(ResolvePlayerForward(), frontOffset_));
}

void PlayerChargeGatherEffectController::ClampSettings() {
    chargeGatherStartDelay_ = std::clamp(chargeGatherStartDelay_, 0.0f, 0.5f);
    testChargeRate_ = std::clamp(testChargeRate_, 0.0f, 1.0f);
    frontOffset_ = std::clamp(frontOffset_, 0.0f, 5.0f);
    gatherRadiusMin_ = std::clamp(gatherRadiusMin_, 0.0f, 10.0f);
    gatherRadiusMax_ = std::clamp(gatherRadiusMax_, gatherRadiusMin_, 10.0f);
    gatherEmissionRateMin_ = std::clamp(gatherEmissionRateMin_, 0.0f, 1000.0f);
    gatherEmissionRateMax_ = std::clamp(gatherEmissionRateMax_, gatherEmissionRateMin_, 1000.0f);
    gatherStrengthMin_ = std::clamp(gatherStrengthMin_, 0.0f, 100.0f);
    gatherStrengthMax_ = std::clamp(gatherStrengthMax_, gatherStrengthMin_, 100.0f);
    gatherSwirlMin_ = std::clamp(gatherSwirlMin_, 0.0f, 100.0f);
    gatherSwirlMax_ = std::clamp(gatherSwirlMax_, gatherSwirlMin_, 100.0f);
    gatherBrightnessScale_ = std::clamp(gatherBrightnessScale_, 1.0f, 10.0f);
    forceGatherStrength_ = std::clamp(forceGatherStrength_, 0.0f, 100.0f);
    maxCoreRadius_ = std::clamp(maxCoreRadius_, 0.01f, 2.0f);
    maxCoreEmissionRate_ = std::clamp(maxCoreEmissionRate_, 0.0f, 1000.0f);
}

void PlayerChargeGatherEffectController::SyncPathBuffers() {
    gatherEffectPathBuffer_.fill('\0');
    maxCoreEffectPathBuffer_.fill('\0');
    std::copy_n(gatherEffectPath_.c_str(), (std::min)(gatherEffectPath_.size(), gatherEffectPathBuffer_.size() - 1), gatherEffectPathBuffer_.begin());
    std::copy_n(maxCoreEffectPath_.c_str(), (std::min)(maxCoreEffectPath_.size(), maxCoreEffectPathBuffer_.size() - 1), maxCoreEffectPathBuffer_.begin());
}