#include "RailShooterCameraRig.h"
#include "Engine/Game/RailShooter/RailPathRuntimeV2.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Level/LevelRailRuntime.h"
#include "Engine/Level/LevelSceneRuntime.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr float kMinVectorLength = 0.00001f;
    constexpr float kMinRailLength = 0.0001f;
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

    Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 LerpVector3(const Vector3& from, const Vector3& to, float t) {
        return {
            from.x + (to.x - from.x) * t,
            from.y + (to.y - from.y) * t,
            from.z + (to.z - from.z) * t,
        };
    }

    Vector3 ScaleVector3(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    float Length(const Vector3& value) {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    }

    Vector3 Normalize(const Vector3& value, const Vector3& fallback) {
        const float length = Length(value);
        if (length <= kMinVectorLength ||
            !std::isfinite(value.x) ||
            !std::isfinite(value.y) ||
            !std::isfinite(value.z)) {
            return fallback;
        }
        return { value.x / length, value.y / length, value.z / length };
    }

    float Dot(const Vector3& lhs, const Vector3& rhs) {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
    }

    Vector3 Cross(const Vector3& lhs, const Vector3& rhs) {
        return {
            lhs.y * rhs.z - lhs.z * rhs.y,
            lhs.z * rhs.x - lhs.x * rhs.z,
            lhs.x * rhs.y - lhs.y * rhs.x
        };
    }

    float WrapDistance(float distance, float totalLength) {
        if (totalLength <= kMinRailLength) {
            return 0.0f;
        }
        float wrapped = std::fmod(distance, totalLength);
        if (wrapped < 0.0f) {
            wrapped += totalLength;
        }
        return wrapped;
    }

    float ClampDistance(float distance, float totalLength) {
        return std::clamp(distance, 0.0f, (std::max)(0.0f, totalLength));
    }

    float NormalizeT(float distance, float totalLength) {
        if (totalLength <= kMinRailLength) {
            return 0.0f;
        }
        return std::clamp(distance / totalLength, 0.0f, 1.0f);
    }

    Vector3 GetCameraRight(const Camera& camera) {
        const Matrix4x4& matrix = camera.GetWorldMatrix();
        return Normalize({ matrix.m[0][0], matrix.m[0][1], matrix.m[0][2] }, { 1.0f, 0.0f, 0.0f });
    }

    Vector3 GetCameraUp(const Camera& camera) {
        const Matrix4x4& matrix = camera.GetWorldMatrix();
        return Normalize({ matrix.m[1][0], matrix.m[1][1], matrix.m[1][2] }, { 0.0f, 1.0f, 0.0f });
    }

    Vector3 GetCameraForward(const Camera& camera) {
        const Matrix4x4& matrix = camera.GetWorldMatrix();
        return Normalize({ matrix.m[2][0], matrix.m[2][1], matrix.m[2][2] }, { 0.0f, 0.0f, 1.0f });
    }

    Vector3 MakeCameraRotationFromForward(const Vector3& forward) {
        const Vector3 normalized = Normalize(forward, { 0.0f, 0.0f, 1.0f });
        const float yaw = std::atan2(normalized.x, normalized.z);
        const float horizontal = std::sqrt(normalized.x * normalized.x + normalized.z * normalized.z);
        const float pitch = std::atan2(-normalized.y, horizontal);
        return { pitch, yaw, 0.0f };
    }

    void BuildBasis(const Vector3& forward, const Vector3& preferredUp, Vector3& outRight, Vector3& outUp) {
        const Vector3 safeForward = Normalize(forward, { 0.0f, 0.0f, 1.0f });
        Vector3 up = Normalize(preferredUp, { 0.0f, 1.0f, 0.0f });
        if (std::fabs(Dot(safeForward, up)) > 0.98f) {
            up = { 0.0f, 0.0f, 1.0f };
            if (std::fabs(Dot(safeForward, up)) > 0.98f) {
                up = { 1.0f, 0.0f, 0.0f };
            }
        }

        outRight = Normalize(Cross(up, safeForward), { 1.0f, 0.0f, 0.0f });
        outUp = Normalize(Cross(safeForward, outRight), { 0.0f, 1.0f, 0.0f });
    }

}

