#include "RailShooterCameraRig.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Level/LevelSceneRuntime.h"
#include <algorithm>

namespace {
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

    Vector3 DegreesToRadians(const Vector3& degrees) {
        return {
            degrees.x * kDegToRad,
            degrees.y * kDegToRad,
            degrees.z * kDegToRad,
        };
    }
}

void RailShooterCameraRig::ApplyInitialCameraFromLevel() {
    if (!camera_) {
        lastInitialCameraResult_ = "Camera is missing.";
        return;
    }
    if (!levelSceneRuntime_) {
        lastInitialCameraResult_ = "LevelSceneRuntime is missing.";
        return;
    }

    const LevelCameraStart* cameraStart = levelSceneRuntime_->GetEngineCameraStart();
    if (!cameraStart) {
        lastInitialCameraResult_ = "camera_start is not loaded.";
        return;
    }

    camera_->SetTranslate(cameraStart->transform.translation);
    camera_->SetRotate(DegreesToRadians(cameraStart->transform.rotation));
    camera_->SetFovY((std::max)(0.01f, cameraStart->fovY));
    camera_->SetNearClip((std::max)(0.001f, cameraStart->nearClip));
    camera_->SetFarClip((std::max)(cameraStart->nearClip + 0.01f, cameraStart->farClip));
    camera_->Update();

    CaptureCameraPose();
    CaptureDebugHomeCamera();
    lastInitialCameraResult_ = "Applied camera_start: " +
        (cameraStart->name.empty() ? cameraStart->id : cameraStart->name);
}
