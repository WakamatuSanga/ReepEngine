#pragma once
#include <array>
#include <memory>
#include "Matrix4x4.h"

class Sprite;
class SpriteCommon;

// SceneManager owns the wipe so it survives TitleScene::Finalize.
class TitleCloudTransition {
public:
    TitleCloudTransition();
    ~TitleCloudTransition();
    enum class Phase { Idle, Entering, Covered, Waiting, Exiting };
    struct LayerSettings {
        float enterDelay, exitDelay, width;
        Vector2 start, covered, end;
        float rotation;
    };
    struct Settings {
        float enterSeconds = 1.5714286f;
        float holdSeconds = 0.4571429f;
        float exitSeconds = 1.8571429f;
        float edgeFade = 0.08f;
        float backingEnterStart = 0.78f;
        float backingExitEnd = 0.42f;
        // Drawing order: cloud 2, cloud 1, cloud 3. No image is flipped.
        std::array<LayerSettings, 3> layers{{
            {0.1286f, 0.1000f, 1.36f, {0.46f, 1.95f}, {0.51f, 0.52f}, {0.56f, 1.95f}, 0.0f},
            {0.2571f, 0.0000f, 1.52f, {0.52f, 1.95f}, {0.50f, 0.50f}, {0.46f, 1.95f}, 0.0f},
            {0.0000f, 0.2143f, 1.42f, {0.43f, 2.00f}, {0.52f, 0.52f}, {0.59f, 2.00f}, 0.025f}
        }};
    };
    struct LayerPose { Vector2 center, size; float rotation; };
    void Initialize(SpriteCommon* common);
    bool Start();
    void Cancel();
    void Update(float dt);
    void Draw();
    void OnFramePresented();
    void OnSceneChanged();
    bool CanSwitch() const;
    bool IsCovering() const;
    bool IsActive() const { return phase_ != Phase::Idle; }
    Phase GetPhase() const { return phase_; }
    float GetBackingAlpha() const;
    LayerPose GetLayerPose(size_t layer, Vector2 viewportSize) const;
    bool HasCloudImages() const { return imagesAvailable_; }
    static Settings& GetSettings();
    static bool LoadSettings(const char* path);
    static bool SaveSettings(const char* path);
    static void ShowSettings();
    bool DrawImGui(); // Test request; does not exchange scenes.
    bool CanFinishTestLoad() const;
private:
    Phase phase_ = Phase::Idle;
    double elapsed_ = 0.0;
    bool coveredPresented_ = false;
    bool newScenePresented_ = false;
    bool coveredDrawn_ = false;
    bool clearDrawn_ = false;
    bool imagesAvailable_ = false;
    Settings activeSettings_;
    SpriteCommon* common_ = nullptr;
    std::array<std::unique_ptr<Sprite>, 3> clouds_;
    std::unique_ptr<Sprite> backing_;
};
