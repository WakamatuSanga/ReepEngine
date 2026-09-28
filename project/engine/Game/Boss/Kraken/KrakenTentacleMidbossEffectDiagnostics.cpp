#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossEffectController.h"

#include <algorithm>
#include <cmath>

namespace {
bool IsFinite(const Vector3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}
}

KrakenTentacleResolvedEffectPosition ResolveKrakenTentacleEffectPosition(
    const KrakenTentacleEffectPositionCandidates& candidates) {
    KrakenTentacleResolvedEffectPosition result{};
    const std::size_t count = (std::min)(
        candidates.count, candidates.values.size());
    for (std::size_t index = 0; index < count; ++index) {
        const KrakenTentacleEffectPositionCandidate& candidate =
            candidates.values[index];
        if (!IsFinite(candidate.worldPosition) ||
            candidate.source == KrakenTentacleEffectPositionSource::None) {
            ++result.rejectedCandidateCount;
            continue;
        }
        result.worldPosition = candidate.worldPosition;
        result.source = candidate.source;
        result.usedFallback = index > 0;
        result.valid = true;
        return result;
    }
    return result;
}
