#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossHealth.h"

#include <algorithm>
#include <cmath>

void KrakenTentacleMidbossHealth::Initialize() {
    maxHp_ = kProvisionalMaxHp;
    currentHp_ = maxHp_;
    weakPointMultiplier_ = kRecommendedWeakPointMultiplier;
    defeatPending_ = false;
    valid_ = true;
}

void KrakenTentacleMidbossHealth::Reset() {
    Initialize();
}

void KrakenTentacleMidbossHealth::Finalize() {
    maxHp_ = 0.0f;
    currentHp_ = 0.0f;
    weakPointMultiplier_ = kRecommendedWeakPointMultiplier;
    defeatPending_ = false;
    valid_ = false;
}

bool KrakenTentacleMidbossHealth::SetMaxHp(float maxHp, bool refill) {
    if (!valid_ || !std::isfinite(maxHp) || maxHp <= 0.0f) {
        return false;
    }
    maxHp_ = maxHp;
    currentHp_ = refill ? maxHp_ : std::clamp(currentHp_, 0.0f, maxHp_);
    defeatPending_ = currentHp_ <= 0.0f;
    return true;
}

bool KrakenTentacleMidbossHealth::SetWeakPointMultiplier(float multiplier) {
    if (!valid_ || !std::isfinite(multiplier) || multiplier < 1.0f ||
        multiplier > 10.0f) {
        return false;
    }
    weakPointMultiplier_ = multiplier;
    return true;
}

bool KrakenTentacleMidbossHealth::HealFull() {
    if (!valid_ || !std::isfinite(maxHp_) || maxHp_ <= 0.0f) {
        return false;
    }
    currentHp_ = maxHp_;
    defeatPending_ = false;
    return true;
}

bool KrakenTentacleMidbossHealth::ForceDefeatForDebug() {
    if (!valid_ || !std::isfinite(maxHp_) || maxHp_ <= 0.0f) {
        return false;
    }
    currentHp_ = 0.0f;
    defeatPending_ = true;
    return true;
}

bool KrakenTentacleMidbossHealth::TryApplyDamage(
    float damage,
    KrakenTentacleHealthDamageApplication& application) {
    application = {};
    application.hpBefore = currentHp_;
    application.hpAfter = currentHp_;
    if (!valid_ || defeatPending_ || !std::isfinite(maxHp_) ||
        !std::isfinite(currentHp_) || maxHp_ <= 0.0f || currentHp_ <= 0.0f) {
        application.result = KrakenTentacleHealthDamageResult::InvalidState;
        return false;
    }
    if (!std::isfinite(damage) || damage <= 0.0f) {
        application.result = KrakenTentacleHealthDamageResult::InvalidDamage;
        return false;
    }

    currentHp_ = std::clamp(currentHp_ - damage, 0.0f, maxHp_);
    application.hpAfter = currentHp_;
    application.appliedDamage = application.hpBefore - application.hpAfter;
    defeatPending_ = currentHp_ <= 0.0f;
    application.result = defeatPending_
        ? KrakenTentacleHealthDamageResult::DefeatStarted
        : KrakenTentacleHealthDamageResult::Applied;
    return application.appliedDamage > 0.0f;
}

float KrakenTentacleMidbossHealth::GetHpRatio() const {
    if (!valid_ || !std::isfinite(maxHp_) || !std::isfinite(currentHp_) ||
        maxHp_ <= 0.0f) {
        return 0.0f;
    }
    return std::clamp(currentHp_ / maxHp_, 0.0f, 1.0f);
}
