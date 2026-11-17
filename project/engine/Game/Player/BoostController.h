#pragma once

#include <cstdint>
#include <string>

class Camera;
class Player;
class PostEffectController;
class VolumetricCloudPass;
struct Vector2;

class BoostController {
public:
    enum class State {
        Idle,
        Boosting,
        Cooldown,
    };

    enum class InputKey {
        LeftShift,
        RightShift,
        B,
    };

public:
    BoostController();
    ~BoostController();

    void Initialize(VolumetricCloudPass* cloudPass);
    void Finalize();
    void BeginFrame(float unscaledDeltaTime, bool ordinaryGameplay, float realDeltaTime = -1.0f);
    float ResolveGameplayDeltaTime(float deltaTime, float combatScale);
    float GetNormalMovementDeltaTime(float deltaTime, float combatScale) const;
    bool HasMovementStep() const { return movementBlend_ > 0.0f; }
    float GetEvadeSweepFraction() const { return evadeSweepFraction_; }
    Vector2 MovePlayer(Vector2 position, Vector2 input, Vector2 limits);
    void RefreshPresentation();
    void Cancel();
    float GetTimeScale() const { return currentTimeScale_; }
    void DrawImGui();
    void DrawEvadeSettingsImGui();

    void SetGameViewInputActive(bool isActive) { gameViewInputActive_ = isActive; }
    void SetCloudPass(VolumetricCloudPass* cloudPass) { cloudPass_ = cloudPass; }
    void SetPostEffectContext(Player* player, Camera* camera, PostEffectController* postEffectController);

    State GetState() const { return state_; }
    bool IsBoosting() const { return state_ == State::Boosting; }
    bool IsCoolingDown() const { return state_ == State::Cooldown; }
    bool IsEvadeActive() const { return IsBoosting(); }
    float GetCooldownRemaining() const { return boostCooldownTimer_; }
    float GetCooldownDuration() const { return boostCooldown_; }
    float GetCurrentBoostPower() const { return currentBoostPower_; }
    float GetCloudFlowMultiplier() const;
    float GetCurrentCloudBoostExtraSpeed() const { return currentCloudBoostExtraSpeed_; }
    float GetBoostGaugeRate() const;
    float GetTargetCloudBoostExtraSpeed() const { return targetCloudBoostExtraSpeed_; }
    float GetVisualSpeedMultiplier() const;
    float GetRadialBlurIntensity() const { return radialBlurIntensity_ * currentBoostPower_; }
    float GetFovBoostAmount() const { return fovBoostAmount_ * currentBoostPower_; }
    uint64_t GetBoostCount() const { return boostCount_; }

private:
    void StartBoost(bool ignoreCooldown);
    void ResetBoost();
    void ClearActiveEffects();
    void PushMultiplierToCloud();
    void UpdateBoostPostEffect();
    bool ProjectPlayerToScreen(float& outX, float& outY) const;
    unsigned char ResolveInputKey() const;
    const char* GetStateName() const;
    const char* GetInputKeyName() const;
    static float SmoothStep(float value);
    static float Saturate(float value);

private:
    VolumetricCloudPass* cloudPass_ = nullptr;
    Player* player_ = nullptr;
    Camera* camera_ = nullptr;
    PostEffectController* postEffectController_ = nullptr;

    State state_ = State::Idle;
    InputKey boostInputKey_ = InputKey::LeftShift;
    bool enableBoost_ = true;
    bool gameViewInputActive_ = false;
    bool lastInputTriggered_ = false;
    bool lastCanBoost_ = false;

    float boostDuration_ = 0.20f;
    float boostCooldown_ = 3.0f;
    float boostTimer_ = 0.0f;
    float startSide_ = 1.0f;
    float startX_ = 0.0f;
    float startY_ = 0.0f;
    float boostFrameFraction_ = 0.0f;
    float evadeSweepFraction_ = 0.0f;
    float movementBlend_ = 0.0f;
    float steeringBlend_ = 0.0f;
    float steeringX_ = 0.0f;
    float steeringY_ = 0.0f;
    bool steeringInitialized_ = false;
    float currentTimeScale_ = 1.0f;
    float boostCooldownTimer_ = 0.0f;
    float boostVisualSpeedMultiplier_ = 2.0f;
    float currentBoostPower_ = 0.0f;
    float maxCloudBoostExtraSpeed_ = 28.0f;
    float currentCloudFlowMultiplier_ = 1.0f;
    float currentCloudBoostExtraSpeed_ = 0.0f;
    float targetCloudBoostExtraSpeed_ = 0.0f;
    float radialBlurIntensity_ = 0.08f;
    float lastBoostEffectCenterX_ = 0.5f;
    float lastBoostEffectCenterY_ = 0.5f;
    bool lastBoostEffectCenterValid_ = false;
    float fovBoostAmount_ = 5.0f;
    uint64_t boostCount_ = 0;
    std::string inputBlockedReason_ = "Not updated";
};
