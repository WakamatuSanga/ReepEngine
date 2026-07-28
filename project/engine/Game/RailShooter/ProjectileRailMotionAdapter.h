#pragma once

#include "Engine/math/Matrix4x4.h"

#include <cstddef>
#include <cstdint>
#include <string>

class EnemyBullet;
class RailShooterCameraRig;

class ProjectileRailMotionAdapter {
public:
    enum class ProjectileKind {
        Player,
        Enemy,
    };

    void Initialize(const RailShooterCameraRig* railCameraRig);
    void Finalize();
    void BeginFrame(bool playerAlive);

    void RegisterProjectile(EnemyBullet& projectile);
    void ApplyToProjectile(EnemyBullet& projectile, ProjectileKind kind);
    bool TransportDirectionForProjectile(
        const EnemyBullet& projectile,
        ProjectileKind kind,
        const Vector3& direction,
        Vector3& transportedDirection) const;
    bool IsTrackingActive(ProjectileKind kind) const;
    bool ShouldSuppressCameraVelocityInheritance() const;

    void RecordShot(
        ProjectileKind kind,
        const Vector3& muzzleWorldPosition,
        const Vector3& targetWorldPosition,
        const Vector3& shotDirection,
        const Vector3& relativeVelocity);
    void RecordDespawn(ProjectileKind kind, const std::string& reason);

    void DrawImGui(size_t playerActiveCount, size_t enemyActiveCount);
    void ResetDiagnostics();
    void ClearForcedStates();

private:
    struct FrameSnapshot {
        Vector3 position{ 0.0f, 0.0f, 0.0f };
        Vector3 right{ 1.0f, 0.0f, 0.0f };
        Vector3 up{ 0.0f, 1.0f, 0.0f };
        Vector3 forward{ 0.0f, 0.0f, 1.0f };
        float railDistance = 0.0f;
        uint64_t revision = 0;
        uint64_t continuityRevision = 0;
        int railIndex = -1;
        bool railActive = false;
        bool running = false;
        bool runtimeV2Active = false;
        bool gameModeActive = false;
        bool valid = false;
    };

    struct KindDiagnostics {
        Vector3 lastMuzzleWorldPosition{ 0.0f, 0.0f, 0.0f };
        Vector3 lastTargetWorldPosition{ 0.0f, 0.0f, 0.0f };
        Vector3 lastShotDirection{ 0.0f, 0.0f, 1.0f };
        Vector3 lastRelativeVelocity{ 0.0f, 0.0f, 0.0f };
        uint64_t shotCount = 0;
        uint64_t applyCount = 0;
        uint64_t skipCount = 0;
        uint64_t spawnFrameSkipCount = 0;
        uint64_t doubleApplyCount = 0;
        std::string lastDespawnReason = "未記録";
    };

    FrameSnapshot CaptureFrame() const;
    std::string ResolveResyncReason(
        const FrameSnapshot& previous,
        const FrameSnapshot& current,
        bool hadPreviousFrame,
        bool playerAlive) const;
    std::string ResolveInactiveReason(bool playerAlive) const;
    bool IsBaseTrackingActive() const;
    KindDiagnostics& GetDiagnostics(ProjectileKind kind);
    const KindDiagnostics& GetDiagnostics(ProjectileKind kind) const;
    void RecordFrameSkip(const std::string& reason, bool resync);

    const RailShooterCameraRig* railCameraRig_ = nullptr;
    FrameSnapshot previousFrame_{};
    FrameSnapshot currentFrame_{};
    KindDiagnostics playerDiagnostics_{};
    KindDiagnostics enemyDiagnostics_{};

    uint64_t frameSequence_ = 0;
    uint64_t beginFrameCount_ = 0;
    uint64_t registeredProjectileCount_ = 0;
    uint64_t frameDeltaApplyCount_ = 0;
    uint64_t frameDeltaSkipCount_ = 0;
    uint64_t frameResyncCount_ = 0;
    uint64_t doubleApplicationCount_ = 0;
    float frameTranslationDistance_ = 0.0f;
    float frameRotationDegrees_ = 0.0f;
    float maxFrameTranslation_ = 50.0f;
    float maxFrameRotationDegrees_ = 60.0f;
    std::string lastSkipReason_ = "未評価";
    std::string lastFrameStatus_ = "未初期化";

    bool initialized_ = false;
    bool hasCurrentFrame_ = false;
    bool hasPlayerAliveState_ = false;
    bool previousPlayerAlive_ = false;
    bool currentPlayerAlive_ = false;
    bool frameDeltaReady_ = false;
    bool doubleApplicationDetected_ = false;
    bool forceDisableTracking_ = false;
    bool forceDisablePlayerTracking_ = false;
    bool forceDisableEnemyTracking_ = false;
    bool forceResyncRequested_ = false;
};
