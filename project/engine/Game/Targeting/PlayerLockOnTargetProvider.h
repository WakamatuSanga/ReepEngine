#pragma once

#include "Engine/math/Matrix4x4.h"
#include "Engine/Game/KrakenTentacleWeakPointAnchorSnapshot.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class EnemyManager;
class AimCorridorTargetingController;
class KrakenTentacleMidbossController;
class PlayerBulletManager;
struct EnemyTargetView;

enum class PlayerLockOnTargetKind : std::uint8_t {
    NormalEnemy,
    KrakenWeakPoint,
};

struct PlayerLockOnTargetSnapshot {
    std::string id{};
    std::uint64_t sourceColliderId = 0;
    Vector3 worldPosition{};
    float worldRadius = 0.0f;
    KrakenTentacleLockOnGeometrySnapshot krakenGeometry{};
    PlayerLockOnTargetKind kind = PlayerLockOnTargetKind::NormalEnemy;
    std::uint32_t subTargetIndex = 0;
    bool alive = false;
    bool targetable = false;
    bool valid = false;
};

class PlayerLockOnTargetProvider {
public:
    PlayerLockOnTargetProvider() = default;
    ~PlayerLockOnTargetProvider();

    void Initialize(
        const EnemyManager* enemyManager,
        const KrakenTentacleMidbossController* krakenRuntime);
    void Reset();
    void Finalize();

    void CollectTargetableTargets(
        std::vector<PlayerLockOnTargetSnapshot>& outTargets) const;
    bool TryGetTargetSnapshot(
        std::string_view targetId,
        PlayerLockOnTargetSnapshot& outSnapshot) const;
    void DrawImGui(
        const AimCorridorTargetingController* targetingController,
        const PlayerBulletManager* playerBulletManager);

    bool IsInitialized() const { return initialized_; }
    bool IsEnemyManagerConnected() const { return enemyManager_ != nullptr; }
    bool IsKrakenRuntimeConnected() const { return krakenRuntime_ != nullptr; }
    std::size_t GetLastCandidateCount() const { return lastCandidateCount_; }
    std::size_t GetLastNormalEnemyCandidateCount() const {
        return lastNormalEnemyCandidateCount_;
    }
    std::size_t GetLastKrakenCandidateCount() const {
        return lastKrakenCandidateCount_;
    }
    std::size_t GetDuplicateIdCount() const { return duplicateIdCount_; }
    std::size_t GetNonFinitePositionCount() const {
        return nonFinitePositionCount_;
    }
    std::size_t GetKrakenSourceTargetCount() const {
        SyncKrakenTargetIds();
        return krakenTargetIds_.size();
    }

private:
    struct KrakenTargetId {
        std::string id{};
        std::uint32_t chainIndex = 0;
    };

    void ResetDiagnostics() const;
    void SyncKrakenTargetIds() const;
    bool BuildKrakenSnapshot(
        const KrakenTargetId& target,
        PlayerLockOnTargetSnapshot& outSnapshot) const;
    bool ContainsId(
        const std::vector<PlayerLockOnTargetSnapshot>& targets,
        std::string_view id) const;

    const EnemyManager* enemyManager_ = nullptr;
    const KrakenTentacleMidbossController* krakenRuntime_ = nullptr;
    bool initialized_ = false;

    mutable std::vector<EnemyTargetView> normalEnemyViews_;
    mutable std::vector<PlayerLockOnTargetSnapshot> debugTargets_;
    mutable std::vector<KrakenTargetId> krakenTargetIds_;
    mutable std::size_t cachedKrakenChainCount_ = 0;

    mutable std::size_t candidateCollectionCount_ = 0;
    mutable std::size_t idQueryCount_ = 0;
    mutable std::size_t idQuerySuccessCount_ = 0;
    mutable std::size_t idQueryFailureCount_ = 0;
    mutable std::size_t normalEnemyResolveCount_ = 0;
    mutable std::size_t krakenResolveCount_ = 0;
    mutable std::size_t invalidCandidateCount_ = 0;
    mutable std::size_t duplicateIdCount_ = 0;
    mutable std::size_t duplicateIdRejectionCount_ = 0;
    mutable std::size_t nonFinitePositionCount_ = 0;
    mutable std::size_t emptyIdCount_ = 0;
    mutable std::size_t invalidKindCount_ = 0;
    mutable std::size_t targetLostCount_ = 0;
    mutable std::size_t krakenDefeatTargetLostCount_ = 0;
    mutable std::size_t targetSwitchCount_ = 0;
    mutable std::size_t lastCandidateCount_ = 0;
    mutable std::size_t lastNormalEnemyCandidateCount_ = 0;
    mutable std::size_t lastKrakenCandidateCount_ = 0;
    mutable std::size_t lastInvalidCandidateCount_ = 0;
    mutable std::size_t lastNormalSourceCount_ = 0;
    mutable std::size_t lastNormalIdMatchCount_ = 0;
    mutable std::size_t lastNormalOrderDifferenceCount_ = 0;
    mutable float lastNormalPositionMaximumDifference_ = 0.0f;
};
