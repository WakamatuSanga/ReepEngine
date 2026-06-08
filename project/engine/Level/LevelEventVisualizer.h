#pragma once
#include <cstddef>
#include <memory>
#include <vector>

class Camera;
class Object3dCommon;
struct LevelEventVisualObject;
struct LevelSceneData;

class LevelEventVisualizer {
public:
    LevelEventVisualizer();
    ~LevelEventVisualizer();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Clear();
    void Rebuild(const LevelSceneData& sceneData, bool axisConversionEnabled);
    void Update();
    void Draw();
    bool DrawImGui();

    size_t GetVisualCount() const;

private:
    std::vector<std::unique_ptr<LevelEventVisualObject>> visuals_;
    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    bool showEventFlags_ = true;
    bool showDisabledEventFlags_ = false;
    float eventFlagAlpha_ = 0.35f;
    float eventFlagScaleMultiplier_ = 1.0f;
};
