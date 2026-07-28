#include "PlayerBulletManager.h"
#include "Engine/Game/Enemy/EnemyBullet.h"
#include "Engine/Game/RailShooter/ProjectileRailMotionAdapter.h"
#include "Engine/Utility/Logger.h"
#include <algorithm>

EnemyBullet* PlayerBulletManager::SpawnBullet(
    const Vector3& position, const Vector3& velocity, int damage) {
    if (!object3dCommon_ || !camera_) {
        Logger::Log("[PlayerBulletManager] SpawnBullet skipped: Object3dCommon or Camera is null");
        return nullptr;
    }

    PlayerBulletInstance instance;
    instance.bullet = std::make_unique<EnemyBullet>();
    if (!instance.bullet->Initialize(object3dCommon_, camera_)) {
        Logger::Log("[PlayerBulletManager] SpawnBullet failed: EnemyBullet visual initialize failed");
        return nullptr;
    }
    instance.bullet->SetModelPath(modelPath_);
    instance.bullet->SetPosition(position);
    instance.bullet->SetVelocity(velocity);
    instance.bullet->SetScale(defaultScale_);
    instance.bullet->SetRotation(defaultRotation_);
    instance.bullet->SetModelRotationOffset(playerBulletModelRotationOffset_);
    instance.bullet->SetRadius(bulletRadius_);
    instance.bullet->SetLifeTime(bulletLifeTime_);
    instance.bullet->SetUseLightweightVisual(useLightweightBulletVisual_);
    instance.damage = (std::max)(1, damage);
    if (projectileRailMotionAdapter_) {
        projectileRailMotionAdapter_->RegisterProjectile(*instance.bullet);
    }

    EnemyBullet* bulletPtr = instance.bullet.get();
    bullets_.push_back(std::move(instance));
    ++firedBulletCount_;
    return bulletPtr;
}
