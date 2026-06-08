#pragma once
#include "Engine/Level/LevelSceneData.h"
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Camera;
class LevelEventConnectionVisualizer;
class LevelEventVisualizer;
class LevelSceneLoader;
struct LevelSceneDebugObject;
struct LevelSceneRuntimeTransform;
class Object3dCommon;

class LevelSceneRuntime {
public:
    LevelSceneRuntime();
    ~LevelSceneRuntime();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Update();
    void Draw();
    void DrawImGui();
    void ApplySceneData(
        const LevelSceneData& sceneData,
        const std::string& statusMessage,
        const std::string& sourceName);
    void SetLiveSyncDiagnostics(bool autoApplyEnabled, uint64_t lastPacketApplied);

    const LevelSceneData& GetSceneData() const { return sceneData_; }
    const std::string& GetLastLoadStatus() const { return lastLoadStatus_; }
    bool IsLiveApplyPaused() const { return pauseLiveApply_; }
    bool IsRebuildOnlyWhenJsonChangedEnabled() const { return rebuildOnlyWhenJsonChanged_; }

private:
    void LoadJsonFromBuffer();
    void SetPathBufferText(const std::string& text);
    void RequestRebuild(const std::string& applySource);
    void RebuildDebugObjects();
    void UpdateDebugObjectMatrices();
    void BuildDebugObjectsRecursive(
        const LevelObject& object,
        const LevelSceneRuntimeTransform& parentTransform);

    std::unique_ptr<LevelSceneLoader> loader_;
    std::unique_ptr<LevelEventVisualizer> eventVisualizer_;
    std::unique_ptr<LevelEventConnectionVisualizer> connectionVisualizer_;
    LevelSceneData sceneData_;
    std::vector<std::unique_ptr<LevelSceneDebugObject>> debugObjects_;
    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    std::array<char, 260> jsonPathBuffer_{};
    std::string jsonPath_ = "resources/level_editor/level_editor.json";
    std::string lastLoadStatus_;
    std::string lastResolvedPath_;
    int drawMode_ = 0;
    bool showLevelObjects_ = true;
    bool showDebugColliders_ = true;
    bool showBlenderHelpers_ = false;
    bool axisConversionEnabled_ = true;
    bool freezeDebugObjects_ = false;
    bool pauseLiveApply_ = false;
    bool rebuildOnlyWhenJsonChanged_ = true;
    bool rebuildDirty_ = false;
    uint64_t frameCounter_ = 0;
    uint64_t rebuildCount_ = 0;
    uint64_t lastRebuildFrame_ = 0;
    uint64_t lastDebugMatrixUpdateFrame_ = 0;
    uint64_t lastPacketApplied_ = 0;
    bool liveAutoApplyEnabled_ = true;
    size_t missingModelCount_ = 0;
    int selectedObjectIndex_ = -1;
    std::string lastApplySource_ = "(none)";
    std::string pendingRebuildSource_;
};
