#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstdint>
#include <string>

class Camera;
class Enemy;
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
        PlayerDeathSequenceController* deathSequence,
        const Camera* camera);
    void Finalize();
    void Update(float deltaTime);
    void DrawImGui();

private:
    void FireFromActiveEnemies();
    bool IsEnemyAllowedToAttack(const Enemy& enemy, std::string& reason) const;
    bool IsExternalAttackEnemy(const Enemy& enemy) const;

    EnemyManager* enemyManager_ = nullptr;
    EnemyBulletManager* bulletManager_ = nullptr;
    Player* player_ = nullptr;
    PlayerDeathSequenceController* deathSequence_ = nullptr;
    const Camera* camera_ = nullptr;

    bool enableShooting_ = true;
    bool enableOffscreenAttackGate_ = true;
    float fireInterval_ = 1.5f;
    float bulletSpeed_ = 9.0f;
    float screenGateNdcLimitX_ = 1.2f;
    float screenGateNdcLimitY_ = 1.2f;
    float fireTimer_ = 0.0f;
    uint64_t firedBulletCount_ = 0;
    uint64_t blockedAttackCount_ = 0;
    Vector3 lastTargetPosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 lastFireDirection_{ 0.0f, 0.0f, 1.0f };
    int lastShooterCount_ = 0;
    std::string lastBlockedEnemyId_ = "(none)";
    std::string lastBlockedEnemyType_ = "(none)";
    std::string lastBlockedReason_ = "Not updated";
};
