#include "RailShooterCameraRig.h"

#include "Engine/Game/Player/Player.h"
#include "Engine/Game/Player/FlightTuning.h"
#include "Engine/Game/Player/BoostController.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Game/RailShooter/RailPathRuntimeV2.h"
#include "Engine/Game/RailShooter/RailPathRuntimeV2Adapter.h"
#include "Engine/Level/LevelRailRuntime.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace {
constexpr float kMinimumVectorLengthSquared = 0.0000001f;
constexpr float kMinimumRailLength = 0.0001f;
constexpr float kRadiansToDegrees = 180.0f / 3.14159265358979323846f;

Vector3 Add(const Vector3& a, const Vector3& b) {
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

Vector3 Subtract(const Vector3& a, const Vector3& b) {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

Vector3 Scale(const Vector3& value, float scale) {
    return { value.x * scale, value.y * scale, value.z * scale };
}

float LengthSquared(const Vector3& value) {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

bool IsFinite(const Vector3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool IsUsableForward(const Vector3& value) {
    return IsFinite(value) && LengthSquared(value) > kMinimumVectorLengthSquared;
}

Vector3 Normalize(const Vector3& value, const Vector3& fallback) {
    if (!IsUsableForward(value)) return fallback;
    const float inverseLength = 1.0f / std::sqrt(LengthSquared(value));
    return Scale(value, inverseLength);
}

float Dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 Cross(const Vector3& a, const Vector3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

void BuildBasis(const Vector3& forward, Vector3& outRight, Vector3& outUp) {
    const Vector3 safeForward = Normalize(forward, { 0.0f, 0.0f, 1.0f });
    Vector3 preferredUp{ 0.0f, 1.0f, 0.0f };
    if (std::fabs(Dot(safeForward, preferredUp)) > 0.98f) {
        preferredUp = { 0.0f, 0.0f, 1.0f };
    }
    outRight = Normalize(Cross(preferredUp, safeForward), { 1.0f, 0.0f, 0.0f });
    outUp = Normalize(Cross(safeForward, outRight), { 0.0f, 1.0f, 0.0f });
}

float WrapDistance(float distance, float totalLength) {
    if (totalLength <= kMinimumRailLength) return 0.0f;
    float wrapped = std::fmod(distance, totalLength);
    if (wrapped < 0.0f) wrapped += totalLength;
    return wrapped;
}
}

bool RailShooterCameraRig::EnsureRuntimeV2Trial() {
    if (!enableRuntimeV2Trial_) {
        runtimeV2BuildSucceeded_ = false;
        runtimeV2BuildResult_ = "Runtime V2試験接続が無効です。";
        return false;
    }
    if (!railRuntime_ || selectedRailIndex_ < 0 || selectedRailId_.empty()) {
        runtimeV2BuildSucceeded_ = false;
        runtimeV2BuildResult_ = "確認対象Railを解決できません。";
        return false;
    }
    if (!runtimeV2Trial_) runtimeV2Trial_ = std::make_unique<RailPathRuntimeV2>();
    const uint64_t rebuildCount = railRuntime_->GetRebuildCount();
    if (runtimeV2BuildSucceeded_ && runtimeV2Trial_->IsValid() &&
        runtimeV2BuiltRailId_ == selectedRailId_ && runtimeV2BuiltRebuildCount_ == rebuildCount) {
        return true;
    }

    std::vector<Vector3> points;
    LevelRailSampleTable sampleTable;
    if (!railRuntime_->CopyRailBuildInput(static_cast<size_t>(selectedRailIndex_), points, sampleTable)) {
        runtimeV2Trial_->Clear();
        runtimeV2BuildSucceeded_ = false;
        runtimeV2BuildResult_ = "LevelRailRuntimeから変換済みRailを取得できません。";
        return false;
    }
    runtimeV2BuildSucceeded_ = RailPathRuntimeV2Adapter::BuildFromWaypointPositions(
        *runtimeV2Trial_, points, sampleTable, "Camera試験接続（座標変換・反転適用後）");
    runtimeV2BuiltRailId_ = selectedRailId_;
    runtimeV2BuiltRebuildCount_ = rebuildCount;
    hasPreviousRailForward_ = false;
    runtimeV2PoseValid_ = false;
    runtimeV2BuildResult_ = runtimeV2BuildSucceeded_
        ? "Runtime V2のBuildに成功しました。"
        : "Runtime V2のBuildに失敗したためLegacy Railへ復帰します。";
    return runtimeV2BuildSucceeded_;
}

RailShooterCameraRig::RailCameraPose RailShooterCameraRig::BuildRailCameraPose(
    float railDistance, bool loopEnabled, const LevelRailEvaluation& legacyEvaluation) {
    RailCameraPose pose;
    if (!EnsureRuntimeV2Trial() || !runtimeV2Trial_ || !runtimeV2Trial_->IsValid()) {
        forwardFallbackReason_ = runtimeV2BuildResult_;
        return pose;
    }

    const float totalLength = runtimeV2Trial_->GetTotalLength();
    const float currentDistance = loopEnabled
        ? WrapDistance(railDistance, totalLength)
        : std::clamp(railDistance, 0.0f, totalLength);
    const float aheadRawDistance = currentDistance + std::clamp(runtimeV2LookAheadDistance_, 0.1f, 100.0f);
    const float aheadDistance = loopEnabled
        ? WrapDistance(aheadRawDistance, totalLength)
        : std::clamp(aheadRawDistance, 0.0f, totalLength);
    const RailPathSample currentSample = runtimeV2Trial_->SampleByDistance(currentDistance);
    const RailPathSample aheadSample = runtimeV2Trial_->SampleByDistance(aheadDistance);
    if (!currentSample.valid || !IsFinite(currentSample.position)) {
        forwardFallbackReason_ = "Current Sampleを取得できないためLegacy Railへ復帰しました。";
        return pose;
    }

    Vector3 forward{};
    if (aheadSample.valid) {
        forward = Subtract(aheadSample.position, currentSample.position);
        forwardFallbackReason_ = "なし（Current / Ahead位置差を使用）";
    } else {
        forwardFallbackReason_ = "Ahead Sampleを取得できないためCurrent Tangentを使用しました。";
    }
    if (!IsUsableForward(forward)) {
        forward = currentSample.tangent;
        forwardFallbackReason_ = "Current / Ahead位置差が小さいためCurrent Tangentを使用しました。";
    }
    if (!IsUsableForward(forward) && hasPreviousRailForward_) {
        forward = previousRailForward_;
        forwardFallbackReason_ = "Tangentが無効なため前フレームのRail前方向を使用しました。";
    }
    if (!IsUsableForward(forward) && legacyEvaluation.valid) {
        forward = legacyEvaluation.forward;
        forwardFallbackReason_ = "前回方向も無効なためLegacy Camera Forwardを使用しました。";
    }
    if (!IsUsableForward(forward)) {
        forward = { 0.0f, 0.0f, 1.0f };
        forwardFallbackReason_ = "有効な方向を取得できないためGame World Forwardを使用しました。";
    }

    pose.position = currentSample.position;
    pose.aheadPosition = aheadSample.valid ? aheadSample.position : Add(currentSample.position, forward);
    pose.forward = Normalize(forward, { 0.0f, 0.0f, 1.0f });
    pose.up = { 0.0f, 1.0f, 0.0f };
    pose.railDistance = currentDistance;
    pose.valid = IsFinite(pose.position) && IsFinite(pose.forward);
    if (pose.valid) {
        previousRailForward_ = pose.forward;
        hasPreviousRailForward_ = true;
    }
    return pose;
}

bool RailShooterCameraRig::ApplyRuntimeV2Pose(
    const LevelRailEvaluation& legacyEvaluation, float deltaTime, bool loopEnabled) {
    if (forceLegacyForward_) {
        runtimeV2PoseValid_ = false;
        forwardFallbackReason_ = "デバッグ指定によりLegacy Railへ復帰しました。";
        return false;
    }
    railCameraPose_ = BuildRailCameraPose(railDistance_, loopEnabled, legacyEvaluation);
    runtimeV2PoseValid_ = railCameraPose_.valid;
    if (!runtimeV2PoseValid_) return false;

    currentPosition_ = railCameraPose_.position;
    currentForward_ = SmoothCameraForward(railCameraPose_.forward, deltaTime);
    BuildBasis(currentForward_, currentRight_, currentUp_);
    railDistance_ = railCameraPose_.railDistance;
    railT_ = GetActiveRailTotalLength() > kMinimumRailLength
        ? std::clamp(railDistance_ / GetActiveRailTotalLength(), 0.0f, 1.0f) : 0.0f;
    ++runtimeV2PoseApplyCount_;
    activePoseSource_ = "Runtime V2";
    if (pendingGameModePoseSync_) {
        ++gameModePoseSyncCount_;
        pendingGameModePoseSync_ = false;
    }
    return true;
}

bool RailShooterCameraRig::BuildRuntimeV2VisualPose() {
    if (!runtimeV2PoseValid_) return false;
    visualCameraPosition_ = currentPosition_;
    if (enableAngledPlayerCamera_) {
        visualCameraPosition_ = Add(
            Add(currentPosition_, Scale(currentUp_, cameraHeightOffset_)),
            Scale(currentRight_, cameraSideOffset_));
    }
    cameraLookTarget_ = Add(
        visualCameraPosition_,
        Scale(currentForward_, std::clamp(cameraLookDistance_, 1.0f, 1000.0f)));
    visualCameraForward_ = Normalize(
        Subtract(cameraLookTarget_, visualCameraPosition_), currentForward_);
    railCameraForwardDot_ = Dot(railCameraPose_.forward, visualCameraForward_);
    playerDisplayForward_ = player_ ? player_->GetBaseForward() : railCameraPose_.forward;
    railPlayerForwardDot_ = Dot(railCameraPose_.forward, Normalize(playerDisplayForward_, railCameraPose_.forward));
    cameraPitchDegrees_ = ComputePitchDegrees(visualCameraForward_);
    playerPitchDegrees_ = ComputePitchDegrees(playerDisplayForward_);
    return true;
}

void RailShooterCameraRig::ResetRuntimeV2PoseState(bool clearRuntime) {
    runtimeV2PoseValid_ = false;
    railCameraPose_ = {};
    hasPreviousRailForward_ = false;
    previousRailForward_ = { 0.0f, 0.0f, 1.0f };
    forwardFallbackReason_ = "未評価";
    activePoseSource_ = "未評価";
    pendingGameModePoseSync_ = false;
    if (clearRuntime && runtimeV2Trial_) {
        runtimeV2Trial_->Clear();
        runtimeV2BuildSucceeded_ = false;
        runtimeV2BuiltRailId_.clear();
        runtimeV2BuiltRebuildCount_ = 0;
        runtimeV2BuildResult_ = "未構築";
    }
}

void RailShooterCameraRig::ForceResyncRuntimeV2Pose() {
    InvalidateProjectileRailContinuity();
    hasPreviousRailForward_ = false;
    hasSmoothedForward_ = false;
    if (mode_ == Mode::Rail && IsCameraRigActive()) UpdateRail(0.0f);
}

float RailShooterCameraRig::GetActiveRailTotalLength() const {
    if (runtimeV2BuildSucceeded_ && runtimeV2Trial_ && runtimeV2Trial_->IsValid()) {
        return runtimeV2Trial_->GetTotalLength();
    }
    return selectedRailTotalLength_;
}

float RailShooterCameraRig::ComputePitchDegrees(const Vector3& forward) {
    const Vector3 safe = Normalize(forward, { 0.0f, 0.0f, 1.0f });
    const float horizontal = std::sqrt(safe.x * safe.x + safe.z * safe.z);
    return std::atan2(-safe.y, horizontal) * kRadiansToDegrees;
}

bool RailShooterCameraRig::UsesGameplayComposition() const {
    // Rail advancement is optional. Game Mode owns this same render camera
    // even when no rail exists, is disabled, or cannot be evaluated.
    return gameModeActive_ && player_ && camera_;
}

bool RailShooterCameraRig::IsGameplayPresentationActive() const {
    // Midboss combat can retain its FOV framing without owning normal input/attitude.
    return presentationOverrideActive_ || presentationReturnActive_;
}

void RailShooterCameraRig::ResetGameplayInputHistory(bool resetRotation) {
    pilotHistoryBegin_ = pilotHistoryCount_ = 0;
    pilotInputClock_ = 0;
    pilotLastInput_ = pilotDelayedInput_ = {};
    if (resetRotation) playerLookInput_ = {};
}

void RailShooterCameraRig::UpdatePlayerFollow(float deltaTime, float unscaledDeltaTime) {
    if (!UsesGameplayComposition()) {
        playerFollowInitialized_ = false;
        ResetGameplayInputHistory();
        if (camera_) camera_->SetViewRotationOffset({});
        return;
    }
    const bool presentation = IsGameplayPresentationActive();
    const Vector2 offset = player_->GetPresentationOffset();
    const Vector2 followTarget = presentationOverrideActive_ ? offset : Vector2{};
    Vector2 input = presentation ? Vector2{} : player_->GetMoveInput();
    const float length = (std::max)(1.0f, std::sqrt(input.x * input.x + input.y * input.y));
    input.x /= length;
    input.y /= length;
    if (!playerFollowInitialized_) {
        playerFollowOffset_ = followTarget;
        ResetGameplayInputHistory();
        playerFollowInitialized_ = true;
    }
    const float dt = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    const auto& tuning = FlightTuning::Get();
    const float elapsed = unscaledDeltaTime < 0 ? deltaTime : unscaledDeltaTime;
    const float realDt = std::isfinite(elapsed) ? (std::max)(0.0f, elapsed) : 0.0f;
    if (std::fabs(input.x) < 0.04f) input.x = 0;
    if (std::fabs(input.y) < 0.04f) input.y = 0;
    // Store input transitions, bounded independently of frame rate. Integrate
    // the response in real time, splitting frames at delayed transition times.
    const bool suspended = presentation || !player_->UsesWASDInput() || dt <= 0 || realDt <= 0;
    if (suspended) {
        ResetGameplayInputHistory(dt <= 0 || realDt <= 0);
        input = {};
    }
    if (input.x != pilotLastInput_.x || input.y != pilotLastInput_.y) {
        if (pilotHistoryCount_ == pilotInputHistory_.size()) {
            pilotHistoryBegin_ = (pilotHistoryBegin_ + 1) % pilotInputHistory_.size();
            --pilotHistoryCount_;
        }
        pilotInputHistory_[(pilotHistoryBegin_ + pilotHistoryCount_) % pilotInputHistory_.size()] =
            { pilotInputClock_, input };
        ++pilotHistoryCount_;
        pilotLastInput_ = input;
    }
    const double frameEnd = pilotInputClock_ + realDt;
    const auto filter = [this, &tuning](double seconds) {
        const float duration = static_cast<float>((std::max)(0.0, seconds));
        playerLookInput_.x += (pilotDelayedInput_.x - playerLookInput_.x) *
            (1.0f - std::exp(-duration / (std::max)(.001f, tuning.cameraRollTime)));
        playerLookInput_.y += (pilotDelayedInput_.y - playerLookInput_.y) *
            (1.0f - std::exp(-duration / (std::max)(.001f, tuning.cameraPitchTime)));
    };
    while (pilotHistoryCount_ > 0) {
        const auto& sample = pilotInputHistory_[pilotHistoryBegin_];
        const double activation = (std::max)(pilotInputClock_, sample.time + tuning.cameraInputDelay);
        if (activation > frameEnd) break;
        filter(activation - pilotInputClock_);
        pilotInputClock_ = activation;
        pilotDelayedInput_ = sample.input;
        pilotHistoryBegin_ = (pilotHistoryBegin_ + 1) % pilotInputHistory_.size();
        --pilotHistoryCount_;
    }
    filter(frameEnd - pilotInputClock_);
    pilotInputClock_ = frameEnd;
    // Normal XY movement never translates the camera. Dedicated framing owns its
    // existing follow; its return blends in the same filter until it releases.
    if (presentation) {
        const float followAlpha = 1.0f - std::exp(-dt / playerFollowTime_);
        playerFollowOffset_.x += (followTarget.x - playerFollowOffset_.x) * followAlpha;
        playerFollowOffset_.y += (followTarget.y - playerFollowOffset_.y) * followAlpha;
        if (presentationReturnActive_ && std::fabs(playerFollowOffset_.x) < 0.00001f
            && std::fabs(playerFollowOffset_.y) < 0.00001f) {
            presentationReturnActive_ = false;
            playerFollowOffset_ = {};
        }
    } else {
        playerFollowOffset_ = {};
    }
    ApplyToCamera(dt);
}

bool RailShooterCameraRig::BuildGameplayVisualPose([[maybe_unused]] float followDeltaTime) {
    if (!UsesGameplayComposition()) return false;
    const Vector3 anchor = Add(Add(currentPosition_, Scale(currentUp_, cameraHeightOffset_)),
        Scale(currentForward_, player_->GetDistanceFromCamera()));
    player_->SetBaseMode(Player::BaseMode::Rail);
    player_->SetExternalBasePosition(anchor);
    player_->SetExternalBaseForward(currentForward_);
    player_->SetExternalBaseUp(currentUp_);

    const float aspect = (std::max)(0.1f, camera_->GetAspectRatio());
    const float distanceScale = (std::max)(1.0f, (16.0f / 9.0f) / aspect);
    const auto& tuning = FlightTuning::Get();
    const float distance = tuning.cameraDistance * distanceScale;
    const float height = gameplayCameraHeight_ * distanceScale;
    const Vector3 basePosition = Add(Add(anchor, Scale(currentForward_, -distance)), Scale(currentUp_, height));
    gameplayReferenceFrame_ = MatrixMath::MakeIdentity4x4();
    const Vector3 axes[] = { currentRight_, currentUp_, currentForward_, basePosition };
    for (int row = 0; row < 4; ++row) {
        gameplayReferenceFrame_.m[row][0] = axes[row].x;
        gameplayReferenceFrame_.m[row][1] = axes[row].y;
        gameplayReferenceFrame_.m[row][2] = axes[row].z;
    }
    constexpr float radians = 3.14159265358979323846f / 180.0f;
    const float renderFov = 2.0f * std::atan(std::tan(gameplayBaseFovY_ * 0.5f) * tuning.viewExpansion);
    if (!IsGameplayPresentationActive()) camera_->SetFovY(renderFov);
    // Full logical Gameplay projection, never the shaken final view matrix.
    player_->SetGameplayMovementViewport(distance, height, renderFov, aspect, 0, 0);

    visualCameraPosition_ = Add(Add(basePosition, Scale(currentRight_, playerFollowOffset_.x)),
        Scale(currentUp_, playerFollowOffset_.y));
    const float pitch = gameplayCameraPitchDegrees_ * radians;
    visualCameraForward_ = Normalize(Add(Scale(currentForward_, std::cos(pitch)),
        Scale(currentUp_, -std::sin(pitch))), currentForward_);
    gameplayCameraRotation_ = { ComputePitchDegrees(visualCameraForward_) * radians,
        std::atan2(visualCameraForward_.x, visualCameraForward_.z), 0.0f };
    const Vector3 pilotRotation{ -playerLookInput_.y * tuning.cameraPitch,
        0.0f, playerLookInput_.x * tuning.cameraRoll };
    camera_->SetViewRotationOffset(pilotRotation);
    // Presentation mapping keeps fixed normalized controls at the visible edges.
    // This explicit, unshaken pose precedes event shake; bounds remain logical.
    player_->SetGameplayViewportPose(MatrixMath::MakeAffine({ 1, 1, 1 },
        gameplayCameraRotation_, basePosition), pilotRotation);
    cameraLookTarget_ = Add(visualCameraPosition_, Scale(visualCameraForward_, cameraLookDistance_));
    return true;
}
