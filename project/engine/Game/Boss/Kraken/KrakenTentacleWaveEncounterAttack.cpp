#include "KrakenTentacleWaveEncounterController.h"

#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossController.h"

#include <cmath>

void KrakenTentacleWaveEncounterController::UpdateAttackScheduler(
    float gameplayDeltaTime) {
    if (!schedulerEnabled_ ||
        state_ != KrakenTentacleWaveEncounterState::Active || !kraken_) {
        return;
    }
    if (kraken_->IsDefeatPending() || kraken_->IsDefeatStarted() ||
        kraken_->IsDefeatCompleted() || kraken_->GetCurrentHp() <= 0.0f) {
        schedulerEnabled_ = false;
        attackTimer_ = 0.0f;
        ++defeatAttackSuppressionCount_;
        return;
    }
    if (!kraken_->IsVisible() || !kraken_->IsAttackDamageEnabled() ||
        !kraken_->IsProjectileDamageEnabled()) {
        return;
    }
    if (kraken_->GetRuntimeState() != KrakenTentacleMidbossState::Idle) {
        attackTimer_ = 0.0f;
        return;
    }
    if (detectedChainCount_ == 0 ||
        detectedChainCount_ != kraken_->GetDetectedChainCount()) {
        ++zeroChainCount_;
        EnterError("攻撃Chain数が無効または開始時から変更されました。");
        return;
    }

    attackTimer_ += gameplayDeltaTime;
    if (!std::isfinite(attackTimer_) || !std::isfinite(currentAttackDelay_)) {
        EnterError("自動攻撃の待機時間が有限値ではありません。");
        return;
    }
    if (attackTimer_ < currentAttackDelay_) {
        return;
    }

    const std::size_t chain = nextAttackChain_ % detectedChainCount_;
    if (!kraken_->SetSelectedAttackChainForWaveEncounter(chain) ||
        !kraken_->TryStartAttackForWaveEncounter()) {
        ++attackRejectedCount_;
        attackTimer_ = 0.0f;
        currentAttackDelay_ = retryDelay_;
        lastWarning_ = "攻撃開始が拒否されたため、再試行待機に入りました。";
        return;
    }

    ++attackStartCount_;
    nextAttackChain_ = (chain + 1) % detectedChainCount_;
    attackTimer_ = 0.0f;
    currentAttackDelay_ = nextAttackInterval_;
    firstAttackPending_ = false;
    lastWarning_ = "なし";
}