RailShooterCameraRig::RailShooterCameraRig() = default;

RailShooterCameraRig::~RailShooterCameraRig() = default;

void RailShooterCameraRig::Initialize(Camera* camera, LevelRailRuntime* railRuntime) {
    InvalidateProjectileRailContinuity();
    camera_ = camera;
    railRuntime_ = railRuntime;
    runtimeV2Trial_ = std::make_unique<RailPathRuntimeV2>();
    CaptureCameraPose();
}

void RailShooterCameraRig::SetLevelSceneRuntime(const LevelSceneRuntime* levelSceneRuntime) {
    levelSceneRuntime_ = levelSceneRuntime;
}

void RailShooterCameraRig::Finalize() {
    InvalidateProjectileRailContinuity();
    ClearRuntimeV2ForceTests();
    ResetBoostRailSpeedState();
    ResetRuntimeV2PoseState(true);
    runtimeV2Trial_.reset();
    player_ = nullptr;
    boostController_ = nullptr;
    camera_ = nullptr;
    railRuntime_ = nullptr;
    levelSceneRuntime_ = nullptr;
}

void RailShooterCameraRig::Update(float deltaTime) {
    railCount_ = railRuntime_ ? railRuntime_->GetRailCount() : 0;
    if (autoApplyInitialCameraOnLoad_ && !hasAppliedInitialCameraOnce_ &&
        !IsCameraRigActive() && levelSceneRuntime_ && levelSceneRuntime_->HasEngineCameraStart()) {
        ApplyInitialCameraFromLevel();
        hasAppliedInitialCameraOnce_ = true;
    } else if (levelSceneRuntime_ && !levelSceneRuntime_->HasEngineCameraStart()) {
        hasAppliedInitialCameraOnce_ = false;
    }
    if (!IsCameraRigActive()) {
        currentEvaluationValid_ = false;
        if (wasCameraRigActive_ && restoreCameraOnDisable_) {
            RestoreDebugHomeCamera();
        }
        if (wasCameraRigActive_) {
            ResetBoostRailSpeedState();
            ResetRuntimeV2PoseState(false);
            ClearRuntimeV2ForceTests();
        }
        wasCameraRigActive_ = false;
        return;
    }

    if (!wasCameraRigActive_ && mode_ != Mode::Rail) {
        SaveDebugHomeCameraIfNeeded();
        CaptureCameraPose();
    }
    wasCameraRigActive_ = true;

    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    switch (mode_) {
    case Mode::Straight:
        UpdateStraight(safeDeltaTime);
        break;
    case Mode::Rail:
        if (railBlendActive_) {
            UpdateRailBlend(safeDeltaTime);
        } else {
            UpdateRail(safeDeltaTime);
        }
        break;
    case Mode::RailFinishedStraight:
        UpdateRailFinishedStraight(safeDeltaTime);
        break;
    case Mode::RailEndStopped:
        currentEvaluationValid_ = true;
        railEndReached_ = true;
        break;
    case Mode::Disabled:
    default:
        break;
    }

    if (IsCameraRigActive() && mode_ != Mode::Rail && mode_ != Mode::Disabled) {
        ApplyToCamera();
    }
}

bool RailShooterCameraRig::IsCameraRigActive() const {
    return camera_ && enableCameraRig_ && useCameraRig_ && mode_ != Mode::Disabled;
}

void RailShooterCameraRig::SyncSelectedRailDefaults() {
    if (selectedRailId_ == previousRailId_ && selectedRailIndex_ == previousSelectedRailIndex_) {
        return;
    }

    railDistance_ = 0.0f;
    railT_ = 0.0f;
    railSpeed_ = selectedRailTotalLength_ > kMinRailLength ? railSpeed_ : 5.0f;
    debugLoop_ = selectedRailLoop_;
    previousRailId_ = selectedRailId_;
    previousSelectedRailIndex_ = selectedRailIndex_;
}

