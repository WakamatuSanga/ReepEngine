#include "ProjectileRailMotionAdapter.h"

#include "Engine/Game/Camera/RailShooterCameraRig.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float kMinVectorLength = 0.00001f;
    constexpr float kRailDistanceRewindTolerance = 0.001f;
    constexpr float kRadiansToDegrees = 57.29577951308232f;

    Vector3 Subtract(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    }

    Vector3 Scale(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    float Dot(const Vector3& lhs, const Vector3& rhs) {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
    }

    float Length(const Vector3& value) {
        return std::sqrt(Dot(value, value));
    }

    bool IsFinite(const Vector3& value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    Vector3 Normalize(const Vector3& value, const Vector3& fallback) {
        const float length = Length(value);
        if (!std::isfinite(length) || length <= kMinVectorLength) {
            return fallback;
        }
        return Scale(value, 1.0f / length);
    }

    float AngleDegrees(const Vector3& lhs, const Vector3& rhs) {
        const Vector3 normalizedLhs = Normalize(lhs, { 0.0f, 0.0f, 0.0f });
        const Vector3 normalizedRhs = Normalize(rhs, { 0.0f, 0.0f, 0.0f });
        if (Length(normalizedLhs) <= kMinVectorLength || Length(normalizedRhs) <= kMinVectorLength) {
            return 180.0f;
        }
        return std::acos(std::clamp(Dot(normalizedLhs, normalizedRhs), -1.0f, 1.0f)) * kRadiansToDegrees;
    }

    float FrameRotationDegrees(
        const Vector3& previousRight,
        const Vector3& previousUp,
        const Vector3& previousForward,
        const Vector3& currentRight,
        const Vector3& currentUp,
        const Vector3& currentForward) {
        return (std::max)({
            AngleDegrees(previousRight, currentRight),
            AngleDegrees(previousUp, currentUp),
            AngleDegrees(previousForward, currentForward),
        });
    }

}

void ProjectileRailMotionAdapter::Initialize(const RailShooterCameraRig* railCameraRig) {
    railCameraRig_ = railCameraRig;
    currentFrame_ = CaptureFrame();
    previousFrame_ = currentFrame_;
    ResetDiagnostics();
}

void ProjectileRailMotionAdapter::Finalize() {
    Initialize(nullptr);
}

void ProjectileRailMotionAdapter::BeginFrame() {
    previousFrame_ = currentFrame_;
    currentFrame_ = CaptureFrame();
    const auto& previous = previousFrame_;
    const auto& current = currentFrame_;
    if (!current.valid || !previous.valid || previous.revision != current.revision
        || previous.continuityRevision != current.continuityRevision
        || previous.railIndex != current.railIndex || previous.gameModeActive != current.gameModeActive
        || previous.runtimeV2Active != current.runtimeV2Active
        || current.railDistance + kRailDistanceRewindTolerance < previous.railDistance) {
        previousFrame_ = currentFrame_;
        frameTranslationDistance_ = frameRotationDegrees_ = 0.0f;
        ++frameResyncCount_;
        return;
    }
    frameTranslationDistance_ = Length(Subtract(current.position, previous.position));
    frameRotationDegrees_ = FrameRotationDegrees(
        previous.right, previous.up, previous.forward, current.right, current.up, current.forward);
}

Vector3 ProjectileRailMotionAdapter::GetLockedWingLaunchRailDisplacement(
    const Vector3& worldPosition) const {
    const auto& previous = previousFrame_;
    const auto& current = currentFrame_;
    if (!current.valid || !current.gameModeActive || !IsFinite(worldPosition)) {
        return {};
    }

    const Vector3 offset = Subtract(worldPosition, previous.position);
    const Vector3 translation = Subtract(current.position, previous.position);
    const Vector3 rightShift = Scale(Subtract(current.right, previous.right), Dot(offset, previous.right));
    const Vector3 upShift = Scale(Subtract(current.up, previous.up), Dot(offset, previous.up));
    const Vector3 forwardShift = Scale(Subtract(current.forward, previous.forward), Dot(offset, previous.forward));
    return {
        translation.x + rightShift.x + upShift.x + forwardShift.x,
        translation.y + rightShift.y + upShift.y + forwardShift.y,
        translation.z + rightShift.z + upShift.z + forwardShift.z,
    };
}

void ProjectileRailMotionAdapter::RecordShot(
    ProjectileKind kind,
    const Vector3& muzzleWorldPosition,
    const Vector3& targetWorldPosition,
    const Vector3& shotDirection,
    const Vector3& worldVelocity) {
    KindDiagnostics& diagnostics = GetDiagnostics(kind);
    diagnostics.lastMuzzleWorldPosition = muzzleWorldPosition;
    diagnostics.lastTargetWorldPosition = targetWorldPosition;
    diagnostics.lastShotDirection = shotDirection;
    diagnostics.lastWorldVelocity = worldVelocity;
    ++diagnostics.shotCount;
}

void ProjectileRailMotionAdapter::RecordDespawn(ProjectileKind kind, const std::string& reason) {
    GetDiagnostics(kind).lastDespawnReason = reason.empty() ? "理由未指定" : reason;
}

void ProjectileRailMotionAdapter::ResetDiagnostics() {
    playerDiagnostics_ = {};
    enemyDiagnostics_ = {};
    frameResyncCount_ = 0;
    frameTranslationDistance_ = frameRotationDegrees_ = 0.0f;
}

ProjectileRailMotionAdapter::FrameSnapshot ProjectileRailMotionAdapter::CaptureFrame() const {
    FrameSnapshot result;
    if (!railCameraRig_) {
        return result;
    }

    const RailShooterCameraRig::ProjectileRailFrame source = railCameraRig_->GetProjectileRailFrame();
    result.position = source.position;
    result.right = source.right;
    result.up = source.up;
    result.forward = source.forward;
    result.railDistance = source.railDistance;
    result.revision = source.railRevision;
    result.continuityRevision = source.continuityRevision;
    result.railIndex = source.railIndex;
    result.railActive = source.railActive;
    result.running = source.running;
    result.runtimeV2Active = source.runtimeV2Active;
    result.gameModeActive = source.gameModeActive;
    result.valid = source.valid &&
        IsFinite(source.position) && IsFinite(source.right) && IsFinite(source.up) && IsFinite(source.forward) &&
        std::isfinite(source.railDistance) &&
        Length(source.right) > kMinVectorLength &&
        Length(source.up) > kMinVectorLength &&
        Length(source.forward) > kMinVectorLength;
    if (result.valid) {
        result.right = Normalize(source.right, { 1.0f, 0.0f, 0.0f });
        result.up = Normalize(source.up, { 0.0f, 1.0f, 0.0f });
        result.forward = Normalize(source.forward, { 0.0f, 0.0f, 1.0f });
    }
    return result;
}

ProjectileRailMotionAdapter::KindDiagnostics& ProjectileRailMotionAdapter::GetDiagnostics(ProjectileKind kind) {
    return kind == ProjectileKind::Player ? playerDiagnostics_ : enemyDiagnostics_;
}
