#include "SkinningEditorKrakenAttackMotion.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float kMaximumDeltaTime = 0.10f;
    constexpr float kTimeEpsilon = 0.000001f;
}

SkinningEditorKrakenAttackMotion::SkinningEditorKrakenAttackMotion() {
    Reset();
}

void SkinningEditorKrakenAttackMotion::Reset() {
    settings_ = KrakenTentacleAttackPreviewSettings{};
    selectedChainIndex_ = 0;
    loop_ = false;
    impactStartCount_ = 0;
    completedCycleCount_ = 0;
    lastImpactStartTime_ = 0.0f;
    lastError_.clear();
    SetStoppedState();
}

void SkinningEditorKrakenAttackMotion::SetStoppedState() {
    phase_ = KrakenTentacleAttackPreviewPhase::Windup;
    elapsedTime_ = 0.0f;
    phaseElapsedTime_ = 0.0f;
    loopWaitElapsedTime_ = 0.0f;
    playing_ = false;
    paused_ = false;
    waitingForLoop_ = false;
}

void SkinningEditorKrakenAttackMotion::Stop() {
    SetStoppedState();
}

bool SkinningEditorKrakenAttackMotion::PlayOnce(
    std::size_t detectedChainCount) {
    return Start(detectedChainCount, false);
}

bool SkinningEditorKrakenAttackMotion::PlayLoop(
    std::size_t detectedChainCount) {
    return Start(detectedChainCount, true);
}

bool SkinningEditorKrakenAttackMotion::Restart(
    std::size_t detectedChainCount) {
    return Start(detectedChainCount, loop_);
}

void SkinningEditorKrakenAttackMotion::SetLoopEnabled(bool enabled) {
    loop_ = enabled;
    if (!loop_ &&
        phase_ == KrakenTentacleAttackPreviewPhase::Completed) {
        playing_ = false;
        paused_ = false;
        waitingForLoop_ = false;
        loopWaitElapsedTime_ = 0.0f;
    }
}

bool SkinningEditorKrakenAttackMotion::Start(
    std::size_t detectedChainCount,
    bool loop) {
    if (!RevalidateSelectedChain(detectedChainCount)) {
        return false;
    }
    loop_ = loop;
    impactStartCount_ = 0;
    completedCycleCount_ = 0;
    lastImpactStartTime_ = 0.0f;
    lastError_.clear();
    BeginCycle();
    return true;
}

void SkinningEditorKrakenAttackMotion::BeginCycle() {
    phase_ = KrakenTentacleAttackPreviewPhase::Windup;
    elapsedTime_ = 0.0f;
    phaseElapsedTime_ = 0.0f;
    loopWaitElapsedTime_ = 0.0f;
    playing_ = true;
    paused_ = false;
    waitingForLoop_ = false;
}

void SkinningEditorKrakenAttackMotion::Pause() {
    if (playing_) {
        paused_ = true;
    }
}

void SkinningEditorKrakenAttackMotion::Resume() {
    if (playing_) {
        paused_ = false;
    }
}

bool SkinningEditorKrakenAttackMotion::SelectChain(
    std::size_t chainIndex,
    std::size_t detectedChainCount) {
    if (detectedChainCount == 0) {
        SetError("攻撃対象の触手Chainがありません。");
        return false;
    }
    if (chainIndex >= detectedChainCount) {
        SetError("範囲外の触手Chainは選択できません。");
        return false;
    }
    selectedChainIndex_ = chainIndex;
    lastError_.clear();
    return true;
}

bool SkinningEditorKrakenAttackMotion::RevalidateSelectedChain(
    std::size_t detectedChainCount) {
    if (detectedChainCount == 0) {
        SetError("攻撃対象の触手Chainを検出できないため、攻撃Previewを停止しました。");
        return false;
    }
    if (selectedChainIndex_ >= detectedChainCount) {
        selectedChainIndex_ = detectedChainCount - 1;
    }
    return true;
}

void SkinningEditorKrakenAttackMotion::SetError(
    const std::string& errorMessage) {
    SetStoppedState();
    lastError_ = errorMessage;
}

