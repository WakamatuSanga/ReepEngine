#include "KrakenTentacleWaveEncounterController.h"

#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossController.h"
#include "Engine/Game/Boss/Kraken/KrakenTentacleWaveEncounterConfig.h"
#include "Engine/Game/Camera/RailShooterCameraRig.h"
#include "Engine/Game/Camera/CameraShakeController.h"
#include "Engine/Game/RailShooter/EnemyWaveManager.h"
#include "Engine/Graphics/Camera/Camera.h"

#include <cmath>

namespace {
bool IsFinite(const Vector3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}
}

bool KrakenTentacleWaveEncounterController::BeginEncounter() {
    ClearEntrance();
    state_ = KrakenTentacleWaveEncounterState::Starting;
    if (!ValidateContexts() || !ValidateObjective()) {
        EnterError("Wave 4 Encounterの開始Contextが無効です。");
        return false;
    }
    if (!waveManager_->SetExternalWaveObjectiveCompleted(false)) {
        ++invalidObjectiveCount_;
        EnterError("External Objectiveを未完了へ再同期できませんでした。");
        return false;
    }
    if (!SetRailHold(true)) {
        EnterError("Encounter Rail Holdを有効化できませんでした。");
        return false;
    }
    ++railHoldStartCount_;
    if (!kraken_->ResetForWaveEncounter()) {
        ++spawnFailureCount_;
        EnterError("中ボスRuntimeをEncounter用にResetできませんでした。");
        return false;
    }
    ++bossResetCount_;
    constexpr float kExpectedWave4Hp = 20.0f;
    if (std::fabs(kraken_->GetMaxHp() - kExpectedWave4Hp) > 0.0001f ||
        std::fabs(kraken_->GetCurrentHp() - kExpectedWave4Hp) > 0.0001f) {
        ++damageSetupFailureCount_;
        EnterError("中ボスの開始時体力を20へ復元できませんでした。");
        return false;
    }

    detectedChainCount_ = kraken_->GetDetectedChainCount();
    if (detectedChainCount_ == 0) {
        ++zeroChainCount_;
        EnterError("有効な触手Chainを検出できませんでした。");
        return false;
    }
    DisableDamage();
    if (!kraken_->SetSelectedAttackChainForWaveEncounter(0)) {
        ++zeroChainCount_;
        EnterError("Attack Chain 0を選択できませんでした。");
        return false;
    }

    const Matrix4x4& cameraWorld = camera_->GetWorldMatrix();
    spawnCameraPosition_ = camera_->GetTranslate();
    spawnCameraForward_ = {
        cameraWorld.m[2][0], cameraWorld.m[2][1], cameraWorld.m[2][2] };
    if (!IsFinite(spawnCameraPosition_) || !IsFinite(spawnCameraForward_)) {
        ++spawnFailureCount_;
        EnterError("カメラ前方の配置基準が有限値ではありません。");
        return false;
    }
    if (!kraken_->PlaceInFrontOfCameraForWaveEncounter()) {
        ++spawnFailureCount_;
        EnterError("中ボスをカメラ前方へ配置できませんでした。");
        return false;
    }
    ++bossPlacementCount_;
    spawnWorldPosition_ = kraken_->GetWorldPosition();
    if (!IsFinite(spawnWorldPosition_)) {
        ++spawnFailureCount_;
        EnterError("中ボスの配置座標が有限値ではありません。");
        return false;
    }
    kraken_->SetWaveEncounterControlActive(true);

    attackTimer_ = 0.0f;
    currentAttackDelay_ = firstAttackDelay_;
    nextAttackChain_ = 0;
    firstAttackPending_ = true;
    schedulerEnabled_ = false;
    completionPublished_ = false;
    waitingForWave5Timer_ = 0.0f;
    publishedDefeatSequenceId_ = 0;
    encounterStartedForRevision_ = true;
    ++wave4StartCount_;
    activeEntranceSettings_ = entranceSettings_;
    fovBlendDuration_ = activeEntranceSettings_.cameraBlendDuration;
    entrancePhase_ = EntrancePhase::Shake;
    if (activeEntranceSettings_.shakeDuration > 0.0f) {
        entranceShake_->Start(
            activeEntranceSettings_.shakeDuration,
            activeEntranceSettings_.shakeAmplitude,
            KrakenTentacleWaveEncounterConfig::kEntranceShakeFrequency);
    }
    return true;
}

