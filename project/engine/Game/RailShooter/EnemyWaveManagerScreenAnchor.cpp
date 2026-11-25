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
struct EnemyWaveManager::LaserFrame {
    const EnemyWaveManager& waves;
    EnemyLaserTelegraphController& controller;
    Vector3 origin;
    Vector3 trackingDirection;
    float deltaTime;
    float warningDuration;
    bool useAimLock;
    float lockStartTime;
    float firstWarningDelay;

    void StartWarning(WaveEnemyRuntime& runtime) const {
        runtime.laserTimer = 0.0f;
        runtime.laserAimLocked = false;
        runtime.lockedLaserDirection = trackingDirection;
        runtime.lockedLaserTargetPosition = waves.ComputeLaserTargetPosition(*runtime.enemy);
        controller.StartWarning(runtime.enemy, origin, trackingDirection);
    }

    void LockWarning(WaveEnemyRuntime& runtime) const {
        if (runtime.laserAimLocked) return;
        CaptureAim(runtime);
        controller.LockWarning(runtime.enemy, origin, runtime.lockedLaserTargetPosition, runtime.lockedLaserDirection);
    }

    void StartBeam(WaveEnemyRuntime& runtime) const {
        if (!runtime.laserAimLocked) CaptureAim(runtime);
        runtime.laserTimer = 0.0f;
        controller.StartBeam(runtime.enemy, origin, runtime.lockedLaserDirection);
    }

    float Cooldown() const { return (std::max)(0.0f, waves.laserCooldown_); }

private:
    void CaptureAim(WaveEnemyRuntime& runtime) const {
        runtime.lockedLaserTargetPosition = waves.ComputeLaserTargetPosition(*runtime.enemy);
        runtime.lockedLaserDirection = Normalize(SubtractVector3(runtime.lockedLaserTargetPosition, origin), trackingDirection);
        runtime.laserAimLocked = true;
    }
};

class EnemyWaveManager::LaserPhase {
public:
    virtual ~LaserPhase() = default;
    virtual const LaserPhase& Update(WaveEnemyRuntime& runtime, const LaserFrame& frame) const = 0;
    virtual bool KeepsEnemyOnScreen() const { return false; }
    virtual float TimeUntilExit(const WaveEnemyRuntime&, const EnemyLaserTelegraphController&) const { return 0.0f; }
    static const LaserPhase& For(LaserState phase);

private:
    class Waiting;
    class Warning;
    class WarningTracking;
    class WarningLocked;
    class Beam;
    class Cooldown;
};

class EnemyWaveManager::LaserPhase::Waiting final : public LaserPhase {
public:
    const LaserPhase& Update(WaveEnemyRuntime& runtime, const LaserFrame& frame) const override {
        runtime.laserTimer += frame.deltaTime;
        if (runtime.laserTimer >= frame.firstWarningDelay) {
            frame.StartWarning(runtime);
            return For(LaserState::WarningTracking);
        }
        return *this;
    }
};

class EnemyWaveManager::LaserPhase::Warning : public LaserPhase {
public:
    bool KeepsEnemyOnScreen() const override { return true; }

    float TimeUntilExit(const WaveEnemyRuntime& runtime, const EnemyLaserTelegraphController& controller) const override {
        return controller.GetWarningDuration() - runtime.laserTimer + controller.GetBeamDuration();
    }
};

class EnemyWaveManager::LaserPhase::WarningTracking final : public Warning {
public:
    const LaserPhase& Update(WaveEnemyRuntime& runtime, const LaserFrame& frame) const override {
        runtime.laserTimer += frame.deltaTime;
        const bool lockNow = frame.useAimLock && runtime.laserTimer >= frame.lockStartTime;
        if (lockNow) {
            frame.LockWarning(runtime);
        } else {
            frame.controller.UpdateWarning(runtime.enemy, frame.origin, frame.trackingDirection);
        }
        // An update can cross both thresholds. Lock before firing, without
        // running the locked state's separate update in the same frame.
        if (runtime.laserTimer >= frame.warningDuration) {
            frame.StartBeam(runtime);
            return For(LaserState::Beam);
        }
        return lockNow ? For(LaserState::WarningLocked) : *this;
    }
};

class EnemyWaveManager::LaserPhase::WarningLocked final : public Warning {
public:
    const LaserPhase& Update(WaveEnemyRuntime& runtime, const LaserFrame& frame) const override {
        runtime.laserTimer += frame.deltaTime;
        frame.LockWarning(runtime);
        frame.controller.UpdateWarning(runtime.enemy, frame.origin, runtime.lockedLaserDirection);
        if (runtime.laserTimer >= frame.warningDuration) {
            frame.StartBeam(runtime);
            return For(LaserState::Beam);
        }
        return *this;
    }
};

