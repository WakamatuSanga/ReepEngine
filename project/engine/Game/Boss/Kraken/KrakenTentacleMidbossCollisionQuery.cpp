#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"

#include "Engine/Core/FrameTimer.h"
#include "Engine/Game/Boss/Kraken/KrakenTentacleCollisionQuery.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Game/Player/PlayerBulletManager.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
    constexpr std::uint64_t kPlayerRuntimeId = 1;
    constexpr std::uint32_t kTipColliderIndex = 4;

    struct PlayerCollisionSnapshot {
        Vector3 worldPosition{};
        float radius = 0.0f;
        bool enabled = false;
        bool valid = false;
    };

    bool IsFinite(const Vector3& value) {
        return std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
    }

    bool IsValidSphere(const Vector3& center, float radius) {
        return IsFinite(center) && std::isfinite(radius) && radius > 0.0f;
    }

    bool IsPairKeyLess(
        const KrakenTentacleCollisionPairKey& lhs,
        const KrakenTentacleCollisionPairKey& rhs) {
        if (lhs.chainIndex != rhs.chainIndex) {
            return lhs.chainIndex < rhs.chainIndex;
        }
        if (lhs.colliderIndex != rhs.colliderIndex) {
            return lhs.colliderIndex < rhs.colliderIndex;
        }
        if (lhs.role != rhs.role) {
            return lhs.role < rhs.role;
        }
        if (lhs.targetKind != rhs.targetKind) {
            return lhs.targetKind < rhs.targetKind;
        }
        return lhs.targetRuntimeId < rhs.targetRuntimeId;
    }

    bool IsPairKeyEqual(
        const KrakenTentacleCollisionPairKey& lhs,
        const KrakenTentacleCollisionPairKey& rhs) {
        return !IsPairKeyLess(lhs, rhs) && !IsPairKeyLess(rhs, lhs);
    }

    bool IsPairLess(
        const KrakenTentacleCollisionPairSnapshot& lhs,
        const KrakenTentacleCollisionPairSnapshot& rhs) {
        return IsPairKeyLess(lhs.key, rhs.key);
    }

    bool IsPairEqual(
        const KrakenTentacleCollisionPairSnapshot& lhs,
        const KrakenTentacleCollisionPairSnapshot& rhs) {
        return IsPairKeyEqual(lhs.key, rhs.key);
    }

    bool ContainsPair(
        const std::vector<KrakenTentacleCollisionPairSnapshot>& pairs,
        const KrakenTentacleCollisionPairSnapshot& pair) {
        const auto found = std::lower_bound(
            pairs.begin(), pairs.end(), pair, IsPairLess);
        return found != pairs.end() && IsPairEqual(*found, pair);
    }

    void AddIntersectingPair(
        std::vector<KrakenTentacleCollisionPairSnapshot>& pairs,
        KrakenColliderPreviewRole role,
        KrakenTentacleCollisionTargetKind targetKind,
        std::uint32_t chainIndex,
        std::uint32_t colliderIndex,
        std::uint64_t targetRuntimeId,
        const Vector3& targetWorldPosition,
        const KrakenTentacleCollisionQueryResult& result) {
        if (!result.valid || !result.intersecting || targetRuntimeId == 0) {
            return;
        }
        KrakenTentacleCollisionPairSnapshot pair{};
        pair.key.role = role;
        pair.key.targetKind = targetKind;
        pair.key.chainIndex = chainIndex;
        pair.key.colliderIndex = colliderIndex;
        pair.key.targetRuntimeId = targetRuntimeId;
        pair.colliderClosestPosition = result.closestPoint;
        pair.targetWorldPosition = targetWorldPosition;
        pair.centerDistance = result.centerDistance;
        pair.radiusSum = result.radiusSum;
        pairs.push_back(pair);
    }

    std::size_t CountBodyAndWeakPointSameBullets(
        const std::vector<KrakenTentacleCollisionPairSnapshot>& pairs) {
        std::vector<std::uint64_t> bodyIds;
        std::vector<std::uint64_t> weakPointIds;
        for (const KrakenTentacleCollisionPairSnapshot& pair : pairs) {
            if (pair.key.targetKind !=
                KrakenTentacleCollisionTargetKind::PlayerBullet) {
                continue;
            }
            if (pair.key.role == KrakenColliderPreviewRole::Damage) {
                bodyIds.push_back(pair.key.targetRuntimeId);
            } else if (
                pair.key.role == KrakenColliderPreviewRole::WeakPoint) {
                weakPointIds.push_back(pair.key.targetRuntimeId);
            }
        }
        std::sort(bodyIds.begin(), bodyIds.end());
        bodyIds.erase(
            std::unique(bodyIds.begin(), bodyIds.end()), bodyIds.end());
        std::sort(weakPointIds.begin(), weakPointIds.end());
        weakPointIds.erase(
            std::unique(weakPointIds.begin(), weakPointIds.end()),
            weakPointIds.end());

        std::size_t count = 0;
        auto body = bodyIds.begin();
        auto weakPoint = weakPointIds.begin();
        while (body != bodyIds.end() && weakPoint != weakPointIds.end()) {
            if (*body < *weakPoint) {
                ++body;
            } else if (*weakPoint < *body) {
                ++weakPoint;
            } else {
                ++count;
                ++body;
                ++weakPoint;
            }
        }
        return count;
    }

    std::uint32_t GetStableColliderId(
        const KrakenTentacleCollisionPairKey& key) {
        return key.chainIndex * 5 + key.colliderIndex + 1;
    }

    KrakenTentacleCollisionRoleDiagnostics* GetRoleDiagnostics(
        KrakenTentacleMidbossDiagnostics& diagnostics,
        KrakenColliderPreviewRole role) {
        switch (role) {
        case KrakenColliderPreviewRole::Attack:
            return &diagnostics.attackPlayerCollision;
        case KrakenColliderPreviewRole::Damage:
            return &diagnostics.damageBulletCollision;
        case KrakenColliderPreviewRole::WeakPoint:
            return &diagnostics.weakPointBulletCollision;
        default:
            return nullptr;
        }
    }

    void ResetRoleFrameDiagnostics(
        KrakenTentacleCollisionRoleDiagnostics& diagnostics) {
        diagnostics.currentIntersectionCount = 0;
        diagnostics.frameIntersectionCount = 0;
        diagnostics.frameEnterCount = 0;
        diagnostics.frameStayCount = 0;
        diagnostics.frameExitCount = 0;
    }

    void RecordCurrentRolePair(
        KrakenTentacleMidbossDiagnostics& diagnostics,
        const KrakenTentacleCollisionPairSnapshot& pair,
        KrakenTentacleCollisionTransition transition,
        std::uint64_t frameIndex,
        float runtimeTime) {
        KrakenTentacleCollisionRoleDiagnostics* role =
            GetRoleDiagnostics(diagnostics, pair.key.role);
        if (!role) {
            return;
        }
        ++role->currentIntersectionCount;
        ++role->frameIntersectionCount;
        ++role->totalIntersectionCount;
        role->lastColliderId = GetStableColliderId(pair.key);
        role->lastChainIndex = pair.key.chainIndex;
        role->lastTargetRuntimeId = pair.key.targetRuntimeId;
        role->lastIntersectionFrameIndex = frameIndex;
        role->lastIntersectionRuntimeTime = runtimeTime;
        role->hasLastIntersection = true;
        if (transition == KrakenTentacleCollisionTransition::Enter) {
            ++role->frameEnterCount;
            ++role->totalEnterCount;
        } else {
            ++role->frameStayCount;
            ++role->totalStayCount;
        }
    }

    void RecordRoleExit(
        KrakenTentacleMidbossDiagnostics& diagnostics,
        KrakenColliderPreviewRole roleValue) {
        KrakenTentacleCollisionRoleDiagnostics* role =
            GetRoleDiagnostics(diagnostics, roleValue);
        if (role) {
            ++role->frameExitCount;
            ++role->totalExitCount;
        }
    }
}

