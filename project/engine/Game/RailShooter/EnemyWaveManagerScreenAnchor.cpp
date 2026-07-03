#include "EnemyWaveManager.h"
#include "Engine/Game/Enemy/Enemy.h"
#include "Engine/Game/Enemy/EnemyLaserTelegraphController.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Graphics/Camera/Camera.h"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace {
    constexpr float kMinVectorLength = 0.00001f;

    Vector3 SubtractVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    }

    float Length(const Vector3& value) {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    }

    Vector3 Normalize(const Vector3& value, const Vector3& fallback) {
        const float length = Length(value);
        if (length <= kMinVectorLength || !std::isfinite(length)) {
            return fallback;
        }
        return { value.x / length, value.y / length, value.z / length };
    }

    Vector3 GetCameraForward(const Camera& camera) {
        const Matrix4x4& matrix = camera.GetWorldMatrix();
        return Normalize({ matrix.m[2][0], matrix.m[2][1], matrix.m[2][2] }, { 0.0f, 0.0f, 1.0f });
    }

    std::string TrimCopyForPattern(const std::string& value) {
        const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
        const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
        if (begin >= end) {
            return {};
        }
        return std::string(begin, end);
    }
}
void EnemyWaveManager::ConfigureScreenAnchorSpawn(Enemy* enemy, const EnemyWaveSpawnEntry& spawn, const Vector3& targetPosition) {
    if (!enemy) {
        return;
    }
    const float scale = (std::max)(0.1f, spawn.enemyScale > 0.0f ? spawn.enemyScale : screenAnchorEnemyScale_);
    const float dropDuration = (std::max)(0.05f, spawn.dropDuration > 0.0f ? spawn.dropDuration : screenAnchorDropDuration_);
    const float spawnY = spawn.spawnScreenY != 0.0f ? spawn.spawnScreenY : screenAnchorSpawnScreenY_;
    const float rotationDegrees = spawn.rotationDuringDrop != 0.0f ? spawn.rotationDuringDrop : screenAnchorRotationDuringDrop_;
    enemy->SetScale({ scale, scale, scale });
    enemy->StartSpawnAnimationFrom(
        ComputeScreenPosition(spawn.screenX, spawnY, spawn.depth),
        targetPosition,
        dropDuration,
        rotationDegrees,
        0.0f);
}