class EnemyWaveManager::LaserPhase::Beam final : public LaserPhase {
public:
    const LaserPhase& Update(WaveEnemyRuntime& runtime, const LaserFrame& frame) const override {
        frame.controller.UpdateBeamOrigin(runtime.enemy, frame.origin);
        runtime.laserTimer += frame.deltaTime;
        if (runtime.laserTimer >= frame.controller.GetBeamDuration()) {
            runtime.laserTimer = 0.0f;
            return For(LaserState::Cooldown);
        }
        return *this;
    }

    bool KeepsEnemyOnScreen() const override { return true; }

    float TimeUntilExit(const WaveEnemyRuntime& runtime, const EnemyLaserTelegraphController& controller) const override {
        return controller.GetBeamDuration() - runtime.laserTimer;
    }
};

class EnemyWaveManager::LaserPhase::Cooldown final : public LaserPhase {
public:
    const LaserPhase& Update(WaveEnemyRuntime& runtime, const LaserFrame& frame) const override {
        if (!frame.controller.IsLoopLaserEnabled()) return *this;
        runtime.laserTimer += frame.deltaTime;
        if (runtime.laserTimer >= frame.Cooldown()) {
            frame.StartWarning(runtime);
            return For(LaserState::WarningTracking);
        }
        return *this;
    }
};

const EnemyWaveManager::LaserPhase& EnemyWaveManager::LaserPhase::For(LaserState phase) {
    static const Waiting waiting;
    static const WarningTracking tracking;
    static const WarningLocked locked;
    static const Beam beam;
    static const Cooldown cooldown;
    static const LaserPhase* const phases[]{&waiting, &tracking, &locked, &beam, &cooldown};
    return *phases[static_cast<std::size_t>(phase)];
}

const EnemyWaveManager::LaserPhase& EnemyWaveManager::InitialLaserPhase() {
    return LaserPhase::For(LaserState::Waiting);
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
        ComputeAnchorPosition(spawn.screenX, spawnY, spawn.depth),
        targetPosition,
        dropDuration,
        rotationDegrees,
        0.0f);
}

void EnemyWaveManager::UpdateScreenAnchorEnemy(WaveEnemyRuntime& runtime, float deltaTime) {
    Enemy& enemy = *runtime.enemy;
    const Vector3 anchor = ComputeAnchorPosition(runtime.screenX, runtime.screenY, runtime.depth);
    if (!enemy.CanAttack()) {
        enemy.TranslateSpawnPath(SubtractVector3(anchor, runtime.railPosition));
        runtime.railPosition = anchor;
        return;
    }

    lastScreenAnchorPosition_ = anchor;
    if (!runtime.anchorExiting) {
        enemy.SetPosition(anchor);
        enemy.SetForward(ComputeLaserDirection(enemy));
        runtime.anchorAge += deltaTime;
        const bool holdFinished = runtime.anchorHold > 0.0f && runtime.anchorAge >= runtime.anchorHold;
        if (!holdFinished || runtime.laserPhase->KeepsEnemyOnScreen()) {
            UpdateLaserTelegraph(runtime, deltaTime);
            return;
        }
        runtime.anchorExiting = true;
    }

    runtime.anchorExitAge += deltaTime;
    const Vector2 lane = ComputeScreenAnchorLane(runtime, runtime.anchorExitAge);
    const Vector3 exit = ComputeAnchorPosition(lane.x, lane.y, runtime.depth);
    enemy.SetPosition(exit);
    enemy.SetForward(SubtractVector3(exit, anchor));
    if (runtime.anchorExitAge >= runtime.anchorExitDuration) EscapeWaveEnemy(runtime);
}

Vector2 EnemyWaveManager::ComputeScreenAnchorLane(const WaveEnemyRuntime& runtime, float exitAge) {
    const float progress = std::clamp(exitAge / runtime.anchorExitDuration, 0.0f, 1.0f);
    const float easeIn = progress * progress;
    const float exitX = runtime.screenX < 0.0f ? -1.5f : 1.5f;
    return { std::lerp(runtime.screenX, exitX, easeIn), std::lerp(runtime.screenY, 1.85f, easeIn) };
}

float EnemyWaveManager::GetScreenAnchorTimeUntilExit(const WaveEnemyRuntime& runtime) const {
    const float delay = (std::max)(0.0f, runtime.anchorHold - runtime.anchorAge);
    return laserController_ ? (std::max)(delay, runtime.laserPhase->TimeUntilExit(runtime, *laserController_)) : delay;
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
    const LaserFrame frame{*this, *laserController_, origin, trackingDirection, deltaTime,
        warningDuration, useAimLock, lockStartTime, (std::max)(0.0f, configuredFirstWarningDelay)};
    runtime.laserPhase = &runtime.laserPhase->Update(runtime, frame);
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

