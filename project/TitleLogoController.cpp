#include "TitleLogoController.h"
#include "Engine/Graphics/Sprite/Sprite.h"
#include "Engine/Core/SrvManager.h"
#include "Engine/Core/WinApp.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr double kTwoPi = 6.283185307179586;
    constexpr float kDegrees = 0.01745329252f;
    constexpr double kFadeSeconds = 0.4;
    constexpr Vector2 kJoint{ 440.0f, -125.0f };
    constexpr Vector2 kPivot{ 9.0f, -11.0f };
    constexpr Vector2 kAircraftAnchor{ 235.0f / 1536.0f, 850.0f / 1024.0f };
    constexpr Vector2 kTrailAnchor{ 2015.0f / 2048.0f, 76.0f / 768.0f };
    Vector2 Add(Vector2 a, Vector2 b) { return { a.x + b.x, a.y + b.y }; }
    Vector2 Scale(Vector2 v, float s) { return { v.x * s, v.y * s }; }
    Vector2 Rotate(Vector2 v, float angle) {
        const float c = std::cos(angle), s = std::sin(angle);
        return { v.x * c - v.y * s, v.x * s + v.y * c };
    }
    float Wave(double time, double period) {
        return static_cast<float>(std::sin(std::remainder(time, period) * kTwoPi / period));
    }
}

TitleLogoController::TitleLogoController() = default;
TitleLogoController::~TitleLogoController() = default;

void TitleLogoController::Initialize(SpriteCommon* common) {
    common_ = common;
    elapsed_ = 0.0;
    fadeElapsed_ = -1.0;
    constexpr const char* paths[]{ "resources/ui/titleLogo/Title_Contrail.png",
        "resources/ui/titleLogo/Title.png", "resources/ui/titleLogo/Title_Silhouette.png" };
    for (size_t i = 0; i < sprites_.size(); ++i) {
        sprites_[i] = std::make_unique<Sprite>();
        sprites_[i]->Initialize(common_);
        sprites_[i]->SetTexture(paths[i]);
    }
    // Trim only empty margins via UVs; keep the supplied artwork and aspect ratio.
    sprites_[1]->SetTextureLeftTop({ 104.0f, 96.0f });
    sprites_[1]->SetTextureSize({ 1736.0f, 568.0f });
    Sprite::WhiteGradientSettings lettering;
    lettering.enabled = true;
    lettering.startColor = { 1.0f, 1.0f, 1.0f };
    lettering.endColor = { 0.015f, 0.14f, 0.40f }; // Linear sea blue.
    lettering.startPoint = { 0.5f, 0.10f };
    lettering.endPoint = { 0.5f, 0.95f };
    lettering.strength = 0.85f;
    lettering.brightnessThreshold = 0.50f;
    lettering.saturationThreshold = 0.50f;
    lettering.brightnessSoftness = 0.20f;
    lettering.saturationSoftness = 0.20f;
    sprites_[1]->SetWhiteGradient(lettering);
    startPrompt_ = std::make_unique<Sprite>();
    startPrompt_->Initialize(common_);
    startPrompt_->SetTexture("resources/ui/titleLogo/ClicktoStart.png");
}

void TitleLogoController::Update(float deltaTime, bool starting) {
    const double dt = std::isfinite(deltaTime) ? (std::max)(0.0, static_cast<double>(deltaTime)) : 0.0;
    elapsed_ += dt;
    // First click frame retains the current alpha; subsequent frames fade without resetting motion.
    if (starting && fadeElapsed_ < 0.0) fadeElapsed_ = 0.0;
    else if (fadeElapsed_ >= 0.0) fadeElapsed_ = (std::min)(kFadeSeconds, fadeElapsed_ + dt);
}

