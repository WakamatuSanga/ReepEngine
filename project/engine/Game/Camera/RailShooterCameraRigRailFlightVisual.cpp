#include "RailShooterCameraRig.h"

#include "Engine/Game/RailShooter/RailPathRuntimeV2.h"
#include "Engine/Level/LevelRailRuntime.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kMinimumLengthSquared = 0.0000001f;
constexpr float kMinimumRailLength = 0.0001f;

float LengthSquared(const Vector3& value) {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

bool IsFinite(const Vector3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool IsUsableDirection(const Vector3& value) {
    return IsFinite(value) && LengthSquared(value) > kMinimumLengthSquared;
}

Vector3 Normalize(const Vector3& value, const Vector3& fallback) {
    if (!IsUsableDirection(value)) return fallback;
    const float inverseLength = 1.0f / std::sqrt(LengthSquared(value));
    return { value.x * inverseLength, value.y * inverseLength, value.z * inverseLength };
}

float WrapDistance(float distance, float totalLength) {
    if (totalLength <= kMinimumRailLength) return 0.0f;
    float wrapped = std::fmod(distance, totalLength);
    if (wrapped < 0.0f) wrapped += totalLength;
    return wrapped;
}
}

RailShooterCameraRig::RailFlightPoseSnapshot
RailShooterCameraRig::GetRailFlightPoseSnapshot(float lookAheadDistance) const {
    RailFlightPoseSnapshot snapshot;
    snapshot.railDistance = railDistance_;
    snapshot.railRevision = railRuntime_ ? railRuntime_->GetRebuildCount() : 0;
    snapshot.railIndex = selectedRailIndex_;
    snapshot.running =
        IsCameraRigActive() &&
        mode_ == Mode::Rail &&
        autoPlay_;

    if (!IsCameraRigActive() ||
        mode_ != Mode::Rail ||
        !currentEvaluationValid_ ||
        selectedRailId_.empty()) {
        return snapshot;
    }

    const bool loopEnabled = debugLoop_ || selectedRailLoop_;
    const float safeLookAhead = std::clamp(lookAheadDistance, 0.1f, 100.0f);
    bool resolved = false;

    if (!forceLegacyForward_ &&
        runtimeV2PoseValid_ &&
        runtimeV2Trial_ &&
        runtimeV2Trial_->IsValid()) {
        const float totalLength = runtimeV2Trial_->GetTotalLength();
        const float currentDistance = loopEnabled
            ? WrapDistance(railDistance_, totalLength)
            : std::clamp(railDistance_, 0.0f, totalLength);
        const float aheadDistance = loopEnabled
            ? WrapDistance(currentDistance + safeLookAhead, totalLength)
            : std::clamp(currentDistance + safeLookAhead, 0.0f, totalLength);
        const RailPathSample currentSample = runtimeV2Trial_->SampleByDistance(currentDistance);
        const RailPathSample aheadSample = runtimeV2Trial_->SampleByDistance(aheadDistance);
        if (currentSample.valid &&
            aheadSample.valid &&
            IsUsableDirection(currentSample.tangent) &&
            IsUsableDirection(aheadSample.tangent)) {
            snapshot.currentForward = Normalize(currentSample.tangent, snapshot.currentForward);
            snapshot.aheadForward = Normalize(aheadSample.tangent, snapshot.currentForward);
            snapshot.up = Normalize(railCameraPose_.up, { 0.0f, 1.0f, 0.0f });
            snapshot.runtimeV2Active = true;
            resolved = true;
        }
    }

    if (!resolved && railRuntime_) {
        const float totalLength = selectedRailTotalLength_;
        const float aheadDistance = loopEnabled
            ? WrapDistance(railDistance_ + safeLookAhead, totalLength)
            : std::clamp(railDistance_ + safeLookAhead, 0.0f, totalLength);
        const LevelRailEvaluation current = railRuntime_->EvaluateByDistance(
            selectedRailId_, railDistance_, loopEnabled);
        const LevelRailEvaluation ahead = railRuntime_->EvaluateByDistance(
            selectedRailId_, aheadDistance, loopEnabled);
        if (current.valid && ahead.valid &&
            IsUsableDirection(current.forward) &&
            IsUsableDirection(ahead.forward)) {
            snapshot.currentForward = Normalize(current.forward, snapshot.currentForward);
            snapshot.aheadForward = Normalize(ahead.forward, snapshot.currentForward);
            snapshot.up = { 0.0f, 1.0f, 0.0f };
            resolved = true;
        }
    }

    snapshot.valid = resolved &&
        IsUsableDirection(snapshot.currentForward) &&
        IsUsableDirection(snapshot.aheadForward) &&
        IsUsableDirection(snapshot.up);
    return snapshot;
}
