#pragma once

#include "Matrix4x4.h"

#include <cstddef>
#include <cstdint>
#include <vector>

using KrakenAttackSequenceId = std::uint64_t;

struct KrakenAttackPlayerEnterEvent {
    std::uint64_t frameNumber = 0;
    std::uint64_t krakenColliderId = 0;
    std::uint32_t chainIndex = 0;
    float penetrationDepth = 0.0f;
    Vector3 closestPoint{};
    bool valid = false;
};

enum class KrakenAttackDamageResult : std::uint8_t {
    None,
    Disabled,
    Applied,
    BlockedByDamageInvincibility,
    BlockedByBarrelRoll,
    BlockedByDeath,
    ContextUnavailable,
    DamageApiRejected,
    SequenceUnavailable,
    HitAlreadyConsumed,
    OldFrame,
    ChainMismatch,
    PhaseMismatch,
};

struct KrakenAttackDamageEventSelection {
    KrakenAttackPlayerEnterEvent event{};
    std::size_t enterEventCount = 0;
    std::size_t oldFrameCount = 0;
    std::size_t chainMismatchCount = 0;
    std::size_t phaseMismatchCount = 0;
    KrakenAttackDamageResult result = KrakenAttackDamageResult::None;
    bool ready = false;
};

KrakenAttackSequenceId GetNextKrakenAttackSequenceId(
    KrakenAttackSequenceId previousSequenceId);

KrakenAttackDamageEventSelection SelectKrakenAttackDamageEnterEvent(
    const std::vector<KrakenAttackPlayerEnterEvent>& events,
    std::uint64_t currentFrameNumber,
    std::uint32_t selectedChainIndex,
    bool attackPhaseActive,
    bool attackDamageEnabled,
    KrakenAttackSequenceId currentSequenceId,
    bool hitAttemptConsumed);