bool KrakenTentacleWaveEncounterController::RearmForNewWave4Revision() {
    const auto failRearm = [this](const std::string& message) {
        ++rearmFailureCount_;
        lastRearmSucceeded_ = false;
        lastRearmFailureReason_ = message;
        EnterError(message);
        lastReentryStateAfter_ = state_;
        return false;
    };

    const float railMultiplierBefore =
        railRig_->GetExternalEncounterRailSpeedMultiplier();
    if (!SetRailHold(false)) {
        return failRearm(
            "ウェーブ4再入場時にレール倍率を1.0へ再同期できませんでした。");
    }
    ++railRearmResyncCount_;
    if (!std::isfinite(railMultiplierBefore) ||
        std::fabs(railMultiplierBefore - 1.0f) > 0.0001f) {
        lastWarning_ =
            "ウェーブ4再入場前のレール倍率を1.0へ再同期しました。";
    }

    DisableDamage();
    if (kraken_->IsAttackDamageEnabled() ||
        kraken_->IsProjectileDamageEnabled()) {
        return failRearm(
            "ウェーブ4再入場準備時に中ボスダメージを無効化できませんでした。");
    }
    schedulerEnabled_ = false;
    attackTimer_ = 0.0f;
    currentAttackDelay_ = firstAttackDelay_;
    nextAttackChain_ = 0;
    detectedChainCount_ = 0;
    firstAttackPending_ = true;
    waitingForWave5Timer_ = 0.0f;
    completionPublished_ = false;
    publishedDefeatSequenceId_ = 0;

    objectiveIncompleteResyncAttempted_ = true;
    if (!waveManager_->SetExternalWaveObjectiveCompleted(false) ||
        waveManager_->IsExternalWaveObjectiveCompleted()) {
        ++objectiveIncompleteResyncFailureCount_;
        lastObjectiveIncompleteResyncSucceeded_ = false;
        return failRearm(
            "ウェーブ4再入場時に外部目標を未完了へ再同期できませんでした。");
    }
    ++objectiveIncompleteResyncSuccessCount_;
    lastObjectiveIncompleteResyncSucceeded_ = true;

    handledWaveId_.clear();
    handledWaveRevision_ = 0;
    targetWaveRevision_ = 0;
    nextWaveRevision_ = 0;
    encounterStartedForRevision_ = false;
    railStopSucceeded_ = false;
    state_ = KrakenTentacleWaveEncounterState::WaitingForWave4;
    ++rearmSuccessCount_;
    lastRearmSucceeded_ = true;
    lastRearmFailureReason_ = "なし";
    lastReentryStateAfter_ = state_;
    return true;
}

