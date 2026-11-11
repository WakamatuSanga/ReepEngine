#include "KrakenTentacleWaveEncounterController.h"

#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossController.h"

#include <cmath>

namespace {
constexpr float kDoubleSlamInterval = 0.35f;
}

void KrakenTentacleWaveEncounterController::CancelFollowupAttack() {
    followupPhase_ = FollowupPhase::None;
    firstAttackChain_ = 0;
    attackTimer_ = 0.0f;
    currentAttackDelay_ = nextAttackInterval_;
}

void KrakenTentacleWaveEncounterController::UpdateAttackScheduler(
    float gameplayDeltaTime) {
    if (!schedulerEnabled_ ||
        state_ != KrakenTentacleWaveEncounterState::Active || !kraken_) {
        if (followupPhase_ != FollowupPhase::None) {
            CancelFollowupAttack();
        }
        return;
    }
    if (kraken_->IsDefeatPending() || kraken_->IsDefeatStarted() ||
        kraken_->IsDefeatCompleted() || kraken_->GetCurrentHp() <= 0.0f) {
        schedulerEnabled_ = false;
        CancelFollowupAttack();
        ++defeatAttackSuppressionCount_;
        return;
    }
    if (!kraken_->IsVisible() || !kraken_->IsAttackDamageEnabled() ||
        !kraken_->IsProjectileDamageEnabled()) {
        CancelFollowupAttack();
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

    if (followupPhase_ == FollowupPhase::AwaitRecovery) {
        // Start the gap only after Recovery has returned to Idle; none of the
        // preceding attack frame is counted toward the 0.35 second interval.
        followupPhase_ = FollowupPhase::Interval;
        attackTimer_ = 0.0f;
        currentAttackDelay_ = kDoubleSlamInterval;
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

    const bool isFollowup = followupPhase_ == FollowupPhase::Interval;
    if (!isFollowup && nextAttackIsSweep_) {
        if (!kraken_->TryStartSweepForWaveEncounter()) {
            ++attackRejectedCount_;
            attackTimer_ = 0.0f;
            currentAttackDelay_ = retryDelay_;
            lastWarning_ = "横薙ぎを開始できなかったため、再試行待機に入りました。";
            return;
        }
        ++attackStartCount_;
        nextAttackIsSweep_ = false;
        attackTimer_ = 0.0f;
        currentAttackDelay_ = nextAttackInterval_;
        lastWarning_ = "なし";
        return;
    }
    std::size_t chain = nextAttackChain_ % detectedChainCount_;
    bool selected = false;
    if (isFollowup) {
        for (std::size_t offset = 0; offset < detectedChainCount_; ++offset) {
            chain = (nextAttackChain_ + offset) % detectedChainCount_;
            if (chain != firstAttackChain_ &&
                kraken_->SetSelectedAttackChainForWaveEncounter(chain)) {
                selected = true;
                break;
            }
        }
        if (!selected) {
            CancelFollowupAttack();
            nextAttackIsSweep_ = true;
            lastWarning_ = "別の有効Chainがないため、2連続叩きつけを単発で終了しました。";
            return;
        }
    } else {
        selected = kraken_->SetSelectedAttackChainForWaveEncounter(chain);
    }
    if (!selected || !kraken_->TryStartAttackForWaveEncounter()) {
        ++attackRejectedCount_;
        if (isFollowup) {
            CancelFollowupAttack();
            nextAttackIsSweep_ = true;
            lastWarning_ = "2本目の攻撃開始が拒否されたため、通常待機へ戻りました。";
            return;
        }
        attackTimer_ = 0.0f;
        currentAttackDelay_ = retryDelay_;
        lastWarning_ = "攻撃開始が拒否されたため、再試行待機に入りました。";
        return;
    }

    ++attackStartCount_;
    nextAttackChain_ = (chain + 1) % detectedChainCount_;
    attackTimer_ = 0.0f;
    currentAttackDelay_ = nextAttackInterval_;
    if (isFollowup) {
        followupPhase_ = FollowupPhase::None;
        firstAttackChain_ = 0;
        nextAttackIsSweep_ = true;
    } else {
        if (nextAttackIsDouble_) {
            firstAttackChain_ = chain;
            followupPhase_ = FollowupPhase::AwaitRecovery;
        } else {
            nextAttackIsSweep_ = true;
        }
        // Advance the pattern once per attack group, not once per slam.
        nextAttackIsDouble_ = !nextAttackIsDouble_;
    }
    firstAttackPending_ = false;
    lastWarning_ = "なし";
}
