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

class StageStartSequenceController::State {
public:
    enum class AircraftMotion { Hidden, Crossing, Entry };

    virtual ~State() = default;
    virtual const State& OnPresentationRevealed(StageStartSequenceController&) const { return *this; }
    virtual const State& Update(StageStartSequenceController&, float) const { return *this; }
    virtual const State& OnFramePresented(StageStartSequenceController&) const { return *this; }
    virtual void ApplyPose(StageStartSequenceController&) const {}
    virtual float GetTitleAlpha(const StageStartSequenceController&) const { return 0.0f; }
    virtual float GetHudTopReveal(const StageStartSequenceController&) const {
        return phase_ >= Phase::Crossing ? 1.0f : 0.0f;
    }
    virtual float GetHudBottomReveal(const StageStartSequenceController&) const {
        return phase_ >= Phase::Entry ? 1.0f : 0.0f;
    }

    Phase GetPhase() const { return phase_; }
    AircraftMotion GetAircraftMotion() const { return aircraftMotion_; }
    static const State& For(Phase phase);

protected:
    explicit State(Phase phase, AircraftMotion motion = AircraftMotion::Hidden)
        : phase_(phase), aircraftMotion_(motion) {}

private:
    class AwaitingReveal;
    class Timed;
    class FadeIn;
    class Title;
    class FadeOut;
    class Crossing;
    class CrossingExit;
    class Gap;
    class Entry;
    class Settle;
    class Complete;
    const Phase phase_;
    const AircraftMotion aircraftMotion_;
};

class StageStartSequenceController::State::AwaitingReveal final : public State {
public:
    AwaitingReveal() : State(Phase::AwaitingReveal) {}

    const State& OnPresentationRevealed(StageStartSequenceController& sequence) const override {
        sequence.elapsed_ = 0.0f;
        return For(Phase::FadeIn);
    }
};

class StageStartSequenceController::State::Timed : public State {
public:
    const State& Update(StageStartSequenceController& sequence, float dt) const override {
        sequence.elapsed_ += dt;
        if (sequence.elapsed_ >= Duration(sequence)) {
            sequence.elapsed_ = 0.0f; // Late/load frames advance at most one phase.
            return For(next_);
        }
        return *this;
    }

    void ApplyPose(StageStartSequenceController& sequence) const override {
        sequence.ApplyTitlePose();
    }

protected:
    Timed(Phase phase, Phase next, AircraftMotion motion = AircraftMotion::Hidden)
        : State(phase, motion), next_(next) {}

private:
    virtual float Duration(const StageStartSequenceController& sequence) const = 0;
    const Phase next_;
};

class StageStartSequenceController::State::FadeIn final : public Timed {
public:
    FadeIn() : Timed(Phase::FadeIn, Phase::Title) {}

    float GetTitleAlpha(const StageStartSequenceController& sequence) const override {
        return Smooth(sequence.elapsed_ / kFadeInSeconds);
    }

private:
    float Duration(const StageStartSequenceController&) const override { return kFadeInSeconds; }
};

class StageStartSequenceController::State::Title final : public Timed {
public:
    Title() : Timed(Phase::Title, Phase::FadeOut) {}

    float GetTitleAlpha(const StageStartSequenceController&) const override { return 1.0f; }

private:
    float Duration(const StageStartSequenceController&) const override { return kTitleSeconds; }
};

class StageStartSequenceController::State::FadeOut final : public Timed {
public:
    FadeOut() : Timed(Phase::FadeOut, Phase::Crossing) {}

    float GetTitleAlpha(const StageStartSequenceController& sequence) const override {
        return 1.0f - Smooth(sequence.elapsed_ / kFadeOutSeconds);
    }

private:
    float Duration(const StageStartSequenceController&) const override { return kFadeOutSeconds; }
};

class StageStartSequenceController::State::Crossing final : public Timed {
public:
    Crossing() : Timed(Phase::Crossing, Phase::CrossingExit, AircraftMotion::Crossing) {}

    void ApplyPose(StageStartSequenceController& sequence) const override {
        sequence.SetCameraLift(0.0f);
        sequence.ApplyCrossingPose(sequence.elapsed_ / sequence.settings_.crossingSeconds);
    }

    float GetHudTopReveal(const StageStartSequenceController& sequence) const override {
        return std::clamp(sequence.elapsed_ / 0.45f, 0.0f, 1.0f);
    }

private:
    float Duration(const StageStartSequenceController& sequence) const override {
        return sequence.settings_.crossingSeconds;
    }
};

class StageStartSequenceController::State::CrossingExit final : public State {
public:
    CrossingExit() : State(Phase::CrossingExit, AircraftMotion::Crossing) {}

    void ApplyPose(StageStartSequenceController& sequence) const override {
        sequence.SetCameraLift(0.0f);
        sequence.ApplyCrossingPose(1.0f);
    }

    const State& OnFramePresented(StageStartSequenceController& sequence) const override {
        // Retire only this aircraft's exhaust after its fully exited frame.
        if (sequence.exhaust_) sequence.exhaust_->ResetDisplayState();
        sequence.elapsed_ = 0.0f;
        return For(Phase::Gap);
    }
};