void KrakenTentacleMidbossController::Impl::SetCollisionQueryContext(
    const Player* playerValue,
    const PlayerBulletManager* playerBulletManagerValue) {
    if (collisionPlayer != playerValue ||
        collisionPlayerBulletManager != playerBulletManagerValue) {
        ResetCollisionQueryState(false);
    }
    collisionPlayer = playerValue;
    collisionPlayerBulletManager = playerBulletManagerValue;
    diagnostics.collisionQueryContextConnected =
        collisionPlayer && collisionPlayerBulletManager;
    if (!collisionPlayer || !collisionPlayerBulletManager) {
        lastCollisionQueryWarning =
            "プレイヤーまたはプレイヤー弾の診断参照が未接続です。";
    } else {
        lastCollisionQueryWarning.clear();
    }
}

void KrakenTentacleMidbossController::Impl::RefreshCollisionRegistrationState() {
    diagnostics.gameplayRegisteredCount = 0;
    diagnostics.collisionRegistrationRequestedCount = 0;
    diagnostics.collisionRegistrationFailureCount = 0;
    diagnostics.registeredAttackColliderCount = 0;
    diagnostics.registeredDamageColliderCount = 0;
    diagnostics.registeredWeakPointCount = 0;
    diagnostics.collisionQueryContextConnected =
        collisionPlayer && collisionPlayerBulletManager;
    if (collisionQueryEnabled &&
        !diagnostics.collisionQueryContextConnected) {
        lastCollisionQueryWarning =
            "プレイヤーまたはプレイヤー弾の診断参照が未接続です。";
    } else {
        lastCollisionQueryWarning.clear();
    }

    const bool runtimeReady = collisionQueryEnabled && initialized &&
        IsVisible() && !safetyStopped;
    for (KrakenTentacleMidbossCapsuleSnapshot& snapshot :
        capsuleSnapshots) {
        snapshot.lastRegisteredFrame = 0;
        snapshot.registrationGeneration =
            collisionRegistrationGeneration;
        snapshot.requestedRegistration = false;
        snapshot.gameplayRegistered = false;
        snapshot.registrationFailed = false;
        const bool shapeValid = snapshot.valid && !snapshot.zeroLength &&
            IsValidSphere(snapshot.worldStart, snapshot.worldRadius) &&
            IsFinite(snapshot.worldEnd);
        snapshot.requestedRegistration = runtimeReady &&
            snapshot.enabled && snapshot.phaseActive && shapeValid;
        if (!snapshot.requestedRegistration) {
            continue;
        }
        ++diagnostics.collisionRegistrationRequestedCount;
        const bool targetConnected =
            snapshot.role == KrakenColliderPreviewRole::Attack
            ? collisionPlayer != nullptr
            : snapshot.role == KrakenColliderPreviewRole::Damage
                ? collisionPlayerBulletManager != nullptr
                : false;
        snapshot.gameplayRegistered = targetConnected;
        snapshot.registrationFailed = !snapshot.gameplayRegistered;
        if (!snapshot.gameplayRegistered) {
            ++diagnostics.collisionRegistrationFailureCount;
            continue;
        }
        snapshot.lastRegisteredFrame =
            diagnostics.lastCollisionQueryFrameIndex;
        ++diagnostics.gameplayRegisteredCount;
        if (snapshot.role == KrakenColliderPreviewRole::Attack) {
            ++diagnostics.registeredAttackColliderCount;
        } else {
            ++diagnostics.registeredDamageColliderCount;
        }
    }

    for (KrakenTentacleMidbossTipSnapshot& snapshot : tipSnapshots) {
        snapshot.lastRegisteredFrame = 0;
        snapshot.registrationGeneration =
            collisionRegistrationGeneration;
        snapshot.requestedRegistration = false;
        snapshot.gameplayRegistered = false;
        snapshot.registrationFailed = false;
        const bool shapeValid = snapshot.valid &&
            IsValidSphere(snapshot.worldPosition, snapshot.worldRadius);
        snapshot.requestedRegistration = runtimeReady &&
            snapshot.enabled && snapshot.phaseActive && shapeValid;
        if (!snapshot.requestedRegistration) {
            continue;
        }
        ++diagnostics.collisionRegistrationRequestedCount;
        snapshot.gameplayRegistered =
            collisionPlayerBulletManager != nullptr;
        snapshot.registrationFailed = !snapshot.gameplayRegistered;
        if (!snapshot.gameplayRegistered) {
            ++diagnostics.collisionRegistrationFailureCount;
            continue;
        }
        snapshot.lastRegisteredFrame =
            diagnostics.lastCollisionQueryFrameIndex;
        ++diagnostics.gameplayRegisteredCount;
        ++diagnostics.registeredWeakPointCount;
    }
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
        snapshot.registrationGeneration =
            collisionRegistrationGeneration;
        snapshot.requestedRegistration = false;
        snapshot.gameplayRegistered = false;
        snapshot.registrationFailed = false;
    }
    for (KrakenTentacleMidbossTipSnapshot& snapshot : tipSnapshots) {
        snapshot.lastRegisteredFrame = 0;
        snapshot.registrationGeneration =
            collisionRegistrationGeneration;
        snapshot.requestedRegistration = false;
        snapshot.gameplayRegistered = false;
        snapshot.registrationFailed = false;
    }
    diagnostics.gameplayRegisteredCount = 0;
    diagnostics.playerCollisionSnapshotCount = 0;
    diagnostics.playerBulletCollisionSnapshotCount = 0;
    diagnostics.collisionRegistrationRequestedCount = 0;
    diagnostics.collisionRegistrationFailureCount = 0;
    diagnostics.registeredAttackColliderCount = 0;
    diagnostics.registeredDamageColliderCount = 0;
    diagnostics.registeredWeakPointCount = 0;
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
    diagnostics.playerCollisionSnapshotValid = false;
    diagnostics.lastCollisionQueryFrameIndex = ~std::uint64_t{ 0 };
    ResetRoleFrameDiagnostics(diagnostics.attackPlayerCollision);
    ResetRoleFrameDiagnostics(diagnostics.damageBulletCollision);
    ResetRoleFrameDiagnostics(diagnostics.weakPointBulletCollision);
    if (resetCumulativeDiagnostics) {
        diagnostics.collisionQueryFrameCount = 0;
        diagnostics.totalCollisionQueryTestCount = 0;
        diagnostics.totalCollisionEnterCount = 0;
        diagnostics.totalCollisionStayCount = 0;
        diagnostics.totalCollisionExitCount = 0;
        diagnostics.totalBodyAndWeakPointSameBulletCount = 0;
        diagnostics.duplicateCollisionQueryCount = 0;
        diagnostics.attackPlayerCollision = {};
        diagnostics.damageBulletCollision = {};
        diagnostics.weakPointBulletCollision = {};
        lastCollisionQueryWarning.clear();
    }
}

