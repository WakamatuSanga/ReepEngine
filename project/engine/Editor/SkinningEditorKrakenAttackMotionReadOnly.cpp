#include "SkinningEditorKrakenAttackMotion.h"

#include <algorithm>
#include <cmath>

bool SkinningEditorKrakenAttackMotion::SeekPhaseNormalizedTime(
    KrakenTentacleAttackPreviewPhase phase,
    float normalizedTime,
    std::size_t detectedChainCount) {
    if (!std::isfinite(normalizedTime)) {
        SetError("Phase進行率が有限値ではないため移動できません。");
        return false;
    }
    if (!JumpToPhase(phase, detectedChainCount)) {
        return false;
    }
    const float duration = GetPhaseDuration(phase);
    if (!std::isfinite(duration) || duration <= 0.0f) {
        SetError("Phase時間が不正なため進行率を指定できません。");
        return false;
    }
    const float safeNormalizedTime = std::clamp(
        normalizedTime, 0.0f, 1.0f);
    phaseElapsedTime_ = duration * safeNormalizedTime;
    elapsedTime_ = (std::min)(
        GetMotionDuration(),
        GetPhaseStartTime(phase) + phaseElapsedTime_);
    lastError_.clear();
    return true;
}
