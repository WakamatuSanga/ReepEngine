#include "PlayerBulletManager.h"

#include "Engine/Game/Enemy/EnemyBullet.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
float DistanceSquaredLocal(const Vector3& a, const Vector3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

bool FindSegmentEntry(const Vector3& start, const Vector3& end, const Vector3& center,
    const Vector3& axes, Vector3& impact) {
    const Vector3 offset{ (start.x - center.x) / axes.x,
        (start.y - center.y) / axes.y, (start.z - center.z) / axes.z };
    const Vector3 direction{ (end.x - start.x) / axes.x,
        (end.y - start.y) / axes.y, (end.z - start.z) / axes.z };
    const float startDistance = DistanceSquaredLocal(offset, {}) - 1.0f;
    if (startDistance <= 0.0f) {
        impact = start;
        return true;
    }
    const float lengthSquared = DistanceSquaredLocal(direction, {});
    const float projection = offset.x * direction.x + offset.y * direction.y + offset.z * direction.z;
    const float discriminant = projection * projection - lengthSquared * startDistance;
    if (lengthSquared <= 0.0f || projection >= 0.0f || discriminant < 0.0f) {
        return false;
    }
    const float entry = startDistance / (-projection + std::sqrt(discriminant));
    if (entry > 1.0f) {
        return false;
    }
    impact = { start.x + (end.x - start.x) * entry, start.y + (end.y - start.y) * entry,
        start.z + (end.z - start.z) * entry };
    return true;
}
} // namespace

Vector3 PlayerBulletManager::GetClosestProjectileStepPosition(
    const PlayerBulletInstance& instance, const Vector3& center, const Vector3& axes) {
    const EnemyBullet& bullet = *instance.bullet;
    if (!instance.lockedWingLaunch || instance.lockedWingLaunch->sweep.count < 2) {
        return bullet.GetStepPosition(bullet.GetClosestStepFraction(center, center, axes));
    }

    const auto& sweep = instance.lockedWingLaunch->sweep;
    const auto toScaledSpace = [&](const Vector3& point) {
        return Vector3{(point.x - center.x) / axes.x,
            (point.y - center.y) / axes.y, (point.z - center.z) / axes.z};
    };
    Vector3 closest = sweep.points[0];
    Vector3 start = toScaledSpace(closest);
    float closestDistanceSquared = DistanceSquaredLocal(start, {});
    for (size_t i = 1; i < sweep.count; ++i) {
        const Vector3 end = toScaledSpace(sweep.points[i]);
        const Vector3 step{end.x - start.x, end.y - start.y, end.z - start.z};
        const float lengthSquared = DistanceSquaredLocal(step, {});
        if (lengthSquared > 0.0000001f) {
            const float fraction = std::clamp(
                -(start.x * step.x + start.y * step.y + start.z * step.z) / lengthSquared, 0.0f, 1.0f);
            const Vector3 candidate{start.x + step.x * fraction,
                start.y + step.y * fraction, start.z + step.z * fraction};
            const float distanceSquared = DistanceSquaredLocal(candidate, {});
            if (distanceSquared < closestDistanceSquared) {
                closestDistanceSquared = distanceSquared;
                closest = {center.x + candidate.x * axes.x,
                    center.y + candidate.y * axes.y, center.z + candidate.z * axes.z};
            }
        }
        start = end;
    }
    return closest;
}

Vector3 PlayerBulletManager::GetProjectileImpactPosition(const PlayerBulletInstance& instance,
    const Vector3& center, const Vector3& axes, const Vector3& closestPosition) {
    if (axes.x <= 0.0f || axes.y <= 0.0f || axes.z <= 0.0f) {
        return closestPosition;
    }
    Vector3 impact = closestPosition;
    if (instance.lockedWingLaunch && instance.lockedWingLaunch->sweep.count >= 2) {
        const auto& sweep = instance.lockedWingLaunch->sweep;
        for (size_t index = 1; index < sweep.count; ++index) {
            if (FindSegmentEntry(sweep.points[index - 1], sweep.points[index], center, axes, impact)) {
                break;
            }
        }
    } else {
        FindSegmentEntry(instance.bullet->GetStepPosition(0.0f), instance.bullet->GetStepPosition(1.0f),
            center, axes, impact);
    }
    return impact;
}

