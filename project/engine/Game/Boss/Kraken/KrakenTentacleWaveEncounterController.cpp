#include "KrakenTentacleWaveEncounterController.h"

#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossController.h"
#include "Engine/Game/Boss/Kraken/KrakenTentacleWaveEncounterConfig.h"
#include "Engine/Game/Camera/RailShooterCameraRig.h"
#include "Engine/Game/RailShooter/EnemyWaveManager.h"
#include "Engine/Graphics/Camera/Camera.h"

#include <cmath>

namespace {
constexpr float kMultiplierEpsilon = 0.0001f;
}

bool KrakenTentacleWaveEncounterController::Initialize(
    EnemyWaveManager* waveManager,
    KrakenTentacleMidbossController* kraken,
    RailShooterCameraRig* railRig,
    Camera* camera) {
    waveManager_ = waveManager;
    kraken_ = kraken;
    railRig_ = railRig;
    camera_ = camera;
    initialized_ = true;
    Reset();
    if (!ValidateContexts() || !ValidateObjective()) {
        EnterError("Wave 4 Encounterの初期Contextが無効です。");
        return false;
    }
    return true;
}

void KrakenTentacleWaveEncounterController::Reset() {
    if (railRig_) {
        SetRailHold(false);
    }
    DisableDamage();
    if (kraken_) {
        kraken_->SetWaveEncounterControlActive(false);
    }
    HideKraken();
    if (waveManager_ && waveManager_->IsExternalWaveObjectiveConfigured() &&
        waveManager_->GetExternalWaveObjectiveTargetWaveId() ==
            KrakenTentacleWaveEncounterConfig::kTargetWaveId) {
        waveManager_->SetExternalWaveObjectiveCompleted(false);
    }

    state_ = KrakenTentacleWaveEncounterState::WaitingForWave4;
    handledWaveId_.clear();
    handledWaveRevision_ = 0;
    targetWaveRevision_ = 0;
    nextWaveRevision_ = 0;
    encounterStartedForRevision_ = false;
    completionPublished_ = false;
    schedulerEnabled_ = false;
    firstAttackPending_ = true;
    nextAttackChain_ = 0;
    detectedChainCount_ = 0;
    attackTimer_ = 0.0f;
    currentAttackDelay_ = firstAttackDelay_;
    waitingForWave5Timer_ = 0.0f;
    publishedDefeatSequenceId_ = 0;
    pendingDebugCommand_ = PendingDebugCommand::None;
    spawnWorldPosition_ = {};
    spawnCameraPosition_ = {};
    spawnCameraForward_ = { 0.0f, 0.0f, 1.0f };
    ClearRuntimeDiagnostics();
}

void KrakenTentacleWaveEncounterController::Finalize() {
    if (!initialized_ && !waveManager_ && !kraken_ && !railRig_ && !camera_) {
        return;
    }
    Reset();
    initialized_ = false;
    waveManager_ = nullptr;
    kraken_ = nullptr;
    railRig_ = nullptr;
    camera_ = nullptr;
}

bool KrakenTentacleWaveEncounterController::ValidateContexts() {
    bool valid = true;
    if (!waveManager_) {
        ++managerMissingCount_;
        valid = false;
    }
    if (!kraken_ || !kraken_->IsInitialized()) {
        ++krakenMissingCount_;
        valid = false;
    }
    if (!camera_) {
        ++cameraMissingCount_;
        valid = false;
    }
    if (!railRig_) {
        ++railMissingCount_;
        valid = false;
    }
    return valid;
}

bool KrakenTentacleWaveEncounterController::ValidateObjective() {
    const bool valid = waveManager_ &&
        waveManager_->IsExternalWaveObjectiveConfigured() &&
        waveManager_->GetExternalWaveObjectiveTargetWaveId() ==
            KrakenTentacleWaveEncounterConfig::kTargetWaveId;
    if (!valid) {
        ++invalidObjectiveCount_;
        lastError_ = "External Objectiveがwave_004へ設定されていません。";
    }
    return valid;
}

