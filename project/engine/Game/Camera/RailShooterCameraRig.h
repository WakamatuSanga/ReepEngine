#pragma once
#include "Engine/math/Matrix4x4.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

class BoostController;
class Camera;
class LevelRailRuntime;
class LevelSceneRuntime;
class Player;
class RailPathRuntimeV2;
struct LevelRailEvaluation;

class RailShooterCameraRig {
public:
    enum class Mode {
        Disabled,
        Straight,
        Rail,
        RailFinishedStraight,
        RailEndStopped,
    };

    enum class CameraRailStartMode {
        FromRailStart,
        ClosestPointFromCurrentCamera,
        BlendFromCurrentCameraToRail,
    };

    enum class RailEndBehavior {
        StopAtEnd,
        ContinueStraight,
        RestoreCamera,
    };

    enum class ClosestPointFallback {
        DoNotStart,
        FromRailStart,
    };

    struct RailFlightPoseSnapshot {
        Vector3 currentForward{ 0.0f, 0.0f, 1.0f };
        Vector3 aheadForward{ 0.0f, 0.0f, 1.0f };
        Vector3 up{ 0.0f, 1.0f, 0.0f };
        float railDistance = 0.0f;
        uint64_t railRevision = 0;
        int railIndex = -1;
        bool runtimeV2Active = false;
        bool running = false;
        bool valid = false;
    };

    struct ProjectileRailFrame {
        Vector3 position{ 0.0f, 0.0f, 0.0f };
        Vector3 right{ 1.0f, 0.0f, 0.0f };
        Vector3 up{ 0.0f, 1.0f, 0.0f };
        Vector3 forward{ 0.0f, 0.0f, 1.0f };
        float railDistance = 0.0f;
        uint64_t railRevision = 0;
        uint64_t continuityRevision = 0;
        int railIndex = -1;
        bool railActive = false;
        bool running = false;
        bool runtimeV2Active = false;
        bool gameModeActive = false;
        bool valid = false;
    };

    RailShooterCameraRig();
    ~RailShooterCameraRig();

    void Initialize(Camera* camera, LevelRailRuntime* railRuntime);
    void SetLevelSceneRuntime(const LevelSceneRuntime* levelSceneRuntime);
    void SetRuntimeContext(Player* player, BoostController* boostController, bool gameModeActive);
    void Finalize();
    void Update(float deltaTime);
    void DrawImGui();

    bool IsCameraRigActive() const;
    RailFlightPoseSnapshot GetRailFlightPoseSnapshot(float lookAheadDistance) const;
    ProjectileRailFrame GetProjectileRailFrame() const;
    bool IsControllingCamera() const { return IsCameraRigActive(); }
    bool StartRailByKey(const std::string& railKey, std::string& resultMessage);
    bool StartRailByKey(
        const std::string& railKey,
        CameraRailStartMode startMode,
        std::string& resultMessage);
    void StopAndRestoreCamera();
    bool IsGameplayPreviewModeEnabled() const { return gameplayPreviewMode_; }
    bool ShouldHideRailDebugWhileActive() const { return hideRailDebugWhileActive_ || gameplayPreviewMode_; }
    bool ShouldHideRailPointsWhileActive() const { return hideRailPointsWhileActive_ || gameplayPreviewMode_; }
    bool ShouldHideEventDebugWhileActive() const { return hideEventDebugWhileActive_ || gameplayPreviewMode_; }

private:
    struct RailCameraPose {
        Vector3 position{ 0.0f, 0.0f, 0.0f };
        Vector3 aheadPosition{ 0.0f, 0.0f, 0.0f };
        Vector3 forward{ 0.0f, 0.0f, 1.0f };
        Vector3 up{ 0.0f, 1.0f, 0.0f };
        float railDistance = 0.0f;
        bool valid = false;
    };

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
    bool StartRailByKeyWithSource(
        const std::string& railKey,
        CameraRailStartMode startMode,
        const std::string& startSource,
        std::string& resultMessage);
    bool EnsureRuntimeV2Trial();
    RailCameraPose BuildRailCameraPose(
        float railDistance, bool loopEnabled, const LevelRailEvaluation& legacyEvaluation);
    bool ApplyRuntimeV2Pose(
        const LevelRailEvaluation& legacyEvaluation, float deltaTime, bool loopEnabled);
    bool BuildRuntimeV2VisualPose();
    void ResetRuntimeV2PoseState(bool clearRuntime);
    void ForceResyncRuntimeV2Pose();
    void InvalidateProjectileRailContinuity();
    void UpdateBoostRailSpeed(float deltaTime, bool advancing);
    void ResetBoostRailSpeedState();
    void ClearRuntimeV2ForceTests();
    void DrawRuntimeV2TrialImGui();
    float GetActiveRailTotalLength() const;
    static float ComputePitchDegrees(const Vector3& forward);

