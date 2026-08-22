#pragma once

#include "Matrix4x4.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class KrakenProjectileHitRole : std::uint8_t {
    Body,
    WeakPoint,
};

enum class KrakenProjectileSnapshotType : std::uint8_t {
    Unknown,
    NormalShot,
    LockedWingShot,
};

struct KrakenProjectileEnterEvent {
    std::uint64_t frameNumber = 0;
    std::uint64_t krakenColliderId = 0;
    std::uint32_t chainIndex = 0;
    std::uint64_t projectileRuntimeId = 0;
    KrakenProjectileSnapshotType projectileType =
        KrakenProjectileSnapshotType::Unknown;
    KrakenProjectileHitRole role = KrakenProjectileHitRole::Body;
    float projectileDamage = 0.0f;
    float penetrationDepth = 0.0f;
    Vector3 closestPoint{};
    bool valid = false;
};

struct KrakenProjectileAggregationDiagnostics {
    std::size_t bodyEnterCount = 0;
    std::size_t weakPointEnterCount = 0;
    std::size_t aggregatedCount = 0;
    std::size_t bodyAndWeakPointCount = 0;
    std::size_t weakPointPriorityCount = 0;
    std::size_t bodySuppressionCount = 0;
    std::size_t duplicateBodySuppressionCount = 0;
    std::size_t duplicateWeakPointSuppressionCount = 0;
    std::size_t oldFrameRejectionCount = 0;
    std::size_t runtimeIdZeroCount = 0;
    std::size_t invalidEventCount = 0;
    std::size_t invalidRoleCount = 0;
};

struct KrakenProjectileLastHitDiagnostics {
    KrakenProjectileEnterEvent event{};
    float weakPointMultiplier = 1.0f;
    float finalDamage = 0.0f;
    float hpBefore = 0.0f;
    float hpAfter = 0.0f;
    bool bulletKillSucceeded = false;
    bool bulletKillFailed = false;
    bool valid = false;
};

struct KrakenProjectileDamageDiagnostics {
    KrakenProjectileAggregationDiagnostics frameAggregation{};
    KrakenProjectileLastHitDiagnostics lastHit{};
    std::uint64_t bodyDamageAppliedCount = 0;
    std::uint64_t weakPointDamageAppliedCount = 0;
    std::uint64_t bulletKillSuccessCount = 0;
    std::uint64_t bulletKillFailureCount = 0;
    std::uint64_t consumedProjectileSuppressionCount = 0;
    std::uint64_t stayDamageSuppressionCount = 0;
    std::uint64_t exitDamageSuppressionCount = 0;
    std::uint64_t defeatPendingRejectionCount = 0;
    std::uint64_t projectileDamageDisabledRejectionCount = 0;
    std::uint64_t hiddenRejectionCount = 0;
    std::uint64_t invalidEventRejectionCount = 0;
    std::uint64_t invalidDamageCount = 0;
    std::uint64_t nonFiniteHpCount = 0;
    std::uint64_t nonFiniteDamageCount = 0;
    std::uint64_t nonFiniteMultiplierCount = 0;
    std::uint64_t runtimeIdZeroCount = 0;
    std::uint64_t playerBulletManagerMissingCount = 0;
    std::uint64_t invalidRoleCount = 0;
    std::uint64_t killTargetNotFoundCount = 0;
    std::uint64_t duplicateRuntimeIdCount = 0;
    std::uint64_t sameProjectileDoubleDamageCount = 0;
    std::uint64_t hpZeroReachedCount = 0;
    std::uint64_t defeatPendingStartCount = 0;
    std::uint64_t smallEnemyInstantKillUseCount = 0;
    float totalDamage = 0.0f;
    std::string lastError;
    std::string lastWarning;
};

std::vector<KrakenProjectileEnterEvent>
AggregateKrakenProjectileEnterEvents(
    const std::vector<KrakenProjectileEnterEvent>& events,
    std::uint64_t currentFrame,
    KrakenProjectileAggregationDiagnostics& diagnostics);
