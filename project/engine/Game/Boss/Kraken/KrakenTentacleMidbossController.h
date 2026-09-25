#pragma once

#include "Engine/Game/Boss/Kraken/KrakenTentacleAttackDamage.h"
#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossProjectileDamage.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class Camera;
class CombatEffectController;
class EnemyDefeatEffectController;
class ImpactDistortionController;
class ModelCommon;
class Object3dCommon;
class Player;
class PlayerBulletManager;
class PlayerDamageFeedbackController;
class PlayerDeathSequenceController;
struct KrakenTentacleWeakPointAnchorSnapshot;
struct KrakenTentacleLockOnGeometrySnapshot;
struct KrakenTentacleFramingSnapshot;
struct Vector3;

enum class KrakenTentacleMidbossState : std::uint8_t {
    Hidden,
    Idle,
    Windup,
    WindupHold,
    Slam,
    ImpactHold,
    Recovery,
    Defeated,
    Retreating,
    RetreatCompleted,
};

class KrakenTentacleMidbossController {
public:
    KrakenTentacleMidbossController();
    ~KrakenTentacleMidbossController();

    KrakenTentacleMidbossController(
        const KrakenTentacleMidbossController&) = delete;
    KrakenTentacleMidbossController& operator=(
        const KrakenTentacleMidbossController&) = delete;

    bool Initialize(
        ModelCommon* modelCommon,
        Object3dCommon* object3dCommon);
    void SetCamera(Camera* camera);
    void SetCollisionQueryContext(
        const Player* player,
        const PlayerBulletManager* playerBulletManager,
        bool playerAlive);
    void SetAttackDamageContext(
        PlayerDamageFeedbackController* damageFeedbackController,
        PlayerDeathSequenceController* deathSequenceController,
        CombatEffectController* combatEffectController);
    void SetProjectileDamageContext(PlayerBulletManager* playerBulletManager);
    void SetEffectContext(
        CombatEffectController* combatEffectController,
        EnemyDefeatEffectController* defeatEffectController,
        ImpactDistortionController* impactDistortionController);
    void SetProjectileDamageEnabled(bool enabled);
    bool IsProjectileDamageEnabled() const;
    std::vector<KrakenProjectileEnterEvent>
        GetProjectileEnterEventsThisFrame() const;
    float GetMaxHp() const;
    float GetCurrentHp() const;
#ifdef USE_IMGUI
    void SetDebugHpOne(bool enabled);
#endif
    bool IsDefeatPending() const;
    bool IsDefeatStarted() const;
    bool IsDefeatCompleted() const;
    std::uint64_t GetDefeatSequenceId() const;
    void SetAttackDamageEnabled(bool enabled);
    bool IsAttackDamageEnabled() const;
    std::vector<KrakenAttackPlayerEnterEvent>
        GetAttackPlayerEnterEventsThisFrame() const;
    void Update(float scaledDeltaTime);
    void Draw();
    void DrawDebug(
        float viewX,
        float viewY,
        float viewWidth,
        float viewHeight) const;
    void DrawImGui();
    void DrawPlacementImGui();
    void Reset();
    void Finalize();

    bool IsInitialized() const;
    bool IsVisible() const;
    KrakenTentacleMidbossState GetState() const;
    KrakenTentacleMidbossState GetRuntimeState() const;
    bool ResetForWaveEncounter();
    bool PlaceInFrontOfCameraForWaveEncounter();
    bool ShowForWaveEncounter();
    bool BeginEntranceForWaveEncounter(float riseDistance);
    bool UpdateEntranceForWaveEncounter(float progress);
    void HideForWaveEncounter();
    bool SetSelectedAttackChainForWaveEncounter(std::size_t chainIndex);
    bool TryStartAttackForWaveEncounter();
    std::size_t GetDetectedChainCount() const;
    bool TryGetWeakPointLockOnAnchorSnapshot(
        std::size_t chainIndex,
        KrakenTentacleWeakPointAnchorSnapshot& outSnapshot) const;
    bool TryGetChainLockOnGeometrySnapshot(
        std::size_t chainIndex,
        KrakenTentacleLockOnGeometrySnapshot& outSnapshot) const;
    bool TryGetFramingSnapshot(
        KrakenTentacleFramingSnapshot& outSnapshot) const;
    std::size_t GetSelectedAttackChain() const;
    Vector3 GetWorldPosition() const;
    float GetCameraForwardOffset() const;
    float GetCameraRightOffset() const;
    float GetCameraUpOffset() const;
    void SetWaveEncounterControlActive(bool active);
    bool IsWaveEncounterControlActive() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
