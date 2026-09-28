#include "Engine/Game/Player/PlayerBulletManager.h"

#include "Engine/Game/Enemy/EnemyBullet.h"

bool PlayerBulletManager::TryKillProjectileByRuntimeId(
    std::uint64_t runtimeId,
    PlayerProjectileKillReason reason) {
    if (runtimeId == 0) {
        return false;
    }
    for (PlayerBulletInstance& instance : bullets_) {
        if (instance.runtimeId != runtimeId || !instance.bullet ||
            !instance.bullet->IsActive() || instance.bullet->IsDead()) {
            continue;
        }
        const char* deathReason =
            reason == PlayerProjectileKillReason::KrakenWeakPointHit
                ? "クラーケン触手弱点との衝突"
                : "クラーケン触手本体との衝突";
        instance.bullet->Kill(deathReason);
        UpdateLockedWingMissileExhaust(instance);
        return true;
    }
    return false;
}
