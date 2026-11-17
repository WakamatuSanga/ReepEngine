#include "StageClearResultController.h"

#include "Engine/Core/GameViewport.h"
#include "Engine/Core/SrvManager.h"
#include "Engine/Graphics/Sprite/Sprite.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Input/Input.h"
#include <algorithm>
#include <string>

namespace {
constexpr float kWidth = 1600.0f;
constexpr float kHeight = 900.0f;
constexpr float kHeadingFade = 0.28f;
constexpr float kPanelStart = 0.28f;
constexpr float kPanelFade = 0.28f;
constexpr float kButtonsStart = 0.56f;
constexpr float kButtonsFade = 0.32f;

struct Rect { float x, y, width, height; };
constexpr std::array<Rect, 3> kButtonRects{{
    {90, 764, 265, 72}, {371, 764, 242, 72}, {1021, 729, 491, 107},
}};

struct Layout {
    explicit Layout(Vector2 canvasSize) : canvas(canvasSize),
        scale((std::min)(canvas.x / kWidth, canvas.y / kHeight)),
        offset{(canvas.x - kWidth * scale) * 0.5f, (canvas.y - kHeight * scale) * 0.5f} {}

    Vector2 canvas;
    float scale;
    Vector2 offset;
};

float Fade(float elapsed, float start, float time) {
    const float t = std::clamp((elapsed - start) / time, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

std::unique_ptr<Sprite> MakeSprite(SpriteCommon* common, const char* texture) {
    auto sprite = std::make_unique<Sprite>();
    sprite->Initialize(common);
    if (texture) sprite->SetTexture(texture);
    sprite->SetTextureSampling(true);
    return sprite;
}

void DrawSprite(Sprite& sprite, const Layout& layout, Rect rect, Vector4 color) {
    sprite.SetCanvasSize(layout.canvas);
    sprite.SetPosition({layout.offset.x + rect.x * layout.scale, layout.offset.y + rect.y * layout.scale});
    sprite.SetSize({rect.width * layout.scale, rect.height * layout.scale});
    sprite.SetColor(color);
    sprite.Update();
    sprite.Draw();
}
}

StageClearResultController::StageClearResultController() = default;
StageClearResultController::~StageClearResultController() = default;

void StageClearResultController::Initialize(SpriteCommon* sprites, GameViewport* viewport, Input* input) {
    sprites_ = sprites;
    viewport_ = viewport;
    input_ = input;
    heading_ = MakeSprite(sprites, "resources/ui/stage_clear/clear-heading.png");
    logo_ = MakeSprite(sprites, "resources/ui/stage_clear/stage1_clear.png");
    logo_->SetTextureLeftTop({68, 222});
    logo_->SetTextureSize({2076, 264});
    panel_ = MakeSprite(sprites, "resources/ui/stage_clear/clear-score-panel.png");
    unit_ = MakeSprite(sprites, "resources/ui/stage_clear/clear-kill-unit.png");
    const char* buttonPaths[]{"resources/ui/stage_clear/clear-retry.png",
        "resources/ui/stage_clear/clear-return-title.png", "resources/ui/stage_clear/clear-next-stage.png"};
    for (size_t i = 0; i < buttons_.size(); ++i) buttons_[i] = MakeSprite(sprites, buttonPaths[i]);
    auto* textures = TextureManager::GetInstance();
    for (size_t i = 0; i < digitTextures_.size(); ++i) {
        const std::string path = "resources/ui/hud/parts/digits-score/" + std::to_string(i) + ".png";
        textures->LoadTexture(path);
        digitTextures_[i] = textures->GetTextureIndexByFilePath(path);
    }
    for (auto& digit : digits_) digit = MakeSprite(sprites, nullptr);
}

void StageClearResultController::Begin(int score, std::uint32_t defeats) {
    score_ = std::clamp(score, 0, 99999);
    defeats_ = defeats;
    elapsed_ = 0;
    selected_ = 0;
    hovered_ = -1;
    keyboardSelection_ = false;
    previousMouse_ = {-1, -1};
    buttonsDrawn_ = buttonsPresented_ = inputArmed_ = false;
}

StageClearResultController::Choice StageClearResultController::Update(float deltaTime) {
    elapsed_ = (std::min)(kButtonsStart + kButtonsFade, elapsed_ + deltaTime);
    if (!buttonsPresented_ || !viewport_->IsGameWindowActive()) {
        inputArmed_ = false;
        hovered_ = -1;
        keyboardSelection_ = false;
        return Choice::None;
    }
    return UpdateSelection(viewport_->GetMouseScreenPosition(), viewport_->IsMouseInGameViewport(), viewport_->GetClientSize());
}

StageClearResultController::Choice StageClearResultController::UpdateSelection(Vector2 mouse, bool insideViewport, Vector2 clientSize) {
    const bool moved = mouse.x != previousMouse_.x || mouse.y != previousMouse_.y;
    previousMouse_ = mouse;
    const Layout layout(clientSize);
    int hovered = -1;
    if (insideViewport && layout.scale > 0.0f) {
        const Vector2 point{(mouse.x - layout.offset.x) / layout.scale, (mouse.y - layout.offset.y) / layout.scale};
        for (size_t i = 0; i < kButtonRects.size(); ++i) {
            const auto& rect = kButtonRects[i];
            if (point.x >= rect.x && point.x < rect.x + rect.width && point.y >= rect.y && point.y < rect.y + rect.height) {
                hovered = static_cast<int>(i);
                break;
            }
        }
    }
    if (moved || hovered != hovered_) {
        keyboardSelection_ = false;
        if (hovered >= 0) selected_ = hovered;
    }
    hovered_ = hovered;
    if (!inputArmed_) {
        inputArmed_ = input_->IsMouseStateValid() && !input_->MouseDown(Input::MouseLeft) &&
            !input_->MouseDown(Input::MouseRight) && !input_->PushKey(DIK_RETURN) && !input_->PushKey(DIK_SPACE);
        return Choice::None;
    }
    if (hovered_ >= 0 && input_->IsMouseStateValid() && input_->MouseTrigger(Input::MouseLeft)) {
        keyboardSelection_ = false;
        selected_ = hovered_;
        return static_cast<Choice>(selected_ + 1);
    }
    const int count = static_cast<int>(buttons_.size());
    if (input_->TriggerKey(DIK_LEFT) || input_->TriggerKey(DIK_A)) {
        selected_ = (selected_ + count - 1) % count;
        keyboardSelection_ = true;
    }
    if (input_->TriggerKey(DIK_RIGHT) || input_->TriggerKey(DIK_D)) {
        selected_ = (selected_ + 1) % count;
        keyboardSelection_ = true;
    }
    return input_->TriggerKey(DIK_RETURN) || input_->TriggerKey(DIK_SPACE)
        ? static_cast<Choice>(selected_ + 1) : Choice::None;
}

void StageClearResultController::Draw() {
    auto* dx = sprites_->GetDxCommon();
    const auto viewport = dx->GetBackBufferViewport();
    const auto scissor = dx->GetBackBufferScissorRect();
    const Layout layout({viewport.Width, viewport.Height});
    buttonsDrawn_ = false;
    if (layout.scale <= 0.0f) return;
    auto* command = dx->GetCommandList();
    command->RSSetViewports(1, &viewport);
    command->RSSetScissorRects(1, &scissor);
    SrvManager::GetInstance()->PreDraw();
    sprites_->CommonDrawSetting(true);

    const float headingAlpha = Fade(elapsed_, 0, kHeadingFade);
    DrawSprite(*heading_, layout, {89, 171, 565, 206}, {1, 1, 1, headingAlpha});
    DrawSprite(*logo_, layout, {84, 241, 653, 653 * 264.0f / 2076.0f}, {1, 1, 1, headingAlpha});

    const float panelAlpha = Fade(elapsed_, kPanelStart, kPanelFade);
    if (panelAlpha > 0.0f) {
        DrawSprite(*panel_, layout, {90, 420, 581, 142}, {1, 1, 1, panelAlpha});
        size_t slot = 0;
        auto drawNumber = [&](const std::string& number, float x, float advance, float height) {
            for (char digit : number) {
                Sprite& sprite = *digits_[slot++];
                const auto texture = digitTextures_[digit - '0'];
                const auto& metadata = TextureManager::GetInstance()->GetMetaData(texture);
                sprite.SetTextureIndex(texture);
                sprite.SetTextureSize({static_cast<float>(metadata.width), static_cast<float>(metadata.height)});
                const float width = height * static_cast<float>(metadata.width) / static_cast<float>(metadata.height);
                DrawSprite(sprite, layout, {x, 519 - height, width, height}, {1, 1, 1, panelAlpha});
                x += advance;
            }
            return x;
        };
        std::string score = std::to_string(score_);
        score.insert(0, 5 - score.size(), '0');
        drawNumber(score, 115, 30, 48);
        const std::string kills = std::to_string(defeats_);
        const float advance = (std::min)(32.0f, 215.0f / static_cast<float>(kills.size()));
        const float x = drawNumber(kills, 398, advance, 48 * advance / 32);
        DrawSprite(*unit_, layout, {x + 13, 501, 26, 27}, {1, 1, 1, panelAlpha});
    }

    const float buttonsAlpha = Fade(elapsed_, kButtonsStart, kButtonsFade);
    if (buttonsAlpha <= 0.0f) return;
    const int highlighted = keyboardSelection_ ? selected_ : hovered_;
    for (size_t i = 0; i < buttons_.size(); ++i) {
        buttons_[i]->SetTextureDarkPanel(static_cast<int>(i) == highlighted);
        DrawSprite(*buttons_[i], layout, kButtonRects[i], {1, 1, 1, buttonsAlpha});
    }
    buttonsDrawn_ = elapsed_ >= kButtonsStart + kButtonsFade;
}

void StageClearResultController::OnFramePresented() {
    buttonsPresented_ |= buttonsDrawn_;
}
