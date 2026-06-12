#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstddef>
#include <string>

class Camera;
class LevelRailRuntime;
class LevelSceneRuntime;

class RailShooterCameraRig {
public:
    enum class Mode {
        Disabled,
        Straight,
        Rail,
        RailFinishedStraight,
    };

    enum class CameraRailStartMode {
        FromRailStart,
        ClosestPointFromCurrentCamera,
        BlendFromCurrentCameraToRail,
    };

    RailShooterCameraRig();
    ~RailShooterCameraRig();

    void Initialize(Camera* camera, LevelRailRuntime* railRuntime);
    void SetLevelSceneRuntime(const LevelSceneRuntime* levelSceneRuntime);
    void Finalize();
    void Update(float deltaTime);
    void DrawImGui();

    bool IsCameraRigActive() const;
    bool IsControllingCamera() const { return IsCameraRigActive(); }
    bool StartRailByKey(const std::string& railKey, std::string& resultMessage);
    void StopAndRestoreCamera();
    bool IsGameplayPreviewModeEnabled() const { return gameplayPreviewMode_; }
    bool ShouldHideRailDebugWhileActive() const { return hideRailDebugWhileActive_ || gameplayPreviewMode_; }
    bool ShouldHideRailPointsWhileActive() const { return hideRailPointsWhileActive_ || gameplayPreviewMode_; }
    bool ShouldHideEventDebugWhileActive() const { return hideEventDebugWhileActive_ || gameplayPreviewMode_; }

private:
    void SyncSelectedRailDefaults();
    bool FetchSelectedRailInfo();
    void UpdateStraight(float deltaTime);
    void UpdateRail(float deltaTime);
    void UpdateRailBlend(float deltaTime);
    void UpdateRailFinishedStraight(float deltaTime);
    void ApplyInitialCameraFromLevel();
    void ApplyToCamera();
    void CaptureCameraPose();
    void CaptureDebugHomeCamera();
    void RestoreDebugHomeCamera();
    void SaveDebugHomeCameraIfNeeded();
    Vector3 SmoothCameraForward(const Vector3& targetForward, float deltaTime);
    void ResetCameraRig();
    void ResetCameraRigAndRestoreCamera();
    void StartRail();
    void StopRail();

    Camera* camera_ = nullptr;
    LevelRailRuntime* railRuntime_ = nullptr;
    const LevelSceneRuntime* levelSceneRuntime_ = nullptr;
    std::string selectedRailId_;
    std::string selectedRailName_;
    std::string selectedRailType_;
    std::string previousRailId_;
    Vector3 currentPosition_{ 0.0f, 0.0f, -10.0f };
    Vector3 currentForward_{ 0.0f, 0.0f, 1.0f };
    Vector3 currentUp_{ 0.0f, 1.0f, 0.0f };
    Vector3 currentRight_{ 1.0f, 0.0f, 0.0f };
    Vector3 savedCameraPosition_{ 0.0f, 0.0f, -10.0f };
    Vector3 savedCameraRotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 blendStartPosition_{ 0.0f, 0.0f, -10.0f };
    Vector3 blendStartForward_{ 0.0f, 0.0f, 1.0f };
    Vector3 blendTargetPosition_{ 0.0f, 0.0f, -10.0f };
    Vector3 blendTargetForward_{ 0.0f, 0.0f, 1.0f };
    size_t currentSegmentIndex_ = 0;
    size_t selectedRailPointCount_ = 0;
    size_t selectedRailSampledPointCount_ = 0;
    size_t railCount_ = 0;
    int selectedRailIndex_ = 0;
    int previousSelectedRailIndex_ = -1;
    float railDistance_ = 0.0f;
    float railT_ = 0.0f;
    float railSpeed_ = 5.0f;
    float straightSpeed_ = 5.0f;
    float selectedRailTotalLength_ = 0.0f;
    float railBlendTime_ = 0.5f;
    float railBlendElapsed_ = 0.0f;
    float lastStartDistance_ = 0.0f;
    float lastClosestRailDistance_ = 0.0f;
    bool enableCameraRig_ = false;
    bool useCameraRig_ = true;
    bool autoPlay_ = false;
    bool debugLoop_ = false;
    bool selectedRailLoop_ = false;
    bool currentEvaluationValid_ = false;
    bool wasCameraRigActive_ = false;
    bool hasSavedCameraPose_ = false;
    bool restoreCameraOnStop_ = true;
    bool restoreCameraOnDisable_ = true;
    bool enableCameraForwardSmoothing_ = true;
    bool hasSmoothedForward_ = false;
    bool gameplayPreviewMode_ = false;
    bool hideRailDebugWhileActive_ = true;
    bool hideRailPointsWhileActive_ = true;
    bool hideEventDebugWhileActive_ = true;
    bool railBlendActive_ = false;
    bool autoApplyInitialCameraOnLoad_ = false;
    bool hasAppliedInitialCameraOnce_ = false;
    float forwardSmoothStrength_ = 10.0f;
    Mode mode_ = Mode::Disabled;
    CameraRailStartMode railStartMode_ = CameraRailStartMode::BlendFromCurrentCameraToRail;
    std::string lastStartResult_ = "(none)";
    std::string lastInitialCameraResult_ = "(none)";
};
