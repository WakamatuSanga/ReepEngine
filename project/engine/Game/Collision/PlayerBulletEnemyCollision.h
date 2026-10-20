#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstddef>
#include <string>

class EnemyManager;
class CameraShakeController;
class PlayerBulletManager;
class CombatEffectController;
class EnemyDefeatEffectController;
class StageCombatState;
class ImpactDistortionController;

class PlayerBulletEnemyCollision {
public:
    PlayerBulletEnemyCollision();
    ~PlayerBulletEnemyCollision();

    void Initialize(
        PlayerBulletManager* bulletManager,
        EnemyManager* enemyManager,
        CombatEffectController* combatEffectController);
    void Finalize();
    void SetCameraShakeController(CameraShakeController* controller) { cameraShakeController_ = controller; }
    void SetEnemyDefeatEffectController(EnemyDefeatEffectController* controller);
    void SetImpactDistortionController(ImpactDistortionController* controller);
    void Update();
    void SetCombatState(StageCombatState* state) { combatState_ = state; }
    void DrawImGui();

private:
    PlayerBulletManager* bulletManager_ = nullptr;
    CameraShakeController* cameraShakeController_ = nullptr;
    EnemyManager* enemyManager_ = nullptr;
    CombatEffectController* combatEffectController_ = nullptr;
    StageCombatState* combatState_ = nullptr;
    EnemyDefeatEffectController* enemyDefeatEffectController_ = nullptr;
    ImpactDistortionController* impactDistortionController_ = nullptr;
    Vector3 lastHitPosition_{ 0.0f, 0.0f, 0.0f };
    float enemyHitRadiusForDebug_ = 0.6f;
    Vector3 enemyHitScaleForDebug_{ 1.6f, 1.0f, 1.0f };
    bool useEllipsoidHitShapeForDebug_ = true;
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