    Camera* camera_ = nullptr;
    LevelRailRuntime* railRuntime_ = nullptr;
    const LevelSceneRuntime* levelSceneRuntime_ = nullptr;
    Player* player_ = nullptr;
    BoostController* boostController_ = nullptr;
    std::unique_ptr<RailPathRuntimeV2> runtimeV2Trial_;
    RailCameraPose railCameraPose_{};
    std::string selectedRailId_;
    std::string selectedRailName_;
    std::string selectedRailType_;
    std::string previousRailId_;
    Vector3 currentPosition_{ 0.0f, 0.0f, -10.0f };
    Vector3 currentForward_{ 0.0f, 0.0f, 1.0f };
    Vector3 currentUp_{ 0.0f, 1.0f, 0.0f };
    Vector3 currentRight_{ 1.0f, 0.0f, 0.0f };
    Vector3 projectileRailPosition_{ 0.0f, 0.0f, -10.0f };
    Vector3 projectileRailForward_{ 0.0f, 0.0f, 1.0f };
    Vector3 projectileRailUp_{ 0.0f, 1.0f, 0.0f };
    Vector3 projectileRailRight_{ 1.0f, 0.0f, 0.0f };
    Vector3 savedCameraPosition_{ 0.0f, 0.0f, -10.0f };
    Vector3 savedCameraRotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 blendStartPosition_{ 0.0f, 0.0f, -10.0f };
    Vector3 blendStartForward_{ 0.0f, 0.0f, 1.0f };
    Vector3 blendTargetPosition_{ 0.0f, 0.0f, -10.0f };
    Vector3 blendTargetForward_{ 0.0f, 0.0f, 1.0f };
    Vector3 lastClosestWorldPosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 lastBlendStartCameraPosition_{ 0.0f, 0.0f, -10.0f };
    Vector3 lastBlendTargetRailPosition_{ 0.0f, 0.0f, -10.0f };
    Vector3 visualCameraPosition_{ 0.0f, 0.0f, -10.0f };
    Vector3 visualCameraForward_{ 0.0f, 0.0f, 1.0f };
    size_t currentSegmentIndex_ = 0;
    size_t selectedRailPointCount_ = 0;
    size_t selectedRailSampledPointCount_ = 0;
    size_t lastClosestSampledPointCount_ = 0;
    size_t railCount_ = 0;
    int selectedRailIndex_ = 0;
    int previousSelectedRailIndex_ = -1;
    float railDistance_ = 0.0f;
    float railT_ = 0.0f;
    float railSpeed_ = 5.0f;
    float straightSpeed_ = 5.0f;
    float effectiveRailSpeed_ = 5.0f;
    float existingRailSpeedScale_ = 1.0f;
    float boostRailSpeedMultiplier_ = 1.15f;
    float currentRailSpeedMultiplier_ = 1.0f;
    float targetRailSpeedMultiplier_ = 1.0f;
    float boostRailAccelerationTime_ = 0.10f;
    float boostRailReturnTime_ = 0.45f;
    float selectedRailTotalLength_ = 0.0f;
    float railBlendTime_ = 0.5f;
    float railBlendElapsed_ = 0.0f;
    float lastStartDistance_ = 0.0f;
    float lastClosestRailDistance_ = 0.0f;
    float lastClosestRailPathDistance_ = 0.0f;
    float lastCameraToRailDistance_ = 0.0f;
    float lastBlendTargetDistance_ = 0.0f;
    float lastBlendProgress_ = 0.0f;
    float lastBlendTime_ = 0.0f;
    float lastStartDistanceRatio_ = 0.0f;
    float lastPreviousRailDistance_ = 0.0f;
    float maxAttachDistance_ = 50.0f;
    float nearEndWarningRatio_ = 0.9f;
    float runtimeV2LookAheadDistance_ = 5.0f;
    float cameraLookDistance_ = 10.0f;
    float lastRailAdvance_ = 0.0f;
    float railCameraForwardDot_ = 1.0f;
    float railPlayerForwardDot_ = 1.0f;
    float cameraPitchDegrees_ = 0.0f;
    float playerPitchDegrees_ = 0.0f;
    uint64_t runtimeV2BuiltRebuildCount_ = 0;
    uint64_t runtimeV2PoseApplyCount_ = 0;
    uint64_t legacyPoseApplyCount_ = 0;
    uint64_t gameModePoseSyncCount_ = 0;
    uint64_t initialCameraRailPoseOverwriteCount_ = 0;
    uint64_t boostMultiplierApplyCount_ = 0;
    uint64_t projectileRailContinuityRevision_ = 0;
    bool enableCameraRig_ = false;
    bool useCameraRig_ = true;
    bool autoPlay_ = false;
    bool debugLoop_ = false;
    bool selectedRailLoop_ = false;
    bool currentEvaluationValid_ = false;
    bool railEndReached_ = false;
    bool wasCameraRigActive_ = false;
    bool hasSavedCameraPose_ = false;
    bool lastClosestValid_ = false;
    bool lastBlendValid_ = false;
    bool lastStartEvaluationValid_ = false;
    bool lastStartedNearEnd_ = false;
    bool lastFallbackUsed_ = false;
    bool lastClosestTooFar_ = false;
    bool lastRailDirectionReversed_ = false;
    bool selectedRailReverseDirection_ = false;
    bool restoreCameraOnStop_ = true;
    bool restoreCameraOnDisable_ = true;
    bool enableCameraForwardSmoothing_ = true;
    bool hasSmoothedForward_ = false;
    bool gameplayPreviewMode_ = false;
    bool hideRailDebugWhileActive_ = true;
    bool hideRailPointsWhileActive_ = true;
    bool hideEventDebugWhileActive_ = true;
    bool enableAngledPlayerCamera_ = true;
    bool useVisualCameraForwardOnly_ = true;
    bool showCameraCompositionDebug_ = false;
    bool railBlendActive_ = false;
    bool autoApplyInitialCameraOnLoad_ = false;
    bool hasAppliedInitialCameraOnce_ = false;
    bool enableRuntimeV2Trial_ = true;
    bool runtimeV2PoseValid_ = false;
    bool runtimeV2BuildSucceeded_ = false;
    bool hasPreviousRailForward_ = false;
    bool forceLegacyForward_ = false;
    bool forceRuntimeV2Forward_ = false;
    bool forceBoostRailSpeed_ = false;
    bool forceBoostRailSpeedOff_ = false;
    bool boostControllerActive_ = false;
    bool boostStateActive_ = false;
    bool gameModeActive_ = false;
    bool runtimeContextInitialized_ = false;
    bool pendingGameModePoseSync_ = false;
    bool boostDoubleApplicationDetected_ = false;
    bool enemyBoostCompensationExists_ = false;
    float forwardSmoothStrength_ = 10.0f;
    float cameraHeightOffset_ = 1.2f;
    float cameraSideOffset_ = 0.0f;
    float lookDownAngleDeg_ = 6.0f;
    float lookAtYOffset_ = -0.4f;
    float playerScreenYOffset_ = -0.2f;
    float compositionLookAhead_ = 10.0f;
    Mode mode_ = Mode::Disabled;
    CameraRailStartMode railStartMode_ = CameraRailStartMode::BlendFromCurrentCameraToRail;
    RailEndBehavior railEndBehavior_ = RailEndBehavior::StopAtEnd;
    ClosestPointFallback closestPointFallback_ = ClosestPointFallback::FromRailStart;
    std::array<char, 128> manualStartRailKeyBuffer_{};
    std::string lastStartCameraRailTarget_ = "(none)";
    std::string lastResolvedRailId_ = "(none)";
    std::string lastResolvedRailName_ = "(none)";
    std::string lastStartMode_ = "(none)";
    std::string lastRequestedStartMode_ = "(none)";
    std::string lastActualStartMode_ = "(none)";
    std::string lastStartSource_ = "(none)";
    std::string lastStartWarning_ = "(none)";
    std::string lastStartResult_ = "(none)";
    std::string lastInitialCameraResult_ = "(none)";
    std::string runtimeV2BuiltRailId_;
    std::string runtimeV2BuildResult_ = "未構築";
    std::string forwardFallbackReason_ = "未評価";
    std::string activePoseSource_ = "未評価";
    Vector3 previousRailForward_{ 0.0f, 0.0f, 1.0f };
    Vector3 cameraLookTarget_{ 0.0f, 0.0f, 0.0f };
    Vector3 playerDisplayForward_{ 0.0f, 0.0f, 1.0f };
};
