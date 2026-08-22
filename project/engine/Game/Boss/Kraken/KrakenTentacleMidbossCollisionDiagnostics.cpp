#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossCollisionDiagnostics.h"

#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {
bool IsFinite(const Vector3& value) {
    return std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

bool IsSnapshotValid(
    const PlayerBulletManager::PlayerBulletCollisionSnapshot& snapshot) {
    return snapshot.runtimeId != 0 && snapshot.active && !snapshot.killed &&
        IsFinite(snapshot.worldPosition) && IsFinite(snapshot.velocity) &&
        std::isfinite(snapshot.radius) && snapshot.radius > 0.0f &&
        std::isfinite(snapshot.lifeTime) && snapshot.lifeTime >= 0.0f &&
        std::isfinite(snapshot.elapsedTime) && snapshot.elapsedTime >= 0.0f;
}

bool IsEqual(const Vector3& lhs, const Vector3& rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

bool IsEqual(
    const PlayerBulletManager::PlayerBulletCollisionSnapshot& lhs,
    const PlayerBulletManager::PlayerBulletCollisionSnapshot& rhs) {
    return lhs.runtimeId == rhs.runtimeId &&
        IsEqual(lhs.worldPosition, rhs.worldPosition) &&
        IsEqual(lhs.velocity, rhs.velocity) && lhs.radius == rhs.radius &&
        lhs.lifeTime == rhs.lifeTime && lhs.elapsedTime == rhs.elapsedTime &&
        lhs.damage == rhs.damage && lhs.projectileType == rhs.projectileType &&
        lhs.lockedTargetId == rhs.lockedTargetId &&
        lhs.launchPhase == rhs.launchPhase && lhs.active == rhs.active &&
        lhs.killed == rhs.killed && lhs.homingReady == rhs.homingReady &&
        lhs.exhaustEnabled == rhs.exhaustEnabled;
}

void ResetRoleFrameDiagnostics(
    KrakenTentacleCollisionRoleDiagnostics& diagnostics) {
    diagnostics.currentIntersectionCount = 0;
    diagnostics.frameIntersectionCount = 0;
    diagnostics.frameEnterCount = 0;
    diagnostics.frameStayCount = 0;
    diagnostics.frameExitCount = 0;
}
}

KrakenTentacleCollisionProjectileType ToKrakenProjectileType(
    PlayerBulletManager::PlayerProjectileType type) {
    return type == PlayerBulletManager::PlayerProjectileType::LockedWingShot
        ? KrakenTentacleCollisionProjectileType::LockedWingShot
        : KrakenTentacleCollisionProjectileType::NormalShot;
}

void RefreshKrakenBulletSnapshotDiagnostics(
    KrakenTentacleMidbossDiagnostics& diagnostics,
    const std::vector<PlayerBulletManager::PlayerBulletCollisionSnapshot>&
        snapshots) {
    diagnostics.playerBulletCollisionSnapshotCount = snapshots.size();
    diagnostics.playerBulletSnapshotValidCount = 0;
    diagnostics.playerBulletSnapshotInvalidCount = 0;
    diagnostics.normalShotSnapshotCount = 0;
    diagnostics.lockedWingShotSnapshotCount = 0;
    diagnostics.invalidBulletDamageSnapshotCount = 0;
    diagnostics.stableRuntimeIdDuplicateCount = 0;
    diagnostics.lastPlayerBulletRuntimeId = 0;
    diagnostics.maximumConcurrentBulletSnapshotCount = (std::max)(
        diagnostics.maximumConcurrentBulletSnapshotCount, snapshots.size());

    std::vector<std::uint64_t> runtimeIds;
    runtimeIds.reserve(snapshots.size());
    for (const auto& snapshot : snapshots) {
        diagnostics.lastPlayerBulletRuntimeId = snapshot.runtimeId;
        if (IsSnapshotValid(snapshot)) {
            ++diagnostics.playerBulletSnapshotValidCount;
        } else {
            ++diagnostics.playerBulletSnapshotInvalidCount;
        }
        if (!IsFinite(snapshot.worldPosition) ||
            !std::isfinite(snapshot.radius)) {
            ++diagnostics.nonFiniteSphereCount;
        }
        if (snapshot.projectileType ==
            PlayerBulletManager::PlayerProjectileType::LockedWingShot) {
            ++diagnostics.lockedWingShotSnapshotCount;
        } else {
            ++diagnostics.normalShotSnapshotCount;
        }
        if (snapshot.damage <= 0) {
            ++diagnostics.invalidBulletDamageSnapshotCount;
        }
        if (snapshot.runtimeId != 0) {
            runtimeIds.push_back(snapshot.runtimeId);
        }
    }

    std::sort(runtimeIds.begin(), runtimeIds.end());
    for (std::size_t index = 1; index < runtimeIds.size(); ++index) {
        diagnostics.stableRuntimeIdDuplicateCount +=
            runtimeIds[index - 1] == runtimeIds[index] ? 1 : 0;
    }
}

bool AreKrakenBulletSnapshotsEquivalent(
    const std::vector<PlayerBulletManager::PlayerBulletCollisionSnapshot>& lhs,
    const std::vector<PlayerBulletManager::PlayerBulletCollisionSnapshot>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (!IsEqual(lhs[index], rhs[index])) {
            return false;
        }
    }
    return true;
}

