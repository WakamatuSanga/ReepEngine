#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class Camera;
class Object3dCommon;
struct LevelEventObjectActionVisualObject;
struct LevelSceneData;

class LevelEventObjectActionVisualizer {
public:
    LevelEventObjectActionVisualizer();
    ~LevelEventObjectActionVisualizer();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Clear();
    void Rebuild(const LevelSceneData& sceneData, bool axisConversionEnabled, uint64_t frameCounter = 0);
    void Update(uint64_t frameCounter);
    void Draw(uint64_t frameCounter);
    bool DrawImGui();

    size_t GetLinkCount() const;
    size_t GetMissingObjectLinkCount() const { return missingObjectLinkCount_; }
    uint64_t GetRebuildCount() const { return rebuildCount_; }
    uint64_t GetLastRebuildFrame() const { return lastRebuildFrame_; }
    uint64_t GetLastMatrixUpdateFrame() const { return lastMatrixUpdateFrame_; }

private:
    std::vector<std::unique_ptr<LevelEventObjectActionVisualObject>> links_;
    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    std::array<float, 4> objectActionLinkColor_{ 1.0f, 0.12f, 0.08f, 0.9f };
    size_t missingObjectLinkCount_ = 0;
    bool showObjectActionLinks_ = true;
    bool pauseObjectActionLinkRebuild_ = false;
    bool updateMatricesWithLatestCamera_ = true;
    uint64_t rebuildCount_ = 0;
    uint64_t lastRebuildFrame_ = 0;
    uint64_t lastMatrixUpdateFrame_ = 0;
};
