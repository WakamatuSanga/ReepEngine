#pragma once

#include "Engine/math/Matrix4x4.h"
#include <array>
#include <cstdint>
#include <memory>

class GameViewport;
class Input;
class Sprite;
class SpriteCommon;

class StageClearResultController {
public:
    enum class Choice { None, Retry, Title, NextStage };

    StageClearResultController();
    ~StageClearResultController();
    void Initialize(SpriteCommon* sprites, GameViewport* viewport, Input* input);
    void Begin(int score, std::uint32_t defeats);
    Choice Update(float deltaTime);
    void Draw();
    void OnFramePresented();

private:
    Choice UpdateSelection(Vector2 mouse, bool insideViewport, Vector2 clientSize);
    SpriteCommon* sprites_ = nullptr;
    GameViewport* viewport_ = nullptr;
    Input* input_ = nullptr;
    std::unique_ptr<Sprite> heading_, logo_, panel_, unit_;
    std::array<std::unique_ptr<Sprite>, 3> buttons_;
    std::array<std::unique_ptr<Sprite>, 15> digits_;
    std::array<std::uint32_t, 10> digitTextures_{};
    int score_ = 0;
    std::uint32_t defeats_ = 0;
    float elapsed_ = 0.0f;
    Vector2 previousMouse_{-1.0f, -1.0f};
    int selected_ = 0;
    int hovered_ = -1;
    bool keyboardSelection_ = false;
    bool buttonsDrawn_ = false;
    bool buttonsPresented_ = false;
    bool inputArmed_ = false;
};
