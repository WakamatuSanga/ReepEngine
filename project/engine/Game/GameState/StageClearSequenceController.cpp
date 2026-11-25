#include "StageClearSequenceController.h"

#include "Engine/Game/Player/Player.h"
#include "Engine/Game/Player/PlayerJetExhaustController.h"
#include "Engine/Game/UI/StageClearResultController.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Primitive/BeamBatchRenderer.h"
#include "Engine/Graphics/Sprite/SpriteCommon.h"
#include "GameScene.h"
#include "SceneManager.h"
#include "TitleCloudTransition.h"
#include "TitleScene.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <iterator>
#include <numbers>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
constexpr float kPitchUpSeconds = 0.30f;
constexpr float kClimbRadians = 20.0f * std::numbers::pi_v<float> / 180.0f;
constexpr float kDepartureAcceleration = 96.0f;
constexpr float kContrailLife = 0.72f;
constexpr std::array<Vector3, 2> kWingTips{{{-0.7906f, -0.110f, -0.344f}, {0.7906f, -0.110f, -0.344f}}};
bool midbossClearEnabled = true;
float departureBlurStrength = 0.24f;

Vector3 Axis(const Matrix4x4& frame, int row) {
    return {frame.m[row][0], frame.m[row][1], frame.m[row][2]};
}

