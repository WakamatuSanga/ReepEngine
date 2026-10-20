#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstdint>
#include <memory>
#include <string>

class Camera;
class CombatEffectController;
class CombatSlowMotionController;
class EnemyBulletManager;
class Model;
class Object3d;
class Object3dCommon;
class PlayerActionController;
class PlayerBarrelRollRingController;
class PlayerBulletCancelEffectController;
class PlayerDamageFeedbackController;
class PlayerAfterimageController;
class CameraShakeController;

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
    void RefreshCameraPresentation(float deltaTime);
    void Draw();
    void DrawImGui(PlayerDamageFeedbackController* damageFeedbackController = nullptr);

    void SetGameViewInputActive(bool isActive);
    void SetActionDebugVisualsEnabled(bool isEnabled);
    void SetBarrelRollDependencies(EnemyBulletManager* enemyBulletManager, CombatEffectController* combatEffectController);
    void SetBarrelRollEffectControllers(PlayerBarrelRollRingController* rollRingController, PlayerBulletCancelEffectController* bulletCancelEffectController);
    void SetBarrelRollSlowMotionController(CombatSlowMotionController* slowMotionController);
    void SetCameraShakeController(CameraShakeController* controller);
    void DrawAfterimages();
    void ClearFlightTransient();
    void SetBoostTrajectory(float envelope, const Vector2& orbit, float depth, const Vector3& attitude, float rollRadians = 0) {
        boostRollRadians_ = rollRadians;
        boostEnvelope_ = envelope; boostOrbit_ = orbit; boostDepth_ = depth; boostAttitude_ = attitude;
    }
    uint32_t GetAfterimageDrawCount() const;
    void SetDamageFeedbackAlpha(float alpha);
    void SetBaseMode(BaseMode baseMode);
    void SetExternalBasePosition(const Vector3& position);
    void SetExternalBaseForward(const Vector3& forward);
    void SetExternalBaseUp(const Vector3& up);
    bool UsesWASDInput() const;
    BaseMode GetBaseMode() const { return baseMode_; }
    const Vector3& GetBasePosition() const { return basePosition_; }
    const Vector3& GetWorldPosition() const { return worldPosition_; }
    Vector2 GetMoveInput() const {
        return lastInputApplied_ ? Vector2{ lastRawMoveInput_.x, lastRawMoveInput_.y } : Vector2{};
    }
    const Vector3& GetBaseForward() const { return baseForward_; }
    const Vector3& GetVisualBaseRotation() const { return visualBaseRotation_; }
    const Vector3& GetVisualModelRotation() const { return visualFinalRotation_; }
    Vector3 TransformVisualModelPointToWorld(const Vector3& modelLocalPoint) const;
    Vector3 GetVisualDisplayForward() const;
    Vector3 GetActionVisualRotationOffset() const;
    void SetRailFlightVisualBankRadians(float radians);
    void SetNormalFlightPresentation(bool enabled, bool suppressed) {
        normalFlightPresentation_ = enabled;
        normalFlightSuppressed_ = suppressed;
    }
    void SetGameplayMovementViewport(float distance, float height, float fovY, float aspect,
        float yawMarginRadians, float pitchMarginRadians);
    void SetGameplayViewportPose(const Matrix4x4& frame, const Vector3& pilotRotation) {
        gameplayViewportFrame_ = frame; gameplayPilotRotation_ = pilotRotation;
    }
    bool UsesGameplayViewport() const {
        return normalFlightPresentation_ && gameplayMovementViewportValid_ && flightEffectsActive_;
    }
    Vector2 GetGameplayViewportNdc() const;
    Vector3 GetGameplayViewportPoint(const Vector2& ndc, float depth) const;
    Vector2 GetPresentationOffset() const;
    Vector3 GetAimControlWorldPosition() const;
    float GetDistanceFromCamera() const { return distanceFromCamera_; }
    float GetGameplayReferenceDistance() const { return gameplayReferenceDistance_; }
    void SetFlightEffectsActive(bool active) { flightEffectsActive_ = active; }
    bool GetVisualModelBounds(Vector3& minimum, Vector3& maximum) const;
    float GetRailFlightVisualBankRadians() const { return railFlightVisualBankRadians_; }
    bool IsEnabled() const { return enablePlayer_; }
    float GetEventTriggerRadius() const { return eventTriggerRadius_; }
    float GetHitRadius() const { return hitRadius_; }
    float GetLocalOffsetX() const { return localOffsetX_; }
    float GetLocalOffsetY() const { return localOffsetY_; }
    float GetMoveLimitX() const { return moveLimitX_; }
    float GetMoveLimitY() const { return moveLimitY_; }
    bool IsBarrelRolling() const;
    bool IsInvincible() const;
    bool IsBarrelRollEffectEnabled() const;
    float GetDamageReduction() const;
    float GetBarrelRollClearBulletRadius() const;
    float GetVisualAlpha() const { return currentPlayerAlpha_; }
    bool ShouldCenterFadeAffectFakeShadow() const { return affectCenterFadeToFakeShadow_; }
    bool ShouldCenterFadeAffectJetExhaust() const { return affectCenterFadeToJetExhaust_; }

