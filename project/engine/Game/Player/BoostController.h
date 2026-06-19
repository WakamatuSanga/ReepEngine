#pragma once

#include <cstdint>
#include <string>

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

    bool IsBoosting() const { return state_ == State::Boosting; }
    float GetCurrentBoostPower() const { return currentBoostPower_; }
    float GetCloudFlowMultiplier() const;
    float GetVisualSpeedMultiplier() const;
    float GetRadialBlurIntensity() const { return radialBlurIntensity_ * currentBoostPower_; }
    float GetFovBoostAmount() const { return fovBoostAmount_ * currentBoostPower_; }

private:
    void StartBoost(bool ignoreCooldown);
    void ResetBoost();
    void PushMultiplierToCloud();
    unsigned char ResolveInputKey() const;
    const char* GetStateName() const;
    const char* GetInputKeyName() const;
    static float SmoothStep(float value);
    static float Saturate(float value);

private:
    VolumetricCloudPass* cloudPass_ = nullptr;

    State state_ = State::Idle;
    InputKey boostInputKey_ = InputKey::LeftShift;
    bool enableBoost_ = true;
    bool gameViewInputActive_ = false;
    bool lastInputTriggered_ = false;
    bool lastCanBoost_ = false;

    float boostDuration_ = 1.0f;
    float boostCooldown_ = 1.5f;
    float boostTimer_ = 0.0f;
    float boostCooldownTimer_ = 0.0f;
    float boostCloudFlowMultiplier_ = 3.0f;
    float boostVisualSpeedMultiplier_ = 2.0f;
    float boostEaseInTime_ = 0.1f;
    float boostEaseOutTime_ = 0.3f;
    float currentBoostPower_ = 0.0f;
    float radialBlurIntensity_ = 0.35f;
    float fovBoostAmount_ = 5.0f;
    uint64_t boostCount_ = 0;
    std::string inputBlockedReason_ = "Not updated";
};