bool KrakenTentacleWaveEncounterController::PrepareWave4Reentry(
    bool& rearmedThisUpdate) {
    rearmedThisUpdate = false;
    if (!waveManager_) {
        return true;
    }

    const std::string currentWaveId = waveManager_->GetCurrentWaveId();
    const std::uint64_t currentRevision =
        waveManager_->GetCurrentWaveRevision();
    const bool observationChanged = currentWaveId != observedWaveId_ ||
        currentRevision != observedWaveRevision_;
    const std::string oldWaveId = observedWaveId_;
    const std::uint64_t oldRevision = observedWaveRevision_;
    observedWaveId_ = currentWaveId;
    observedWaveRevision_ = currentRevision;
    if (!observationChanged ||
        currentWaveId != KrakenTentacleWaveEncounterConfig::kTargetWaveId) {
        return true;
    }

    const auto recordReentry = [this, &oldWaveId, oldRevision,
                                &currentWaveId, currentRevision]() {
        lastReentryOldWaveId_ = oldWaveId.empty() ? "なし" : oldWaveId;
        lastReentryNewWaveId_ = currentWaveId;
        lastReentryOldWaveRevision_ = oldRevision;
        lastReentryNewWaveRevision_ = currentRevision;
        lastReentryStateBefore_ = state_;
        lastReentryStateAfter_ = state_;
    };

    if (state_ == KrakenTentacleWaveEncounterState::Completed) {
        recordReentry();
        lastRearmAttempted_ = false;
        lastRearmSucceeded_ = false;
        objectiveIncompleteResyncAttempted_ = false;
        lastObjectiveIncompleteResyncSucceeded_ = false;
        if (currentRevision == 0) {
            lastRearmAttempted_ = true;
            ++invalidRevisionCount_;
            ++rearmFailureCount_;
            lastRearmFailureReason_ =
                "ウェーブ4再入場時の改訂番号が無効です。";
            EnterError(lastRearmFailureReason_);
            lastReentryStateAfter_ = state_;
            return false;
        }
        if (handledWaveRevision_ == currentRevision) {
            ++sameRevisionReentrySuppressionCount_;
            lastRearmFailureReason_ =
                "同一改訂番号のウェーブ4再入場は抑制しました。";
            return false;
        }

        ++newRevisionWave4ReentryDetectionCount_;
        lastRearmAttempted_ = true;
        if (!ValidateContexts()) {
            ++rearmFailureCount_;
            lastRearmFailureReason_ =
                "ウェーブ4再入場に必要な接続情報が不足しています。";
            EnterError(lastRearmFailureReason_);
            lastReentryStateAfter_ = state_;
            return false;
        }
        if (!ValidateObjective()) {
            ++rearmFailureCount_;
            lastRearmFailureReason_ =
                "ウェーブ4再入場用の外部目標設定が無効です。";
            EnterError(lastRearmFailureReason_);
            lastReentryStateAfter_ = state_;
            return false;
        }
        if (!RearmForNewWave4Revision()) {
            return false;
        }
        rearmedThisUpdate = true;
        return true;
    }

    if (state_ == KrakenTentacleWaveEncounterState::WaitingForWave4) {
        return true;
    }

    recordReentry();
    ++invalidStateReentryRejectionCount_;
    lastRearmAttempted_ = false;
    lastRearmSucceeded_ = false;
    lastRearmFailureReason_ =
        "完了状態以外でのウェーブ4再入場は自動処理しません。";
    if (state_ == KrakenTentacleWaveEncounterState::Error) {
        lastWarning_ = lastRearmFailureReason_;
        return false;
    }
    EnterError(lastRearmFailureReason_);
    lastReentryStateAfter_ = state_;
    return false;
}