bool RailShooterCameraRig::FetchSelectedRailInfo() {
    if (!railRuntime_ || railCount_ == 0) {
        selectedRailId_.clear();
        selectedRailName_.clear();
        selectedRailType_.clear();
        selectedRailLoop_ = false;
        selectedRailPointCount_ = 0;
        selectedRailSampledPointCount_ = 0;
        selectedRailTotalLength_ = 0.0f;
        return false;
    }

    selectedRailIndex_ = std::clamp(selectedRailIndex_, 0, (std::max)(0, static_cast<int>(railCount_) - 1));
    LevelRailRuntimeRailInfo info;
    if (!railRuntime_->GetRailInfo(static_cast<size_t>(selectedRailIndex_), info)) {
        return false;
    }

    selectedRailId_ = info.railId.empty() ? info.name : info.railId;
    selectedRailName_ = info.name;
    selectedRailType_ = info.railType;
    selectedRailLoop_ = info.loop;
    selectedRailReverseDirection_ = info.reverseDirection;
    selectedRailPointCount_ = info.pointCount;
    selectedRailSampledPointCount_ = info.sampledPointCount;
    selectedRailTotalLength_ = info.totalLength;
    if (previousRailId_ != selectedRailId_ || previousSelectedRailIndex_ != selectedRailIndex_) {
        railSpeed_ = info.speed;
    }
    return true;
}

void RailShooterCameraRig::UpdateStraight(float deltaTime) {
    currentEvaluationValid_ = false;
    currentPosition_ = AddVector3(currentPosition_, ScaleVector3(currentForward_, straightSpeed_ * deltaTime));
    BuildBasis(currentForward_, currentUp_, currentRight_, currentUp_);
}

void RailShooterCameraRig::UpdateRail(float deltaTime) {
    if (!FetchSelectedRailInfo()) {
        currentEvaluationValid_ = false;
        lastStartResult_ = "Rail update skipped: selected rail info is unavailable.";
        return;
    }
    SyncSelectedRailDefaults();
    if (selectedRailTotalLength_ <= kMinRailLength) {
        currentEvaluationValid_ = false;
        autoPlay_ = false;
        lastStartResult_ = "Rail update skipped: invalid selected rail length.";
        return;
    }

    EnsureRuntimeV2Trial();
    const float activeTotalLength = GetActiveRailTotalLength();
    const bool shouldLoop = debugLoop_ || selectedRailLoop_;
    UpdateBoostRailSpeed(deltaTime, autoPlay_ && activeTotalLength > kMinRailLength);
    if (autoPlay_ && activeTotalLength > kMinRailLength) {
        railDistance_ += lastRailAdvance_;
        if (shouldLoop) {
            railDistance_ = WrapDistance(railDistance_, activeTotalLength);
        } else if (railDistance_ >= activeTotalLength) {
            railDistance_ = activeTotalLength;
            railT_ = 1.0f;
            const LevelRailEvaluation endEvaluation = railRuntime_->EvaluateByDistance(selectedRailId_, railDistance_, false);
            if (!endEvaluation.valid) {
                currentEvaluationValid_ = false;
                autoPlay_ = false;
                lastStartResult_ = "Rail end evaluation invalid; camera was not advanced.";
                return;
            }
            if (!ApplyRuntimeV2Pose(endEvaluation, deltaTime, false)) {
                currentPosition_ = endEvaluation.position;
                currentForward_ = SmoothCameraForward(endEvaluation.forward, deltaTime);
                BuildBasis(currentForward_, { 0.0f, 1.0f, 0.0f }, currentRight_, currentUp_);
                ++legacyPoseApplyCount_;
                activePoseSource_ = "Legacy Rail";
            }
            currentSegmentIndex_ = endEvaluation.segmentIndex;
            currentEvaluationValid_ = true;
            railEndReached_ = true;
            autoPlay_ = false;
            BuildBasis(currentForward_, { 0.0f, 1.0f, 0.0f }, currentRight_, currentUp_);
            if (railEndBehavior_ == RailEndBehavior::RestoreCamera) {
                mode_ = Mode::Disabled;
                enableCameraRig_ = false;
                lastStartResult_ = "Rail end reached: restored saved camera pose.";
                RestoreDebugHomeCamera();
                wasCameraRigActive_ = false;
            } else {
                ApplyToCamera();
                if (railEndBehavior_ == RailEndBehavior::ContinueStraight) {
                    mode_ = Mode::RailFinishedStraight;
                    lastStartResult_ = "Rail end reached: continue straight.";
                } else {
                    mode_ = Mode::RailEndStopped;
                    lastStartResult_ = "Rail end reached: stopped at end.";
                }
            }
            return;
        } else if (railDistance_ < 0.0f) {
            railDistance_ = 0.0f;
            autoPlay_ = false;
        }
        railT_ = NormalizeT(railDistance_, activeTotalLength);
    }

    railDistance_ = shouldLoop
        ? WrapDistance(railDistance_, activeTotalLength)
        : ClampDistance(railDistance_, activeTotalLength);
    railT_ = NormalizeT(railDistance_, activeTotalLength);

    const LevelRailEvaluation evaluation = railRuntime_->EvaluateByDistance(selectedRailId_, railDistance_, shouldLoop);
    currentEvaluationValid_ = evaluation.valid;
    if (!evaluation.valid) {
        autoPlay_ = false;
        lastStartResult_ = "Rail evaluation invalid; camera was not updated.";
        return;
    }

    if (!ApplyRuntimeV2Pose(evaluation, deltaTime, shouldLoop)) {
        currentPosition_ = evaluation.position;
        currentForward_ = SmoothCameraForward(evaluation.forward, deltaTime);
        railDistance_ = evaluation.distance;
        railT_ = evaluation.t;
        BuildBasis(currentForward_, { 0.0f, 1.0f, 0.0f }, currentRight_, currentUp_);
        ++legacyPoseApplyCount_;
        activePoseSource_ = "Legacy Rail";
        if (pendingGameModePoseSync_) {
            ++gameModePoseSyncCount_;
            pendingGameModePoseSync_ = false;
        }
    }
    currentSegmentIndex_ = evaluation.segmentIndex;
    ApplyToCamera();
}

