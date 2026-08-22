#pragma once

enum class KrakenTentacleHealthDamageResult {
    Applied,
    DefeatStarted,
    InvalidState,
    InvalidDamage,
};

struct KrakenTentacleHealthDamageApplication {
    float hpBefore = 0.0f;
    float hpAfter = 0.0f;
    float appliedDamage = 0.0f;
    KrakenTentacleHealthDamageResult result =
        KrakenTentacleHealthDamageResult::InvalidState;
};

class KrakenTentacleMidbossHealth {
public:
    static constexpr float kProvisionalMaxHp = 20.0f;
    static constexpr float kRecommendedWeakPointMultiplier = 2.0f;

    void Initialize();
    void Reset();
    void Finalize();

    bool SetMaxHp(float maxHp, bool refill);
    bool SetWeakPointMultiplier(float multiplier);
    bool HealFull();
    bool ForceDefeatForDebug();
    bool TryApplyDamage(
        float damage,
        KrakenTentacleHealthDamageApplication& application);

    float GetMaxHp() const { return maxHp_; }
    float GetCurrentHp() const { return currentHp_; }
    float GetWeakPointMultiplier() const { return weakPointMultiplier_; }
    float GetHpRatio() const;
    bool IsDefeatPending() const { return defeatPending_; }
    bool IsValid() const { return valid_; }

private:
    float maxHp_ = 0.0f;
    float currentHp_ = 0.0f;
    float weakPointMultiplier_ = kRecommendedWeakPointMultiplier;
    bool defeatPending_ = false;
    bool valid_ = false;
};
