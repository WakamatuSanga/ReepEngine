#include "Engine/Game/Boss/Kraken/KrakenTentacleAttackDamage.h"

KrakenAttackSequenceId GetNextKrakenAttackSequenceId(
    KrakenAttackSequenceId previousSequenceId) {
    KrakenAttackSequenceId nextSequenceId = previousSequenceId + 1;
    if (nextSequenceId == 0) {
        nextSequenceId = 1;
    }
    return nextSequenceId;
}

KrakenAttackDamageEventSelection SelectKrakenAttackDamageEnterEvent(
    const std::vector<KrakenAttackPlayerEnterEvent>& events,
    std::uint64_t currentFrameNumber,
    std::uint32_t selectedChainIndex,
    bool attackPhaseActive,
    bool attackDamageEnabled,
    KrakenAttackSequenceId currentSequenceId,
    bool hitAttemptConsumed) {
    KrakenAttackDamageEventSelection selection{};
    selection.enterEventCount = events.size();
    if (!attackDamageEnabled) {
        selection.result = KrakenAttackDamageResult::Disabled;
        return selection;
    }
    if (currentSequenceId == 0) {
        selection.result = KrakenAttackDamageResult::SequenceUnavailable;
        return selection;
    }

    for (const KrakenAttackPlayerEnterEvent& event : events) {
        if (!event.valid || event.frameNumber == 0 ||
            event.frameNumber != currentFrameNumber) {
            ++selection.oldFrameCount;
            continue;
        }
        if (event.chainIndex != selectedChainIndex) {
            ++selection.chainMismatchCount;
            continue;
        }
        if (!attackPhaseActive) {
            ++selection.phaseMismatchCount;
            continue;
        }
        if (!selection.ready ||
            event.krakenColliderId < selection.event.krakenColliderId) {
            selection.event = event;
            selection.ready = true;
        }
    }

    if (!selection.ready) {
        if (selection.oldFrameCount > 0) {
            selection.result = KrakenAttackDamageResult::OldFrame;
        } else if (selection.chainMismatchCount > 0) {
            selection.result = KrakenAttackDamageResult::ChainMismatch;
        } else if (selection.phaseMismatchCount > 0) {
            selection.result = KrakenAttackDamageResult::PhaseMismatch;
        }
        return selection;
    }
    if (hitAttemptConsumed) {
        selection.ready = false;
        selection.result = KrakenAttackDamageResult::HitAlreadyConsumed;
        return selection;
    }
    selection.result = KrakenAttackDamageResult::None;
    return selection;
}
