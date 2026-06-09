#pragma once
#include "Engine/Level/LevelSceneData.h"
#include <array>
#include <cstdint>
#include <memory>
#include <string>

class Camera;
class LevelEventConnectionVisualizer;
class LevelEventLabelVisualizer;
class LevelEventObjectActionVisualizer;
class LevelEventVisualizer;
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

    std::unique_ptr<LevelSceneLoader> loader_;
    std::unique_ptr<LevelObjectDebugVisualizer> objectDebugVisualizer_;
    std::unique_ptr<LevelEventVisualizer> eventVisualizer_;
    std::unique_ptr<LevelEventConnectionVisualizer> connectionVisualizer_;
    std::unique_ptr<LevelEventObjectActionVisualizer> objectActionVisualizer_;
    std::unique_ptr<LevelEventLabelVisualizer> labelVisualizer_;
    LevelSceneData sceneData_;
    std::array<char, 260> jsonPathBuffer_{};
    std::string jsonPath_ = "resources/level_editor/level_editor.json";
    std::string lastLoadStatus_;
    std::string lastResolvedPath_;
    bool axisConversionEnabled_ = true;
    bool freezeDebugObjects_ = false;
    bool pauseLiveApply_ = false;
    bool rebuildOnlyWhenJsonChanged_ = true;
    bool rebuildDirty_ = false;
    uint64_t frameCounter_ = 0;
    uint64_t rebuildCount_ = 0;
    uint64_t lastRebuildFrame_ = 0;
    uint64_t lastPacketApplied_ = 0;
    bool liveAutoApplyEnabled_ = true;
    int selectedObjectIndex_ = -1;
    std::string lastApplySource_ = "(none)";
    std::string pendingRebuildSource_;
};
