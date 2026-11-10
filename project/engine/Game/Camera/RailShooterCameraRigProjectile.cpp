#include "RailShooterCameraRig.h"

#include "Engine/Level/LevelRailRuntime.h"

#include <cmath>

namespace {
bool IsFinite(const Vector3& value) {
    return std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}
}

RailShooterCameraRig::ProjectileRailFrame
RailShooterCameraRig::GetProjectileRailFrame() const {
    ProjectileRailFrame frame;
    frame.position = projectileRailPosition_;
    frame.right = projectileRailRight_;
    frame.up = projectileRailUp_;
    frame.forward = projectileRailForward_;
    frame.railDistance = GetSceneryDistance();
    frame.railRevision = railRuntime_ ? railRuntime_->GetRebuildCount() : 0;
    frame.continuityRevision = projectileRailContinuityRevision_;
    frame.railIndex = selectedRailIndex_;
    frame.railActive = HasGameplayRail();
    frame.running = frame.railActive && gameModeActive_ && externalEncounterRailSpeedMultiplier_ > 0.0f &&
        ((mode_ == Mode::Rail && autoPlay_) || mode_ == Mode::RailFinishedStraight);
    frame.runtimeV2Active = frame.railActive && runtimeV2PoseValid_ && !forceLegacyForward_;
    frame.gameModeActive = gameModeActive_;
    frame.valid = frame.railActive &&
        (currentEvaluationValid_ || mode_ == Mode::RailFinishedStraight) &&
        IsFinite(frame.position) &&
        IsFinite(frame.right) &&
        IsFinite(frame.up) &&
        IsFinite(frame.forward) &&
        std::isfinite(frame.railDistance);
    return frame;
}

void RailShooterCameraRig::InvalidateProjectileRailContinuity() {
    ++projectileRailContinuityRevision_;
}
