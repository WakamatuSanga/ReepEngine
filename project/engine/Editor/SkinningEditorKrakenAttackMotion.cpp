#include "SkinningEditorKrakenAttackMotion.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float kMinimumWindupDuration = 0.10f;
    constexpr float kMinimumSlamDuration = 0.05f;
    constexpr float kMinimumRecoveryDuration = 0.10f;
    constexpr float kMaximumLongPhaseDuration = 2.00f;
    constexpr float kMaximumShortPhaseDuration = 1.00f;
    constexpr float kMaximumLoopInterval = 2.00f;
    constexpr float kMaximumDeltaTime = 0.10f;
    constexpr float kMinimumTipBias = 0.10f;
    constexpr float kMaximumTipBias = 8.00f;
    constexpr float kTimeEpsilon = 0.000001f;

    float ClampFinite(float value, float minimum, float maximum, float fallback) {
        if (!std::isfinite(value)) {
            return fallback;
        }
        return std::clamp(value, minimum, maximum);
    }

    float NormalizeSign(float value) {
        if (!std::isfinite(value) || value == 0.0f) {
            return 1.0f;
        }
        return value < 0.0f ? -1.0f : 1.0f;
    }

    KrakenTentacleAttackLocalAxis SanitizeAxis(
        KrakenTentacleAttackLocalAxis axis,
        KrakenTentacleAttackLocalAxis fallback) {
        switch (axis) {
        case KrakenTentacleAttackLocalAxis::X:
        case KrakenTentacleAttackLocalAxis::Y:
        case KrakenTentacleAttackLocalAxis::Z:
            return axis;
        default:
            return fallback;
        }
    }

    KrakenTentacleAttackPreviewSettings SanitizeSettings(
        const KrakenTentacleAttackPreviewSettings& source) {
        const KrakenTentacleAttackPreviewSettings defaults{};
        KrakenTentacleAttackPreviewSettings result = source;
        result.windupDuration = ClampFinite(
            source.windupDuration,
            kMinimumWindupDuration,
            kMaximumLongPhaseDuration,
            defaults.windupDuration);
        result.windupHoldDuration = ClampFinite(
            source.windupHoldDuration,
            0.0f,
            kMaximumShortPhaseDuration,
            defaults.windupHoldDuration);
        result.slamDuration = ClampFinite(
            source.slamDuration,
            kMinimumSlamDuration,
            kMaximumShortPhaseDuration,
            defaults.slamDuration);
        result.impactHoldDuration = ClampFinite(
            source.impactHoldDuration,
            0.0f,
            kMaximumShortPhaseDuration,
            defaults.impactHoldDuration);
        result.recoveryDuration = ClampFinite(
            source.recoveryDuration,
            kMinimumRecoveryDuration,
            kMaximumLongPhaseDuration,
            defaults.recoveryDuration);
        result.loopInterval = ClampFinite(
            source.loopInterval,
            0.0f,
            kMaximumLoopInterval,
            defaults.loopInterval);

        result.windupPrimaryTotalDegrees = ClampFinite(
            source.windupPrimaryTotalDegrees,
            -120.0f,
            120.0f,
            defaults.windupPrimaryTotalDegrees);
        result.slamPrimaryTotalDegrees = ClampFinite(
            source.slamPrimaryTotalDegrees,
            -120.0f,
            120.0f,
            defaults.slamPrimaryTotalDegrees);
        result.windupSecondaryTotalDegrees = ClampFinite(
            source.windupSecondaryTotalDegrees,
            -60.0f,
            60.0f,
            defaults.windupSecondaryTotalDegrees);
        result.slamSecondaryTotalDegrees = ClampFinite(
            source.slamSecondaryTotalDegrees,
            -60.0f,
            60.0f,
            defaults.slamSecondaryTotalDegrees);
        result.tipBias = ClampFinite(
            source.tipBias,
            kMinimumTipBias,
            kMaximumTipBias,
            defaults.tipBias);
        result.fixedLeadingBoneCount =
            (std::min)(source.fixedLeadingBoneCount, std::uint32_t{ 1024 });
        result.primaryAxis = SanitizeAxis(
            source.primaryAxis,
            KrakenTentacleAttackLocalAxis::X);
        result.secondaryAxis = SanitizeAxis(
            source.secondaryAxis,
            KrakenTentacleAttackLocalAxis::Z);
        result.primarySign = NormalizeSign(source.primarySign);
        result.secondarySign = NormalizeSign(source.secondarySign);
        return result;
    }

    float Clamp01(float value) {
        return std::clamp(value, 0.0f, 1.0f);
    }

    float Lerp(float start, float end, float t) {
        return start + ((end - start) * t);
    }

    float SmoothStep(float t) {
        const float safeT = Clamp01(t);
        return safeT * safeT * (3.0f - (2.0f * safeT));
    }
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
    settings_ = SanitizeSettings(settings);
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
    return settings_.windupDuration +
        settings_.windupHoldDuration +
        settings_.slamDuration +
        settings_.impactHoldDuration +
        settings_.recoveryDuration;
}

