#pragma once

class CombatSlowMotionController {
public:
    CombatSlowMotionController();
    ~CombatSlowMotionController();

    void Initialize();
    void Finalize();
    void Reset();
    void Update(float unscaledDeltaTime);
    void TriggerBulletCancelSlowMotion(int cancelCount);
    float GetTimeScale() const { return currentTimeScale_; }
    bool IsActive() const { return isActive_; }
    bool ShouldUseScaledDeltaForEffects() const { return useScaledDeltaForEffects_; }
    void DrawImGui();

private:
    void ClampSettings();
    float ResolveDurationForCancelCount(int cancelCount) const;
    float ResolveSlowScaleForCancelCount(int cancelCount) const;

    bool enabled_ = true;
    bool initialized_ = false;
    bool isActive_ = false;
    bool useScaledDeltaForEffects_ = true;

    float currentTimeScale_ = 1.0f;
    float slowTimeScale_ = 0.35f;
    float duration_ = 0.18f;
    float easeInTime_ = 0.025f;
    float easeOutTime_ = 0.12f;
    float cooldown_ = 0.18f;
    float maxExtendTime_ = 0.25f;
    float timer_ = 0.0f;
    float activeDuration_ = 0.0f;
    float cooldownTimer_ = 0.0f;
    float activeSlowTimeScale_ = 0.35f;
    int minCancelCount_ = 1;
    int lastCancelCount_ = 0;
    unsigned int triggerCount_ = 0;
    unsigned int ignoredTriggerCount_ = 0;
};