void KrakenTentacleMidbossController::Impl::ResetCollisionQueryState(
    bool resetCumulativeDiagnostics) {
    ++collisionRegistrationGeneration;
    if (collisionRegistrationGeneration == 0) {
        collisionRegistrationGeneration = 1;
    }
    previousCollisionPairs.clear();
    currentCollisionPairs.clear();
    collisionFrameEvents.clear();
    for (KrakenTentacleMidbossCapsuleSnapshot& snapshot : capsuleSnapshots) {
        snapshot.lastRegisteredFrame = 0;
        snapshot.registrationGeneration = collisionRegistrationGeneration;
        snapshot.queryTarget = false;
        snapshot.gameplayRegistered = false;
        snapshot.registrationFailed = false;
    }
    for (KrakenTentacleMidbossTipSnapshot& snapshot : tipSnapshots) {
        snapshot.lastRegisteredFrame = 0;
        snapshot.registrationGeneration = collisionRegistrationGeneration;
        snapshot.queryTarget = false;
        snapshot.gameplayRegistered = false;
        snapshot.registrationFailed = false;
    }
    diagnostics.gameplayRegisteredCount = 0;
    diagnostics.gameplayRegistrationObserved = false;
    diagnostics.collisionQueryTargetCount = 0;
    diagnostics.queryTargetAttackColliderCount = 0;
    diagnostics.queryTargetDamageColliderCount = 0;
    diagnostics.queryTargetWeakPointCount = 0;
    diagnostics.playerCollisionSnapshotCount = 0;
    diagnostics.playerBulletCollisionSnapshotCount = 0;
    diagnostics.playerBulletSnapshotValidCount = 0;
    diagnostics.playerBulletSnapshotInvalidCount = 0;
    diagnostics.normalShotSnapshotCount = 0;
    diagnostics.lockedWingShotSnapshotCount = 0;
    diagnostics.invalidBulletDamageSnapshotCount = 0;
    diagnostics.stableRuntimeIdDuplicateCount = 0;
    diagnostics.lastPlayerBulletRuntimeId = 0;
    diagnostics.collisionQueryCandidatePairCount = 0;
    diagnostics.collisionQueryTestCount = 0;
    diagnostics.invalidCollisionQueryCount = 0;
    diagnostics.currentCollisionPairCount = 0;
    diagnostics.currentAttackPlayerPairCount = 0;
    diagnostics.currentDamageBulletPairCount = 0;
    diagnostics.currentWeakPointBulletPairCount = 0;
    diagnostics.collisionEnterCount = 0;
    diagnostics.collisionStayCount = 0;
    diagnostics.collisionExitCount = 0;
    diagnostics.bodyAndWeakPointSameBulletCount = 0;
    diagnostics.duplicateCollisionPairCount = 0;
    diagnostics.staleCollisionPairCount = 0;
    diagnostics.invalidPlayerSnapshotCount = 0;
    diagnostics.invalidBulletSnapshotCount = 0;
    diagnostics.nonFiniteSphereCount = 0;
    diagnostics.playerCollisionSnapshotValid = false;
    diagnostics.playerAlive = false;
    diagnostics.playerCollisionEnabled = false;
    diagnostics.playerCollisionCenter = {};
    diagnostics.playerCollisionRadius = 0.0f;
    diagnostics.bulletSnapshotUnchanged = true;
    diagnostics.lastCollisionQueryFrameIndex = ~std::uint64_t{ 0 };
    ResetRoleFrameDiagnostics(diagnostics.attackPlayerCollision);
    ResetRoleFrameDiagnostics(diagnostics.damageBulletCollision);
    ResetRoleFrameDiagnostics(diagnostics.weakPointBulletCollision);
    if (!resetCumulativeDiagnostics) {
        return;
    }
    diagnostics.collisionQueryFrameCount = 0;
    diagnostics.totalCollisionQueryTestCount = 0;
    diagnostics.totalCollisionEnterCount = 0;
    diagnostics.totalCollisionStayCount = 0;
    diagnostics.totalCollisionExitCount = 0;
    diagnostics.totalBodyAndWeakPointSameBulletCount = 0;
    diagnostics.duplicateCollisionQueryCount = 0;
    diagnostics.maximumConcurrentBulletSnapshotCount = 0;
    diagnostics.bulletSnapshotMutationCount = 0;
    diagnostics.playerHpChangeRequestCount = 0;
    diagnostics.playerInvincibilityRequestCount = 0;
    diagnostics.bulletKillRequestCount = 0;
    diagnostics.bulletLifetimeChangeRequestCount = 0;
    diagnostics.midbossHpChangeRequestCount = 0;
    diagnostics.attackPlayerCollision = {};
    diagnostics.damageBulletCollision = {};
    diagnostics.weakPointBulletCollision = {};
    lastCollisionQueryWarning.clear();
}
