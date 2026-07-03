#include "EnemyLaserTelegraphController.h"
#include "Engine/Game/Player/PlayerJetExhaustBeamRenderer.h"
#include "Engine/Graphics/Camera/Camera.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <unordered_set>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kMinVectorLength = 0.00001f;

    Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 ScaleVector3(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    Vector3 Cross(const Vector3& lhs, const Vector3& rhs) {
        return {
            lhs.y * rhs.z - lhs.z * rhs.y,
            lhs.z * rhs.x - lhs.x * rhs.z,
            lhs.x * rhs.y - lhs.y * rhs.x,
        };
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

    Vector3 ResolveCameraForward(const Camera* camera) {
        if (!camera) {
            return { 0.0f, 0.0f, 1.0f };
        }
        const Matrix4x4& matrix = camera->GetWorldMatrix();
        return Normalize({ matrix.m[2][0], matrix.m[2][1], matrix.m[2][2] }, { 0.0f, 0.0f, 1.0f });
    }

    Vector3 ResolveCameraRight(const Camera* camera) {
        if (!camera) {
            return { 1.0f, 0.0f, 0.0f };
        }
        const Matrix4x4& matrix = camera->GetWorldMatrix();
        return Normalize({ matrix.m[0][0], matrix.m[0][1], matrix.m[0][2] }, { 1.0f, 0.0f, 0.0f });
    }

    Vector3 ResolveCameraUp(const Camera* camera) {
        if (!camera) {
            return { 0.0f, 1.0f, 0.0f };
        }
        const Matrix4x4& matrix = camera->GetWorldMatrix();
        return Normalize({ matrix.m[1][0], matrix.m[1][1], matrix.m[1][2] }, { 0.0f, 1.0f, 0.0f });
    }

    void AppendBeamQuad(
        std::vector<PlayerJetExhaustBeamRenderer::Vertex>& vertices,
        const Vector3& origin,
        const Vector3& direction,
        const Vector3& side,
        float width,
        float length) {
        const Vector3 halfSide = ScaleVector3(side, (std::max)(0.001f, width) * 0.5f);
        const Vector3 end = AddVector3(origin, ScaleVector3(direction, (std::max)(0.01f, length)));
        const Vector3 p0 = AddVector3(origin, ScaleVector3(halfSide, -1.0f));
        const Vector3 p1 = AddVector3(origin, halfSide);
        const Vector3 p2 = AddVector3(end, halfSide);
        const Vector3 p3 = AddVector3(end, ScaleVector3(halfSide, -1.0f));
        vertices.push_back({ p0, { 0.0f, 0.0f } });
        vertices.push_back({ p1, { 0.0f, 1.0f } });
        vertices.push_back({ p2, { 1.0f, 1.0f } });
        vertices.push_back({ p0, { 0.0f, 0.0f } });
        vertices.push_back({ p2, { 1.0f, 1.0f } });
        vertices.push_back({ p3, { 1.0f, 0.0f } });
    }
}

EnemyLaserTelegraphController::EnemyLaserTelegraphController() = default;
EnemyLaserTelegraphController::~EnemyLaserTelegraphController() = default;

bool EnemyLaserTelegraphController::Initialize(DirectXCommon* dxCommon, const Camera* camera) {
    initialized_ = false;
    camera_ = camera;
    renderer_ = std::make_unique<PlayerJetExhaustBeamRenderer>();
    initialized_ = dxCommon && camera_ && renderer_ && renderer_->Initialize(dxCommon);
    if (!initialized_) {
        renderer_.reset();
        return false;
    }
    ClampSettings();
    effects_.resize(static_cast<size_t>(maxActiveEffects_));
    return true;
}

void EnemyLaserTelegraphController::Finalize() {
    initialized_ = false;
    effects_.clear();
    renderer_.reset();
    camera_ = nullptr;
}

void EnemyLaserTelegraphController::Update(float deltaTime) {
    if (!initialized_) {
        return;
    }
    ClampSettings();
    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    time_ += safeDeltaTime;
    for (LaserEffect& effect : effects_) {
        if (!effect.active) {
            continue;
        }
        effect.age += safeDeltaTime;
        if (effect.age >= effect.duration) {
            lastEndedOwner_ = effect.owner;
            effect.active = false;
        }
    }
    activeWarningCount_ = CountActiveEffects(EffectType::Warning);
    activeTrackingWarningCount_ = CountLockedWarnings(false);
    activeLockedWarningCount_ = CountLockedWarnings(true);
    activeBeamCount_ = CountActiveEffects(EffectType::Beam);
    activeLaserInstanceCount_ = CountActiveOwners();
}

void EnemyLaserTelegraphController::Draw() {
    DrawLayer(false);
}

void EnemyLaserTelegraphController::DrawAfterCloud() {
    DrawLayer(true);
}

void EnemyLaserTelegraphController::StartWarning(const void* owner, const Vector3& origin, const Vector3& direction) {
    if (!initialized_ || !enableLaserTelegraph_) {
        return;
    }
    LaserEffect* effect = FindEffect(owner, EffectType::Warning);
    if (!effect) {
        effect = AllocateEffect();
    }
    if (!effect) {
        ++droppedEffectCount_;
        return;
    }
    effect->owner = owner;
    effect->origin = origin;
    effect->direction = Normalize(direction, ResolveCameraForward(camera_));
    effect->lockedTargetPosition = AddVector3(origin, ScaleVector3(effect->direction, warningLineLength_));
    effect->age = 0.0f;
    effect->duration = warningDuration_;
    effect->type = EffectType::Warning;
    effect->aimLocked = false;
    effect->active = true;
    lastOrigin_ = origin;
    lastDirection_ = effect->direction;
    lastStartedOwner_ = owner;
}

void EnemyLaserTelegraphController::UpdateWarning(const void* owner, const Vector3& origin, const Vector3& direction) {
    if (LaserEffect* effect = FindEffect(owner, EffectType::Warning)) {
        effect->origin = origin;
        if (!effect->aimLocked) {
            effect->direction = Normalize(direction, effect->direction);
        }
        lastOrigin_ = origin;
        lastDirection_ = effect->direction;
    }
}

void EnemyLaserTelegraphController::LockWarning(const void* owner, const Vector3& origin, const Vector3& targetPosition, const Vector3& direction) {
    if (LaserEffect* effect = FindEffect(owner, EffectType::Warning)) {
        effect->origin = origin;
        effect->lockedTargetPosition = targetPosition;
        effect->direction = Normalize(direction, effect->direction);
        effect->aimLocked = true;
        lastOrigin_ = origin;
        lastDirection_ = effect->direction;
        lastLockedTargetPosition_ = targetPosition;
        lastLockedDirection_ = effect->direction;
    }
}

void EnemyLaserTelegraphController::StartBeam(const void* owner, const Vector3& origin, const Vector3& direction) {
    if (!initialized_ || !enableLaserTelegraph_) {
        return;
    }
    Vector3 beamDirection = direction;
    if (LaserEffect* warning = FindEffect(owner, EffectType::Warning)) {
        beamDirection = warning->aimLocked ? warning->direction : direction;
        warning->active = false;
    }
    LaserEffect* effect = FindEffect(owner, EffectType::Beam);
    if (!effect) {
        effect = AllocateEffect();
    }
    if (!effect) {
        ++droppedEffectCount_;
        return;
    }
    effect->owner = owner;
    effect->origin = origin;
    effect->direction = Normalize(beamDirection, ResolveCameraForward(camera_));
    effect->lockedTargetPosition = AddVector3(origin, ScaleVector3(effect->direction, beamLength_));
    effect->age = 0.0f;
    effect->duration = beamDuration_;
    effect->type = EffectType::Beam;
    effect->aimLocked = true;
    effect->active = true;
    lastOrigin_ = origin;
    lastDirection_ = effect->direction;
    lastStartedOwner_ = owner;
}

void EnemyLaserTelegraphController::ClearOwner(const void* owner) {
    for (LaserEffect& effect : effects_) {
        if (effect.owner == owner) {
            lastEndedOwner_ = owner;
            effect.active = false;
        }
    }
}

void EnemyLaserTelegraphController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(430.0f, 430.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Enemy Laser Telegraph Debug")) {
        ImGui::End();
        return;
    }
    ImGui::Checkbox("Enable Laser Telegraph", &enableLaserTelegraph_);
    ImGui::Checkbox("Draw After Cloud", &drawAfterCloud_);
    ImGui::Checkbox("Loop Laser", &loopLaser_);
    ImGui::Checkbox("Aim Lock Enabled", &aimLockEnabled_);
    ImGui::Checkbox("Show Locked Direction Debug", &showLockedDirectionDebug_);
    ImGui::SliderInt("Max Laser Instances", &maxActiveEffects_, 1, 32);

    const Vector3 forward = ResolveCameraForward(camera_);
    const Vector3 right = ResolveCameraRight(camera_);
    const Vector3 up = ResolveCameraUp(camera_);
    const Vector3 base = camera_ ? AddVector3(camera_->GetTranslate(), ScaleVector3(forward, 10.0f)) : Vector3{};
    const Vector3 leftOrigin = AddVector3(base, AddVector3(ScaleVector3(right, -4.0f), ScaleVector3(up, 2.0f)));
    const Vector3 rightOrigin = AddVector3(base, AddVector3(ScaleVector3(right, 4.0f), ScaleVector3(up, 2.0f)));

    if (ImGui::Button("Test Spawn Left Warning")) {
        StartWarning(&testOwnerLeft_, leftOrigin, forward);
    }
    ImGui::SameLine();
    if (ImGui::Button("Test Spawn Right Warning")) {
        StartWarning(&testOwnerRight_, rightOrigin, forward);
    }
    if (ImGui::Button("Force Lock Test")) {
        StartWarning(&testOwnerLeft_, leftOrigin, forward);
        LockWarning(&testOwnerLeft_, leftOrigin, AddVector3(leftOrigin, ScaleVector3(forward, 20.0f)), forward);
        StartWarning(&testOwnerRight_, rightOrigin, forward);
        LockWarning(&testOwnerRight_, rightOrigin, AddVector3(rightOrigin, ScaleVector3(forward, 20.0f)), forward);
    }
    if (ImGui::Button("Test Fire Left Beam")) {
        StartBeam(&testOwnerLeft_, leftOrigin, forward);
    }
    ImGui::SameLine();
    if (ImGui::Button("Test Fire Right Beam")) {
        StartBeam(&testOwnerRight_, rightOrigin, forward);
    }
    if (ImGui::Button("Clear All Laser Instances")) {
        for (LaserEffect& effect : effects_) {
            effect.active = false;
        }
    }

    ImGui::DragFloat("Warning Duration", &warningDuration_, 0.02f, 0.05f, 5.0f, "%.2f");
    ImGui::DragFloat("Lock Before Fire Time", &lockBeforeFireTime_, 0.01f, 0.05f, 0.4f, "%.2f");
    ImGui::DragFloat("Blink Rate", &blinkRate_, 0.2f, 1.0f, 30.0f, "%.1f");
    ImGui::DragFloat("Locked Blink Rate", &lockedBlinkRate_, 0.2f, 1.0f, 30.0f, "%.1f");
    ImGui::DragFloat("Warning Line Width", &warningLineWidth_, 0.002f, 0.005f, 0.3f, "%.3f");
    ImGui::DragFloat("Warning Line Length", &warningLineLength_, 1.0f, 5.0f, 200.0f, "%.1f");
    ImGui::DragFloat("Warning Alpha", &warningAlpha_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Beam Duration", &beamDuration_, 0.02f, 0.05f, 3.0f, "%.2f");
    ImGui::DragFloat("Beam Core Width", &beamCoreWidth_, 0.002f, 0.005f, 0.4f, "%.3f");
    ImGui::DragFloat("Beam Glow Width", &beamGlowWidth_, 0.002f, 0.01f, 0.8f, "%.3f");
    ImGui::DragFloat("Beam Length", &beamLength_, 1.0f, 5.0f, 250.0f, "%.1f");
    ImGui::DragFloat("Beam Alpha", &beamAlpha_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Locked Warning Alpha", &lockedWarningAlpha_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Beam Glow Alpha", &beamGlowAlpha_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Beam Brightness", &beamBrightness_, 0.05f, 0.1f, 8.0f, "%.2f");
    ImGui::Text("Active Warning Count: %d", activeWarningCount_);
    ImGui::Text("Active Tracking Warning Count: %d", activeTrackingWarningCount_);
    ImGui::Text("Active Locked Warning Count: %d", activeLockedWarningCount_);
    ImGui::Text("Active Beam Count: %d", activeBeamCount_);
    ImGui::Text("Active Laser Instance Count: %d", activeLaserInstanceCount_);
    ImGui::Text("Dropped Laser Request Count: %llu", static_cast<unsigned long long>(droppedEffectCount_));
    ImGui::Text("Last Started Owner: 0x%p", lastStartedOwner_);
    ImGui::Text("Last Ended Owner: 0x%p", lastEndedOwner_);
    ImGui::Text("Last Origin: %.2f, %.2f, %.2f", lastOrigin_.x, lastOrigin_.y, lastOrigin_.z);
    ImGui::Text("Last Direction: %.2f, %.2f, %.2f", lastDirection_.x, lastDirection_.y, lastDirection_.z);
    ImGui::Text("Last Locked Target Position: %.2f, %.2f, %.2f", lastLockedTargetPosition_.x, lastLockedTargetPosition_.y, lastLockedTargetPosition_.z);
    ImGui::Text("Last Locked Direction: %.2f, %.2f, %.2f", lastLockedDirection_.x, lastLockedDirection_.y, lastLockedDirection_.z);
    ImGui::End();
#endif
}

EnemyLaserTelegraphController::LaserEffect* EnemyLaserTelegraphController::FindEffect(const void* owner, EffectType type) {
    for (LaserEffect& effect : effects_) {
        if (effect.active && effect.owner == owner && effect.type == type) {
            return &effect;
        }
    }
    return nullptr;
}

EnemyLaserTelegraphController::LaserEffect* EnemyLaserTelegraphController::AllocateEffect() {
    for (LaserEffect& effect : effects_) {
        if (!effect.active) {
            return &effect;
        }
    }
    return nullptr;
}

void EnemyLaserTelegraphController::DrawLayer(bool afterCloudLayer) {
    if (!initialized_ || !enableLaserTelegraph_ || !renderer_ || !renderer_->IsInitialized() || !camera_ || afterCloudLayer != drawAfterCloud_) {
        return;
    }
    const Vector3 cameraForward = ResolveCameraForward(camera_);
    const Vector3 cameraRight = ResolveCameraRight(camera_);
    const Vector3 cameraUp = ResolveCameraUp(camera_);
    std::vector<PlayerJetExhaustBeamRenderer::Vertex> vertices;
    vertices.reserve(effects_.size() * 12);

    float alphaScale = 0.0f;
    float brightness = 1.1f;
    for (const LaserEffect& effect : effects_) {
        if (!effect.active || effect.duration <= 0.0f) {
            continue;
        }
        const Vector3 direction = Normalize(effect.direction, cameraForward);
        Vector3 side = Normalize(Cross(direction, cameraForward), cameraRight);
        if (Length(side) <= kMinVectorLength) {
            side = Normalize(Cross(direction, cameraUp), cameraRight);
        }
        const float t = std::clamp(effect.age / effect.duration, 0.0f, 1.0f);
        if (effect.type == EffectType::Warning) {
            const float blinkRate = effect.aimLocked ? lockedBlinkRate_ : blinkRate_;
            const float warningAlpha = effect.aimLocked ? lockedWarningAlpha_ : warningAlpha_;
            const float blink = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(effect.age * blinkRate * std::numbers::pi_v<float> * 2.0f));
            AppendBeamQuad(vertices, effect.origin, direction, side, warningLineWidth_, warningLineLength_);
            alphaScale = (std::max)(alphaScale, warningAlpha * blink);
            brightness = (std::max)(brightness, 1.1f);
        } else {
            const float fade = 1.0f - t;
            AppendBeamQuad(vertices, effect.origin, direction, side, beamGlowWidth_, beamLength_);
            AppendBeamQuad(vertices, effect.origin, direction, side, beamCoreWidth_, beamLength_);
            alphaScale = (std::max)(alphaScale, (std::max)(beamAlpha_, beamGlowAlpha_) * fade);
            brightness = (std::max)(brightness, beamBrightness_);
        }
    }

    if (!vertices.empty()) {
        renderer_->Draw(vertices, camera_, brightness, std::clamp(alphaScale, 0.0f, 1.0f), 0.02f, 3.5f, 0.25f, time_, 3u);
    }
}

int EnemyLaserTelegraphController::CountActiveEffects(EffectType type) const {
    int count = 0;
    for (const LaserEffect& effect : effects_) {
        if (effect.active && effect.type == type) {
            ++count;
        }
    }
    return count;
}

int EnemyLaserTelegraphController::CountLockedWarnings(bool locked) const {
    int count = 0;
    for (const LaserEffect& effect : effects_) {
        if (effect.active && effect.type == EffectType::Warning && effect.aimLocked == locked) {
            ++count;
        }
    }
    return count;
}

int EnemyLaserTelegraphController::CountActiveOwners() const {
    std::unordered_set<const void*> owners;
    for (const LaserEffect& effect : effects_) {
        if (effect.active) {
            owners.insert(effect.owner);
        }
    }
    return static_cast<int>(owners.size());
}

void EnemyLaserTelegraphController::ClampSettings() {
    maxActiveEffects_ = std::clamp(maxActiveEffects_, 1, 64);
    if (effects_.size() != static_cast<size_t>(maxActiveEffects_)) {
        effects_.resize(static_cast<size_t>(maxActiveEffects_));
    }
    warningDuration_ = (std::max)(0.01f, warningDuration_);
    lockBeforeFireTime_ = std::clamp(lockBeforeFireTime_, 0.0f, (std::min)(0.4f, warningDuration_));
    blinkRate_ = (std::max)(0.1f, blinkRate_);
    lockedBlinkRate_ = (std::max)(0.1f, lockedBlinkRate_);
    warningLineWidth_ = (std::max)(0.001f, warningLineWidth_);
    warningLineLength_ = (std::max)(0.01f, warningLineLength_);
    warningAlpha_ = std::clamp(warningAlpha_, 0.0f, 1.0f);
    lockedWarningAlpha_ = std::clamp(lockedWarningAlpha_, 0.0f, 1.0f);
    beamDuration_ = (std::max)(0.01f, beamDuration_);
    beamCoreWidth_ = (std::max)(0.001f, beamCoreWidth_);
    beamGlowWidth_ = (std::max)(beamGlowWidth_, beamCoreWidth_);
    beamLength_ = (std::max)(0.01f, beamLength_);
    beamAlpha_ = std::clamp(beamAlpha_, 0.0f, 1.0f);
    beamGlowAlpha_ = std::clamp(beamGlowAlpha_, 0.0f, 1.0f);
    beamBrightness_ = std::clamp(beamBrightness_, 0.1f, 8.0f);
}



