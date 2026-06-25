#include "Enemy.h"

#include <algorithm>
void Enemy::SetHitRadius(float hitRadius) {
    hitRadius_ = (std::max)(0.001f, hitRadius);
}

void Enemy::SetHitScale(const Vector3& hitScale) {
    hitScale_ = {
        (std::max)(hitScale.x, 0.1f),
        (std::max)(hitScale.y, 0.1f),
        (std::max)(hitScale.z, 0.1f)
    };
}

void Enemy::SetUseEllipsoidHitShape(bool enabled) {
    useEllipsoidHitShape_ = enabled;
}

