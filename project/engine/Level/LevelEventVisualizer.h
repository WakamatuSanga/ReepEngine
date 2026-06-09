#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
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

    size_t GetVisualCount() const;
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
    bool freezeEventVisuals_ = false;
    bool pauseEventVisualRebuild_ = false;
    bool updateMatricesWithLatestCamera_ = true;
    float eventFlagAlpha_ = 0.35f;
    float eventFlagScaleMultiplier_ = 1.0f;
    uint64_t rebuildCount_ = 0;
    uint64_t lastRebuildFrame_ = 0;
    uint64_t lastMatrixUpdateFrame_ = 0;
};
