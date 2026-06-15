#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstdint>

class EnemyBulletManager;
class EnemyManager;
class Player;
class PlayerDeathSequenceController;

class EnemyAttackController {
public:
    EnemyAttackController();
    ~EnemyAttackController();

    void Initialize(
        EnemyManager* enemyManager,
        EnemyBulletManager* bulletManager,
        Player* player,
        PlayerDeathSequenceController* deathSequence);
    void Finalize();
    void Update(float deltaTime);
    void DrawImGui();

private:
    void FireFromActiveEnemies();

    EnemyManager* enemyManager_ = nullptr;
    EnemyBulletManager* bulletManager_ = nullptr;
    Player* player_ = nullptr;
    PlayerDeathSequenceController* deathSequence_ = nullptr;

    bool enableShooting_ = true;
    float fireInterval_ = 1.5f;
    float bulletSpeed_ = 6.0f;
    float fireTimer_ = 0.0f;
    uint64_t firedBulletCount_ = 0;
    Vector3 lastTargetPosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 lastFireDirection_{ 0.0f, 0.0f, 1.0f };
    int lastShooterCount_ = 0;
    const char* lastBlockedReason_ = "Not updated";
};
