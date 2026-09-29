#pragma once

#include "Engine/math/Matrix4x4.h"

// Fan shot tuning lives here; charge duration uses the existing charge UI value.
struct PlayerFanChargeSettings {
    float burstDuration = 2.0f;
    float rechargeDuration = 5.0f;
    float volleyInterval = 0.20f;
    int bulletsPerSide = 1;
    float speed = 28.0f;
    float spread = 0.8f; // Maximum distance along either upward diagonal (world units).
    float bendDistanceRate = 0.65f; // Finish bending before the captured aim point.
    int damage = 1;
};

class PlayerFanChargeAttack {
public:
    struct UpdateResult {
        int volleys = 0;
        bool suppressNormalShot = false;
    };
    void Reset();
    UpdateResult Update(float deltaTime, bool held, bool inputAllowed, float chargeDuration);
    float GetChargeTime() const { return chargeTime_; }
    bool IsBursting() const { return phase_ == Phase::Burst; }
    float GetRechargeRemaining() const { return rechargeRemaining_; }
    const PlayerFanChargeSettings& GetSettings() const { return settings_; }

private:
    enum class Phase { Ready, Burst, Recharge };
    UpdateResult AdvanceBurst(float deltaTime);
    PlayerFanChargeSettings settings_{};
    Phase phase_ = Phase::Ready;
    float chargeTime_ = 0.0f;
    float burstTime_ = 0.0f;
    float nextVolleyTime_ = 0.0f;
    float rechargeRemaining_ = 0.0f;
    bool needsRelease_ = true;
};

// Immutable launch geometry; no target ID, cursor reference or camera frame tracking.
struct PlayerFanChargeTrajectory {
    Vector3 origin{};
    Vector3 direction{ 0.0f, 0.0f, 1.0f };
    Vector3 up{ 0.0f, 1.0f, 0.0f }; // Captured unit spread direction.
    float speed = 28.0f;
    float bendDistance = 1.0f;
    float spread = 0.0f;
    Vector3 Position(float elapsed) const;
    Vector3 Velocity(float elapsed) const;
};
