#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstdint>

class EnemyBulletManager;
class Player;
class CombatEffectController;
class PlayerDeathSequenceController;
class PlayerBulletCancelEffectController;

class PlayerEnemyBulletCollision {
public:
    PlayerEnemyBulletCollision();
    ~PlayerEnemyBulletCollision();

    void Initialize(
        Player* player,
        EnemyBulletManager* bulletManager,
        PlayerDeathSequenceController* deathSequence,
        CombatEffectController* combatEffectController);
    void SetBulletCancelEffectController(PlayerBulletCancelEffectController* controller);
    void Finalize();
    void Update();
    void DrawImGui();

private:
    Player* player_ = nullptr;
    EnemyBulletManager* bulletManager_ = nullptr;
    PlayerDeathSequenceController* deathSequence_ = nullptr;
    CombatEffectController* combatEffectController_ = nullptr;
    PlayerBulletCancelEffectController* bulletCancelEffectController_ = nullptr;
    bool enableCollision_ = true;
    bool lastHit_ = false;
    bool lastBlockedByBarrelRoll_ = false;
    uint64_t hitCount_ = 0;
    uint64_t barrelRollBlockCount_ = 0;
    float lastDistance_ = -1.0f;
    float lastRadiusSum_ = 0.0f;
    float lastPlayerHitRadius_ = 0.0f;
    float lastBulletRadius_ = 0.0f;
    float lastPlayerDamageReduction_ = 0.0f;
    Vector3 lastPlayerPosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 lastHitPosition_{ 0.0f, 0.0f, 0.0f };
    const char* lastBlockedReason_ = "Not updated";
};