bool KrakenTentacleWaveEncounterController::SetRailHold(bool hold) {
    if (!railRig_) {
        ++railMissingCount_;
        return false;
    }
    const float expected = hold ? 0.0f : 1.0f;
    const bool succeeded =
        railRig_->SetExternalEncounterRailSpeedMultiplier(expected) &&
        std::isfinite(railRig_->GetExternalEncounterRailSpeedMultiplier()) &&
        std::fabs(railRig_->GetExternalEncounterRailSpeedMultiplier() - expected) <=
            kMultiplierEpsilon;
    if (!succeeded) {
        if (hold) {
            ++railHoldFailureCount_;
        } else {
            ++railResumeFailureCount_;
        }
        return false;
    }
    railHoldEnabled_ = hold;
    if (hold) {
        railStopSucceeded_ = true;
    } else {
        railResumeSucceeded_ = true;
    }
    return true;
}

void KrakenTentacleWaveEncounterController::DisableDamage() {
    if (!kraken_) {
        return;
    }
    kraken_->SetAttackDamageEnabled(false);
    kraken_->SetProjectileDamageEnabled(false);
}

void KrakenTentacleWaveEncounterController::HideKraken() {
    if (kraken_) {
        kraken_->HideForWaveEncounter();
    }
}

void KrakenTentacleWaveEncounterController::EnterError(
    const std::string& message) {
    schedulerEnabled_ = false;
    const bool resumed = SetRailHold(false);
    errorRailResumeSucceeded_ = resumed;
    DisableDamage();
    if (kraken_) {
        kraken_->SetWaveEncounterControlActive(false);
    }
    HideKraken();
    if (!completionPublished_ && waveManager_ &&
        waveManager_->IsExternalWaveObjectiveConfigured() &&
        waveManager_->IsExternalWaveObjectiveCompleted()) {
        waveManager_->SetExternalWaveObjectiveCompleted(false);
    }
    state_ = KrakenTentacleWaveEncounterState::Error;
    lastError_ = message;
}

void KrakenTentacleWaveEncounterController::ClearRuntimeDiagnostics() {
    wave4StartCount_ = 0;
    wave5TransitionCount_ = 0;
    attackStartCount_ = 0;
    attackRejectedCount_ = 0;
    defeatAttackSuppressionCount_ = 0;
    completionPublishCount_ = 0;
    duplicateCompletionSuppressionCount_ = 0;
    manualObjectiveCompletionCount_ = 0;
    managerMissingCount_ = 0;
    krakenMissingCount_ = 0;
    cameraMissingCount_ = 0;
    railMissingCount_ = 0;
    invalidObjectiveCount_ = 0;
    invalidRevisionCount_ = 0;
    zeroChainCount_ = 0;
    spawnFailureCount_ = 0;
    railHoldFailureCount_ = 0;
    railResumeFailureCount_ = 0;
    damageSetupFailureCount_ = 0;
    objectiveCompletionFailureCount_ = 0;
    waveTransitionTimeoutCount_ = 0;
    duplicateStartSuppressionCount_ = 0;
    duplicateSpawnSuppressionCount_ = 0;
    unexpectedWaveChangeCount_ = 0;
    railStopSucceeded_ = false;
    railResumeSucceeded_ = false;
    errorRailResumeSucceeded_ = false;
    lastError_ = "なし";
    lastWarning_ = "なし";
}

bool KrakenTentacleWaveEncounterController::IsTargetWaveCurrent() const {
    return waveManager_ && waveManager_->IsCurrentWave(
        KrakenTentacleWaveEncounterConfig::kTargetWaveId);
}

bool KrakenTentacleWaveEncounterController::IsNextWaveCurrent() const {
    return waveManager_ && waveManager_->IsCurrentWave(
        KrakenTentacleWaveEncounterConfig::kNextWaveId);
}

bool KrakenTentacleWaveEncounterController::IsControllingKraken() const {
    return state_ == KrakenTentacleWaveEncounterState::Starting ||
        state_ == KrakenTentacleWaveEncounterState::Active ||
        state_ == KrakenTentacleWaveEncounterState::Defeating;
}
