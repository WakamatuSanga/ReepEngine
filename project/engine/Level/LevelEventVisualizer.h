#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Camera;
class LevelEventRuntime;
class Object3dCommon;
struct LevelEventVisualObject;
struct LevelSceneData;

class LevelEventVisualizer {
public:
    LevelEventVisualizer();
    ~LevelEventVisualizer();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void SetRuntimeStateProvider(const LevelEventRuntime* runtime);
    void Clear();
    void Rebuild(const LevelSceneData& sceneData, bool axisConversionEnabled, uint64_t frameCounter = 0);
    void Update(uint64_t frameCounter);
    void Draw(uint64_t frameCounter);
    bool DrawImGui();
    void SetExternalDebugHidden(bool hidden);

    size_t GetVisualCount() const;
    size_t GetTotalEventFlagCount() const { return totalEventFlagCount_; }
    size_t GetVisibleEventFlagCount() const { return visibleEventFlagCount_; }
    size_t GetHiddenEventFlagCount() const { return hiddenEventFlagCount_; }
    size_t GetHiddenVisibleInEditorFalseCount() const { return hiddenVisibleInEditorFalseCount_; }
    size_t GetHiddenDisabledCount() const { return hiddenDisabledCount_; }
    size_t GetHiddenCameraRigDebugCount() const { return hiddenCameraRigDebugCount_; }
    size_t GetHiddenNoVisualObjectCount() const { return hiddenNoVisualObjectCount_; }
    size_t GetHiddenRuntimeInactiveCount() const { return hiddenRuntimeInactiveCount_; }
    const std::string& GetHiddenReasonSummary() const { return hiddenReasonSummary_; }
    uint64_t GetRebuildCount() const { return rebuildCount_; }
    uint64_t GetLastRebuildFrame() const { return lastRebuildFrame_; }
    uint64_t GetLastMatrixUpdateFrame() const { return lastMatrixUpdateFrame_; }

private:
    std::vector<std::unique_ptr<LevelEventVisualObject>> visuals_;
    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    const LevelEventRuntime* runtimeStateProvider_ = nullptr;
    bool showEventFlags_ = true;
    bool showDisabledEventFlags_ = false;
    bool showRuntimeInactiveFlags_ = true;
    bool externalDebugHidden_ = false;
    bool freezeEventVisuals_ = false;
    bool pauseEventVisualRebuild_ = false;
    bool updateMatricesWithLatestCamera_ = true;
    float eventFlagAlpha_ = 0.35f;
    float eventFlagScaleMultiplier_ = 1.0f;
    size_t totalEventFlagCount_ = 0;
    size_t visibleEventFlagCount_ = 0;
    size_t hiddenEventFlagCount_ = 0;
    size_t hiddenVisibleInEditorFalseCount_ = 0;
    size_t hiddenDisabledCount_ = 0;
    size_t baseHiddenDisabledCount_ = 0;
    size_t hiddenCameraRigDebugCount_ = 0;
    size_t hiddenNoVisualObjectCount_ = 0;
    size_t hiddenRuntimeInactiveCount_ = 0;
    std::string hiddenReasonSummary_;
    uint64_t rebuildCount_ = 0;
    uint64_t lastRebuildFrame_ = 0;
    uint64_t lastMatrixUpdateFrame_ = 0;
};
