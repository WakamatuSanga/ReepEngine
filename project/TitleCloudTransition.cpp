#include "TitleCloudTransition.h"
#include "MyGame.h"
#include "Engine/Graphics/Sprite/Sprite.h"
#include <algorithm>

namespace {
    constexpr float kCoverSeconds = 0.7f;
    constexpr float kRevealSeconds = 0.6f;
    constexpr float kWidth = static_cast<float>(WinApp::kClientWidth);
    constexpr float kHeight = static_cast<float>(WinApp::kClientHeight);
    constexpr std::array<float, 9> kRadii{ 125, 155, 112, 168, 138, 180, 120, 158, 135 };
    float Smooth(float t) { return t * t * (3.0f - 2.0f * t); }
}

TitleCloudTransition::TitleCloudTransition() = default;
TitleCloudTransition::~TitleCloudTransition() = default;

void TitleCloudTransition::Initialize() {
    auto* common = MyGame::GetInstance()->GetSpriteCommon();
    body_ = std::make_unique<Sprite>();
    body_->Initialize(common);
    body_->SetTexture("resources/human/white.png");
    body_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    for (size_t i = 0; i < lobes_.size(); ++i) {
        lobes_[i] = std::make_unique<Sprite>();
        lobes_[i]->Initialize(common);
        lobes_[i]->SetTexture("resources/ui/titleTransitionCircle.png");
        lobes_[i]->SetColor((i / kLobes) % 2 == 0 ? Vector4{ 0.83f, 0.94f, 1.0f, 1.0f }
                                      : Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
    }
}

void TitleCloudTransition::Update(float dt) {
    if (phase_ != Phase::Cover && phase_ != Phase::Reveal) return;
    elapsed_ += (std::max)(0.0f, dt);
    const float duration = phase_ == Phase::Cover ? kCoverSeconds : kRevealSeconds;
    if (elapsed_ >= duration) {
        phase_ = phase_ == Phase::Cover ? Phase::Covered : Phase::Finished;
        elapsed_ = 0.0f;
        presented_ = false;
    }
}

void TitleCloudTransition::OnFramePresented() {
    presented_ = true;
    if (phase_ == Phase::Loading) {
        phase_ = Phase::Reveal;
        elapsed_ = 0.0f;
        presented_ = false;
    }
}
void TitleCloudTransition::OnSceneChanged() {
    phase_ = Phase::Loading;
    elapsed_ = 0.0f;
    presented_ = false;
}
bool TitleCloudTransition::CanSwitch() const { return phase_ == Phase::Covered && presented_; }
bool TitleCloudTransition::IsCovering() const { return phase_ == Phase::Cover; }
bool TitleCloudTransition::IsFinished() const { return phase_ == Phase::Finished && presented_; }

void TitleCloudTransition::Draw() {
    if (phase_ == Phase::Finished) return;
    auto* game = MyGame::GetInstance();
    auto* dx = game->GetDxCommon();
    auto* cmd = dx->GetCommandList();
    // Sprite's logical canvas maps onto the complete backbuffer, not the docked Game View.
    const auto viewport = dx->GetBackBufferViewport();
    const auto scissor = dx->GetBackBufferScissorRect();
    cmd->RSSetViewports(1, &viewport);
    cmd->RSSetScissorRects(1, &scissor);
    SrvManager::GetInstance()->PreDraw();
    game->GetSpriteCommon()->CommonDrawSetting();

    // Opaque body starts at the circle centres: no holes below the rounded crest.
    float top = -200.0f;
    if (phase_ == Phase::Cover) {
        top = kHeight + 210.0f - (kHeight + 410.0f) * Smooth(elapsed_ / kCoverSeconds);
    } else if (phase_ == Phase::Reveal) {
        // Include the downward lobes (maximum radius 180) in the exit distance.
        top -= (kHeight + 214.0f) * Smooth(elapsed_ / kRevealSeconds);
    }
    for (size_t i = 0; i < lobes_.size(); ++i) {
        const size_t lobe = i % kLobes;
        const float radius = kRadii[lobe];
        const float x = kWidth * static_cast<float>(lobe) / static_cast<float>(kLobes - 1);
        const bool bottom = i >= kLobes * 2;
        const bool white = (i / kLobes) % 2 != 0;
        // White inset points into the opaque body on both sides. Neighbouring
        // discs overlap at the body edge so its straight edge is never exposed.
        const float center = bottom ? top + kHeight + 232.0f : top;
        const float inset = white ? (bottom ? -12.0f : 12.0f) : 0.0f;
        lobes_[i]->SetPosition({ x - radius, center - radius + inset });
        lobes_[i]->SetSize({ radius * 2.0f, radius * 2.0f });
        lobes_[i]->Update();
        lobes_[i]->Draw();
    }
    body_->SetPosition({ -2.0f, top + 12.0f });
    body_->SetSize({ kWidth + 4.0f, kHeight + 220.0f });
    body_->Update();
    body_->Draw();
}