void KrakenTentacleMidbossController::Impl::UpdateCollisionQuery() {
    const std::uint64_t frameIndex =
        FrameTimer::GetInstance().GetFrameIndex();
    if (diagnostics.lastCollisionQueryFrameIndex == frameIndex) {
        ++diagnostics.duplicateCollisionQueryCount;
        return;
    }
    diagnostics.lastCollisionQueryFrameIndex = frameIndex;
    ++diagnostics.collisionQueryFrameCount;
    RefreshCollisionRegistrationState();

    diagnostics.playerCollisionSnapshotCount = 0;
    diagnostics.playerBulletCollisionSnapshotCount = 0;
    diagnostics.collisionQueryTestCount = 0;
    diagnostics.invalidCollisionQueryCount = 0;
    diagnostics.currentAttackPlayerPairCount = 0;
    diagnostics.currentDamageBulletPairCount = 0;
    diagnostics.currentWeakPointBulletPairCount = 0;
    diagnostics.collisionEnterCount = 0;
    diagnostics.collisionStayCount = 0;
    diagnostics.collisionExitCount = 0;
    diagnostics.bodyAndWeakPointSameBulletCount = 0;
    diagnostics.playerCollisionSnapshotValid = false;
    ResetRoleFrameDiagnostics(diagnostics.attackPlayerCollision);
    ResetRoleFrameDiagnostics(diagnostics.damageBulletCollision);
    ResetRoleFrameDiagnostics(diagnostics.weakPointBulletCollision);
    currentCollisionPairs.clear();
    collisionFrameEvents.clear();

    PlayerCollisionSnapshot playerSnapshot{};
    if (collisionQueryEnabled && collisionPlayer) {
        playerSnapshot.enabled = collisionPlayer->IsEnabled();
        playerSnapshot.worldPosition = collisionPlayer->GetWorldPosition();
        playerSnapshot.radius = collisionPlayer->GetHitRadius();
        playerSnapshot.valid = playerSnapshot.enabled && IsValidSphere(
            playerSnapshot.worldPosition, playerSnapshot.radius);
        diagnostics.playerCollisionSnapshotValid = playerSnapshot.valid;
        diagnostics.playerCollisionSnapshotCount =
            diagnostics.playerCollisionSnapshotValid ? 1 : 0;
    }

    std::vector<PlayerBulletManager::PlayerBulletCollisionSnapshot>
        bulletSnapshots;
    if (collisionQueryEnabled && collisionPlayerBulletManager) {
        bulletSnapshots =
            collisionPlayerBulletManager->GetActiveCollisionSnapshots();
        diagnostics.playerBulletCollisionSnapshotCount =
            bulletSnapshots.size();
    }

    for (const KrakenTentacleMidbossCapsuleSnapshot& collider :
        capsuleSnapshots) {
        if (!collider.gameplayRegistered) {
            continue;
        }
        if (collider.role == KrakenColliderPreviewRole::Attack) {
            if (!diagnostics.playerCollisionSnapshotValid) {
                continue;
            }
            ++diagnostics.collisionQueryTestCount;
            const KrakenTentacleCollisionQueryResult result =
                QueryKrakenCapsuleSphereIntersection(
                    collider.worldStart,
                    collider.worldEnd,
                    collider.worldRadius,
                    playerSnapshot.worldPosition,
                    playerSnapshot.radius);
            diagnostics.invalidCollisionQueryCount += result.valid ? 0 : 1;
            AddIntersectingPair(
                currentCollisionPairs,
                collider.role,
                KrakenTentacleCollisionTargetKind::Player,
                collider.chainIndex,
                collider.colliderIndex,
                kPlayerRuntimeId,
                playerSnapshot.worldPosition,
                result);
            continue;
        }
        if (collider.role != KrakenColliderPreviewRole::Damage) {
            continue;
        }
        for (const auto& bullet : bulletSnapshots) {
            ++diagnostics.collisionQueryTestCount;
            const KrakenTentacleCollisionQueryResult result =
                QueryKrakenCapsuleSphereIntersection(
                    collider.worldStart,
                    collider.worldEnd,
                    collider.worldRadius,
                    bullet.worldPosition,
                    bullet.radius);
            diagnostics.invalidCollisionQueryCount += result.valid ? 0 : 1;
            AddIntersectingPair(
                currentCollisionPairs,
                collider.role,
                KrakenTentacleCollisionTargetKind::PlayerBullet,
                collider.chainIndex,
                collider.colliderIndex,
                bullet.runtimeId,
                bullet.worldPosition,
                result);
        }
    }

    for (const KrakenTentacleMidbossTipSnapshot& collider : tipSnapshots) {
        if (!collider.gameplayRegistered) {
            continue;
        }
        for (const auto& bullet : bulletSnapshots) {
            ++diagnostics.collisionQueryTestCount;
            const KrakenTentacleCollisionQueryResult result =
                QueryKrakenSphereSphereIntersection(
                    collider.worldPosition,
                    collider.worldRadius,
                    bullet.worldPosition,
                    bullet.radius);
            diagnostics.invalidCollisionQueryCount += result.valid ? 0 : 1;
            AddIntersectingPair(
                currentCollisionPairs,
                collider.role,
                KrakenTentacleCollisionTargetKind::PlayerBullet,
                collider.chainIndex,
                kTipColliderIndex,
                bullet.runtimeId,
                bullet.worldPosition,
                result);
        }
    }

    std::sort(
        currentCollisionPairs.begin(),
        currentCollisionPairs.end(),
        IsPairLess);
    currentCollisionPairs.erase(
        std::unique(
            currentCollisionPairs.begin(),
            currentCollisionPairs.end(),
            IsPairEqual),
        currentCollisionPairs.end());

    for (const KrakenTentacleCollisionPairSnapshot& pair :
        currentCollisionPairs) {
        KrakenTentacleCollisionEventSnapshot event{};
        event.pair = pair;
        event.transition = ContainsPair(previousCollisionPairs, pair)
            ? KrakenTentacleCollisionTransition::Stay
            : KrakenTentacleCollisionTransition::Enter;
        collisionFrameEvents.push_back(event);
        RecordCurrentRolePair(
            diagnostics, pair, event.transition, frameIndex, totalActiveTime);
        if (event.transition == KrakenTentacleCollisionTransition::Enter) {
            ++diagnostics.collisionEnterCount;
        } else {
            ++diagnostics.collisionStayCount;
        }
        if (pair.key.role == KrakenColliderPreviewRole::Attack) {
            ++diagnostics.currentAttackPlayerPairCount;
        } else if (pair.key.role == KrakenColliderPreviewRole::Damage) {
            ++diagnostics.currentDamageBulletPairCount;
        } else if (pair.key.role == KrakenColliderPreviewRole::WeakPoint) {
            ++diagnostics.currentWeakPointBulletPairCount;
        }
    }
    for (const KrakenTentacleCollisionPairSnapshot& pair :
        previousCollisionPairs) {
        if (ContainsPair(currentCollisionPairs, pair)) {
            continue;
        }
        collisionFrameEvents.push_back({
            pair,
            KrakenTentacleCollisionTransition::Exit,
        });
        ++diagnostics.collisionExitCount;
        RecordRoleExit(diagnostics, pair.key.role);
    }

    diagnostics.currentCollisionPairCount = currentCollisionPairs.size();
    diagnostics.bodyAndWeakPointSameBulletCount =
        CountBodyAndWeakPointSameBullets(currentCollisionPairs);
    diagnostics.totalCollisionQueryTestCount +=
        diagnostics.collisionQueryTestCount;
    diagnostics.totalCollisionEnterCount += diagnostics.collisionEnterCount;
    diagnostics.totalCollisionStayCount += diagnostics.collisionStayCount;
    diagnostics.totalCollisionExitCount += diagnostics.collisionExitCount;
    diagnostics.totalBodyAndWeakPointSameBulletCount +=
        diagnostics.bodyAndWeakPointSameBulletCount;
    for (KrakenTentacleCollisionRoleDiagnostics* role : {
            &diagnostics.attackPlayerCollision,
            &diagnostics.damageBulletCollision,
            &diagnostics.weakPointBulletCollision }) {
        if (role->frameIntersectionCount > 0) {
            ++role->cumulativeIntersectionFrameCount;
        }
    }
    previousCollisionPairs = currentCollisionPairs;
}