void RailShooterCameraRig::UpdateRailBlend(float deltaTime) {
    if (!FetchSelectedRailInfo()) {
        currentEvaluationValid_ = false;
        railBlendActive_ = false;
        lastBlendValid_ = false;
        lastStartResult_ = "Rail blend stopped: selected rail info is unavailable.";
        return;
    }
    if (selectedRailTotalLength_ <= kMinRailLength) {
        currentEvaluationValid_ = false;
        railBlendActive_ = false;
        lastBlendValid_ = false;
        lastStartResult_ = "Rail blend stopped: invalid selected rail length.";
        return;
    }

    railBlendElapsed_ += (std::max)(0.0f, deltaTime);
    const float safeBlendTime = (std::max)(0.05f, railBlendTime_);
    const float t = std::clamp(railBlendElapsed_ / safeBlendTime, 0.0f, 1.0f);
    lastBlendProgress_ = t;
    lastBlendTime_ = safeBlendTime;
    lastBlendValid_ = true;
    const float smoothT = t * t * (3.0f - 2.0f * t);

    currentPosition_ = LerpVector3(blendStartPosition_, blendTargetPosition_, smoothT);
    currentForward_ = Normalize(LerpVector3(blendStartForward_, blendTargetForward_, smoothT), blendTargetForward_);
    BuildBasis(currentForward_, { 0.0f, 1.0f, 0.0f }, currentRight_, currentUp_);
    currentEvaluationValid_ = true;
    ApplyToCamera();

    if (t >= 1.0f) {
        railBlendActive_ = false;
        hasSmoothedForward_ = false;
        UpdateRail(0.0f);
    }
}