bool SkinningEditorKrakenAttackMotion::JumpToPhase(
    KrakenTentacleAttackPreviewPhase phase,
    std::size_t detectedChainCount) {
    if (!RevalidateSelectedChain(detectedChainCount)) {
        return false;
    }
    if (phase == KrakenTentacleAttackPreviewPhase::Completed) {
        SetError("Completed Phaseへ直接移動することはできません。");
        return false;
    }
    switch (phase) {
    case KrakenTentacleAttackPreviewPhase::Windup:
    case KrakenTentacleAttackPreviewPhase::WindupHold:
    case KrakenTentacleAttackPreviewPhase::Slam:
    case KrakenTentacleAttackPreviewPhase::ImpactHold:
    case KrakenTentacleAttackPreviewPhase::Recovery:
        break;
    default:
        SetError("移動先の攻撃Phaseが不正です。");
        return false;
    }

    phase_ = phase;
    phaseElapsedTime_ = 0.0f;
    elapsedTime_ = GetPhaseStartTime(phase);
    loopWaitElapsedTime_ = 0.0f;
    waitingForLoop_ = false;
    lastError_.clear();
    if (phase == KrakenTentacleAttackPreviewPhase::ImpactHold) {
        EnterImpactHold();
    }
    return true;
}

void SkinningEditorKrakenAttackMotion::SetSettings(
    const KrakenTentacleAttackPreviewSettings& settings) {
    settings_ = SanitizeKrakenTentacleAttackSettings(settings);
    const float phaseDuration = GetPhaseDuration(phase_);
    phaseElapsedTime_ = std::clamp(
        std::isfinite(phaseElapsedTime_) ? phaseElapsedTime_ : 0.0f,
        0.0f,
        phaseDuration);
    elapsedTime_ = (std::min)(
        GetMotionDuration(),
        GetPhaseStartTime(phase_) + phaseElapsedTime_);
}

void SkinningEditorKrakenAttackMotion::ResetRecommendedSettings() {
    SetSettings(KrakenTentacleAttackPreviewSettings{});
}

float SkinningEditorKrakenAttackMotion::GetMotionDuration() const {
    return GetKrakenTentacleAttackMotionDuration(settings_);
}

float SkinningEditorKrakenAttackMotion::GetPhaseDuration(
    KrakenTentacleAttackPreviewPhase phase) const {
    return GetKrakenTentacleAttackPhaseDuration(settings_, phase);
}

float SkinningEditorKrakenAttackMotion::GetPhaseStartTime(
    KrakenTentacleAttackPreviewPhase phase) const {
    return GetKrakenTentacleAttackPhaseStartTime(settings_, phase);
}

void SkinningEditorKrakenAttackMotion::Update(
    float unscaledDeltaTime,
    std::size_t detectedChainCount) {
    if (!playing_ || paused_) {
        return;
    }
    if (!RevalidateSelectedChain(detectedChainCount)) {
        return;
    }

    float remainingTime = std::clamp(
        std::isfinite(unscaledDeltaTime) ? unscaledDeltaTime : 0.0f,
        0.0f,
        kMaximumDeltaTime);
    int transitionGuard = 0;
    while (transitionGuard++ < 16) {
        if (phase_ == KrakenTentacleAttackPreviewPhase::Completed) {
            if (!loop_) {
                playing_ = false;
                paused_ = false;
                return;
            }
            waitingForLoop_ = true;
            const float intervalRemaining =
                settings_.loopInterval - loopWaitElapsedTime_;
            if (intervalRemaining > kTimeEpsilon &&
                remainingTime <= kTimeEpsilon) {
                return;
            }
            const float step = intervalRemaining > 0.0f
                ? (std::min)(remainingTime, intervalRemaining)
                : 0.0f;
            loopWaitElapsedTime_ += step;
            remainingTime -= step;
            if (loopWaitElapsedTime_ + kTimeEpsilon <
                settings_.loopInterval) {
                return;
            }
            BeginCycle();
            if (remainingTime <= kTimeEpsilon) {
                return;
            }
            continue;
        }

        const float phaseDuration = GetPhaseDuration(phase_);
        if (phaseDuration <= kTimeEpsilon) {
            AdvancePhase();
            continue;
        }
        if (remainingTime <= kTimeEpsilon) {
            return;
        }
        const float phaseRemaining =
            (std::max)(phaseDuration - phaseElapsedTime_, 0.0f);
        const float step = (std::min)(remainingTime, phaseRemaining);
        phaseElapsedTime_ += step;
        elapsedTime_ = (std::min)(GetMotionDuration(), elapsedTime_ + step);
        remainingTime -= step;
        if (phaseElapsedTime_ + kTimeEpsilon >= phaseDuration) {
            phaseElapsedTime_ = phaseDuration;
            AdvancePhase();
            continue;
        }
        return;
    }
    SetError("攻撃Phase遷移が安全上限を超えたため停止しました。");
}

