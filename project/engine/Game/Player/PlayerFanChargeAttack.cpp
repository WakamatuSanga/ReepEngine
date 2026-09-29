#include "PlayerFanChargeAttack.h"

#include <algorithm>
#include <cmath>

void PlayerFanChargeAttack::Reset() {
    phase_ = Phase::Ready;
    chargeTime_ = burstTime_ = nextVolleyTime_ = rechargeRemaining_ = 0.0f;
    needsRelease_ = true;
}

PlayerFanChargeAttack::UpdateResult PlayerFanChargeAttack::AdvanceBurst(float deltaTime) {
    UpdateResult result{ 0, true };
    burstTime_ += deltaTime;
    const float end = (std::min)(burstTime_, settings_.burstDuration);
    while (nextVolleyTime_ <= end + 0.000001f &&
        nextVolleyTime_ < settings_.burstDuration - 0.000001f) {
        ++result.volleys;
        nextVolleyTime_ += (std::max)(settings_.volleyInterval, 0.01f);
    }
    if (burstTime_ >= settings_.burstDuration - 0.00001f) {
        phase_ = Phase::Recharge;
        rechargeRemaining_ = (std::max)(0.0f,
            settings_.rechargeDuration - (std::max)(0.0f, burstTime_ - settings_.burstDuration));
        needsRelease_ = true;
    }
    return result;
}

PlayerFanChargeAttack::UpdateResult PlayerFanChargeAttack::Update(
    float deltaTime, bool held, bool inputAllowed, float chargeDuration) {
    const float dt = std::isfinite(deltaTime) ? (std::max)(deltaTime, 0.0f) : 0.0f;
    if (phase_ == Phase::Burst) {
        return dt > 0.0f ? AdvanceBurst(dt) : UpdateResult{ 0, true };
    }
    if (phase_ == Phase::Recharge) {
        rechargeRemaining_ = (std::max)(0.0f, rechargeRemaining_ - dt);
        // A press held across the cooldown boundary must be released first.
        // If already released, a fresh press after the boundary can charge normally.
        needsRelease_ = held || !inputAllowed;
        if (rechargeRemaining_ <= 0.0001f) {
            rechargeRemaining_ = 0.0f;
            phase_ = Phase::Ready;
        }
        return {};
    }
    if (!inputAllowed) {
        chargeTime_ = 0.0f;
        needsRelease_ = true;
        return {};
    }
    if (needsRelease_) {
        needsRelease_ = held;
        return {};
    }
    if (dt <= 0.0f) {
        return {};
    }
    const float duration = std::isfinite(chargeDuration)
        ? (std::max)(chargeDuration, 0.05f) : 1.5f;
    if (held) {
        chargeTime_ = (std::min)(duration, chargeTime_ + dt);
        if (chargeTime_ >= duration - 0.00001f) {
            chargeTime_ = duration;
        }
        return {};
    }
    const bool charged = chargeTime_ >= duration;
    chargeTime_ = 0.0f;
    if (!charged) {
        return {};
    }
    phase_ = Phase::Burst;
    burstTime_ = nextVolleyTime_ = 0.0f;
    return AdvanceBurst(dt);
}

Vector3 PlayerFanChargeTrajectory::Position(float elapsed) const {
    const float distance = speed * (std::max)(elapsed, 0.0f);
    const float t = std::clamp(distance / (std::max)(bendDistance, 0.001f), 0.0f, 1.0f);
    // Cubic bow: peaks at t=1/3, rejoins the aimed line with zero lateral velocity.
    const float offset = spread * 6.75f * t * (1.0f - t) * (1.0f - t);
    return { origin.x + direction.x * distance + up.x * offset,
        origin.y + direction.y * distance + up.y * offset,
        origin.z + direction.z * distance + up.z * offset };
}

Vector3 PlayerFanChargeTrajectory::Velocity(float elapsed) const {
    const float length = (std::max)(bendDistance, 0.001f);
    const float t = std::clamp(speed * (std::max)(elapsed, 0.0f) / length, 0.0f, 1.0f);
    const float lateral = spread * 6.75f * (1.0f - t) * (1.0f - 3.0f * t) * speed / length;
    return { direction.x * speed + up.x * lateral,
        direction.y * speed + up.y * lateral,
        direction.z * speed + up.z * lateral };
}
