#pragma once
#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class Camera;
struct LevelEventLabelItem;
struct LevelSceneData;

class LevelEventLabelVisualizer {
public:
    LevelEventLabelVisualizer();
    ~LevelEventLabelVisualizer();

    void Initialize(Camera* camera);
    void Clear();
    void Rebuild(const LevelSceneData& sceneData, bool axisConversionEnabled);
    void SetViewportRect(float x, float y, float width, float height);
    void ClearViewportRect();
    void DrawOverlay() const;
    bool DrawImGui();

    size_t GetLabelCount() const;

private:
    Camera* camera_ = nullptr;
    std::vector<std::unique_ptr<LevelEventLabelItem>> labels_;
    bool showEventLabels_ = true;
    bool showEventNames_ = true;
    bool showEventDescriptions_ = true;
    bool showActionDescriptions_ = true;
    float eventLabelFontSize_ = 16.0f;
    float eventLabelMaxDistance_ = 120.0f;
    std::array<float, 4> eventLabelColor_{ 0.65f, 1.0f, 1.0f, 1.0f };
    std::array<float, 4> actionLabelColor_{ 1.0f, 0.42f, 0.35f, 1.0f };
    bool useGameViewRectForLabels_ = true;
    bool showLabelDebugPoints_ = false;
    bool hasViewportRect_ = false;
    float viewportX_ = 0.0f;
    float viewportY_ = 0.0f;
    float viewportWidth_ = 0.0f;
    float viewportHeight_ = 0.0f;
    mutable size_t lastVisibleCount_ = 0;
    mutable size_t lastClippedCount_ = 0;
    mutable size_t lastProjectionFailedCount_ = 0;
};
