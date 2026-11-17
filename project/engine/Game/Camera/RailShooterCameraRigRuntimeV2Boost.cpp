#include "RailShooterCameraRig.h"

#include "Engine/Game/Player/BoostController.h"
#include "Engine/Game/Player/Player.h"

#include <algorithm>
#include <cmath>

void RailShooterCameraRig::SetRuntimeContext(
    Player* player, BoostController* boostController, bool gameModeActive,
    bool presentationOverrideActive, bool suppressPlayerAttitude) {
    if (presentationOverrideActive_ != presentationOverrideActive) ResetGameplayInputHistory(false);
    player_ = player;
    boostController_ = boostController;
    if (presentationOverrideActive_ && !presentationOverrideActive) presentationReturnActive_ = true;
    if (presentationOverrideActive) presentationReturnActive_ = false;
    presentationOverrideActive_ = presentationOverrideActive;
    if (player_) {
        player_->SetNormalFlightPresentation(gameModeActive && camera_,
            suppressPlayerAttitude);
        player_->SetFlightEffectsActive(gameModeActive && camera_ &&
            !presentationOverrideActive_ && !presentationReturnActive_);
    }
    boostControllerActive_ = boostController_ != nullptr;
    if (!runtimeContextInitialized_) {
        gameModeActive_ = gameModeActive;
        runtimeContextInitialized_ = true;
        return;
    }
    if (gameModeActive == gameModeActive_) return;

    InvalidateProjectileRailContinuity();
    if (gameplayStartRailActive_) {
        gameplayStartRailAttempted_ = false;
        StartConfiguredGameplayRail();
    }
    gameModeActive_ = gameModeActive;
    ResetGameplayInputHistory();
    playerFollowInitialized_ = false;
    presentationReturnActive_ = false;
    // Non-stage camera previews retain their logical basis across mode switches.
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
        if (gameplayStartRailActive_) effectiveRailSpeed_ = 0.0f;
    }
}

void RailShooterCameraRig::UpdateBoostRailSpeed(float deltaTime, bool advancing) {
    boostRailSpeedMultiplier_ = std::clamp(boostRailSpeedMultiplier_, 1.0f, 1.3f);
    boostRailAccelerationTime_ = std::clamp(boostRailAccelerationTime_, 0.01f, 0.5f);
    boostControllerActive_ = boostController_ != nullptr;
    boostStateActive_ = boostController_ && boostController_->IsBoosting();
    if (forceBoostRailSpeed_) boostStateActive_ = true;
    if (forceBoostRailSpeedOff_) boostStateActive_ = false;

    targetRailSpeedMultiplier_ = boostStateActive_ ? boostRailSpeedMultiplier_ : 1.0f;
    const float safeDeltaTime = (std::max)(0.0f, deltaTime);
    const float alpha = 1.0f - std::exp(-safeDeltaTime / boostRailAccelerationTime_);
    currentRailSpeedMultiplier_ = boostStateActive_
        ? std::lerp(currentRailSpeedMultiplier_, targetRailSpeedMultiplier_, std::clamp(alpha, 0.0f, 1.0f))
        : 1.0f;
    currentRailSpeedMultiplier_ = std::clamp(currentRailSpeedMultiplier_, 1.0f, boostRailSpeedMultiplier_);
    effectiveRailSpeed_ = railSpeed_ * existingRailSpeedScale_ *
        currentRailSpeedMultiplier_ * externalEncounterRailSpeedMultiplier_;
    lastRailAdvance_ = advancing ? effectiveRailSpeed_ * safeDeltaTime : 0.0f;
    if (advancing) ++boostMultiplierApplyCount_;
    boostDoubleApplicationDetected_ = false;
}

void RailShooterCameraRig::ResetBoostRailSpeedState() {
    currentRailSpeedMultiplier_ = 1.0f;
    targetRailSpeedMultiplier_ = 1.0f;
    effectiveRailSpeed_ = railSpeed_ * existingRailSpeedScale_ *
        externalEncounterRailSpeedMultiplier_;
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
