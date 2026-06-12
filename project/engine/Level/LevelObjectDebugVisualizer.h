#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class Camera;
class Object3dCommon;
struct LevelObject;
struct LevelSceneData;

class LevelObjectDebugVisualizer {
public:
    LevelObjectDebugVisualizer();
    ~LevelObjectDebugVisualizer();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Clear();
    void Rebuild(const LevelSceneData& sceneData, bool axisConversionEnabled);
    void Update(uint64_t frameCounter);
    void Draw();
    bool DrawImGui();
    void DrawObjectDetails(const LevelObject& object) const;
    void DrawObjectDebugDetails(const LevelObject& object) const;

    size_t GetVisibleObjectCount() const;
    size_t GetMissingModelCount() const { return missingModelCount_; }
    size_t GetDebugObjectCount() const { return debugObjects_.size(); }
    uint64_t GetLastMatrixUpdateFrame() const { return lastMatrixUpdateFrame_; }
    bool IsBlenderHelper(const LevelObject& object) const;

private:
    struct DebugObject;

    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    std::vector<std::unique_ptr<DebugObject>> debugObjects_;
    int drawMode_ = 0;
    bool showLevelObjects_ = true;
    bool showDebugColliders_ = true;
    bool showBlenderHelpers_ = false;
    size_t missingModelCount_ = 0;
    uint64_t lastMatrixUpdateFrame_ = 0;
};
