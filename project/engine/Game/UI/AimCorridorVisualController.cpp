#include "AimCorridorVisualController.h"

#include "AimCorridorVisualRenderer.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Graphics/Camera/Camera.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float kFadeDuration = 0.2f;
    constexpr float kMinDistance = 0.1f;
    constexpr float kDistanceGap = 10.0f;
    constexpr float kMinWorldHeight = 0.01f;
    constexpr float kMaxWorldHeight = 100.0f;
    constexpr float kTwoPi = 6.28318530717958647692f;
    constexpr float kDefaultNearDistance = 28.0f;
    constexpr float kDefaultFarDistance = 70.0f;

    Vector3 Add(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 Scale(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    Vector3 Subtract(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    }

    float Dot(const Vector3& lhs, const Vector3& rhs) {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
    }

    float LengthSquared(const Vector3& value) {
        return Dot(value, value);
    }

    float LengthSquared(const Vector2& value) {
        return value.x * value.x + value.y * value.y;
    }

    bool IsFinite(const Vector2& value) {
        return std::isfinite(value.x) && std::isfinite(value.y);
    }

    bool IsFinite(const Vector3& value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    Vector2 ClampLength(const Vector2& value, float maxLength) {
        if (!IsFinite(value)) {
            return {};
        }
        const float lengthSquared = LengthSquared(value);
        if (lengthSquared <= maxLength * maxLength) {
            return value;
        }
        const float scale = maxLength / std::sqrt(lengthSquared);
        return { value.x * scale, value.y * scale };
    }

    Vector2 Lerp(const Vector2& current, const Vector2& target, float alpha) {
        return {
            current.x + (target.x - current.x) * alpha,
            current.y + (target.y - current.y) * alpha,
        };
    }

    float ComputeResponseAlpha(float deltaTime, float responseTime) {
        const float safeResponseTime = (std::max)(responseTime, 0.001f);
        return 1.0f - std::exp(-deltaTime / safeResponseTime);
    }

    float ClampFinite(float value, float minimum, float maximum, float fallback) {
        if (!std::isfinite(value)) {
            return std::clamp(fallback, minimum, maximum);
        }
        return std::clamp(value, minimum, maximum);
    }

    Vector3 NormalizeOr(const Vector3& value, const Vector3& fallback) {
        const float lengthSquared = LengthSquared(value);
        if (lengthSquared <= 0.000001f || !std::isfinite(lengthSquared)) {
            return fallback;
        }
        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        return Scale(value, inverseLength);
    }

    Vector3 GetCameraAxis(const Camera& camera, uint32_t row, const Vector3& fallback) {
        const Matrix4x4& world = camera.GetWorldMatrix();
        return NormalizeOr({ world.m[row][0], world.m[row][1], world.m[row][2] }, fallback);
    }

    float MoveToward(float current, float target, float maxDelta) {
        if (current < target) {
            return (std::min)(current + maxDelta, target);
        }
        return (std::max)(current - maxDelta, target);
    }
}

AimCorridorVisualController::AimCorridorVisualController() = default;

AimCorridorVisualController::~AimCorridorVisualController() = default;

bool AimCorridorVisualController::Initialize(DirectXCommon* dxCommon, Player* player, Camera* camera) {
    Finalize();
    dxCommon_ = dxCommon;
    player_ = player;
    camera_ = camera;
    axisMode_ = AxisMode::CameraThroughAimOrigin;
    freezePlayerPositionForDebug_ = false;
    freezeCameraForDebug_ = false;
    ResetVisualParameters();
    ResetLeadParameters();
    ResetPositionParameters();
    ResetLeadState();

    renderer_ = std::make_unique<AimCorridorVisualRenderer>();
    initialized_ = renderer_->Initialize(dxCommon_, nearTexturePath_, farTexturePath_);
    Reset();
    return initialized_;
}

void AimCorridorVisualController::Finalize() {
    if (renderer_) {
        renderer_->Finalize();
    }
    renderer_.reset();
    ResetLeadState();
    ResetBaseScreenOffsetRuntimeState();
    ResetPresentationRuntimeState();
    dxCommon_ = nullptr;
    player_ = nullptr;
    camera_ = nullptr;
    initialized_ = false;
    gameModeActive_ = false;
    isVisible_ = false;
    currentAlpha_ = 0.0f;
    lastDrawCount_ = 0;
    aimOriginProjectionValid_ = false;
    nearProjectionValid_ = false;
    farProjectionValid_ = false;
    worldPlacementValid_ = false;
    forceMoveInputForDebug_ = false;
}

void AimCorridorVisualController::Reset() {
    pulsePhase_ = 0.0f;
    currentAlpha_ = 0.0f;
    isVisible_ = false;
    lastDrawCount_ = 0;
    ResetLeadState();
    displayMode_ = DisplayMode::MainReticleOnly;
    debugDiamondPreview_ = false;
    reticleVisualState_ = AimReticleVisualState::Normal;
    targetingLockProgress_ = 0.0f;
    reticleTintAmount_ = 0.0f;
    ResetPresentationRuntimeState();
    SyncWorldPlacement(0.0f);
}

void AimCorridorVisualController::SetGameModeActive(bool active) {
    if (gameModeActive_ == active) {
        return;
    }
    gameModeActive_ = active;
    pulsePhase_ = 0.0f;
    ResetLeadState();
    debugDiamondPreview_ = false;
    reticleVisualState_ = AimReticleVisualState::Normal;
    targetingLockProgress_ = 0.0f;
    reticleTintAmount_ = 0.0f;
    if (active) {
        displayMode_ = DisplayMode::MainReticleOnly;
    }
    if (active) {
        currentAlpha_ = 0.0f;
        SyncWorldPlacement(0.0f);
    } else {
        currentAlpha_ = 0.0f;
        isVisible_ = false;
        lastDrawCount_ = 0;
        SyncWorldPlacement(0.0f);
    }
}

void AimCorridorVisualController::SetPlayerAlive(bool alive) {
    if (playerAlive_ == alive) {
        return;
    }
    playerAlive_ = alive;
    ResetLeadState();
    debugDiamondPreview_ = false;
    reticleVisualState_ = AimReticleVisualState::Normal;
    targetingLockProgress_ = 0.0f;
    reticleTintAmount_ = 0.0f;
    if (alive) {
        pulsePhase_ = 0.0f;
        SyncWorldPlacement(0.0f);
    }
}

void AimCorridorVisualController::Update(float unscaledDeltaTime) {
    if (!initialized_ || !player_ || !camera_) {
        currentAlpha_ = 0.0f;
        isVisible_ = false;
        worldPlacementValid_ = false;
        ResetLeadState();
        ResetBaseScreenOffsetRuntimeState();
        return;
    }

    const float deltaTime = std::clamp(
        std::isfinite(unscaledDeltaTime) ? unscaledDeltaTime : 0.0f,
        0.0f,
        0.1f);
    ClampParameters();
    UpdateTargetingAppearance(deltaTime);
    SyncWorldPlacement(deltaTime);

    if (!freezePulse_) {
        const float activePulseRate = displayMode_ == DisplayMode::MainReticleOnly ? mainReticlePulseRate_ : pulseRate_;
        pulsePhase_ = std::fmod(pulsePhase_ + deltaTime * activePulseRate * kTwoPi, kTwoPi);
    }

    const bool normallyVisible = enabled_ && gameModeActive_ && playerAlive_;
    const bool targetVisible = !forceHide_ && (normallyVisible || forceShow_);
    if (!gameModeActive_ && !forceShow_) {
        currentAlpha_ = 0.0f;
    } else {
        const float targetAlpha = targetVisible ? 1.0f : 0.0f;
        currentAlpha_ = MoveToward(currentAlpha_, targetAlpha, deltaTime / kFadeDuration);
    }
    isVisible_ = currentAlpha_ > 0.001f && HasDrawableTexture();
}

void AimCorridorVisualController::Draw() {
    lastDrawCount_ = 0;
    if (!isVisible_ || !worldPlacementValid_ || !renderer_ || !camera_) {
        return;
    }

    DrawPresentation(std::sin(pulsePhase_));
}

void AimCorridorVisualController::SyncWorldPlacement(float unscaledDeltaTime) {
    worldPlacementValid_ = false;
    aimOriginProjectionValid_ = false;
    nearProjectionValid_ = false;
    farProjectionValid_ = false;
    if (!player_ || !camera_) {
        ResetBaseScreenOffsetRuntimeState();
        return;
    }

    const Vector3 liveCameraPosition = camera_->GetTranslate();
    const Vector3 liveCameraRight = GetCameraAxis(*camera_, 0, { 1.0f, 0.0f, 0.0f });
    const Vector3 liveCameraUp = GetCameraAxis(*camera_, 1, { 0.0f, 1.0f, 0.0f });
    const Vector3 liveCameraForward = GetCameraAxis(*camera_, 2, { 0.0f, 0.0f, 1.0f });
    const Vector3 livePlayerRenderPosition = player_->GetWorldPosition();

    cameraPosition_ = freezeCameraForDebug_ ? frozenCameraPosition_ : liveCameraPosition;
    cameraRight_ = freezeCameraForDebug_ ? frozenCameraRight_ : liveCameraRight;
    cameraUp_ = freezeCameraForDebug_ ? frozenCameraUp_ : liveCameraUp;
    cameraForward_ = freezeCameraForDebug_ ? frozenCameraForward_ : liveCameraForward;
    playerRenderPosition_ = freezePlayerPositionForDebug_ ? frozenPlayerRenderPosition_ : livePlayerRenderPosition;
    aimOrigin_ = Add(
        Add(
            Add(playerRenderPosition_, Scale(cameraRight_, originOffset_.x)),
            Scale(cameraUp_, originOffset_.y)),
        Scale(cameraForward_, originOffset_.z));

    const Vector3 cameraThroughAimRay = Subtract(aimOrigin_, cameraPosition_);
    const float rayLengthSquared = LengthSquared(cameraThroughAimRay);
    aimRayLength_ = (rayLengthSquared > 0.000001f && std::isfinite(rayLengthSquared))
        ? std::sqrt(rayLengthSquared)
        : 0.0f;
    const Vector3 cameraThroughAimForward = NormalizeOr(cameraThroughAimRay, cameraForward_);
    aimForward_ = axisMode_ == AxisMode::CameraThroughAimOrigin ? cameraThroughAimForward : cameraForward_;
    aimForwardDotCameraForward_ = std::clamp(Dot(aimForward_, cameraForward_), -1.0f, 1.0f);

    nearBaseCenter_ = Add(aimOrigin_, Scale(aimForward_, nearDistance_));
    farBaseCenter_ = Add(aimOrigin_, Scale(aimForward_, farDistance_));
    UpdateLeadLag(unscaledDeltaTime);

    leadFovY_ = camera_->GetFovY();
    leadAspectRatio_ = camera_->GetAspectRatio();
    nearLeadDepth_ = Dot(Subtract(nearBaseCenter_, cameraPosition_), cameraForward_);
    farLeadDepth_ = Dot(Subtract(farBaseCenter_, cameraPosition_), cameraForward_);
    UpdateDepthAppearance();
    UpdateBaseScreenOffsetLayout();

    nearCenter_ = Add(
        Add(Add(nearBaseCenter_, nearFinalScreenWorldOffset_), Scale(cameraRight_, nearOffset_.x)),
        Scale(cameraUp_, nearOffset_.y));
    farCenter_ = Add(
        Add(Add(farBaseCenter_, farFinalScreenWorldOffset_), Scale(cameraRight_, farOffset_.x)),
        Scale(cameraUp_, farOffset_.y));

    worldPlacementValid_ = IsFinite(aimOrigin_) && IsFinite(aimForward_)
        && IsFinite(nearCenter_) && IsFinite(farCenter_)
        && IsFinite(cameraRight_) && IsFinite(cameraUp_);
    if (!worldPlacementValid_) {
        nearCenter_ = {};
        farCenter_ = {};
        ResetBaseScreenOffsetRuntimeState();
        return;
    }

    const float nearAspect = renderer_ ? renderer_->GetNearAspectRatio() : 1.0f;
    const float farAspect = renderer_ ? renderer_->GetFarAspectRatio() : 1.0f;
    nearWorldWidth_ = nearWorldHeight_ * ((std::isfinite(nearAspect) && nearAspect > 0.0f) ? nearAspect : 1.0f);
    farWorldWidth_ = effectiveFarWorldHeight_ * ((std::isfinite(farAspect) && farAspect > 0.0f) ? farAspect : 1.0f);
    UpdatePresentation();
    UpdateProjectionDebug();
}

void AimCorridorVisualController::UpdateLeadLag(float unscaledDeltaTime) {
    rawPlayerMoveInput_ = player_ ? player_->GetMoveInput() : Vector2{};
    if (!IsFinite(rawPlayerMoveInput_)) {
        rawPlayerMoveInput_ = {};
    }

    Vector2 selectedMoveInput = forceMoveInputForDebug_ ? forcedMoveInputForDebug_ : rawPlayerMoveInput_;
    if ((!gameModeActive_ && !forceShow_) || !playerAlive_) {
        selectedMoveInput = {};
    }
    normalizedMoveInput_ = ClampLength(selectedMoveInput, 1.0f);

    if (!leadLagEnabled_) {
        nearLeadTargetScreen_ = {};
        nearLeadCurrentScreen_ = {};
        farLeadTargetScreen_ = {};
        farLeadCurrentScreen_ = {};
        return;
    }
    if (freezeLeadState_) {
        return;
    }

    nearLeadTargetScreen_ = {
        normalizedMoveInput_.x * nearLeadAmountX_,
        normalizedMoveInput_.y * nearLeadAmountY_,
    };
    farLeadTargetScreen_ = {
        normalizedMoveInput_.x * farLeadAmountX_,
        normalizedMoveInput_.y * farLeadAmountY_,
    };

    const bool nearReturning = LengthSquared(nearLeadTargetScreen_) <= 0.000001f;
    const bool farReturning = LengthSquared(farLeadTargetScreen_) <= 0.000001f;
    const float nearAlpha = ComputeResponseAlpha(
        unscaledDeltaTime,
        nearReturning ? nearReturnTime_ : nearResponseTime_);
    const float farAlpha = ComputeResponseAlpha(
        unscaledDeltaTime,
        farReturning ? farReturnTime_ : farResponseTime_);
    nearLeadCurrentScreen_ = Lerp(nearLeadCurrentScreen_, nearLeadTargetScreen_, nearAlpha);
    farLeadCurrentScreen_ = Lerp(farLeadCurrentScreen_, farLeadTargetScreen_, farAlpha);

    if (!IsFinite(nearLeadCurrentScreen_)) {
        nearLeadCurrentScreen_ = {};
    }
    if (!IsFinite(farLeadCurrentScreen_)) {
        farLeadCurrentScreen_ = {};
    }
    nearLeadCurrentScreen_.x = std::clamp(nearLeadCurrentScreen_.x, -0.10f, 0.10f);
    nearLeadCurrentScreen_.y = std::clamp(nearLeadCurrentScreen_.y, -0.08f, 0.08f);
    farLeadCurrentScreen_.x = std::clamp(farLeadCurrentScreen_.x, -0.18f, 0.18f);
    farLeadCurrentScreen_.y = std::clamp(farLeadCurrentScreen_.y, -0.14f, 0.14f);
}

Vector3 AimCorridorVisualController::ConvertScreenOffsetToWorld(const Vector2& screenOffset, float depth) const {
    if (!IsFinite(screenOffset) || !std::isfinite(depth) || depth <= 0.0f
        || !std::isfinite(leadFovY_) || leadFovY_ <= 0.0f || leadFovY_ >= 3.14159265358979323846f
        || !std::isfinite(leadAspectRatio_) || leadAspectRatio_ <= 0.0f) {
        return {};
    }

    const float halfHeightAtDepth = std::tan(leadFovY_ * 0.5f) * depth;
    const float halfWidthAtDepth = halfHeightAtDepth * leadAspectRatio_;
    if (!std::isfinite(halfHeightAtDepth) || !std::isfinite(halfWidthAtDepth)) {
        return {};
    }

    const Vector3 worldOffset = Add(
        Scale(cameraRight_, screenOffset.x * halfWidthAtDepth),
        Scale(cameraUp_, screenOffset.y * halfHeightAtDepth));
    return IsFinite(worldOffset) ? worldOffset : Vector3{};
}

void AimCorridorVisualController::UpdateDepthAppearance() {
    depthAppearanceValid_ = std::isfinite(nearLeadDepth_) && nearLeadDepth_ > 0.0001f
        && std::isfinite(farLeadDepth_) && farLeadDepth_ > 0.0001f
        && std::isfinite(nearWorldHeight_) && nearWorldHeight_ > 0.0f;
    effectiveFarWorldHeight_ = farWorldHeight_;
    if (autoFarSizeFromScreenRatio_ && depthAppearanceValid_) {
        const float automaticFarHeight = nearWorldHeight_
            * (farLeadDepth_ / nearLeadDepth_)
            * targetFarToNearScreenHeightRatio_;
        effectiveFarWorldHeight_ = ClampFinite(
            automaticFarHeight,
            kMinWorldHeight,
            kMaxWorldHeight,
            farWorldHeight_);
    }

    if (!depthAppearanceValid_ || !std::isfinite(effectiveFarWorldHeight_) || effectiveFarWorldHeight_ <= 0.0f) {
        effectiveFarWorldHeight_ = farWorldHeight_;
        nearProjectedHeightEstimate_ = 0.0f;
        farProjectedHeightEstimate_ = 0.0f;
        currentFarToNearScreenHeightRatio_ = 0.0f;
        depthAppearanceRatioError_ = -targetFarToNearScreenHeightRatio_;
        return;
    }

    nearProjectedHeightEstimate_ = nearWorldHeight_ / nearLeadDepth_;
    farProjectedHeightEstimate_ = effectiveFarWorldHeight_ / farLeadDepth_;
    currentFarToNearScreenHeightRatio_ = nearProjectedHeightEstimate_ > 0.000001f
        ? farProjectedHeightEstimate_ / nearProjectedHeightEstimate_
        : 0.0f;
    depthAppearanceRatioError_ = currentFarToNearScreenHeightRatio_ - targetFarToNearScreenHeightRatio_;
}

void AimCorridorVisualController::UpdateProjectionDebug() {
    aimOriginProjectionValid_ = ProjectWorldToScreenUv(aimOrigin_, aimOriginScreenUv_);
    nearProjectionValid_ = ProjectWorldToScreenUv(nearCenter_, nearScreenUv_);
    farProjectionValid_ = ProjectWorldToScreenUv(farCenter_, farScreenUv_);
    nearScreenDelta_ = (aimOriginProjectionValid_ && nearProjectionValid_)
        ? Vector2{ nearScreenUv_.x - aimOriginScreenUv_.x, nearScreenUv_.y - aimOriginScreenUv_.y }
        : Vector2{};
    farScreenDelta_ = (aimOriginProjectionValid_ && farProjectionValid_)
        ? Vector2{ farScreenUv_.x - aimOriginScreenUv_.x, farScreenUv_.y - aimOriginScreenUv_.y }
        : Vector2{};
    UpdateBaseScreenOffsetDebug();
}

bool AimCorridorVisualController::ProjectWorldToScreenUv(const Vector3& worldPosition, Vector2& screenUv) const {
    screenUv = {};
    if (!camera_) {
        return false;
    }

    const Matrix4x4& viewProjection = camera_->GetViewProjectionMatrix();
    const float clipX = worldPosition.x * viewProjection.m[0][0] + worldPosition.y * viewProjection.m[1][0]
        + worldPosition.z * viewProjection.m[2][0] + viewProjection.m[3][0];
    const float clipY = worldPosition.x * viewProjection.m[0][1] + worldPosition.y * viewProjection.m[1][1]
        + worldPosition.z * viewProjection.m[2][1] + viewProjection.m[3][1];
    const float clipW = worldPosition.x * viewProjection.m[0][3] + worldPosition.y * viewProjection.m[1][3]
        + worldPosition.z * viewProjection.m[2][3] + viewProjection.m[3][3];
    if (clipW <= 0.0001f || !std::isfinite(clipW)) {
        return false;
    }

    const float ndcX = clipX / clipW;
    const float ndcY = clipY / clipW;
    if (!std::isfinite(ndcX) || !std::isfinite(ndcY)) {
        return false;
    }
    screenUv = { ndcX * 0.5f + 0.5f, -ndcY * 0.5f + 0.5f };
    return true;
}

void AimCorridorVisualController::ClampParameters() {
    nearDistance_ = ClampFinite(nearDistance_, kMinDistance, 1000.0f, kDefaultNearDistance);
    farDistance_ = ClampFinite(farDistance_, nearDistance_ + kDistanceGap, 2000.0f, kDefaultFarDistance);
    nearWorldHeight_ = ClampFinite(nearWorldHeight_, kMinWorldHeight, kMaxWorldHeight, 5.0f);
    farWorldHeight_ = ClampFinite(farWorldHeight_, kMinWorldHeight, kMaxWorldHeight, 7.5f);
    targetFarToNearScreenHeightRatio_ = ClampFinite(targetFarToNearScreenHeightRatio_, 0.55f, 0.65f, 0.60f);
    ClampBaseScreenOffsetParameters();
    ClampPresentationParameters();
    pulseRate_ = std::clamp(pulseRate_, 0.0f, 10.0f);
    nearLeadAmountX_ = ClampFinite(nearLeadAmountX_, 0.0f, 0.10f, 0.055f);
    nearLeadAmountY_ = ClampFinite(nearLeadAmountY_, 0.0f, 0.08f, 0.045f);
    farLeadAmountX_ = ClampFinite(farLeadAmountX_, 0.0f, 0.18f, 0.110f);
    farLeadAmountY_ = ClampFinite(farLeadAmountY_, 0.0f, 0.14f, 0.085f);
    nearResponseTime_ = ClampFinite(nearResponseTime_, 0.001f, 2.0f, 0.120f);
    farResponseTime_ = ClampFinite(farResponseTime_, 0.001f, 2.0f, 0.055f);
    nearReturnTime_ = ClampFinite(nearReturnTime_, 0.001f, 2.0f, 0.160f);
    farReturnTime_ = ClampFinite(farReturnTime_, 0.001f, 2.0f, 0.110f);

    auto clampAppearance = [](AppearanceParameters& appearance) {
        appearance.alpha = std::clamp(appearance.alpha, 0.0f, 1.0f);
        appearance.coreIntensity = std::clamp(appearance.coreIntensity, 0.0f, 2.0f);
        appearance.glowIntensity = std::clamp(appearance.glowIntensity, 0.0f, 2.0f);
        appearance.glowAlpha = std::clamp(appearance.glowAlpha, 0.0f, 1.0f);
        appearance.glowRadiusTexels = std::clamp(appearance.glowRadiusTexels, 0.0f, 4.0f);
        appearance.pulseAmount = std::clamp(appearance.pulseAmount, 0.0f, 0.25f);
    };
    clampAppearance(nearAppearance_);
    clampAppearance(farAppearance_);
}

void AimCorridorVisualController::ResetVisualParameters() {
    nearAppearance_ = { 0.82f, 1.15f, 0.75f, 0.38f, 1.5f, 0.08f };
    farAppearance_ = { 0.62f, 0.92f, 0.48f, 0.25f, 1.2f, 0.04f };
    pulseRate_ = 1.4f;
    freezePulse_ = false;
    disableGlow_ = false;
    showCoreOnly_ = false;
    forceFullAlpha_ = false;
    ResetDepthAppearanceParameters();
    ResetPresentationParameters();
}

void AimCorridorVisualController::ResetPositionParameters() {
    useNewDepthDefaults_ = true;
    originOffset_ = {};
    nearOffset_ = {};
    farOffset_ = {};
    nearDistance_ = kDefaultNearDistance;
    farDistance_ = kDefaultFarDistance;
    ResetBaseScreenOffsetParameters();
    ClampParameters();
    ResetLeadState();
    SyncWorldPlacement(0.0f);
}

void AimCorridorVisualController::ResetLeadParameters() {
    leadLagEnabled_ = true;
    nearLeadAmountX_ = 0.055f;
    nearLeadAmountY_ = 0.045f;
    farLeadAmountX_ = 0.110f;
    farLeadAmountY_ = 0.085f;
    nearResponseTime_ = 0.120f;
    farResponseTime_ = 0.055f;
    nearReturnTime_ = 0.160f;
    farReturnTime_ = 0.110f;
    freezeLeadState_ = false;
    forceMoveInputForDebug_ = false;
    forcedMoveInputForDebug_ = {};
    ClampParameters();
}

void AimCorridorVisualController::ResetLeadState() {
    rawPlayerMoveInput_ = {};
    normalizedMoveInput_ = {};
    nearLeadTargetScreen_ = {};
    nearLeadCurrentScreen_ = {};
    farLeadTargetScreen_ = {};
    farLeadCurrentScreen_ = {};
    nearLeadWorldOffset_ = {};
    farLeadWorldOffset_ = {};
}

void AimCorridorVisualController::ResetDepthAppearanceParameters() {
    autoFarSizeFromScreenRatio_ = true;
    targetFarToNearScreenHeightRatio_ = 0.60f;
    farWorldHeight_ = 7.5f;
    effectiveFarWorldHeight_ = 7.5f;
    nearProjectedHeightEstimate_ = 0.0f;
    farProjectedHeightEstimate_ = 0.0f;
    currentFarToNearScreenHeightRatio_ = 0.0f;
    depthAppearanceRatioError_ = 0.0f;
    depthAppearanceValid_ = false;
}

bool AimCorridorVisualController::HasDrawableTexture() const {
    return renderer_ && (renderer_->IsNearTextureLoaded() || renderer_->IsFarTextureLoaded());
}
