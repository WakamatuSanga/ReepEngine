#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstdint>
#include <string>

class Player;
class PrimitiveEffectSystem;

class CombatEffectController {
public:
    CombatEffectController();
    ~CombatEffectController();

    void Initialize(PrimitiveEffectSystem* primitiveEffectSystem, Player* player);
    void Finalize();
    void Update(float deltaTime);
    void DrawImGui();

    void PlayPlayerBulletHitEnemy(const Vector3& position);
    void PlayEnemyBulletHitPlayer(const Vector3& position);
    void PlayPlayerDeathExplosion(const Vector3& position);
    void PlayEnemyDeathExplosion(const Vector3& position);

    bool IsEnabled() const { return enableCombatEffects_; }

private:
    void PlayHitRing(const Vector3& position, const char* effectType);
    void PlayExplosion(const Vector3& position, const char* effectType);
    void RecordEffect(const char* effectType, const Vector3& position, const char* result);

    PrimitiveEffectSystem* primitiveEffectSystem_ = nullptr;
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
};
