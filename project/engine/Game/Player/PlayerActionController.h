#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstdint>
#include <memory>
#include <string>

class Camera;
class CombatEffectController;
class EnemyBulletManager;
class Model;
class PlayerBarrelRollRingController;
class PlayerBulletCancelEffectController;
class Object3d;
class Object3dCommon;

class PlayerActionController {
public:
    enum class BarrelRollDirection {
        None,
        Left,
        Right,
    };

    PlayerActionController();
    ~PlayerActionController();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Finalize();
    void SetDependencies(EnemyBulletManager* enemyBulletManager, CombatEffectController* combatEffectController);
    void SetEffectControllers(PlayerBarrelRollRingController* rollRingController, PlayerBulletCancelEffectController* bulletCancelEffectController);
    void SetDebugVisualsEnabled(bool isEnabled);
    void Update(float deltaTime, bool canUseInput, const Vector3& playerPosition);
    void DrawDebugVisuals();
    void DrawImGui();

    bool IsBarrelRolling() const;
    bool IsInvincible() const;
    bool IsBarrelRollEffectEnabled() const { return enableBarrelRollEffect_; }
    float GetDamageReduction() const { return barrelRollDamageReduction_; }
    float GetBarrelRollClearBulletRadius() const;
    Vector3 GetVisualRotationOffset() const;
    BarrelRollDirection GetLastBarrelRollDirection() const { return lastBarrelRollDirection_; }
    uint32_t GetLastClearedBulletCount() const { return lastClearedBulletCount_; }

private:
    void StartBarrelRoll(BarrelRollDirection direction, const Vector3& playerPosition);
    void UpdateTapTimers(float deltaTime);
    void UpdateDebugRadiusObject(const Vector3& playerPosition);
    const char* GetDirectionName(BarrelRollDirection direction) const;
    float GetEffectiveBarrelRollClearBulletRadius() const;

    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    EnemyBulletManager* enemyBulletManager_ = nullptr;
    CombatEffectController* combatEffectController_ = nullptr;
    PlayerBarrelRollRingController* rollRingController_ = nullptr;
    PlayerBulletCancelEffectController* bulletCancelEffectController_ = nullptr;
    std::unique_ptr<Object3d> clearRadiusObject_;
    Model* clearRadiusModel_ = nullptr;

    bool enableBarrelRoll_ = true;
    bool enableBarrelRollEffect_ = false;
    bool showBarrelRollClearRadius_ = false;
    bool debugVisualsEnabled_ = true;
    bool isBarrelRolling_ = false;
    BarrelRollDirection barrelRollDirection_ = BarrelRollDirection::None;
    BarrelRollDirection lastBarrelRollDirection_ = BarrelRollDirection::None;
    float barrelRollDuration_ = 0.5f;
    float barrelRollTimer_ = 0.0f;
    float barrelRollCooldown_ = 0.6f;
    float barrelRollCooldownTimer_ = 0.0f;
    float barrelRollInvincibleTime_ = 0.5f;
    float barrelRollDamageReduction_ = 1.0f;
    float barrelRollClearBulletRadius_ = 1.5f;
    float barrelRollClearBulletRadiusScale_ = 1.4f;
    float barrelRollInputDoubleTapTime_ = 0.25f;
    float leftTapTimer_ = 999.0f;
    float rightTapTimer_ = 999.0f;
    uint32_t lastClearedBulletCount_ = 0;
    uint64_t barrelRollCount_ = 0;
    bool lastLeftTrigger_ = false;
    bool lastRightTrigger_ = false;
    bool lastInputAllowed_ = false;
    std::string lastBlockedReason_ = "Not updated";
};
