#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Camera;
class Object3dCommon;
struct LevelRailDebugVisualObject;
struct LevelSceneData;

struct LevelRailSummary {
    std::string railId;
    std::string name;
    std::string railType;
    bool loop = false;
    bool reverseDirection = false;
    float speed = 1.0f;
    size_t pointCount = 0;
    size_t segmentCount = 0;
};

class LevelRailDebugVisualizer {
public:
    LevelRailDebugVisualizer();
    ~LevelRailDebugVisualizer();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Clear();
    void Rebuild(const LevelSceneData& sceneData, bool axisConversionEnabled, uint64_t frameCounter = 0);
    void Update(uint64_t frameCounter);
    void Draw(uint64_t frameCounter);
    bool DrawImGui();
    void SetExternalDebugVisibility(bool hideRails, bool hideRailPoints);

    size_t GetRailCount() const { return railSummaries_.size(); }
    size_t GetRailPointCount() const { return railPointCount_; }
    size_t GetRailSegmentCount() const { return lineObjects_.size(); }
    uint64_t GetLastRebuildFrame() const { return lastRebuildFrame_; }
    uint64_t GetLastMatrixUpdateFrame() const { return lastMatrixUpdateFrame_; }

private:
    std::vector<std::unique_ptr<LevelRailDebugVisualObject>> lineObjects_;
    std::vector<std::unique_ptr<LevelRailDebugVisualObject>> pointObjects_;
    std::vector<LevelRailSummary> railSummaries_;
    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    std::array<float, 4> railLineColor_{ 0.15f, 0.95f, 1.0f, 0.85f };
    std::array<float, 4> railPointColor_{ 1.0f, 0.82f, 0.12f, 0.95f };
    std::array<float, 4> railStartPointColor_{ 0.15f, 1.0f, 0.25f, 1.0f };
    std::array<float, 4> railEndPointColor_{ 1.0f, 0.12f, 0.08f, 1.0f };
    bool showRails_ = true;
    bool showRailPoints_ = true;
    bool externalHideRails_ = false;
    bool externalHideRailPoints_ = false;
    bool pauseRailRebuild_ = false;
    bool updateMatricesWithLatestCamera_ = true;
    float railLineThickness_ = 0.045f;
    float railPointScale_ = 0.10f;
    size_t railPointCount_ = 0;
    int selectedRailIndex_ = 0;
    uint64_t rebuildCount_ = 0;
    uint64_t lastRebuildFrame_ = 0;
    uint64_t lastMatrixUpdateFrame_ = 0;
};
