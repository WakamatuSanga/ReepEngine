#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class Camera;
class Object3dCommon;
struct LevelEventConnectionVisualObject;
struct LevelSceneData;

class LevelEventConnectionVisualizer {
public:
    LevelEventConnectionVisualizer();
    ~LevelEventConnectionVisualizer();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Clear();
    void Rebuild(const LevelSceneData& sceneData, bool axisConversionEnabled, uint64_t frameCounter = 0);
    void Update(uint64_t frameCounter);
    void Draw(uint64_t frameCounter);
    bool DrawImGui();

    size_t GetLinkCount() const;
    size_t GetMissingFlagLinkCount() const { return missingFlagLinkCount_; }
    uint64_t GetRebuildCount() const { return rebuildCount_; }
    uint64_t GetLastRebuildFrame() const { return lastRebuildFrame_; }
    uint64_t GetLastMatrixUpdateFrame() const { return lastMatrixUpdateFrame_; }

private:
    std::vector<std::unique_ptr<LevelEventConnectionVisualObject>> links_;
    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    std::array<float, 4> flagLinkColor_{ 0.12f, 0.42f, 1.0f, 0.85f };
    size_t missingFlagLinkCount_ = 0;
    bool showFlagLinks_ = true;
    bool pauseFlagLinkRebuild_ = false;
    bool updateMatricesWithLatestCamera_ = true;
    uint64_t rebuildCount_ = 0;
    uint64_t lastRebuildFrame_ = 0;
    uint64_t lastMatrixUpdateFrame_ = 0;
};