void KrakenTentacleWaveEncounterController::PreRailUpdate(
    float gameplayDeltaTime) {
    if (!initialized_) {
        return;
    }
    if (!std::isfinite(gameplayDeltaTime) || gameplayDeltaTime < 0.0f) {
        EnterError("Gameplay Delta Timeが有限値ではありません。");
        return;
    }
    entranceShake_->BeginFrame(camera_);
    UpdateFovOverride(gameplayDeltaTime);
    if (ProcessPendingDebugCommand()) {
        return;
    }
    bool rearmedThisUpdate = false;
    if (!PrepareWave4Reentry(rearmedThisUpdate)) {
        return;
    }
    if (!ValidateContexts()) {
        EnterError("Wave 4 EncounterのContextが失われました。");
        return;
    }

    if (waveManager_->GetCurrentWaveRevision() == 0 &&
        waveManager_->GetCurrentWaveId().empty() &&
        state_ != KrakenTentacleWaveEncounterState::WaitingForWave4 &&
        waveManager_->IsExternalWaveObjectiveConfigured() &&
        !waveManager_->IsExternalWaveObjectiveCompleted()) {
        Reset();
        return;
    }

    if (IsControllingKraken() &&
        waveManager_->IsExternalWaveObjectiveCompleted()) {
        ++manualObjectiveCompletionCount_;
        waveManager_->SetExternalWaveObjectiveCompleted(false);
        EnterError("中ボス撃破前にExternal Objectiveが手動完了されました。");
        return;
    }

    if (state_ == KrakenTentacleWaveEncounterState::WaitingForWave4) {
        if (IsNextWaveCurrent()) {
            ++unexpectedWaveChangeCount_;
            EnterError("Wave 4を開始せずにWave 5へ移行しました。");
            return;
        }
        if (!IsTargetWaveCurrent()) {
            return;
        }
        const std::uint64_t revision = waveManager_->GetCurrentWaveRevision();
        if (revision == 0) {
            ++invalidRevisionCount_;
            EnterError("Wave 4 Revisionが無効です。");
            return;
        }
        if (encounterStartedForRevision_ && handledWaveId_ ==
            KrakenTentacleWaveEncounterConfig::kTargetWaveId &&
            handledWaveRevision_ == revision) {
            ++duplicateStartSuppressionCount_;
            return;
        }
        handledWaveId_ = std::string(
            KrakenTentacleWaveEncounterConfig::kTargetWaveId);
        handledWaveRevision_ = revision;
        targetWaveRevision_ = revision;
        const bool started = BeginEncounter();
        if (rearmedThisUpdate) {
            if (started) {
                ++rearmStartingSuccessCount_;
                lastRearmFailureReason_ = "なし";
            } else {
                ++rearmStartingFailureCount_;
                lastRearmFailureReason_ = lastError_;
            }
            lastReentryStateAfter_ = state_;
        }
        return;
    }

    if ((state_ == KrakenTentacleWaveEncounterState::Starting ||
         state_ == KrakenTentacleWaveEncounterState::Active ||
         state_ == KrakenTentacleWaveEncounterState::Defeating) &&
        !IsTargetWaveCurrent()) {
        ++unexpectedWaveChangeCount_;
        EnterError("中ボス撃破完了前にWaveが変更されました。");
        return;
    }
    if (state_ == KrakenTentacleWaveEncounterState::Starting) {
        UpdateEntrance(gameplayDeltaTime);
        return;
    }
    if (state_ == KrakenTentacleWaveEncounterState::Active) {
        if (!kraken_->IsVisible()) {
            ++duplicateSpawnSuppressionCount_;
            EnterError("Encounter中に中ボスが非表示になりました。");
            return;
        }
        if ((!kraken_->IsAttackDamageEnabled() ||
             !kraken_->IsProjectileDamageEnabled()) &&
            !kraken_->IsDefeatPending()) {
            ++damageSetupFailureCount_;
            EnterError("Encounter中に中ボスDamageが無効化されました。");
            return;
        }
        UpdateAttackScheduler(gameplayDeltaTime);
    }
}

