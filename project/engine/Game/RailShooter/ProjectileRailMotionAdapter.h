#pragma once

#include "Engine/math/Matrix4x4.h"

#include <cstddef>
#include <cstdint>
#include <string>

class RailShooterCameraRig;

class ProjectileRailMotionAdapter {
public:
    enum class ProjectileKind {
        Player,
        Enemy,
    };

    void Initialize(const RailShooterCameraRig* railCameraRig);
    void Finalize();
    void BeginFrame();
    Vector3 GetLockedWingLaunchRailDisplacement(const Vector3& worldPosition) const;

    void RecordShot(
        ProjectileKind kind,
        const Vector3& muzzleWorldPosition,
        const Vector3& targetWorldPosition,
        const Vector3& shotDirection,
        const Vector3& worldVelocity);
    void RecordDespawn(ProjectileKind kind, const std::string& reason);

    void DrawImGui(size_t playerActiveCount, size_t enemyActiveCount);
    void ResetDiagnostics();

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
        Vector3 lastWorldVelocity{ 0.0f, 0.0f, 0.0f };
        uint64_t shotCount = 0;
        std::string lastDespawnReason = "未記録";
    };

    FrameSnapshot CaptureFrame() const;
    KindDiagnostics& GetDiagnostics(ProjectileKind kind);

    const RailShooterCameraRig* railCameraRig_ = nullptr;
    FrameSnapshot previousFrame_{};
    FrameSnapshot currentFrame_{};
    KindDiagnostics playerDiagnostics_{};
    KindDiagnostics enemyDiagnostics_{};

    uint64_t frameResyncCount_ = 0;
    float frameTranslationDistance_ = 0.0f;
    float frameRotationDegrees_ = 0.0f;
};