void SkinningEditorKrakenAttackMotion::AdvancePhase() {
    switch (phase_) {
    case KrakenTentacleAttackPreviewPhase::Windup:
        phase_ = KrakenTentacleAttackPreviewPhase::WindupHold;
        break;
    case KrakenTentacleAttackPreviewPhase::WindupHold:
        phase_ = KrakenTentacleAttackPreviewPhase::Slam;
        break;
    case KrakenTentacleAttackPreviewPhase::Slam:
        phase_ = KrakenTentacleAttackPreviewPhase::ImpactHold;
        EnterImpactHold();
        break;
    case KrakenTentacleAttackPreviewPhase::ImpactHold:
        phase_ = KrakenTentacleAttackPreviewPhase::Recovery;
        break;
    case KrakenTentacleAttackPreviewPhase::Recovery:
        CompleteCycle();
        return;
    case KrakenTentacleAttackPreviewPhase::Completed:
    default:
        return;
    }
    phaseElapsedTime_ = 0.0f;
    elapsedTime_ = GetPhaseStartTime(phase_);
}

void SkinningEditorKrakenAttackMotion::EnterImpactHold() {
    ++impactStartCount_;
    lastImpactStartTime_ = GetPhaseStartTime(
        KrakenTentacleAttackPreviewPhase::ImpactHold);
}

void SkinningEditorKrakenAttackMotion::CompleteCycle() {
    phase_ = KrakenTentacleAttackPreviewPhase::Completed;
    phaseElapsedTime_ = 0.0f;
    elapsedTime_ = GetMotionDuration();
    loopWaitElapsedTime_ = 0.0f;
    waitingForLoop_ = loop_;
    ++completedCycleCount_;
    if (!loop_) {
        playing_ = false;
        paused_ = false;
    }
}

KrakenTentacleAttackPoseTotals
SkinningEditorKrakenAttackMotion::EvaluatePoseTotals() const {
    return EvaluateKrakenTentacleAttackPoseTotals(
        settings_,
        phase_,
        phaseElapsedTime_);
}

const char* GetKrakenTentacleAttackPhaseJapaneseLabel(
    KrakenTentacleAttackPreviewPhase phase) {
    switch (phase) {
    case KrakenTentacleAttackPreviewPhase::Windup:
        return "振りかぶり";
    case KrakenTentacleAttackPreviewPhase::WindupHold:
        return "振りかぶり停止";
    case KrakenTentacleAttackPreviewPhase::Slam:
        return "振り下ろし";
    case KrakenTentacleAttackPreviewPhase::ImpactHold:
        return "打撃位置停止";
    case KrakenTentacleAttackPreviewPhase::Recovery:
        return "復帰";
    case KrakenTentacleAttackPreviewPhase::Completed:
        return "完了";
    default:
        return "不明";
    }
}

const char* GetKrakenTentacleAttackAxisLabel(
    KrakenTentacleAttackLocalAxis axis) {
    switch (axis) {
    case KrakenTentacleAttackLocalAxis::X:
        return "X";
    case KrakenTentacleAttackLocalAxis::Y:
        return "Y";
    case KrakenTentacleAttackLocalAxis::Z:
        return "Z";
    default:
        return "不明";
    }
}
