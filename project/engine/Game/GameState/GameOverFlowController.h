#pragma once
#include <memory>
class PlayerDeathSequenceController;
class SpriteCommon;
class GameViewport;
class Input;

// Product Sprite menu inside GameScene; owns no gameplay/death state.
class GameOverFlowController {
public:
    struct Settings {
        bool glitchEnabled = true;
        float titleCenterX = .5f;
        float titleCenterY = .42f;
        float titleWidthFraction = .76f;
        float introSeconds = .6f;
        float faultIntervalMin = 1.4f;
        float faultIntervalMax = 2.8f;
        float faultSeconds = .1f;
        bool screenNoiseEnabled = true;
        float screenNoiseStrength = .025f;
        float screenNoiseGrainPixels = 2;
        bool titleNoiseEnabled = true;
        float titleNoiseStrength = .22f;
    };
    static Settings& GetTuning();
    GameOverFlowController();
    ~GameOverFlowController();
    void Initialize(PlayerDeathSequenceController*, SpriteCommon*, GameViewport*, Input*);
    bool PrepareForPresentation();
    void Finalize();
    void Update();
    void DrawOverlay();
    void DrawImGui();
    bool IsVisible() const;
    bool IsInputArmed() const;
private:
    struct View;
    std::unique_ptr<View> view_;
};