float Dot(Vector3 a, Vector3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vector3 Scale(Vector3 a, float amount) { return {a.x * amount, a.y * amount, a.z * amount}; }
Vector3 Add(Vector3 a, Vector3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vector3 Subtract(Vector3 a, Vector3 b) { return Add(a, Scale(b, -1.0f)); }
Vector3 Cross(Vector3 a, Vector3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vector3 UnitOr(Vector3 value, Vector3 fallback) {
    const float lengthSq = Dot(value, value);
    return lengthSq > 0.000001f ? Scale(value, 1.0f / std::sqrt(lengthSq)) : fallback;
}
float Smooth(float value) {
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
Vector4 Project(Vector3 point, const Camera& camera) {
    const auto& vp = camera.GetViewProjectionMatrix();
    const auto coord = [&](int i) {
        return point.x * vp.m[0][i] + point.y * vp.m[1][i] + point.z * vp.m[2][i] + vp.m[3][i];
    };
    return {coord(0), coord(1), coord(2), coord(3)};
}
Vector2 ProjectedUv(Vector4 clip, const D3D12_VIEWPORT& viewport, float width, float height) {
    return {(viewport.TopLeftX + (0.5f + clip.x / (2.0f * clip.w)) * viewport.Width) / width,
        (viewport.TopLeftY + (0.5f - clip.y / (2.0f * clip.w)) * viewport.Height) / height};
}
float BlendAngle(float from, float to, float amount) {
    return from + std::remainder(to - from, 2.0f * std::numbers::pi_v<float>) * amount;
}
}

class StageClearSequenceController::Contrails {
public:
    void Initialize(DirectXCommon* dx) {
        renderer_.Initialize(dx, true, true, kMaxSamples * 12);
        vertices_.reserve(kMaxSamples * 12);
    }

    void Advance(float dt) {
        for (auto& trail : trails_) {
            for (auto& sample : trail) sample.age += dt;
            while (!trail.empty() && trail.front().age >= kContrailLife) trail.pop_front();
        }
    }

    void Emit(const Player& player) {
        for (size_t i = 0; i < trails_.size(); ++i) {
            auto& trail = trails_[i];
            const auto point = player.TransformVisualModelPointToWorld(kWingTips[i]);
            if (!trail.empty()) {
                const Vector3 displacement = Subtract(point, trail.back().position);
                if (Dot(displacement, displacement) < 0.000025f) continue;
            }
            if (trail.size() >= kMaxSamples) trail.pop_front();
            trail.push_back({point, 0.0f});
        }
    }

    void Draw(const Camera& camera, DirectXCommon& dx) {
        vertices_.clear();
        const auto eye = camera.GetViewTranslate();
        const auto& frame = camera.GetViewWorldMatrix();
        const float pixelScale = 2.0f * std::tan(camera.GetFovY() * 0.5f) / (std::max)(1.0f, dx.GetOffscreenViewport().Height);
        for (const auto& trail : trails_) {
            std::array<BeamBatchRenderer::Vertex, 2> previous{};
            for (size_t i = 0; i < trail.size(); ++i) {
                const auto& point = trail[i];
                const auto relative = Subtract(point.position, eye);
                const auto tangent = Subtract(trail[(std::min)(i + 1, trail.size() - 1)].position,
                    trail[i > 0 ? i - 1 : 0].position);
                const auto side = UnitOr(Cross(relative, tangent), Axis(frame, 0));
                const float life = 1.0f - point.age / kContrailLife;
                const float width = (std::max)(camera.GetNearClip(), Dot(relative, Axis(frame, 2))) * pixelScale * (0.40f + life * 1.12f);
                const auto offset = Scale(side, width);
                // Use the existing alpha-blended projectile trail branch. UV.x carries age opacity.
                const std::array<BeamBatchRenderer::Vertex, 2> pair{{
                    {Subtract(point.position, offset), {life, 2.0f}},
                    {Add(point.position, offset), {life, 3.0f}},
                }};
                if (i > 0) vertices_.insert(vertices_.end(), {previous[0], previous[1], pair[0], pair[0], previous[1], pair[1]});
                previous = pair;
            }
        }
        if (vertices_.empty()) return;
        const auto rtv = dx.GetRenderTextureRTV();
        const auto depth = dx.GetDepthStencilView();
        dx.GetCommandList()->OMSetRenderTargets(1, &rtv, FALSE, &depth);
        const Vector4 color{0.75f, 0.88f, 1.0f, 1.0f};
        renderer_.Draw(vertices_, &camera, 1.0f, 0.36f, 0.0f, 1.0f, 1.0f, 0.0f, 14u, 0.0f, &color);
    }

private:
    struct Sample { Vector3 position; float age; };
    static constexpr size_t kMaxSamples = 96;
    std::array<std::deque<Sample>, 2> trails_;
    std::vector<BeamBatchRenderer::Vertex> vertices_;
    BeamBatchRenderer renderer_;
};

class StageClearSequenceController::State {
public:
    virtual ~State() = default;
    virtual const State& Begin(StageClearSequenceController&, int, std::uint32_t) const { return *this; }
    virtual const State& Update(StageClearSequenceController&, float, bool) const { return *this; }
    virtual const State& OnFramePresented(StageClearSequenceController&) const { return *this; }
    virtual void DrawOverlay(StageClearSequenceController&) const {}

    Phase GetPhase() const { return phase_; }
    static const State& For(Phase phase);

protected:
    explicit State(Phase phase) : phase_(phase) {}

private:
    class Idle;
    class Tentacles;
    class Departure;
    class ExitedFrame;
    class Clouds;
    class Results;
    const Phase phase_;
};

class StageClearSequenceController::State::Idle final : public State {
public:
    Idle() : State(Phase::Idle) {}

    const State& Begin(StageClearSequenceController& clear, int score, std::uint32_t defeats) const override {
        clear.score_ = score;
        clear.defeats_ = defeats;
        clear.elapsed_ = 0.0f;
        return For(Phase::Tentacles);
    }
};

class StageClearSequenceController::State::Tentacles final : public State {
public:
    Tentacles() : State(Phase::Tentacles) {}

    const State& Update(StageClearSequenceController& clear, float, bool tentaclesGone) const override {
        if (!tentaclesGone) return *this;
        clear.StartDeparture();
        return For(Phase::Departure);
    }
};

class StageClearSequenceController::State::Departure final : public State {
public:
    Departure() : State(Phase::Departure) {}

    const State& Update(StageClearSequenceController& clear, float dt, bool) const override {
        clear.AdvanceDeparturePose(dt);
        if (clear.HasAircraftPassedUpperEdge()) {
            clear.dx_->SetDepartureRadialBlur({});
            return For(Phase::ExitedFrame);
        }

        clear.UpdateDepartureEffects(dt);
        return *this;
    }
};

class StageClearSequenceController::State::ExitedFrame final : public State {
public:
    ExitedFrame() : State(Phase::ExitedFrame) {}

    const State& OnFramePresented(StageClearSequenceController& clear) const override {
        if (clear.exhaust_) clear.exhaust_->ResetDisplayState();
        return clear.clouds_->StartAtLowerEdge() ? For(Phase::Clouds) : *this;
    }
};

class StageClearSequenceController::State::Clouds final : public State {
public:
    Clouds() : State(Phase::Clouds) {}

    const State& Update(StageClearSequenceController& clear, float dt, bool) const override {
        clear.clouds_->Update(dt);
        if (clear.clouds_->CanSwitch()) {
            clear.resultBackground_ = true;
            clear.clouds_->OnSceneChanged();
        }
        return *this;
    }

    void DrawOverlay(StageClearSequenceController& clear) const override { clear.clouds_->Draw(); }

    const State& OnFramePresented(StageClearSequenceController& clear) const override {
        clear.clouds_->OnFramePresented();
        if (clear.clouds_->IsActive()) return *this;
        clear.result_->Begin(clear.score_, clear.defeats_);
        return For(Phase::Results);
    }
};

class StageClearSequenceController::State::Results final : public State {
public:
    Results() : State(Phase::Results) {}

    const State& Update(StageClearSequenceController& clear, float dt, bool) const override {
        if (clear.transitionRequested_) return *this;

        const auto choice = clear.result_->Update(dt);
        if (choice == StageClearResultController::Choice::Retry) {
            clear.transitionRequested_ = true;
            SceneManager::GetInstance()->ChangeScene(std::make_unique<GameScene>());
        } else if (choice == StageClearResultController::Choice::Title) {
            clear.transitionRequested_ = true;
            SceneManager::GetInstance()->ChangeSceneWithTitleClouds(std::make_unique<TitleScene>());
        }
        // NextStage stays here until a scene is available for that action.
        return *this;
    }

    void DrawOverlay(StageClearSequenceController& clear) const override { clear.result_->Draw(); }

    const State& OnFramePresented(StageClearSequenceController& clear) const override {
        clear.result_->OnFramePresented();
        return *this;
    }
};

const StageClearSequenceController::State& StageClearSequenceController::State::For(Phase phase) {
    static const Idle idle;
    static const Tentacles tentacles;
    static const Departure departure;
    static const ExitedFrame exitedFrame;
    static const Clouds clouds;
    static const Results results;
    static const std::array<const State*, 6> states{&idle, &tentacles, &departure, &exitedFrame, &clouds, &results};
    return *states[static_cast<std::size_t>(phase)];
}

StageClearSequenceController::StageClearSequenceController() : state_(&State::For(Phase::Idle)) {}
StageClearSequenceController::~StageClearSequenceController() {
    if (dx_ && IsActive()) dx_->SetDepartureRadialBlur({});
}

bool StageClearSequenceController::IsMidbossClearEnabled() { return midbossClearEnabled; }

void StageClearSequenceController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::Checkbox("中ボス撃破でステージ1クリア（暫定）", &midbossClearEnabled);
    ImGui::TextUnformatted("オフにすると、撃破演出の後に WAVE_005 へ進みます。");
    ImGui::SliderFloat("クリア退場のラジアルブラー強度", &departureBlurStrength, 0.0f, 0.5f, "%.2f");
#endif
}

void StageClearSequenceController::Initialize(SpriteCommon* sprites, Camera* camera, Player* player,
    PlayerJetExhaustController* exhaust, GameViewport* viewport, Input* input) {
    camera_ = camera;
    player_ = player;
    exhaust_ = exhaust;
    dx_ = sprites->GetDxCommon();
    contrails_ = std::make_unique<Contrails>();
    contrails_->Initialize(dx_);
    clouds_ = std::make_unique<TitleCloudTransition>();
    clouds_->Initialize(sprites);
    result_ = std::make_unique<StageClearResultController>();
    result_->Initialize(sprites, viewport, input);
}

void StageClearSequenceController::Begin(int score, std::uint32_t defeats) {
    state_ = &state_->Begin(*this, score, defeats);
}

void StageClearSequenceController::StartDeparture() {
    aircraftStart_ = player_->GetVisualTransform();
    aircraftOffset_ = {};
    player_->GetVisualModelBounds(aircraftBoundsMin_, aircraftBoundsMax_);
    const Matrix4x4& frame = camera_->GetViewWorldMatrix();
    const float climb = (std::max)(kClimbRadians, camera_->GetFovY() * 0.5f + 0.11f);
    const auto direction = Add(Scale(Axis(frame, 2), std::cos(climb)), Scale(Axis(frame, 1), std::sin(climb)));
    // The display model points along local +Z. Pitch is negative when +Z points up.
    aircraftEndRotation_ = {-std::asin(std::clamp(direction.y, -1.0f, 1.0f)), std::atan2(direction.x, direction.z), 0.0f};
    elapsed_ = 0.0f;
    contrails_->Emit(*player_);
}

void StageClearSequenceController::AdvanceDeparturePose(float deltaTime) {
    elapsed_ += deltaTime;
    const float pitch = Smooth(elapsed_ / kPitchUpSeconds);
    const Vector3 rotation{
        BlendAngle(aircraftStart_.rotate.x, aircraftEndRotation_.x, pitch),
        BlendAngle(aircraftStart_.rotate.y, aircraftEndRotation_.y, pitch),
        BlendAngle(aircraftStart_.rotate.z, aircraftEndRotation_.z, pitch),
    };
    const auto heading = Axis(MatrixMath::MakeAffine({1, 1, 1}, rotation, {}), 2);
    const float distance = kDepartureAcceleration * (elapsed_ - 0.5f * deltaTime) * deltaTime;
    aircraftOffset_ = Add(aircraftOffset_, Scale(heading, distance));
    player_->SetDisplayPose(aircraftOffset_, rotation);
}

void StageClearSequenceController::UpdateDepartureEffects(float deltaTime) {
    contrails_->Emit(*player_);
    if (exhaust_) {
        const auto pose = player_->GetVisualTransform();
        exhaust_->UpdateDisplay(deltaTime, pose, aircraftStart_.scale.x, Smooth(elapsed_ / 0.9f) * 0.25f, 1.0f, true);
    }
}

bool StageClearSequenceController::HasAircraftPassedUpperEdge() const {
    // All transformed corners must clear the top clip plane. The mesh is inside their convex hull.
    for (unsigned corner = 0; corner < 8; ++corner) {
        const Vector3 point{(corner & 1) ? aircraftBoundsMax_.x : aircraftBoundsMin_.x,
            (corner & 2) ? aircraftBoundsMax_.y : aircraftBoundsMin_.y,
            (corner & 4) ? aircraftBoundsMax_.z : aircraftBoundsMin_.z};
        const auto clip = Project(player_->TransformVisualModelPointToWorld(point), *camera_);
        if (clip.w <= camera_->GetNearClip() || clip.y <= clip.w) return false;
    }
    return true;
}

void StageClearSequenceController::DrawDepartureEffects() {
    contrails_->Draw(*camera_, *dx_);
    DirectXCommon::DepartureRadialBlur effect{};
    if (state_->GetPhase() == Phase::Departure) {
        const auto viewport = dx_->GetOffscreenViewport();
        const float width = static_cast<float>(dx_->GetRenderTextureWidth());
        const float height = static_cast<float>(dx_->GetRenderTextureHeight());
        const auto clip = Project(player_->TransformVisualModelPointToWorld({0.0f, 0.0f, 0.06f}), *camera_);
        if (clip.w > camera_->GetNearClip() && width > 0.0f && height > 0.0f) {
            const Vector2 center = ProjectedUv(clip, viewport, width, height);
            const float viewportY = 0.5f - clip.y / (2.0f * clip.w);
            effect.center = {center.x, center.y};
            effect.aspect = width / height;
            effect.strength = departureBlurStrength * Smooth(elapsed_ / 0.47f) * Smooth(viewportY / 0.28f);
            effect.clearRadius = 0.055f;
            for (const auto wing : kWingTips) {
                const auto wingClip = Project(player_->TransformVisualModelPointToWorld(wing), *camera_);
                if (wingClip.w <= camera_->GetNearClip()) continue;
                const auto uv = ProjectedUv(wingClip, viewport, width, height);
                effect.clearRadius = (std::max)(effect.clearRadius,
                    std::hypot((uv.x - center.x) * effect.aspect, uv.y - center.y) + 0.03f);
            }
        }
    }
    dx_->SetDepartureRadialBlur(effect);
}

bool StageClearSequenceController::IsActive() const {
    return state_->GetPhase() != Phase::Idle;
}

bool StageClearSequenceController::NeedsTentacleUpdate() const {
    return state_->GetPhase() == Phase::Tentacles;
}

bool StageClearSequenceController::IsAircraftVisible() const {
    return NeedsTentacleUpdate() || state_->GetPhase() == Phase::Departure;
}

void StageClearSequenceController::Update(float deltaTime, bool tentaclesGone) {
    contrails_->Advance(deltaTime);
    state_ = &state_->Update(*this, deltaTime, tentaclesGone);
}

void StageClearSequenceController::DrawOverlay() {
    state_->DrawOverlay(*this);
}

void StageClearSequenceController::OnFramePresented() {
    state_ = &state_->OnFramePresented(*this);
}