void RailShooterCameraRig::UpdateRailFinishedStraight(float deltaTime) {
    currentEvaluationValid_ = false;
    currentPosition_ = AddVector3(currentPosition_, ScaleVector3(currentForward_, straightSpeed_ * deltaTime));
    BuildBasis(currentForward_, currentUp_, currentRight_, currentUp_);
}

void RailShooterCameraRig::ApplyToCamera() {
    if (!camera_) {
        return;
    }

    currentForward_ = Normalize(currentForward_, { 0.0f, 0.0f, 1.0f });
    BuildBasis(currentForward_, currentUp_, currentRight_, currentUp_);
    projectileRailPosition_ = currentPosition_;
    projectileRailForward_ = currentForward_;
    projectileRailUp_ = currentUp_;
    projectileRailRight_ = currentRight_;

    visualCameraPosition_ = currentPosition_;
    visualCameraForward_ = currentForward_;
    if (!BuildRuntimeV2VisualPose() && enableAngledPlayerCamera_) {
        visualCameraPosition_ = AddVector3(
            AddVector3(currentPosition_, ScaleVector3(currentUp_, cameraHeightOffset_)),
            ScaleVector3(currentRight_, cameraSideOffset_));

        const Vector3 lookTarget = AddVector3(
            AddVector3(currentPosition_, ScaleVector3(currentForward_, (std::max)(0.1f, compositionLookAhead_))),
            ScaleVector3(currentUp_, lookAtYOffset_ + playerScreenYOffset_));
        visualCameraForward_ = Normalize({
            lookTarget.x - visualCameraPosition_.x,
            lookTarget.y - visualCameraPosition_.y,
            lookTarget.z - visualCameraPosition_.z,
            }, currentForward_);

        const float clampedLookDown = std::clamp(lookDownAngleDeg_, -30.0f, 30.0f);
        const float lookDownTangent = std::tan(clampedLookDown * kDegToRad);
        visualCameraForward_ = Normalize(
            AddVector3(visualCameraForward_, ScaleVector3(currentUp_, -lookDownTangent)),
            visualCameraForward_);

        if (!useVisualCameraForwardOnly_) {
            currentForward_ = visualCameraForward_;
            BuildBasis(currentForward_, currentUp_, currentRight_, currentUp_);
        }
    }

    camera_->SetTranslate(visualCameraPosition_);
    camera_->SetRotate(MakeCameraRotationFromForward(visualCameraForward_));
}

void RailShooterCameraRig::CaptureCameraPose() {
    if (!camera_) {
        return;
    }

    currentPosition_ = camera_->GetTranslate();
    currentForward_ = GetCameraForward(*camera_);
    currentUp_ = GetCameraUp(*camera_);
    currentRight_ = GetCameraRight(*camera_);
    hasSmoothedForward_ = true;
}

void RailShooterCameraRig::CaptureDebugHomeCamera() {
    if (!camera_) {
        return;
    }

    savedCameraPosition_ = camera_->GetTranslate();
    savedCameraRotation_ = camera_->GetRotate();
    hasSavedCameraPose_ = true;
}

void RailShooterCameraRig::RestoreDebugHomeCamera() {
    if (!camera_ || !hasSavedCameraPose_) {
        return;
    }

    InvalidateProjectileRailContinuity();
    camera_->SetTranslate(savedCameraPosition_);
    camera_->SetRotate(savedCameraRotation_);
    camera_->Update();
    CaptureCameraPose();
}

void RailShooterCameraRig::SaveDebugHomeCameraIfNeeded() {
    if (!hasSavedCameraPose_) {
        CaptureDebugHomeCamera();
    }
}

Vector3 RailShooterCameraRig::SmoothCameraForward(const Vector3& targetForward, float deltaTime) {
    const Vector3 safeTarget = Normalize(targetForward, currentForward_);
    if (!enableCameraForwardSmoothing_ || forwardSmoothStrength_ <= 0.0f || !hasSmoothedForward_ || deltaTime <= 0.0f) {
        hasSmoothedForward_ = true;
        return safeTarget;
    }

    const float alpha = std::clamp(1.0f - std::exp(-forwardSmoothStrength_ * (std::max)(0.0f, deltaTime)), 0.0f, 1.0f);
    return Normalize({
        currentForward_.x + (safeTarget.x - currentForward_.x) * alpha,
        currentForward_.y + (safeTarget.y - currentForward_.y) * alpha,
        currentForward_.z + (safeTarget.z - currentForward_.z) * alpha,
        }, safeTarget);
}

