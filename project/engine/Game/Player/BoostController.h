#pragma once

#include <cstdint>
#include <string>

class Camera;
class Player;
class PostEffectController;
class VolumetricCloudPass;

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
    void Update(float deltaTime);
    void DrawImGui();

    void SetGameViewInputActive(bool isActive) { gameViewInputActive_ = isActive; }
    void SetCloudPass(VolumetricCloudPass* cloudPass) { cloudPass_ = cloudPass; }
    void SetPostEffectContext(Player* player, Camera* camera, PostEffectController* postEffectController);

    bool IsBoosting() const { return state_ == State::Boosting; }
    float GetCurrentBoostPower() const { return currentBoostPower_; }
    float GetCloudFlowMultiplier() const;
    float GetCurrentCloudBoostExtraSpeed() const { return currentCloudBoostExtraSpeed_; }
    float GetTargetCloudBoostExtraSpeed() const { return targetCloudBoostExtraSpeed_; }
    float GetVisualSpeedMultiplier() const;
    float GetRadialBlurIntensity() const { return radialBlurIntensity_ * currentBoostPower_; }
    float GetFovBoostAmount() const { return fovBoostAmount_ * currentBoostPower_; }

private:
    void StartBoost(bool ignoreCooldown);
    void ResetBoost();
    void PushMultiplierToCloud();
    void UpdateCloudFlowMultiplier(float deltaTime);
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

    float boostDuration_ = 1.4f;
    float boostCooldown_ = 1.5f;
    float boostTimer_ = 0.0f;
    float boostCooldownTimer_ = 0.0f;
    float boostCloudFlowMultiplier_ = 2.0f;
    float boostVisualSpeedMultiplier_ = 2.0f;
    float boostEaseInTime_ = 0.1f;
    float boostEaseOutTime_ = 0.3f;
    float currentBoostPower_ = 0.0f;
    float maxCloudBoostMultiplier_ = 2.0f;
    float maxCloudBoostExtraSpeed_ = 28.0f;
    float cloudBoostMultiplierSmoothSpeed_ = 8.0f;
    float currentCloudFlowMultiplier_ = 1.0f;
    float targetCloudFlowMultiplier_ = 1.0f;
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
