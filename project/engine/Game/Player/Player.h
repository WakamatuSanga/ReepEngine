#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstdint>
#include <memory>
#include <string>

class Camera;
class CombatEffectController;
class EnemyBulletManager;
class Model;
class Object3d;
class Object3dCommon;
class PlayerActionController;

class Player {
public:
    enum class BaseMode {
        CameraFront,
        Rail,
    };

    enum class ModelForwardAxis {
        PositiveZ,
        NegativeZ,
        PositiveX,
        NegativeX,
    };

    Player();
    ~Player();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Finalize();
    void Update(float deltaTime);
    void Draw();
    void DrawImGui();

    void SetGameViewInputActive(bool isActive);
    void SetActionDebugVisualsEnabled(bool isEnabled);
    void SetBarrelRollDependencies(EnemyBulletManager* enemyBulletManager, CombatEffectController* combatEffectController);
    void SetBaseMode(BaseMode baseMode);
    void SetExternalBasePosition(const Vector3& position);
    void SetExternalBaseForward(const Vector3& forward);
    void SetExternalBaseUp(const Vector3& up);
    bool UsesWASDInput() const;
    BaseMode GetBaseMode() const { return baseMode_; }
    const Vector3& GetBasePosition() const { return basePosition_; }
    const Vector3& GetWorldPosition() const { return worldPosition_; }
    const Vector3& GetBaseForward() const { return baseForward_; }
    const Vector3& GetVisualModelRotation() const { return visualFinalRotation_; }
    float GetEventTriggerRadius() const { return eventTriggerRadius_; }
    float GetHitRadius() const { return hitRadius_; }
    float GetLocalOffsetX() const { return localOffsetX_; }
    float GetLocalOffsetY() const { return localOffsetY_; }
    bool IsBarrelRolling() const;
    bool IsInvincible() const;
    bool IsBarrelRollEffectEnabled() const;
    float GetDamageReduction() const;
    float GetBarrelRollClearBulletRadius() const;

private:
    void LoadModel();
    void ResetPosition();
    void UpdateWorldPosition();
    void UpdateObjectTransform();
    void UpdateVisualTilt(float deltaTime);

    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    std::unique_ptr<Object3d> object_;
    Model* model_ = nullptr;
    std::unique_ptr<Object3d> hitRadiusObject_;
    std::unique_ptr<PlayerActionController> actionController_;
    Model* hitRadiusModel_ = nullptr;
    std::string modelPath_ = "resources/Player/player.obj";
    std::string resolvedModelPath_;
    std::string texturePath_;
    std::string loadStatus_;
    Vector3 basePosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 baseForward_{ 0.0f, 0.0f, 1.0f };
    Vector3 worldPosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 externalBasePosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 externalBaseForward_{ 0.0f, 0.0f, 1.0f };
    Vector3 externalBaseUp_{ 0.0f, 1.0f, 0.0f };
    Vector3 modelScale_{ 0.25f, 0.25f, 0.25f };
    Vector3 modelRotationOffset_{ 0.0f, 0.0f, 0.0f };
    Vector3 visualBaseRotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 visualFinalRotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 currentVisualTilt_{ 0.0f, 0.0f, 0.0f };
    ModelForwardAxis modelForwardAxis_ = ModelForwardAxis::PositiveZ;
    float localOffsetX_ = 0.0f;
    float localOffsetY_ = 0.0f;
    float moveLimitX_ = 2.5f;
    float moveLimitY_ = 1.4f;
    float moveSpeed_ = 3.0f;
    float distanceFromCamera_ = 3.0f;
    float eventTriggerRadius_ = 0.25f;
    float hitRadius_ = 0.30f;
    float visualPitchTiltAmount_ = 0.18f;
    float visualRollTiltAmount_ = 0.28f;
    float visualTiltSmoothSpeed_ = 10.0f;
    bool enablePlayer_ = true;
    bool showPlayer_ = true;
    bool showHitRadius_ = false;
    bool useFallbackModel_ = false;
    bool useLightweightVisual_ = false;
    bool enableVisualTilt_ = true;
    bool gameViewInputActive_ = false;
    bool hasExternalBase_ = false;
    BaseMode baseMode_ = BaseMode::CameraFront;
    uint64_t updateCount_ = 0;
    std::string inputBlockedReason_ = "Not updated";
    bool lastWPressed_ = false;
    bool lastAPressed_ = false;
    bool lastSPressed_ = false;
    bool lastDPressed_ = false;
    bool lastLeftMouseDown_ = false;
    bool lastRightMouseDown_ = false;
    bool lastImGuiTextInputActive_ = false;
    bool lastInputApplied_ = false;
    Vector3 lastRawMoveInput_{ 0.0f, 0.0f, 0.0f };
};