void RailShooterCameraRig::ResetCameraRig() {
    InvalidateProjectileRailContinuity();
    railDistance_ = 0.0f;
    railT_ = 0.0f;
    currentSegmentIndex_ = 0;
    currentEvaluationValid_ = false;
    railEndReached_ = false;
    autoPlay_ = false;
    previousRailId_.clear();
    previousSelectedRailIndex_ = -1;
    hasSmoothedForward_ = false;
    railBlendActive_ = false;
    railBlendElapsed_ = 0.0f;
    ResetBoostRailSpeedState();
    ResetRuntimeV2PoseState(false);
    ClearRuntimeV2ForceTests();
    CaptureCameraPose();
}

void RailShooterCameraRig::ResetCameraRigAndRestoreCamera() {
    InvalidateProjectileRailContinuity();
    railDistance_ = 0.0f;
    railT_ = 0.0f;
    currentSegmentIndex_ = 0;
    currentEvaluationValid_ = false;
    railEndReached_ = false;
    autoPlay_ = false;
    mode_ = Mode::Disabled;
    enableCameraRig_ = false;
    previousRailId_.clear();
    previousSelectedRailIndex_ = -1;
    hasSmoothedForward_ = false;
    railBlendActive_ = false;
    railBlendElapsed_ = 0.0f;
    ResetBoostRailSpeedState();
    ResetRuntimeV2PoseState(false);
    ClearRuntimeV2ForceTests();
    RestoreDebugHomeCamera();
    wasCameraRigActive_ = false;
}

void RailShooterCameraRig::StartRail() {
    railCount_ = railRuntime_ ? railRuntime_->GetRailCount() : 0;
    if (!FetchSelectedRailInfo()) {
        currentEvaluationValid_ = false;
        lastStartResult_ = "Manual Start Rail failed: selected rail info is unavailable.";
        return;
    }
    SyncSelectedRailDefaults();
    if (selectedRailTotalLength_ <= kMinRailLength) {
        currentEvaluationValid_ = false;
        lastStartResult_ = "Manual Start Rail failed: invalid selected rail length.";
        return;
    }

    InvalidateProjectileRailContinuity();
    SaveDebugHomeCameraIfNeeded();
    lastStartSource_ = "Manual";
    lastRequestedStartMode_ = "SelectedRail";
    lastActualStartMode_ = "SelectedRail";
    lastPreviousRailDistance_ = railDistance_;
    lastStartDistance_ = 0.0f;
    lastStartDistanceRatio_ = 0.0f;
    lastStartedNearEnd_ = false;
    lastFallbackUsed_ = false;
    lastStartWarning_ = "(none)";
    railEndReached_ = false;
    railDistance_ = 0.0f;
    railT_ = 0.0f;
    enableCameraRig_ = true;
    useCameraRig_ = true;
    mode_ = Mode::Rail;
    hasSmoothedForward_ = false;
    railBlendActive_ = false;
    railBlendElapsed_ = 0.0f;
    SyncSelectedRailDefaults();
    UpdateRail(0.0f);
}

void RailShooterCameraRig::StopRail() {
    InvalidateProjectileRailContinuity();
    mode_ = Mode::Disabled;
    autoPlay_ = false;
    currentEvaluationValid_ = false;
    railEndReached_ = false;
    railBlendActive_ = false;
    railBlendElapsed_ = 0.0f;
    ResetBoostRailSpeedState();
    ResetRuntimeV2PoseState(false);
    ClearRuntimeV2ForceTests();
    if (restoreCameraOnStop_) {
        RestoreDebugHomeCamera();
    } else {
        CaptureCameraPose();
    }
    wasCameraRigActive_ = false;
}