float SkinningEditorKrakenAttackMotion::GetPhaseDuration(
    KrakenTentacleAttackPreviewPhase phase) const {
    switch (phase) {
    case KrakenTentacleAttackPreviewPhase::Windup:
        return settings_.windupDuration;
    case KrakenTentacleAttackPreviewPhase::WindupHold:
        return settings_.windupHoldDuration;
    case KrakenTentacleAttackPreviewPhase::Slam:
        return settings_.slamDuration;
    case KrakenTentacleAttackPreviewPhase::ImpactHold:
        return settings_.impactHoldDuration;
    case KrakenTentacleAttackPreviewPhase::Recovery:
        return settings_.recoveryDuration;
    case KrakenTentacleAttackPreviewPhase::Completed:
    default:
        return 0.0f;
    }
}

float SkinningEditorKrakenAttackMotion::GetPhaseStartTime(
    KrakenTentacleAttackPreviewPhase phase) const {
    switch (phase) {
    case KrakenTentacleAttackPreviewPhase::Windup:
        return 0.0f;
    case KrakenTentacleAttackPreviewPhase::WindupHold:
        return settings_.windupDuration;
    case KrakenTentacleAttackPreviewPhase::Slam:
        return settings_.windupDuration + settings_.windupHoldDuration;
    case KrakenTentacleAttackPreviewPhase::ImpactHold:
        return settings_.windupDuration + settings_.windupHoldDuration +
            settings_.slamDuration;
    case KrakenTentacleAttackPreviewPhase::Recovery:
        return settings_.windupDuration + settings_.windupHoldDuration +
            settings_.slamDuration + settings_.impactHoldDuration;
    case KrakenTentacleAttackPreviewPhase::Completed:
    default:
        return GetMotionDuration();
    }
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
    KrakenTentacleAttackPoseTotals result{};
    const float duration = GetPhaseDuration(phase_);
    const float t = duration > kTimeEpsilon
        ? std::clamp(phaseElapsedTime_ / duration, 0.0f, 1.0f)
        : 1.0f;
    switch (phase_) {
    case KrakenTentacleAttackPreviewPhase::Windup: {
        const float smoothT = SmoothStep(t);
        result.primaryDegrees = Lerp(
            0.0f,
            settings_.windupPrimaryTotalDegrees,
            smoothT);
        result.secondaryDegrees = Lerp(
            0.0f,
            settings_.windupSecondaryTotalDegrees,
            smoothT);
        break;
    }
    case KrakenTentacleAttackPreviewPhase::WindupHold:
        result.primaryDegrees = settings_.windupPrimaryTotalDegrees;
        result.secondaryDegrees = settings_.windupSecondaryTotalDegrees;
        break;
    case KrakenTentacleAttackPreviewPhase::Slam: {
        const float cubicT = t * t * t;
        result.primaryDegrees = Lerp(
            settings_.windupPrimaryTotalDegrees,
            settings_.slamPrimaryTotalDegrees,
            cubicT);
        result.secondaryDegrees = Lerp(
            settings_.windupSecondaryTotalDegrees,
            settings_.slamSecondaryTotalDegrees,
            cubicT);
        break;
    }
    case KrakenTentacleAttackPreviewPhase::ImpactHold:
        result.primaryDegrees = settings_.slamPrimaryTotalDegrees;
        result.secondaryDegrees = settings_.slamSecondaryTotalDegrees;
        break;
    case KrakenTentacleAttackPreviewPhase::Recovery: {
        const float smoothT = SmoothStep(t);
        result.primaryDegrees = Lerp(
            settings_.slamPrimaryTotalDegrees,
            0.0f,
            smoothT);
        result.secondaryDegrees = Lerp(
            settings_.slamSecondaryTotalDegrees,
            0.0f,
            smoothT);
        break;
    }
    case KrakenTentacleAttackPreviewPhase::Completed:
    default:
        break;
    }
    result.finite = std::isfinite(result.primaryDegrees) &&
        std::isfinite(result.secondaryDegrees);
    if (!result.finite) {
        result.primaryDegrees = 0.0f;
        result.secondaryDegrees = 0.0f;
    }
    return result;
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
