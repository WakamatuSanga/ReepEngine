#include "StageStartSequenceController.h"
#include "Engine/Game/Camera/RailShooterCameraRig.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Game/Player/PlayerJetExhaustController.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Sprite/Sprite.h"
#include "Engine/Graphics/Sprite/SpriteCommon.h"
#include <algorithm>
#include <cmath>
#include <limits>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
StageStartSequenceController::Settings tuning;
float Smooth(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
float Dot(Vector3 a, Vector3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
Vector3 Add(Vector3 a, Vector3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
Vector3 Scale(Vector3 a, float t) { return {a.x*t, a.y*t, a.z*t}; }
Vector3 Axis(const Matrix4x4& m, int row) { return {m.m[row][0], m.m[row][1], m.m[row][2]}; }
Vector3 Rotate(Vector3 p, const Matrix4x4& m) {
    return Add(Add(Scale(Axis(m, 0), p.x), Scale(Axis(m, 1), p.y)), Scale(Axis(m, 2), p.z));
}
Vector3 EulerXYZ(const Matrix4x4& m) {
    const float y = std::asin(std::clamp(-m.m[0][2], -1.0f, 1.0f));
    if (std::fabs(std::cos(y)) > 0.0001f)
        return {std::atan2(m.m[1][2], m.m[2][2]), y, std::atan2(m.m[0][1], m.m[0][0])};
    return {std::atan2(-m.m[2][1], m.m[1][1]), y, 0.0f};
}
}

StageStartSequenceController::Settings& StageStartSequenceController::GetTuning() { return tuning; }
StageStartSequenceController::StageStartSequenceController() = default;
StageStartSequenceController::~StageStartSequenceController() = default;

void StageStartSequenceController::Initialize(SpriteCommon* sprites, Camera* camera, Player* player,
    RailShooterCameraRig* rig, PlayerJetExhaustController* exhaust) {
    sprites_ = sprites;
    camera_ = camera;
    player_ = player;
    rig_ = rig;
    exhaust_ = exhaust;
    settings_ = GetTuning();
    settings_.crossingSeconds = std::clamp(settings_.crossingSeconds, 0.25f, 5.0f);
    settings_.gapSeconds = std::clamp(settings_.gapSeconds, 0.05f, 1.0f);
    settings_.entrySeconds = std::clamp(settings_.entrySeconds, 0.25f, 5.0f);
    settings_.crossingWidth = std::clamp(settings_.crossingWidth, 0.20f, 0.55f);
    settings_.crossingDistanceScale = std::clamp(settings_.crossingDistanceScale, 0.7f, 2.0f);
    settings_.exitMargin = std::clamp(settings_.exitMargin, 0.10f, 0.5f);
    settings_.entryBelowScreen = std::clamp(settings_.entryBelowScreen, 1.05f, 2.0f);
    settings_.entryCameraLift = std::clamp(settings_.entryCameraLift, 0.0f, 0.3f);

    const char* paths[]{"resources/ui/stage_start/Stage_1.png", "resources/ui/stage_start/Stage_Logo_1.png"};
    const Vector2 origins[]{{345,221},{131,187}};
    const Vector2 sizes[]{{1444,289},{1910,366}};
    for (size_t i = 0; i < titles_.size(); ++i) {
        titles_[i] = std::make_unique<Sprite>();
        titles_[i]->Initialize(sprites_);
        titles_[i]->SetTexture(paths[i]);
        titles_[i]->SetTextureLeftTop(origins[i]);
        titles_[i]->SetTextureSize(sizes[i]);
        titles_[i]->SetTextureSampling(true);
    }
    shade_ = std::make_unique<Sprite>();
    shade_->Initialize(sprites_);
    shade_->SetTexture("resources/human/white.png");
    cameraTranslationOffset_ = camera_->GetViewTranslationOffset();
    cameraRotationOffset_ = camera_->GetViewRotationOffset();
    cameraFovY_ = camera_->GetFovY();
    referenceFrame_ = camera_->GetViewWorldMatrix();
    normalPose_ = player_->GetVisualTransform();

    // The loaded player.obj has +Z nose and +Y upper surface. Keep its existing
    // model alignment, then place the flight basis at left / toward the camera.
    Matrix4x4 top = MatrixMath::MakeIdentity4x4();
    const Vector3 axes[]{Axis(referenceFrame_, 1), Scale(Axis(referenceFrame_, 2), -1),
        Scale(Axis(referenceFrame_, 0), -1)};
    for (int row = 0; row < 3; ++row) {
        top.m[row][0] = axes[row].x;
        top.m[row][1] = axes[row].y;
        top.m[row][2] = axes[row].z;
    }
    const auto normalRotation = MatrixMath::MakeAffine({1,1,1}, normalPose_.rotate, {});
    const auto flightRotation = MatrixMath::MakeAffine({1,1,1}, player_->GetVisualBaseRotation(), {});
    crossingRotation_ = EulerXYZ(MatrixMath::Multipty(
        MatrixMath::Multipty(normalRotation, MatrixMath::Inverse(flightRotation)), top));
    Vector3 minimum{}, maximum{};
    player_->GetVisualModelBounds(minimum, maximum);
    for (size_t i = 0; i < modelCorners_.size(); ++i) {
        modelCorners_[i] = {(i & 1 ? maximum.x : minimum.x) * normalPose_.scale.x,
            (i & 2 ? maximum.y : minimum.y) * normalPose_.scale.y,
            (i & 4 ? maximum.z : minimum.z) * normalPose_.scale.z};
    }
    phase_ = Phase::AwaitingReveal;
    elapsed_ = 0;
    paused_ = false;
    rig_->SetStageStartPresentationActive(true);
    ApplyDisplayPose();
}

void StageStartSequenceController::OnPresentationRevealed() {
    if (phase_ != Phase::AwaitingReveal) return;
    phase_ = Phase::FadeIn;
    elapsed_ = 0;
}

bool StageStartSequenceController::IsAircraftVisible() const {
    return phase_ == Phase::Crossing || phase_ == Phase::CrossingExit ||
        phase_ == Phase::Entry || phase_ == Phase::Settle;
}

void StageStartSequenceController::Update(float deltaTime) {
    if (phase_ == Phase::AwaitingReveal || phase_ == Phase::Complete) return;
    const float dt = !paused_ && std::isfinite(deltaTime) ? std::clamp(deltaTime, 0.0f, 0.1f) : 0.0f;
    const float duration = phase_ == Phase::FadeIn ? kFadeInSeconds : phase_ == Phase::Title ? kTitleSeconds :
        phase_ == Phase::FadeOut ? kFadeOutSeconds : phase_ == Phase::Crossing ? settings_.crossingSeconds :
        phase_ == Phase::Gap ? settings_.gapSeconds : settings_.entrySeconds;
    if (phase_ != Phase::Settle && phase_ != Phase::CrossingExit) {
        elapsed_ += dt;
        if (elapsed_ >= duration) {
            if (phase_ == Phase::Crossing) {
                phase_ = Phase::CrossingExit; // Draw the entire exhaust beyond the edge before changing pose.
            } else {
                phase_ = phase_ == Phase::FadeIn ? Phase::Title : phase_ == Phase::Title ? Phase::FadeOut :
                    phase_ == Phase::FadeOut ? Phase::Crossing : phase_ == Phase::Gap ? Phase::Entry : Phase::Settle;
            }
            elapsed_ = 0; // Late/load frames never skip a whole visual phase.
        }
    }
    ApplyDisplayPose();
    if (IsAircraftVisible() && exhaust_) {
        const auto pose = player_->GetVisualTransform();
        // Reuse the title's nozzle coordinate path for the lateral pass only.
        // Its bounded flame follows this one aircraft; normal entry uses world particles.
        const bool lateral = phase_ == Phase::Crossing || phase_ == Phase::CrossingExit;
        exhaust_->UpdateDisplay(dt, pose, normalPose_.scale.x, 0, 1, lateral);
    }
}

void StageStartSequenceController::SetCameraLift(float lift) {
    camera_->SetViewTranslationOffset(Add(cameraTranslationOffset_, Scale(Axis(referenceFrame_, 1), lift)));
    camera_->SetViewRotationOffset(cameraRotationOffset_);
    camera_->SetFovY(cameraFovY_);
    camera_->Update();
}

void StageStartSequenceController::ApplyDisplayPose() {
    if (phase_ == Phase::Crossing || phase_ == Phase::CrossingExit) {
        SetCameraLift(0);
        ApplyCrossingPose(phase_ == Phase::CrossingExit ? 1.0f : elapsed_ / settings_.crossingSeconds);
    } else if (phase_ == Phase::Gap || phase_ == Phase::Entry || phase_ == Phase::Settle) {
        ApplyEntryPose(phase_ == Phase::Settle ? 1.0f : phase_ == Phase::Gap ? 0.0f : elapsed_ / settings_.entrySeconds);
    } else {
        SetCameraLift(0);
        player_->ClearDisplayPose();
    }
}

void StageStartSequenceController::ApplyCrossingPose(float progress) {
    const auto right = Axis(referenceFrame_, 0), up = Axis(referenceFrame_, 1), forward = Axis(referenceFrame_, 2);
    const auto rotation = MatrixMath::MakeAffine({1,1,1}, crossingRotation_, {});
    Vector3 center{};
    for (const auto& corner : modelCorners_) center = Add(center, Scale(corner, 1.0f / 8));
    center = Rotate(center, rotation);
    Vector3 radius{};
    for (const auto& corner : modelCorners_) {
        const auto offset = Add(Rotate(corner, rotation), Scale(center, -1));
        radius.x = (std::max)(radius.x, std::fabs(Dot(offset, right)));
        radius.y = (std::max)(radius.y, std::fabs(Dot(offset, up)));
        radius.z = (std::max)(radius.z, std::fabs(Dot(offset, forward)));
    }
    const float tanY = std::tan(cameraFovY_ * 0.5f);
    const float tanX = tanY * camera_->GetAspectRatio();
    // Size by real projected extent and depth, with 22% vertical safety for the wings.
    crossingDepth_ = radius.z + (std::max)(radius.x / (tanX * settings_.crossingWidth) * settings_.crossingDistanceScale,
        radius.y / (tanY * 0.78f));
    crossingDepth_ = (std::max)(crossingDepth_, radius.z + camera_->GetNearClip() * 2);
    const float edge = (crossingDepth_ + radius.z) * tanX;
    const float start = edge + radius.x + settings_.exitMargin;
    float tail = radius.x;
    float tailDepth = radius.z;
    if (exhaust_) {
        const auto bounds = exhaust_->GetUnboostedDisplayBounds();
        tail = (std::max)(tail, bounds.backwardReach);
        tailDepth = (std::max)(tailDepth, bounds.radialReach);
    }
    crossingExitX_ = -(crossingDepth_ + tailDepth) * tanX - tail - settings_.exitMargin;
    const float t = std::clamp(progress, 0.0f, 1.0f);
    const float travel = 0.25f * t + 0.75f * t * t;
    const float x = start + (crossingExitX_ - start) * travel;
    const auto target = Add(Add(camera_->GetViewTranslate(), Scale(forward, crossingDepth_)), Scale(right, x));
    const auto position = Add(target, Scale(center, -1));
    player_->SetDisplayPose(Add(position, Scale(normalPose_.translate, -1)), crossingRotation_);
}

void StageStartSequenceController::ApplyEntryPose(float progress) {
    const float remaining = 1.0f - Smooth(progress);
    const auto up = Axis(referenceFrame_, 1), forward = Axis(referenceFrame_, 2);
    const auto rotation = MatrixMath::MakeAffine({1,1,1}, normalPose_.rotate, {});
    const auto relative = Add(normalPose_.translate, Scale(Axis(referenceFrame_, 3), -1));
    const float depth = (std::max)(Dot(relative, forward), camera_->GetNearClip() * 2);
    float halfHeight = 0;
    for (const auto& corner : modelCorners_) halfHeight = (std::max)(halfHeight, std::fabs(Dot(Rotate(corner, rotation), up)));
    const float drop = (depth * std::tan(cameraFovY_ * 0.5f) + halfHeight + std::fabs(Dot(relative, up))) * settings_.entryBelowScreen;
    SetCameraLift(settings_.entryCameraLift * remaining);
    player_->SetDisplayPose(Scale(up, -drop * remaining), normalPose_.rotate);
}

float StageStartSequenceController::GetTitleAlpha() const {
    if (phase_==Phase::FadeIn) return Smooth(elapsed_/kFadeInSeconds);
    if (phase_==Phase::Title) return 1;
    if (phase_==Phase::FadeOut) return 1-Smooth(elapsed_/kFadeOutSeconds);
    return 0;
}
void StageStartSequenceController::DrawOverlay() {
    const float alpha=GetTitleAlpha();if (alpha<=0 || !sprites_ || !shade_) return;
    auto* dx=sprites_->GetDxCommon();const auto viewport=dx->GetBackBufferViewport();
    const auto scissor=dx->GetBackBufferScissorRect();
    if (viewport.Width<=0 || viewport.Height<=0) return;
    auto* cmd=dx->GetCommandList();cmd->RSSetViewports(1,&viewport);cmd->RSSetScissorRects(1,&scissor);
    sprites_->CommonDrawSetting(true);const Vector2 canvas{viewport.Width,viewport.Height};
    shade_->SetCanvasSize(canvas);shade_->SetSize(canvas);shade_->SetPosition({});shade_->SetColor({0.025f,0.05f,0.09f,0.18f*alpha});shade_->Update();shade_->Draw();
    const float maxWidth[]{0.52f,0.74f},maxHeight[]{0.13f,0.19f},centerY[]{0.42f,0.60f};
    for (size_t i=0;i<titles_.size();++i) {
        auto& sprite=*titles_[i];const auto crop=sprite.GetTextureSize();
        const float scale=(std::min)(canvas.x*maxWidth[i]/crop.x,canvas.y*maxHeight[i]/crop.y);
        const Vector2 size{crop.x*scale,crop.y*scale};sprite.SetCanvasSize(canvas);sprite.SetSize(size);
        sprite.SetPosition({(canvas.x-size.x)*0.5f,canvas.y*centerY[i]-size.y*0.5f});
        sprite.SetColor({1,1,1,alpha});sprite.Update();sprite.Draw();
    }
}
void StageStartSequenceController::RestoreDisplayPose() {
    if (camera_) {
        camera_->SetViewTranslationOffset(cameraTranslationOffset_);
        camera_->SetViewRotationOffset(cameraRotationOffset_);
        camera_->SetFovY(cameraFovY_);
        camera_->Update();
    }
    if (player_) player_->ClearDisplayPose();
    if (rig_) rig_->SetStageStartPresentationActive(false);
}

void StageStartSequenceController::OnFramePresented() {
    if (paused_) return;
    if (phase_ == Phase::CrossingExit) {
        // Only this aircraft's exhaust state is retired, after its fully exited frame.
        // No cloud phase, stage particle system, emitter preset or GPU resource is reset.
        if (exhaust_) exhaust_->ResetDisplayState();
        phase_ = Phase::Gap;
        elapsed_ = 0;
        ApplyDisplayPose();
    } else if (phase_ == Phase::Settle) {
        RestoreDisplayPose();
        phase_ = Phase::Complete;
    }
}

void StageStartSequenceController::Cancel() {
    if (IsActive()) {
        RestoreDisplayPose();
        if (exhaust_) exhaust_->ResetDisplayState();
    }
    phase_ = Phase::Complete;
    elapsed_ = 0;
    paused_ = false;
}

void StageStartSequenceController::DrawImGui() {
#ifdef USE_IMGUI
    auto& s = GetTuning();
    ImGui::TextUnformatted("STAGE 1／雲海をこえて：上面で横切り → 空白 → 背後から通常入場");
    ImGui::Text("文字 %.2f秒（表示 %.2f + 保持 %.2f + フェード %.2f）", kFadeInSeconds + kTitleSeconds + kFadeOutSeconds,
        kFadeInSeconds, kTitleSeconds, kFadeOutSeconds);
    ImGui::TextWrapped("調整値は次回Restartから適用します。PNG・モデルScale・ジェット設定・JSONは変更しません。");
    ImGui::DragFloat("横切り時間（秒）", &s.crossingSeconds, 0.01f, 0.25f, 5.0f);
    ImGui::DragFloat("画面外の空白（秒）", &s.gapSeconds, 0.01f, 0.05f, 1.0f);
    ImGui::DragFloat("通常入場時間（秒）", &s.entrySeconds, 0.01f, 0.25f, 5.0f);
    ImGui::SliderFloat("横切りの画面幅割合", &s.crossingWidth, 0.20f, 0.55f);
    ImGui::SliderFloat("横切り距離の倍率", &s.crossingDistanceScale, 0.7f, 2.0f);
    ImGui::SliderFloat("完全退出の余白", &s.exitMargin, 0.10f, 0.5f);
    ImGui::SliderFloat("画面下からの入場距離倍率", &s.entryBelowScreen, 1.05f, 2.0f);
    ImGui::SliderFloat("入場カメラの上下移動", &s.entryCameraLift, 0.0f, 0.3f);
    ImGui::Text("機体パート %.2f秒／全体 %.2f秒", s.crossingSeconds + s.gapSeconds + s.entrySeconds,
        kFadeInSeconds + kTitleSeconds + kFadeOutSeconds + s.crossingSeconds + s.gapSeconds + s.entrySeconds);
    ImGui::Text("直前の横切り奥行 %.3f／退出位置 %.3f", crossingDepth_, crossingExitX_);
    ImGui::TextUnformatted("演出中はPauseキーで一時停止・復帰、Tでタイトルへ戻れます。");
    if (ImGui::Button("登場演出の採用値へ戻す")) s = Settings{};
#endif
}

float StageStartSequenceController::GetHudTopReveal() const {
    if (phase_ < Phase::Crossing) return 0;
    return phase_ == Phase::Crossing ? std::clamp(elapsed_ / 0.45f, 0.0f, 1.0f) : 1.0f;
}
float StageStartSequenceController::GetHudBottomReveal() const {
    if (phase_ < Phase::Entry) return 0;
    return phase_ == Phase::Entry ? std::clamp(elapsed_ / (std::max)(0.1f, settings_.entrySeconds - 0.20f), 0.0f, 1.0f) : 1.0f;
}
