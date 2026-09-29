#include "PlayerBulletManager.h"
#include "PlayerFanChargeAttack.h"
#include "Engine/Game/Enemy/EnemyBullet.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Graphics/Camera/Camera.h"

#include <algorithm>
#include <cmath>

namespace {
    Vector3 Add(const Vector3& a, const Vector3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
    Vector3 Subtract(const Vector3& a, const Vector3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
    Vector3 Scale(const Vector3& a, float s) { return { a.x * s, a.y * s, a.z * s }; }
    float Dot(const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    Vector3 Normalize(const Vector3& a, const Vector3& fallback) {
        const float length = std::sqrt(Dot(a, a));
        return std::isfinite(length) && length > 0.00001f ? Scale(a, 1.0f / length) : fallback;
    }
}

PlayerBulletManager::PlayerBulletInstance::PlayerBulletInstance() = default;
PlayerBulletManager::PlayerBulletInstance::~PlayerBulletInstance() = default;
PlayerBulletManager::PlayerBulletInstance::PlayerBulletInstance(PlayerBulletInstance&&) noexcept = default;
PlayerBulletManager::PlayerBulletInstance& PlayerBulletManager::PlayerBulletInstance::operator=(PlayerBulletInstance&&) noexcept = default;

void PlayerBulletManager::ResetFanCharge() {
    if (fanChargeAttack_) {
        fanChargeAttack_->Reset();
    }
    chargeTime_ = chargeRate_ = 0.0f;
    isChargeMax_ = false;
}

bool PlayerBulletManager::UpdateChargeState(float deltaTime, bool inputBlocked) {
    if (!fanChargeAttack_ || !player_ || !player_->IsEnabled() || !aimPlayerAlive_ || !enablePlayerShot_) {
        ResetFanCharge();
        return false;
    }
    maxChargeTime_ = std::isfinite(maxChargeTime_) ? (std::max)(maxChargeTime_, 0.05f) : 1.5f;
    const auto result = fanChargeAttack_->Update(deltaTime, lastLeftClickHeld_,
        !inputBlocked && enableChargeFeedbackInput_, maxChargeTime_);
    chargeTime_ = fanChargeAttack_->GetChargeTime();
    chargeRate_ = std::clamp(chargeTime_ / maxChargeTime_, 0.0f, 1.0f);
    isChargeMax_ = chargeRate_ >= 1.0f;
    for (int i = 0; i < result.volleys; ++i) {
        FireFanChargeVolley();
    }
    return result.suppressNormalShot;
}

void PlayerBulletManager::FireFanChargeVolley() {
    if (!player_ || !camera_ || !fanChargeAttack_) {
        return;
    }
    const auto& settings = fanChargeAttack_->GetSettings();
    const Matrix4x4& world = camera_->GetWorldMatrix();
    const Vector3 forward = Normalize({ world.m[2][0], world.m[2][1], world.m[2][2] }, { 0, 0, 1 });
    const Vector3 initialDirection = ResolveAimDirection(player_->GetWorldPosition(), forward);
    const Vector3 origin = Add(player_->GetWorldPosition(), Scale(initialDirection, muzzleOffset_));
    const Vector3 direction = ResolveFinalShotDirection(origin, initialDirection);
    if (!lastShotSpawnDataValid_) {
        return;
    }
    const Vector3 toTarget = Subtract(lastAimPoint_, origin);
    const float distance = std::sqrt(Dot(toTarget, toTarget));
    if (!std::isfinite(distance) || distance < 0.001f) {
        return;
    }
    // Unit diagonals in the camera plane: +/-45 degrees from screen up.
    // Do not project onto the aim plane, which would skew the screen-space angle.
    const Vector3 cameraUp{ world.m[1][0], world.m[1][1], world.m[1][2] };
    const Vector3 cameraRight{ world.m[0][0], world.m[0][1], world.m[0][2] };
    const int perSide = std::clamp(settings.bulletsPerSide, 1, 4);
    for (int side : { -1, 1 }) {
        const Vector3 spreadDirection = Normalize(
            Add(cameraUp, Scale(cameraRight, static_cast<float>(side))), cameraUp);
        for (int lane = 1; lane <= perSide; ++lane) {
            auto trajectory = std::make_unique<PlayerFanChargeTrajectory>();
            trajectory->origin = origin;
            trajectory->direction = direction;
            trajectory->up = spreadDirection;
            trajectory->speed = (std::max)(settings.speed, 0.1f);
            trajectory->bendDistance = (std::max)(0.001f, distance * std::clamp(settings.bendDistanceRate, 0.05f, 1.0f));
            // Close targets must not produce a near-vertical fan.
            trajectory->spread = static_cast<float>(lane) / static_cast<float>(perSide)
                * (std::min)((std::max)(settings.spread, 0.0f), trajectory->bendDistance * 0.10f);
            const Vector3 velocity = trajectory->Velocity(0.0f);
            if (EnemyBullet* bullet = SpawnBullet(origin, velocity, settings.damage)) {
                bullet->SetLifeTime((std::max)(bulletLifeTime_, distance / trajectory->speed + 1.0f));
                bullet->SetVisualForwardOverride(velocity);
                bullets_.back().projectileType = PlayerProjectileType::FanChargeShot;
                bullets_.back().fanTrajectory = std::move(trajectory);
                lastFirePosition_ = origin;
                lastFireDirection_ = direction;
                lastShotVelocity_ = velocity;
                RecordAimShot();
            }
        }
    }
}

void PlayerBulletManager::UpdateFanChargeShot(PlayerBulletInstance& instance, float deltaTime) {
    if (!instance.fanTrajectory || !instance.bullet || instance.bullet->IsDead() || deltaTime <= 0.0f) {
        return;
    }
    EnemyBullet& bullet = *instance.bullet;
    const float nextTime = bullet.GetElapsedTime() + deltaTime;
    const Vector3 next = instance.fanTrajectory->Position(nextTime);
    // Existing EnemyBullet::Update performs the move and updates the visual transform.
    // Collision snapshots read that same position; no second visual-only motion exists.
    bullet.SetVelocity(Scale(Subtract(next, bullet.GetPosition()), 1.0f / deltaTime));
    bullet.SetVisualForwardOverride(instance.fanTrajectory->Velocity(nextTime));
}