bool PlayerBulletManager::CheckHitAndKillFirstEllipsoid(
    const Vector3& center,
    float radius,
    const Vector3& axisScale,
    Vector3* hitPosition,
    int* damage,
    float* lastDistance,
    float* lastRadiusSum,
    float* lastBulletRadius, PlayerProjectileType* projectileType) {
    const float safeRadius = (std::max)(0.001f, radius);
    const Vector3 safeScale{
        (std::max)(axisScale.x, 0.1f),
        (std::max)(axisScale.y, 0.1f),
        (std::max)(axisScale.z, 0.1f)
    };
    float closestDistance = -1.0f;
    float closestRadiusSum = safeRadius * (std::max)(safeScale.x, (std::max)(safeScale.y, safeScale.z));
    float closestBulletRadius = 0.0f;

    for (PlayerBulletInstance& instance : bullets_) {
        EnemyBullet* bullet = instance.bullet.get();
        if (!bullet || !bullet->IsActive() || bullet->IsDead()) {
            continue;
        }

        const float bulletRadius = bullet->GetRadius();
        const float axisX = safeRadius * safeScale.x + bulletRadius;
        const float axisY = safeRadius * safeScale.y + bulletRadius;
        const float axisZ = safeRadius * safeScale.z + bulletRadius;
        const Vector3 bulletPosition =
            GetClosestProjectileStepPosition(instance, center, {axisX, axisY, axisZ});
        const Vector3 delta{ bulletPosition.x - center.x, bulletPosition.y - center.y, bulletPosition.z - center.z };
        const float normalizedDistanceSq =
            (delta.x * delta.x) / (axisX * axisX) +
            (delta.y * delta.y) / (axisY * axisY) +
            (delta.z * delta.z) / (axisZ * axisZ);
        const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        const float debugRadiusSum = (std::max)(axisX, (std::max)(axisY, axisZ));
        if (closestDistance < 0.0f || distance < closestDistance) {
            closestDistance = distance;
            closestRadiusSum = debugRadiusSum;
            closestBulletRadius = bulletRadius;
        }
        if (normalizedDistanceSq <= 1.0f) {
            if (projectileType) {
                *projectileType = instance.projectileType;
            }
            RecordLockedWingHomingHit(instance);
            if (hitPosition) {
                *hitPosition = GetProjectileImpactPosition(instance, center, { axisX, axisY, axisZ }, bulletPosition);
            }
            if (damage) {
                *damage = instance.damage;
            }
            if (lastDistance) {
                *lastDistance = distance;
            }
            if (lastRadiusSum) {
                *lastRadiusSum = debugRadiusSum;
            }
            if (lastBulletRadius) {
                *lastBulletRadius = bulletRadius;
            }
            bullet->Kill("Enemyとの衝突");
            return true;
        }
    }

    if (lastDistance) {
        *lastDistance = closestDistance;
    }
    if (lastRadiusSum) {
        *lastRadiusSum = closestRadiusSum;
    }
    if (lastBulletRadius) {
        *lastBulletRadius = closestBulletRadius;
    }
    return false;
}

bool PlayerBulletManager::CheckHitAndKillFirstSphere(
    const Vector3& center,
    float radius,
    Vector3* hitPosition,
    int* damage,
    float* lastDistance,
    float* lastRadiusSum,
    float* lastBulletRadius, PlayerProjectileType* projectileType) {
    const float safeRadius = (std::max)(0.0f, radius);
    float closestDistance = -1.0f;
    float closestRadiusSum = safeRadius;
    float closestBulletRadius = 0.0f;
    const float closestInitial = -1.0f;

    for (PlayerBulletInstance& instance : bullets_) {
        EnemyBullet* bullet = instance.bullet.get();
        if (!bullet || !bullet->IsActive() || bullet->IsDead()) {
            continue;
        }

        const float bulletRadius = bullet->GetRadius();
        const float combinedRadius = safeRadius + bulletRadius;
        const Vector3 bulletPosition =
            GetClosestProjectileStepPosition(instance, center, {1.0f, 1.0f, 1.0f});
        const float distanceSquared = DistanceSquaredLocal(center, bulletPosition);
        const float distance = std::sqrt(distanceSquared);
        if (closestDistance == closestInitial || distance < closestDistance) {
            closestDistance = distance;
            closestRadiusSum = combinedRadius;
            closestBulletRadius = bulletRadius;
        }
        if (distanceSquared <= combinedRadius * combinedRadius) {
            if (projectileType) {
                *projectileType = instance.projectileType;
            }
            RecordLockedWingHomingHit(instance);
            if (hitPosition) {
                *hitPosition = GetProjectileImpactPosition(instance, center,
                    { combinedRadius, combinedRadius, combinedRadius }, bulletPosition);
            }
            if (damage) {
                *damage = instance.damage;
            }
            if (lastDistance) {
                *lastDistance = distance;
            }
            if (lastRadiusSum) {
                *lastRadiusSum = combinedRadius;
            }
            if (lastBulletRadius) {
                *lastBulletRadius = bulletRadius;
            }
            bullet->Kill("Enemyとの衝突");
            return true;
        }
    }

    if (lastDistance) {
        *lastDistance = closestDistance;
    }
    if (lastRadiusSum) {
        *lastRadiusSum = closestRadiusSum;
    }
    if (lastBulletRadius) {
        *lastBulletRadius = closestBulletRadius;
    }
    return false;
}

std::vector<PlayerBulletManager::PlayerBulletCollisionSnapshot>
PlayerBulletManager::GetActiveCollisionSnapshots() const {
    std::vector<PlayerBulletCollisionSnapshot> snapshots;
    snapshots.reserve(GetActiveCount());

    for (const PlayerBulletInstance& instance : bullets_) {
        if (!instance.bullet || !instance.bullet->IsActive() || instance.bullet->IsDead()) {
            continue;
        }

        PlayerBulletCollisionSnapshot snapshot{};
        snapshot.runtimeId = instance.runtimeId;
        snapshot.worldPosition = instance.bullet->GetPosition();
        snapshot.velocity = instance.bullet->GetVelocity();
        snapshot.radius = instance.bullet->GetRadius();
        snapshot.lifeTime = instance.bullet->GetLifeTime();
        snapshot.elapsedTime = instance.bullet->GetElapsedTime();
        snapshot.damage = instance.damage;
        snapshot.projectileType = instance.projectileType;
        snapshot.active = instance.bullet->IsActive();
        snapshot.killed = instance.bullet->IsDead();
        if (instance.lockedWingLaunch) {
            snapshot.lockedTargetId =
                instance.lockedWingLaunch->lockedTargetId;
            snapshot.launchPhase = static_cast<uint8_t>(
                instance.lockedWingLaunch->phase);
            snapshot.homingReady =
                instance.lockedWingLaunch->homingReady;
            snapshot.exhaustEnabled =
                instance.lockedWingLaunch->exhaustEnabled;
            snapshot.lockedWingSweep = instance.lockedWingLaunch->sweep;
        }
        snapshots.push_back(std::move(snapshot));
    }

    return snapshots;
}
