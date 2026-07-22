#include "RailShooterCameraRig.h"

#include "Engine/Game/Player/BoostController.h"

#include <algorithm>
#include <cmath>

void RailShooterCameraRig::SetRuntimeContext(
    Player* player, BoostController* boostController, bool gameModeActive) {
    player_ = player;
    boostController_ = boostController;
    boostControllerActive_ = boostController_ != nullptr;
    if (!runtimeContextInitialized_) {
        gameModeActive_ = gameModeActive;
        runtimeContextInitialized_ = true;
        return;
    }
    if (gameModeActive == gameModeActive_) return;

    gameModeActive_ = gameModeActive;
    ClearRuntimeV2ForceTests();
    ResetBoostRailSpeedState();
    hasPreviousRailForward_ = false;
    hasSmoothedForward_ = false;
    if (gameModeActive_) {
        pendingGameModePoseSync_ = true;
        targetRailSpeedMultiplier_ = boostController_ && boostController_->IsBoosting()
            ? boostRailSpeedMultiplier_ : 1.0f;
    } else {
        pendingGameModePoseSync_ = false;
    }
}

void RailShooterCameraRig::UpdateBoostRailSpeed(float deltaTime, bool advancing) {
    boostRailSpeedMultiplier_ = std::clamp(boostRailSpeedMultiplier_, 1.0f, 1.3f);
    boostRailAccelerationTime_ = std::clamp(boostRailAccelerationTime_, 0.01f, 0.5f);
    boostRailReturnTime_ = std::clamp(boostRailReturnTime_, 0.05f, 1.5f);
    boostControllerActive_ = boostController_ != nullptr;
    boostStateActive_ = boostController_ && boostController_->IsBoosting();
    if (forceBoostRailSpeed_) boostStateActive_ = true;
    if (forceBoostRailSpeedOff_) boostStateActive_ = false;

    targetRailSpeedMultiplier_ = boostStateActive_ ? boostRailSpeedMultiplier_ : 1.0f;
    const float responseTime = boostStateActive_ ? boostRailAccelerationTime_ : boostRailReturnTime_;
    const float safeDeltaTime = (std::max)(0.0f, deltaTime);
    const float alpha = 1.0f - std::exp(-safeDeltaTime / (std::max)(responseTime, 0.001f));
    currentRailSpeedMultiplier_ +=
        (targetRailSpeedMultiplier_ - currentRailSpeedMultiplier_) * std::clamp(alpha, 0.0f, 1.0f);
    currentRailSpeedMultiplier_ = std::clamp(currentRailSpeedMultiplier_, 1.0f, boostRailSpeedMultiplier_);
    effectiveRailSpeed_ = railSpeed_ * existingRailSpeedScale_ * currentRailSpeedMultiplier_;
    lastRailAdvance_ = advancing ? effectiveRailSpeed_ * safeDeltaTime : 0.0f;
    if (advancing) ++boostMultiplierApplyCount_;
    boostDoubleApplicationDetected_ = false;
}

void RailShooterCameraRig::ResetBoostRailSpeedState() {
    currentRailSpeedMultiplier_ = 1.0f;
    targetRailSpeedMultiplier_ = 1.0f;
    effectiveRailSpeed_ = railSpeed_ * existingRailSpeedScale_;
    lastRailAdvance_ = 0.0f;
    boostStateActive_ = false;
    boostDoubleApplicationDetected_ = false;
}

void RailShooterCameraRig::ClearRuntimeV2ForceTests() {
    forceLegacyForward_ = false;
    forceRuntimeV2Forward_ = false;
    forceBoostRailSpeed_ = false;
    forceBoostRailSpeedOff_ = false;
}
