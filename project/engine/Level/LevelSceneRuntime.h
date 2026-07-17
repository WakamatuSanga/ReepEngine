#pragma once
#include "Engine/Level/LevelSceneData.h"
#include <array>
#include <cstdint>
#include <memory>
#include <string>

class Camera;
class BlenderRailPipelineVerification;
class LevelEventConnectionVisualizer;
class LevelEventLabelVisualizer;
class LevelEventObjectActionVisualizer;
class LevelEventRuntime;
class LevelEventVisualizer;
class LevelRailDebugVisualizer;
class LevelRailRuntime;
class LevelObjectDebugVisualizer;
class LevelSceneLoader;
class Object3dCommon;

class LevelSceneRuntime {
public:
    LevelSceneRuntime();
    ~LevelSceneRuntime();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Update();
    void Draw();
    void DrawImGui();
    void SetGameViewRect(float x, float y, float width, float height);
    void ClearGameViewRect();
    void SetCameraRigPreviewState(
        bool cameraRigActive,
        bool hideRailDebug,
        bool hideRailPoints,
        bool hideEventDebug,
        bool gameplayPreviewMode);
    void ApplySceneData(
        const LevelSceneData& sceneData,
        const std::string& statusMessage,
        const std::string& sourceName);
    void SetLiveSyncDiagnostics(
        bool autoApplyEnabled,
        uint64_t lastPacketApplied,
        bool receiverRunning = false,
        uint64_t receivedPacketCount = 0,
        const std::string& lastReceiveTime = {},
        const std::string& lastApplyStatus = {},
        const std::string& lastError = {});

    const LevelSceneData& GetSceneData() const { return sceneData_; }
    LevelEventRuntime* GetEventRuntime() const { return eventRuntime_.get(); }
    LevelRailRuntime* GetRailRuntime() const { return railRuntime_.get(); }
    const LevelCameraStart* GetEngineCameraStart() const { return hasEngineCameraStart_ ? &engineCameraStart_ : nullptr; }
    bool HasEngineCameraStart() const { return hasEngineCameraStart_; }
    bool TryFindObjectWorldPosition(
        const std::string& objectId,
        const std::string& objectName,
        Vector3& outPosition) const;
    const std::string& GetLastLoadStatus() const { return lastLoadStatus_; }
    bool IsLiveApplyPaused() const { return pauseLiveApply_; }
    bool IsRebuildOnlyWhenJsonChangedEnabled() const { return rebuildOnlyWhenJsonChanged_; }

private:
    void LoadJsonFromBuffer();
    void SetPathBufferText(const std::string& text);
    void RequestRebuild(const std::string& applySource);
    void RebuildDebugObjects();
    bool ShouldHideRailDebug() const;
    bool ShouldHideRailPoints() const;
    bool ShouldHideEventDebug() const;

    std::unique_ptr<LevelSceneLoader> loader_;
    std::unique_ptr<LevelObjectDebugVisualizer> objectDebugVisualizer_;
    std::unique_ptr<LevelEventVisualizer> eventVisualizer_;
    std::unique_ptr<LevelEventConnectionVisualizer> connectionVisualizer_;
    std::unique_ptr<LevelEventObjectActionVisualizer> objectActionVisualizer_;
    std::unique_ptr<LevelEventLabelVisualizer> labelVisualizer_;
    std::unique_ptr<LevelEventRuntime> eventRuntime_;
    std::unique_ptr<LevelRailDebugVisualizer> railDebugVisualizer_;
    std::unique_ptr<LevelRailRuntime> railRuntime_;
    std::unique_ptr<BlenderRailPipelineVerification> blenderRailVerification_;
    LevelSceneData sceneData_;
    LevelCameraStart engineCameraStart_;
    std::array<char, 260> jsonPathBuffer_{};
    std::string jsonPath_ = "resources/level_editor/level_editor.json";
    std::string lastLoadStatus_;
    std::string lastResolvedPath_;
    bool axisConversionEnabled_ = true;
    bool freezeDebugObjects_ = false;
    bool pauseLiveApply_ = false;
    bool rebuildOnlyWhenJsonChanged_ = true;
    bool rebuildDirty_ = false;
    bool cameraRigActiveForDebug_ = false;
    bool hideRailDebugWhileCameraRigActive_ = true;
    bool hideRailPointsWhileCameraRigActive_ = true;
    bool hideEventDebugWhileCameraRigActive_ = true;
    bool gameplayPreviewMode_ = false;
    bool hasEngineCameraStart_ = false;
    uint64_t frameCounter_ = 0;
    uint64_t rebuildCount_ = 0;
    uint64_t lastRebuildFrame_ = 0;
    uint64_t lastPacketApplied_ = 0;
    bool liveAutoApplyEnabled_ = true;
    int selectedObjectIndex_ = -1;
    std::string lastApplySource_ = "(none)";
    std::string pendingRebuildSource_;
};
