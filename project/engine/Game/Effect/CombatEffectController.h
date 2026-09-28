#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstdint>
#include <string>

class Camera;
class GpuParticleEffectPlayer;
class Player;
class PrimitiveEffectSystem;

class CombatEffectController {
public:
    CombatEffectController();
    ~CombatEffectController();

    void Initialize(PrimitiveEffectSystem* primitiveEffectSystem, GpuParticleEffectPlayer* gpuParticleEffectPlayer, Player* player);
    void Finalize();
    void Update(float deltaTime, const Camera* camera);
    void Draw();
    void DrawImGui();

    void PlayPlayerBulletHitEnemy(const Vector3& position);
    void PlayPlayerBulletHitEnemy(const Vector3& position, bool lethalHit);
    void PlayPlayerBulletHitEnemy(
        const Vector3& position,
        bool lethalHit,
        float runtimeScale);
    bool TryPlayPlayerBulletHitEnemy(
        const Vector3& position,
        bool lethalHit,
        float runtimeScale);
    void PlayEnemyBulletHitPlayer(const Vector3& position);
    void PlayPlayerDeathExplosion(const Vector3& position);
    void PlayEnemyDeathExplosion(const Vector3& position);

    bool IsEnabled() const { return enableCombatEffects_; }

private:
    bool PlayHitRing(
        const Vector3& position,
        const char* effectType,
        float ringScale,
        float ringAlpha,
        bool ringEnabled);
    void PlayExplosion(const Vector3& position, const char* effectType);
    bool PlayGpuEffectAt(const std::string& jsonPath, const Vector3& position, const char* label);
    void RecordEffect(const char* effectType, const Vector3& position, const char* result);

    PrimitiveEffectSystem* primitiveEffectSystem_ = nullptr;
    GpuParticleEffectPlayer* gpuParticleEffectPlayer_ = nullptr;
    Player* player_ = nullptr;
    Vector3 lastEffectPosition_{ 0.0f, 0.0f, 0.0f };
    std::string lastEffectType_ = "None";
    std::string lastEffectResult_ = "Not initialized";
    uint64_t playerHitEffectCount_ = 0;
    uint64_t enemyHitEffectCount_ = 0;
    uint64_t playerDeathEffectCount_ = 0;
    uint64_t enemyDeathEffectCount_ = 0;
    bool enableCombatEffects_ = true;
    bool ensurePrimitiveEffectsVisible_ = true;
    bool enablePrimitiveRing_ = true;
    bool enablePrimitiveHit_ = false;
    bool enablePrimitiveCylinder_ = false;
    bool enableGpuParticleEffects_ = true;
    bool enableHitSparks_ = true;
    bool enableSmallExplosion_ = true;
    bool enableDeathExplosion_ = true;
    bool enableEnemyHitRing_ = true;
    float enemyHitRingScale_ = 0.35f;
    float enemyHitRingAlpha_ = 0.75f;
    float enemyDeathEffectScale_ = 1.0f;
    float playerHitRingScale_ = 1.0f;
    float playerDeathExplosionScale_ = 1.0f;
    std::string hitSparksJsonPath_ = "resources/effects/gpu/hit_sparks.json";
    std::string smallExplosionJsonPath_ = "resources/effects/gpu/small_explosion.json";
    std::string deathExplosionJsonPath_ = "resources/effects/gpu/death_explosion.json";
};