void EnemyWaveManager::UpdateLaserTelegraph(WaveEnemyRuntime& runtime, float deltaTime) {
    if (!runtime.enemy || !laserController_ || !laserController_->IsEnabled() || !IsLaserTelegraphAttackPattern(runtime.attackPattern)) {
        return;
    }

    const Vector3 origin = runtime.enemy->GetPosition();
    const Vector3 trackingDirection = ComputeLaserDirection(*runtime.enemy);
    const float warningDuration = (std::max)(0.01f, laserController_->GetWarningDuration());
    const bool useAimLock = laserController_->IsAimLockEnabled();
    const float lockBeforeFireTime = useAimLock ? std::clamp(laserController_->GetLockBeforeFireTime(), 0.0f, warningDuration) : 0.0f;
    const float lockStartTime = (std::max)(0.0f, warningDuration - lockBeforeFireTime);
    const float configuredFirstWarningDelay = runtime.screenX > 0.01f ? rightFirstWarningDelay_ :
        (runtime.screenX < -0.01f ? leftFirstWarningDelay_ : firstWarningDelay_);
    const float firstWarningDelay = (std::max)(0.0f, configuredFirstWarningDelay);

    auto lockAim = [&]() {
        if (runtime.laserAimLocked) {
            return;
        }
        runtime.lockedLaserTargetPosition = ComputeLaserTargetPosition(*runtime.enemy);
        runtime.lockedLaserDirection = Normalize(SubtractVector3(runtime.lockedLaserTargetPosition, origin), trackingDirection);
        runtime.laserAimLocked = true;
        laserController_->LockWarning(runtime.enemy, origin, runtime.lockedLaserTargetPosition, runtime.lockedLaserDirection);
        };

    switch (runtime.laserState) {
    case LaserState::Waiting:
        runtime.laserTimer += deltaTime;
        if (runtime.laserTimer >= firstWarningDelay) {
            runtime.laserTimer = 0.0f;
            runtime.laserAimLocked = false;
            runtime.lockedLaserDirection = trackingDirection;
            runtime.lockedLaserTargetPosition = ComputeLaserTargetPosition(*runtime.enemy);
            runtime.laserState = LaserState::WarningTracking;
            laserController_->StartWarning(runtime.enemy, origin, trackingDirection);
        }
        break;
    case LaserState::WarningTracking:
        runtime.laserTimer += deltaTime;
        if (useAimLock && runtime.laserTimer >= lockStartTime) {
            lockAim();
            runtime.laserState = LaserState::WarningLocked;
        } else {
            laserController_->UpdateWarning(runtime.enemy, origin, trackingDirection);
        }
        if (runtime.laserTimer >= warningDuration) {
            if (!runtime.laserAimLocked) {
                runtime.lockedLaserTargetPosition = ComputeLaserTargetPosition(*runtime.enemy);
                runtime.lockedLaserDirection = Normalize(SubtractVector3(runtime.lockedLaserTargetPosition, origin), trackingDirection);
                runtime.laserAimLocked = true;
            }
            runtime.laserTimer = 0.0f;
            runtime.laserState = LaserState::Beam;
            laserController_->StartBeam(runtime.enemy, origin, runtime.lockedLaserDirection);
        }
        break;
    case LaserState::WarningLocked:
        runtime.laserTimer += deltaTime;
        lockAim();
        laserController_->UpdateWarning(runtime.enemy, origin, runtime.lockedLaserDirection);
        if (runtime.laserTimer >= warningDuration) {
            runtime.laserTimer = 0.0f;
            runtime.laserState = LaserState::Beam;
            laserController_->StartBeam(runtime.enemy, origin, runtime.lockedLaserDirection);
        }
        break;
    case LaserState::Beam:
        runtime.laserTimer += deltaTime;
        if (runtime.laserTimer >= laserController_->GetBeamDuration()) {
            runtime.laserTimer = 0.0f;
            runtime.laserState = LaserState::Cooldown;
        }
        break;
    case LaserState::Cooldown:
        if (!laserController_->IsLoopLaserEnabled()) {
            break;
        }
        runtime.laserTimer += deltaTime;
        if (runtime.laserTimer >= (std::max)(0.0f, laserCooldown_)) {
            runtime.laserTimer = 0.0f;
            runtime.laserAimLocked = false;
            runtime.lockedLaserDirection = trackingDirection;
            runtime.lockedLaserTargetPosition = ComputeLaserTargetPosition(*runtime.enemy);
            runtime.laserState = LaserState::WarningTracking;
            laserController_->StartWarning(runtime.enemy, origin, trackingDirection);
        }
        break;
    }
}

Vector3 EnemyWaveManager::ComputeLaserDirection(const Enemy& enemy) const {
    const Vector3 fallback = camera_ ? GetCameraForward(*camera_) : Vector3{ 0.0f, 0.0f, 1.0f };
    return Normalize(SubtractVector3(ComputeLaserTargetPosition(enemy), enemy.GetPosition()), fallback);
}

Vector3 EnemyWaveManager::ComputeLaserTargetPosition(const Enemy& enemy) const {
    if (player_) {
        return player_->GetWorldPosition();
    }
    const Vector3 fallback = camera_ ? GetCameraForward(*camera_) : Vector3{ 0.0f, 0.0f, 1.0f };
    const Vector3 origin = enemy.GetPosition();
    return {
        origin.x + fallback.x * 20.0f,
        origin.y + fallback.y * 20.0f,
        origin.z + fallback.z * 20.0f,
    };
}
bool EnemyWaveManager::IsScreenAnchorMovePattern(const std::string& movePattern) const {
    std::string normalized = TrimCopyForPattern(movePattern);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
        });
    return normalized == "screenanchordropin" || normalized == "screen_anchor_drop_in" ||
        normalized == "screenanchordrop" || normalized == "screen_anchor_drop";
}

bool EnemyWaveManager::IsLaserTelegraphAttackPattern(const std::string& attackPattern) const {
    std::string normalized = TrimCopyForPattern(attackPattern);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
        });
    return normalized == "telegraphlaser" || normalized == "warninglaser" ||
        normalized == "lasertelegraph" || normalized == "laser_warning";
}

