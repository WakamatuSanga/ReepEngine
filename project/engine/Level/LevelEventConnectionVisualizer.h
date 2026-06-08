#pragma once
#include <array>
#include <cstddef>
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
    void Rebuild(const LevelSceneData& sceneData, bool axisConversionEnabled);
    void Update();
    void Draw();
    bool DrawImGui();

    size_t GetLinkCount() const;
    size_t GetMissingFlagLinkCount() const { return missingFlagLinkCount_; }

private:
    std::vector<std::unique_ptr<LevelEventConnectionVisualObject>> links_;
    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    std::array<float, 4> flagLinkColor_{ 0.12f, 0.42f, 1.0f, 0.85f };
    size_t missingFlagLinkCount_ = 0;
    bool showFlagLinks_ = true;
};
