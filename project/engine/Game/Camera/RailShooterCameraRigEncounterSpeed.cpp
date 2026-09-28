#include "RailShooterCameraRig.h"

#include <algorithm>
#include <cmath>

bool RailShooterCameraRig::SetExternalEncounterRailSpeedMultiplier(
    float multiplier) {
    if (!std::isfinite(multiplier)) {
        return false;
    }
    externalEncounterRailSpeedMultiplier_ = std::clamp(multiplier, 0.0f, 1.0f);
    return true;
}

float RailShooterCameraRig::GetExternalEncounterRailSpeedMultiplier() const {
    return externalEncounterRailSpeedMultiplier_;
}

float RailShooterCameraRig::GetBaseRailSpeed() const {
    return railSpeed_;
}

float RailShooterCameraRig::GetExistingRailSpeedScale() const {
    return existingRailSpeedScale_;
}

float RailShooterCameraRig::GetBoostRailSpeedMultiplier() const {
    return currentRailSpeedMultiplier_;
}

float RailShooterCameraRig::GetEffectiveRailSpeed() const {
    return effectiveRailSpeed_;
}

float RailShooterCameraRig::GetRailDistance() const {
    return railDistance_;
}

float RailShooterCameraRig::GetLastRailAdvance() const {
    return lastRailAdvance_;
}
