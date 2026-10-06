#pragma once
#include "Engine/math/Matrix4x4.h"
#include <array>
#include <memory>

class Sprite;
class SpriteCommon;

class TitleLogoController {
public:
    struct LayerPose {
        Vector2 position;
        Vector2 size;
        float rotation;
        float opacity;
    };
    TitleLogoController();
    ~TitleLogoController();
    void Initialize(SpriteCommon* common);
    void Update(float deltaTime, bool starting);
    void Draw();
    // Pure title layout, in the Sprite canvas coordinate system; trail/text/aircraft order.
    static std::array<LayerPose, 3> CalculateLayout(double time, double fadeElapsed,
        float width, float height);
    static LayerPose CalculateStartPromptLayout(float width, float height, float opacity);
private:
    SpriteCommon* common_ = nullptr; // Owned by MyGame.
    std::array<std::unique_ptr<Sprite>, 3> sprites_;
    std::unique_ptr<Sprite> startPrompt_;
    double elapsed_ = 0.0;
    double fadeElapsed_ = -1.0;
};
