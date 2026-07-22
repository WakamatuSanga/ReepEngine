#include "RailShooterCameraRig.h"

#include "Engine/Game/Player/Player.h"
#include "Engine/Game/RailShooter/RailPathRuntimeV2.h"
#include "Engine/Game/RailShooter/RailPathRuntimeV2Adapter.h"
#include "Engine/Level/LevelRailRuntime.h"

#include <algorithm>
#include <cmath>
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
