#include "PlayerBulletManager.h"

#include "Engine/Game/Enemy/EnemyBullet.h"

#include <algorithm>
#include <cmath>

namespace {
float DistanceSquaredLocal(const Vector3& a, const Vector3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}
} // namespace

bool PlayerBulletManager::CheckHitAndKillFirstEllipsoid(
    const Vector3& center,
    float radius,
    const Vector3& axisScale,
    Vector3* hitPosition,
    int* damage,
    float* lastDistance,
    float* lastRadiusSum,
    float* lastBulletRadius) {
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

        const Vector3 bulletPosition = bullet->GetPosition();
        const Vector3 delta{ bulletPosition.x - center.x, bulletPosition.y - center.y, bulletPosition.z - center.z };
        const float bulletRadius = bullet->GetRadius();
        const float axisX = safeRadius * safeScale.x + bulletRadius;
        const float axisY = safeRadius * safeScale.y + bulletRadius;
        const float axisZ = safeRadius * safeScale.z + bulletRadius;
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
            if (hitPosition) {
                *hitPosition = bulletPosition;
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
            bullet->Kill();
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
    float* lastBulletRadius) {
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
        const float distanceSquared = DistanceSquaredLocal(center, bullet->GetPosition());
        const float distance = std::sqrt(distanceSquared);
        if (closestDistance == closestInitial || distance < closestDistance) {
            closestDistance = distance;
            closestRadiusSum = combinedRadius;
            closestBulletRadius = bulletRadius;
        }
        if (distanceSquared <= combinedRadius * combinedRadius) {
            if (hitPosition) {
                *hitPosition = bullet->GetPosition();
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
            bullet->Kill();
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