class StageStartSequenceController::State::Gap final : public Timed {
public:
    Gap() : Timed(Phase::Gap, Phase::Entry) {}

    void ApplyPose(StageStartSequenceController& sequence) const override {
        sequence.ApplyEntryPose(0.0f);
    }

private:
    float Duration(const StageStartSequenceController& sequence) const override {
        return sequence.settings_.gapSeconds;
    }
};

class StageStartSequenceController::State::Entry final : public Timed {
public:
    Entry() : Timed(Phase::Entry, Phase::Settle, AircraftMotion::Entry) {}

    void ApplyPose(StageStartSequenceController& sequence) const override {
        sequence.ApplyEntryPose(sequence.elapsed_ / sequence.settings_.entrySeconds);
    }

    float GetHudBottomReveal(const StageStartSequenceController& sequence) const override {
        return std::clamp(sequence.elapsed_ / (std::max)(0.1f, sequence.settings_.entrySeconds - 0.20f), 0.0f, 1.0f);
    }

private:
    float Duration(const StageStartSequenceController& sequence) const override {
        return sequence.settings_.entrySeconds;
    }
};

class StageStartSequenceController::State::Settle final : public State {
public:
    Settle() : State(Phase::Settle, AircraftMotion::Entry) {}

    void ApplyPose(StageStartSequenceController& sequence) const override {
        sequence.ApplyEntryPose(1.0f);
    }

    const State& OnFramePresented(StageStartSequenceController& sequence) const override {
        sequence.RestoreDisplayPose();
        return For(Phase::Complete);
    }
};

class StageStartSequenceController::State::Complete final : public State {
public:
    Complete() : State(Phase::Complete) {}
};

const StageStartSequenceController::State& StageStartSequenceController::State::For(Phase phase) {
    static const AwaitingReveal awaitingReveal;
    static const FadeIn fadeIn;
    static const Title title;
    static const FadeOut fadeOut;
    static const Crossing crossing;
    static const CrossingExit crossingExit;
    static const Gap gap;
    static const Entry entry;
    static const Settle settle;
    static const Complete complete;
    switch (phase) {
    case Phase::AwaitingReveal: return awaitingReveal;
    case Phase::FadeIn: return fadeIn;
    case Phase::Title: return title;
    case Phase::FadeOut: return fadeOut;
    case Phase::Crossing: return crossing;
    case Phase::CrossingExit: return crossingExit;
    case Phase::Gap: return gap;
    case Phase::Entry: return entry;
    case Phase::Settle: return settle;
    case Phase::Complete: return complete;
    }
    return complete;
}

StageStartSequenceController::Settings& StageStartSequenceController::GetTuning() { return tuning; }
StageStartSequenceController::StageStartSequenceController() : state_(&State::For(Phase::AwaitingReveal)) {}
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
    state_ = &State::For(Phase::AwaitingReveal);
    elapsed_ = 0;
    paused_ = false;
    rig_->SetStageStartPresentationActive(true);
    ApplyTitlePose();
}

void StageStartSequenceController::OnPresentationRevealed() {
    state_ = &state_->OnPresentationRevealed(*this);
}

StageStartSequenceController::Phase StageStartSequenceController::GetPhase() const {
    return state_->GetPhase();
}

bool StageStartSequenceController::IsAircraftVisible() const {
    return state_->GetAircraftMotion() != State::AircraftMotion::Hidden;
}

void StageStartSequenceController::Update(float deltaTime) {
    const float dt = !paused_ && std::isfinite(deltaTime) ? std::clamp(deltaTime, 0.0f, 0.1f) : 0.0f;
    state_ = &state_->Update(*this, dt);
    state_->ApplyPose(*this);
    if (IsAircraftVisible() && exhaust_) {
        const auto pose = player_->GetVisualTransform();
        // Reuse the title's nozzle coordinate path for the lateral pass only.
        // Its bounded flame follows this one aircraft; normal entry uses world particles.
        const bool lateral = state_->GetAircraftMotion() == State::AircraftMotion::Crossing;
        exhaust_->UpdateDisplay(dt, pose, normalPose_.scale.x, 0, 1, lateral);
    }
}

void StageStartSequenceController::SetCameraLift(float lift) {
    camera_->SetViewTranslationOffset(Add(cameraTranslationOffset_, Scale(Axis(referenceFrame_, 1), lift)));
    camera_->SetViewRotationOffset(cameraRotationOffset_);
    camera_->SetFovY(cameraFovY_);
    camera_->Update();
}

void StageStartSequenceController::ApplyTitlePose() {
    SetCameraLift(0.0f);
    player_->ClearDisplayPose();
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
    return state_->GetTitleAlpha(*this);
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
    const auto& next = state_->OnFramePresented(*this);
    if (&next != state_) {
        state_ = &next;
        state_->ApplyPose(*this);
    }
}

void StageStartSequenceController::Cancel() {
    if (IsActive()) {
        RestoreDisplayPose();
        if (exhaust_) exhaust_->ResetDisplayState();
    }
    state_ = &State::For(Phase::Complete);
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
    return state_->GetHudTopReveal(*this);
}
float StageStartSequenceController::GetHudBottomReveal() const {
    return state_->GetHudBottomReveal(*this);
}