std::array<TitleLogoController::LayerPose, 3> TitleLogoController::CalculateLayout(
    double time, double fadeElapsed, float width, float height) {
    const float scale = (std::max)(0.0001f, (std::min)(width * 0.60f / 1320.0f, height * 0.48f / 550.0f));
    const float pixels = height / 1080.0f;
    const Vector2 root{ width * 0.525f, height * 0.29f + 4.0f * pixels * Wave(time, 6.0) };
    const float angle = 0.6f * kDegrees * Wave(time, 10.0);
    const float planeAngle = 0.3f * kDegrees * Wave(time, 3.8);
    const Vector2 planeSize{ 260.0f, 260.0f * 1024.0f / 1536.0f };
    const Vector2 anchorFromCenter{ (kAircraftAnchor.x - 0.5f) * planeSize.x,
        (kAircraftAnchor.y - 0.5f) * planeSize.y };
    const Vector2 planeCenter{ kJoint.x - anchorFromCenter.x,
        kJoint.y - anchorFromCenter.y + 2.0f * pixels / scale * Wave(time, 3.8) };
    // Recompute the shared attachment after the aircraft's own bob/rotation.
    const Vector2 joint = Add(planeCenter, Rotate(anchorFromCenter, planeAngle));
    const auto screen = [&](Vector2 local) {
        return Add(root, Rotate(Scale({ local.x - kPivot.x, local.y - kPivot.y }, scale), angle));
    };
    const float breathing = Wave(time, 5.2);
    const Vector2 trailSize{ 1100.0f * scale, 1100.0f * 768.0f / 2048.0f * scale * (1.0f + 0.01f * breathing) };
    const float fadeT = static_cast<float>(std::clamp(fadeElapsed / kFadeSeconds, 0.0, 1.0));
    const float fade = 1.0f - fadeT * fadeT * (3.0f - 2.0f * fadeT);
    // Visible text bounds in the original PNG: X=112..1828; crop begins at 104.
    // Center the ink, not the padded texture rectangle; keep its height/size unchanged.
    const float textLeft = kPivot.x - ((112.0f + 1828.0f) * 0.5f - 104.0f) / 1736.0f * 1060.0f
        - width * 0.025f / scale;
    return {{
        { Add(screen(joint), Rotate({ -kTrailAnchor.x * trailSize.x, -kTrailAnchor.y * trailSize.y }, angle)),
          trailSize, angle, (0.975f + 0.025f * breathing) * fade },
        { screen({ textLeft, -180.0f }), { 1060.0f * scale, 1060.0f * 568.0f / 1736.0f * scale }, angle, fade },
        { Add(screen(planeCenter), Rotate(Scale(planeSize, -0.5f * scale), angle + planeAngle)),
          Scale(planeSize, scale), angle + planeAngle, fade }
    }};
}

TitleLogoController::LayerPose TitleLogoController::CalculateStartPromptLayout(
    float width, float height, float opacity) {
    // Visible ink (alpha >= 16): [102, 2072) x [218, 515) in the 2172 x 724 PNG.
    // Keep the entire original texture, including faint edge alpha outside these bounds.
    // Cap the height at the 16:9 composition so ultrawide screens do not crowd the aircraft.
    const float scale = (std::min)(width * 0.24f, height * (16.0f / 9.0f) * 0.24f) / 1970.0f;
    return { { width * 0.5f - 1087.0f * scale, height * 0.86f - 366.5f * scale },
        { 2172.0f * scale, 724.0f * scale }, 0.0f, opacity };
}

void TitleLogoController::Draw() {
    if (!common_ || fadeElapsed_ >= kFadeSeconds) return;
    auto* dx = common_->GetDxCommon();
    auto* cmd = dx->GetCommandList();
    const auto backBuffer = dx->GetBackBufferViewport();
    if (backBuffer.Width <= 0.0f || backBuffer.Height <= 0.0f) return;
    // Sprite uses a fixed logical canvas. A uniformly scaled viewport preserves
    // image proportions and rotations at any aspect ratio; scissor stays at the actual screen.
    const float width = static_cast<float>(WinApp::kClientWidth);
    const float height = backBuffer.Height * width / backBuffer.Width;
    auto viewport = backBuffer;
    viewport.Height = backBuffer.Width * static_cast<float>(WinApp::kClientHeight) / width;
    const auto scissor = dx->GetBackBufferScissorRect();
    cmd->RSSetViewports(1, &viewport);
    cmd->RSSetScissorRects(1, &scissor);
    SrvManager::GetInstance()->PreDraw();
    common_->CommonDrawSetting();
    const auto layout = CalculateLayout(elapsed_, fadeElapsed_, width, height);
    // Title-only linear RGB tints. Alpha remains 1 for text/aircraft until the click fade;
    // PNG edge alpha and the original assets are untouched. No scene exposure change.
    constexpr Vector3 tints[]{ { 1, 1, 1 }, { 1, 1, 1 }, { 0.62f, 0.70f, 0.81f } };
    for (size_t i = 0; i < sprites_.size(); ++i) {
        sprites_[i]->SetPosition(layout[i].position);
        sprites_[i]->SetSize(layout[i].size);
        sprites_[i]->SetRotation(layout[i].rotation);
        sprites_[i]->SetColor({ tints[i].x, tints[i].y, tints[i].z, layout[i].opacity });
        sprites_[i]->Update();
        sprites_[i]->Draw();
    }
    const auto prompt = CalculateStartPromptLayout(width, height, layout[1].opacity);
    startPrompt_->SetPosition(prompt.position);
    startPrompt_->SetSize(prompt.size);
    startPrompt_->SetColor({ 1.0f, 1.0f, 1.0f, prompt.opacity });
    startPrompt_->Update();
    startPrompt_->Draw();
    cmd->RSSetViewports(1, &backBuffer);
}
