#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossProjectileDamage.h"

#include <algorithm>

namespace {
bool IsRoleValid(KrakenProjectileHitRole role) {
    return role == KrakenProjectileHitRole::Body ||
        role == KrakenProjectileHitRole::WeakPoint;
}

bool IsEventLess(
    const KrakenProjectileEnterEvent& lhs,
    const KrakenProjectileEnterEvent& rhs) {
    if (lhs.projectileRuntimeId != rhs.projectileRuntimeId) {
        return lhs.projectileRuntimeId < rhs.projectileRuntimeId;
    }
    if (lhs.role != rhs.role) {
        return lhs.role == KrakenProjectileHitRole::WeakPoint;
    }
    return lhs.krakenColliderId < rhs.krakenColliderId;
}
}

std::vector<KrakenProjectileEnterEvent>
AggregateKrakenProjectileEnterEvents(
    const std::vector<KrakenProjectileEnterEvent>& events,
    std::uint64_t currentFrame,
    KrakenProjectileAggregationDiagnostics& diagnostics) {
    diagnostics = {};
    std::vector<KrakenProjectileEnterEvent> validEvents;
    validEvents.reserve(events.size());
    for (const KrakenProjectileEnterEvent& event : events) {
        if (!event.valid) {
            ++diagnostics.invalidEventCount;
            continue;
        }
        if (event.frameNumber != currentFrame) {
            ++diagnostics.oldFrameRejectionCount;
            continue;
        }
        if (event.projectileRuntimeId == 0) {
            ++diagnostics.runtimeIdZeroCount;
            continue;
        }
        if (!IsRoleValid(event.role)) {
            ++diagnostics.invalidRoleCount;
            continue;
        }
        if (event.role == KrakenProjectileHitRole::Body) {
            ++diagnostics.bodyEnterCount;
        } else {
            ++diagnostics.weakPointEnterCount;
        }
        validEvents.push_back(event);
    }

    std::sort(validEvents.begin(), validEvents.end(), IsEventLess);
    std::vector<KrakenProjectileEnterEvent> aggregated;
    for (std::size_t begin = 0; begin < validEvents.size();) {
        std::size_t end = begin + 1;
        while (end < validEvents.size() &&
            validEvents[end].projectileRuntimeId ==
                validEvents[begin].projectileRuntimeId) {
            ++end;
        }

        std::size_t bodyCount = 0;
        std::size_t weakPointCount = 0;
        for (std::size_t index = begin; index < end; ++index) {
            if (validEvents[index].role == KrakenProjectileHitRole::Body) {
                ++bodyCount;
            } else {
                ++weakPointCount;
            }
        }
        if (bodyCount > 0 && weakPointCount > 0) {
            ++diagnostics.bodyAndWeakPointCount;
            ++diagnostics.weakPointPriorityCount;
            diagnostics.bodySuppressionCount += bodyCount;
        }
        diagnostics.duplicateBodySuppressionCount +=
            bodyCount > 0 ? bodyCount - 1 : 0;
        diagnostics.duplicateWeakPointSuppressionCount +=
            weakPointCount > 0 ? weakPointCount - 1 : 0;
        aggregated.push_back(validEvents[begin]);
        begin = end;
    }
    diagnostics.aggregatedCount = aggregated.size();
    return aggregated;
}
