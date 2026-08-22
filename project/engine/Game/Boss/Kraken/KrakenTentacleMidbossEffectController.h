#pragma once

#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossProjectileDamage.h"
#include "Matrix4x4.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class CombatEffectController;
class EnemyDefeatEffectController;
class ImpactDistortionController;
enum class KrakenTentacleMidbossState : std::uint8_t;

enum class KrakenTentacleEffectPositionSource : std::uint8_t {
    None,
    CollisionClosestPoint,
    ColliderWorldCenter,
    ProjectileWorldCenter,
    SkinnedBoundsWorldCenter,
    TentacleTipAverage,
    SkeletonRootWorldPosition,
    MidbossWorldPosition,
};

struct KrakenTentacleEffectPositionCandidate {
    Vector3 worldPosition{};
    KrakenTentacleEffectPositionSource source =
        KrakenTentacleEffectPositionSource::None;
};

struct KrakenTentacleEffectPositionCandidates {
    std::array<KrakenTentacleEffectPositionCandidate, 5> values{};
    std::size_t count = 0;
};

struct KrakenTentacleEffectSettings {
    bool hitEffectEnabled = true;
    bool weakPointHitEffectEnabled = true;
    bool defeatEffectEnabled = true;
    bool useImpactDistortion = true;
    float bodyHitScale = 1.0f;
    float weakPointHitScale = 1.2f;
    float defeatEffectScale = 1.5f;
};

struct KrakenTentacleLastHitEffectDiagnostics {
    KrakenProjectileEnterEvent event{};
    Vector3 worldPosition{};
    KrakenTentacleEffectPositionSource positionSource =
        KrakenTentacleEffectPositionSource::None;
    float runtimeScale = 1.0f;
    bool spawnSucceeded = false;
    bool spawnFailed = false;
    bool valid = false;
};

struct KrakenTentacleLastDefeatEffectDiagnostics {
    std::uint64_t defeatSequenceId = 0;
    Vector3 worldPosition{};
    KrakenTentacleEffectPositionSource positionSource =
        KrakenTentacleEffectPositionSource::None;
    std::uint8_t stateAtSpawn = 0;
    float runtimeScale = 1.5f;
    bool spawned = false;
    bool spawnSucceeded = false;
    bool spawnFailed = false;
    bool valid = false;
};

struct KrakenTentacleEffectDiagnostics {
    KrakenTentacleLastHitEffectDiagnostics lastHit{};
    KrakenTentacleLastDefeatEffectDiagnostics lastDefeat{};
    std::uint64_t bodyHitSpawnCount = 0;
    std::uint64_t weakPointHitSpawnCount = 0;
    std::uint64_t defeatSpawnCount = 0;
    std::uint64_t duplicateHitSuppressionCount = 0;
    std::uint64_t duplicateDefeatSuppressionCount = 0;
    std::uint64_t nonFinitePositionRejectionCount = 0;
    std::uint64_t positionFallbackCount = 0;
    std::uint64_t effectManagerMissingCount = 0;
    std::uint64_t effectPoolShortageCount = 0;
    std::uint64_t spawnFailureCount = 0;
    std::uint64_t damageWithoutEffectSuppressionCount = 0;
    std::uint64_t hpZeroHitEffectSuppressionCount = 0;
    std::string lastError;
    std::string lastWarning;
};

struct KrakenTentacleResolvedEffectPosition {
    Vector3 worldPosition{};
    KrakenTentacleEffectPositionSource source =
        KrakenTentacleEffectPositionSource::None;
    std::size_t rejectedCandidateCount = 0;
    bool usedFallback = false;
    bool valid = false;
};

KrakenTentacleResolvedEffectPosition ResolveKrakenTentacleEffectPosition(
    const KrakenTentacleEffectPositionCandidates& candidates);

class KrakenTentacleMidbossEffectController {
public:
    void SetContext(
        CombatEffectController* combatEffectController,
        EnemyDefeatEffectController* defeatEffectController,
        ImpactDistortionController* impactDistortionController);
    bool PlayHitEffect(
        const KrakenProjectileEnterEvent& event,
        const KrakenTentacleEffectPositionCandidates& candidates);
    bool PlayDefeatEffect(
        std::uint64_t defeatSequenceId,
        KrakenTentacleMidbossState stateAtSpawn,
        const KrakenTentacleEffectPositionCandidates& candidates);
    bool PlayTestHitEffect(
        bool weakPoint,
        const KrakenTentacleEffectPositionCandidates& candidates);
    bool PlayTestDefeatEffect(
        KrakenTentacleMidbossState stateAtSpawn,
        const KrakenTentacleEffectPositionCandidates& candidates);
    void RecordHpZeroHitEffectSuppression(std::size_t count);
    void Reset(bool resetSettings);
    void Finalize();

    KrakenTentacleEffectSettings& GetSettings() { return settings_; }
    const KrakenTentacleEffectSettings& GetSettings() const {
        return settings_;
    }
    const KrakenTentacleEffectDiagnostics& GetDiagnostics() const {
        return diagnostics_;
    }

private:
    struct HitKey {
        std::uint64_t projectileRuntimeId = 0;
        std::uint64_t eventFrame = 0;
        KrakenProjectileHitRole role = KrakenProjectileHitRole::Body;
    };

    bool PlayHitEffectInternal(
        const KrakenProjectileEnterEvent& event,
        const KrakenTentacleEffectPositionCandidates& candidates,
        bool useDeduplication);
    bool PlayDefeatEffectInternal(
        std::uint64_t defeatSequenceId,
        KrakenTentacleMidbossState stateAtSpawn,
        const KrakenTentacleEffectPositionCandidates& candidates,
        bool useDeduplication);

    CombatEffectController* combatEffectController_ = nullptr;
    EnemyDefeatEffectController* defeatEffectController_ = nullptr;
    ImpactDistortionController* impactDistortionController_ = nullptr;
    KrakenTentacleEffectSettings settings_{};
    KrakenTentacleEffectDiagnostics diagnostics_{};
    std::vector<HitKey> spawnedHitKeys_;
    std::uint64_t spawnedDefeatSequenceId_ = 0;
};