bool KrakenTentacleWaveEncounterController::ProcessPendingDebugCommand() {
    const PendingDebugCommand command = pendingDebugCommand_;
    pendingDebugCommand_ = PendingDebugCommand::None;
    switch (command) {
    case PendingDebugCommand::None:
        return false;
    case PendingDebugCommand::Reset:
    case PendingDebugCommand::ClearError:
        Reset();
        return true;
    case PendingDebugCommand::Refresh: {
        const bool valid = ValidateContexts() && ValidateObjective();
        lastWarning_ = valid
            ? "交戦状態を再確認しました。"
            : "再確認で無効な状態を検出しました。";
        return false;
    }
    case PendingDebugCommand::ForceStart:
        if (state_ == KrakenTentacleWaveEncounterState::WaitingForWave4 &&
            IsTargetWaveCurrent() && waveManager_ &&
            waveManager_->GetCurrentWaveRevision() != 0 &&
            !encounterStartedForRevision_) {
            handledWaveId_ = std::string(
                KrakenTentacleWaveEncounterConfig::kTargetWaveId);
            handledWaveRevision_ = waveManager_->GetCurrentWaveRevision();
            targetWaveRevision_ = handledWaveRevision_;
            BeginEncounter();
        } else {
            ++duplicateStartSuppressionCount_;
            lastWarning_ = "現在の状態ではウェーブ4交戦を強制開始できません。";
        }
        return true;
    case PendingDebugCommand::SchedulerOn:
        if (state_ == KrakenTentacleWaveEncounterState::Active) {
            schedulerEnabled_ = true;
        }
        return false;
    case PendingDebugCommand::SchedulerOff:
        schedulerEnabled_ = false;
        CancelFollowupAttack();
        return false;
    case PendingDebugCommand::AttackNow:
        if (state_ == KrakenTentacleWaveEncounterState::Active &&
            schedulerEnabled_) {
            attackTimer_ = currentAttackDelay_;
            UpdateAttackScheduler(0.0f);
        }
        return false;
    case PendingDebugCommand::RailResync:
        if (!SetRailHold(IsControllingKraken())) {
            lastWarning_ = "レール停止の再同期に失敗しました。";
        }
        return false;
    case PendingDebugCommand::ObjectiveResync:
        if (!waveManager_ || !waveManager_->SetExternalWaveObjectiveCompleted(
                completionPublished_)) {
            lastWarning_ = "外部目標の再同期に失敗しました。";
        }
        return false;
    case PendingDebugCommand::SimulateCompletion:
        lastWarning_ =
            "デバッグ模擬では実際の完了通知を行いません。";
        return false;
    }
    return false;
}

void KrakenTentacleWaveEncounterController::PostKrakenUpdate() {
    if (!initialized_ || !kraken_) {
        return;
    }
    if (state_ == KrakenTentacleWaveEncounterState::Active &&
        kraken_->IsDefeatStarted()) {
        schedulerEnabled_ = false;
        CancelFollowupAttack();
        ++defeatAttackSuppressionCount_;
        state_ = KrakenTentacleWaveEncounterState::Defeating;
    }
    if ((state_ == KrakenTentacleWaveEncounterState::Active ||
         state_ == KrakenTentacleWaveEncounterState::Defeating) &&
        kraken_->IsDefeatCompleted()) {
        PublishCompletion();
    }
}

void KrakenTentacleWaveEncounterController::PostWaveUpdate(
    float gameplayDeltaTime) {
    RefreshFramingDiagnostics();
    if (!initialized_ || state_ == KrakenTentacleWaveEncounterState::Error) {
        return;
    }
    if (!std::isfinite(gameplayDeltaTime) || gameplayDeltaTime < 0.0f) {
        EnterError("Wave移行待機時間が有限値ではありません。");
        return;
    }
    if (state_ == KrakenTentacleWaveEncounterState::CompletionPublished) {
        state_ = KrakenTentacleWaveEncounterState::WaitingForWave5;
    }
    if (state_ == KrakenTentacleWaveEncounterState::WaitingForWave5) {
        if (IsNextWaveCurrent() && waveManager_->GetCurrentWaveRevision() != 0) {
            CompleteWave5Transition();
            return;
        }
        if (!waveManager_->GetCurrentWaveId().empty() &&
            !IsTargetWaveCurrent()) {
            ++unexpectedWaveChangeCount_;
            EnterError("Wave 5以外のWaveへ予期せず移行しました。");
            return;
        }
        waitingForWave5Timer_ += gameplayDeltaTime;
        if (!std::isfinite(waitingForWave5Timer_) ||
            waitingForWave5Timer_ > wave5TransitionTimeout_) {
            ++waveTransitionTimeoutCount_;
            EnterError("External Objective完了後にWave 5へ移行しませんでした。");
        }
    }
}
