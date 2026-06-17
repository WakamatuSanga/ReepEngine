#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstddef>
#include <string>

class EnemyManager;
class PlayerBulletManager;
class CombatEffectController;

class PlayerBulletEnemyCollision {
public:
    PlayerBulletEnemyCollision();
    ~PlayerBulletEnemyCollision();

    void Initialize(
        PlayerBulletManager* bulletManager,
        EnemyManager* enemyManager,
        CombatEffectController* combatEffectController);
    void Finalize();
    void Update();
    void DrawImGui();

private:
    PlayerBulletManager* bulletManager_ = nullptr;
    EnemyManager* enemyManager_ = nullptr;
    CombatEffectController* combatEffectController_ = nullptr;
    Vector3 lastHitPosition_{ 0.0f, 0.0f, 0.0f };
    float enemyHitRadiusForDebug_ = 0.6f;
    float lastDistance_ = -1.0f;
    float lastRadiusSum_ = 0.0f;
    float lastBulletRadius_ = 0.0f;
    float lastEnemyRadius_ = 0.0f;
    int lastDamage_ = 0;
    size_t hitCount_ = 0;
    std::string lastHitEnemy_ = "(none)";
    bool enableEnemyHitCollision_ = true;
    bool playCombatEffect_ = true;
    bool lastHitResult_ = false;
};
