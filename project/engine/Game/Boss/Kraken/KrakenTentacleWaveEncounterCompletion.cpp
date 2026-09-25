#include "KrakenTentacleWaveEncounterController.h"

#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossController.h"
#include "Engine/Game/RailShooter/EnemyWaveManager.h"

bool KrakenTentacleWaveEncounterController::PublishCompletion() {
    if (completionPublished_) {
        ++duplicateCompletionSuppressionCount_;
        return true;
    }
    if (!kraken_ || !waveManager_ || !kraken_->IsDefeatCompleted()) {
        ++objectiveCompletionFailureCount_;
        EnterError("中ボス撃破完了を確認できません。");
        return false;
    }

    const std::uint64_t sequenceId = kraken_->GetDefeatSequenceId();
    if (sequenceId == 0) {
        ++objectiveCompletionFailureCount_;
        EnterError("撃破シーケンス識別子が無効です。");
        return false;
    }
    if (publishedDefeatSequenceId_ == sequenceId) {
        ++duplicateCompletionSuppressionCount_;
        return true;
    }

    schedulerEnabled_ = false;
    attackTimer_ = 0.0f;
    EndFovOverride(false);
    DisableDamage();
    kraken_->SetWaveEncounterControlActive(false);
    if (kraken_->IsAttackDamageEnabled() ||
        kraken_->IsProjectileDamageEnabled()) {
        ++damageSetupFailureCount_;
        EnterError("撃破完了時にダメージを無効化できませんでした。");
        return false;
    }
    if (!SetRailHold(false)) {
        EnterError("撃破完了時にレール進行を復帰できませんでした。");
        return false;
    }
    if (!waveManager_->SetExternalWaveObjectiveCompleted(true) ||
        !waveManager_->IsExternalWaveObjectiveCompleted()) {
        ++objectiveCompletionFailureCount_;
        EnterError("ウェーブ4の外部目標を完了にできませんでした。");
        return false;
    }

    publishedDefeatSequenceId_ = sequenceId;
    completionPublished_ = true;
    ++completionPublishCount_;
    waitingForWave5Timer_ = 0.0f;
    state_ = KrakenTentacleWaveEncounterState::CompletionPublished;
    return true;
}

void KrakenTentacleWaveEncounterController::CompleteWave5Transition() {
    if (!waveManager_ || !kraken_) {
        EnterError("ウェーブ5確認時の参照が無効です。");
        return;
    }
    const std::uint64_t revision = waveManager_->GetCurrentWaveRevision();
    if (!completionPublished_ || revision == 0) {
        ++invalidRevisionCount_;
        EnterError("ウェーブ5の完了確認条件が無効です。");
        return;
    }

    schedulerEnabled_ = false;
    EndFovOverride(false);
    DisableDamage();
    kraken_->SetWaveEncounterControlActive(false);
    HideKraken();
    if (!SetRailHold(false)) {
        EnterError("ウェーブ5移行時にレール進行を再確認できませんでした。");
        return;
    }

    nextWaveRevision_ = revision;
    ++wave5TransitionCount_;
    state_ = KrakenTentacleWaveEncounterState::Completed;
}