private:
    void LoadModel();
    void ResetPosition();
    void UpdateWorldPosition();
    void UpdateObjectTransform();
    void UpdateVisualTilt(float deltaTime);
    void UpdateCenterVisibilityAssist(float deltaTime);
    void ApplyModelAlpha(float alpha);

    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    std::unique_ptr<Object3d> object_;
    Model* model_ = nullptr;
    std::unique_ptr<Object3d> hitRadiusObject_;
    std::unique_ptr<PlayerActionController> actionController_;
    std::unique_ptr<PlayerAfterimageController> afterimages_;
    float boostEnvelope_ = 0.0f;
    Vector2 boostOrbit_{};
    float boostDepth_ = 0.0f;
    Vector3 boostAttitude_{};
    float boostRollRadians_ = 0;
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
    Vector3 visualBoundsMin_{};
    Vector3 visualBoundsMax_{};
    bool visualBoundsValid_ = false;
    float visualModelRadius_ = 0.0f;
    bool normalFlightPresentation_ = false;
    bool normalFlightSuppressed_ = false;
    float railFlightVisualBankRadians_ = 0.0f;
    ModelForwardAxis modelForwardAxis_ = ModelForwardAxis::PositiveZ;
    float localOffsetX_ = 0.0f;
    float localOffsetY_ = 0.0f;
    float moveLimitX_ = 2.5f;
    float moveLimitY_ = 1.4f;
    float moveSpeed_ = 3.0f;
    Vector2 gameplayMovementMin_{};
    Vector2 gameplayMovementMax_{};
    bool gameplayMovementViewportValid_ = false;
    float gameplayReferenceDistance_ = 3.30f;
    float gameplayViewportHeight_ = 0.18f;
    float gameplayViewportTanY_ = 0.29f;
    float gameplayViewportAspect_ = 16.0f / 9.0f;
    Matrix4x4 gameplayViewportFrame_{};
    Vector3 gameplayPilotRotation_{};
    bool flightEffectsActive_ = false;
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
    bool enableCenterFade_ = true;
    bool affectCenterFadeToFakeShadow_ = false;
    bool affectCenterFadeToJetExhaust_ = false;
    bool centerProjectionValid_ = false;
    float centerFadeInnerRadius_ = 0.04f;
    float centerFadeOuterRadius_ = 0.20f;
    float centerFadeMinAlpha_ = 0.55f;
    float centerFadeSpeed_ = 10.0f;
    float currentScreenDistance_ = 1.0f;
    float currentPlayerAlpha_ = 1.0f;
    float damageFeedbackAlpha_ = 1.0f;
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


