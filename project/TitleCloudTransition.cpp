#include "TitleCloudTransition.h"
#include "Engine/Core/DebugUiWorkspace.h"
#include "Engine/Graphics/Sprite/Sprite.h"
#include "Engine/Core/SrvManager.h"
#include "externals/DirectXTex/DirectXTex.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <string>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
constexpr const char* kSettingsPath = "resources/ui/cloud_transition.json";
constexpr std::array<const char*, 3> kTextures{
    "resources/ui/Fade_cloud_2.png", "resources/ui/Fade_cloud_1.png", "resources/ui/Fade_cloud_3.png" };
bool settingsVisible = false;
float testLoadDelay = 0.0f;
const char* status = "調整は次の遷移から適用します。保存すると次回起動にも適用します。";
float Smooth(float t) { t = std::clamp(t, 0.0f, 1.0f); return t * t * (3.0f - 2.0f * t); }
float Linear(float srgb) { return srgb <= 0.04045f ? srgb / 12.92f : std::pow((srgb + 0.055f) / 1.055f, 2.4f); }
template<class F> void Fields(TitleCloudTransition::Settings& s, F visit) {
    visit("enterSeconds", s.enterSeconds, 0.2f, 8.0f);
    visit("holdSeconds", s.holdSeconds, 0.05f, 8.0f);
    visit("exitSeconds", s.exitSeconds, 0.2f, 8.0f);
    visit("edgeFade", s.edgeFade, 0.0f, 0.2f);
    visit("backingEnterStart", s.backingEnterStart, 0.5f, 0.95f);
    visit("backingExitEnd", s.backingExitEnd, 0.1f, 0.8f);
    for (size_t i = 0; i < s.layers.size(); ++i) {
        auto& l = s.layers[i];
        const std::string prefix = "cloud" + std::to_string(i == 0 ? 2 : i == 1 ? 1 : 3);
        visit(prefix + "EnterDelay", l.enterDelay, 0.0f, s.enterSeconds * 0.8f);
        visit(prefix + "ExitDelay", l.exitDelay, 0.0f, s.exitSeconds * 0.8f);
        visit(prefix + "Width", l.width, 1.05f, 2.5f);
        visit(prefix + "StartX", l.start.x, 0.2f, 0.8f);
        visit(prefix + "CoveredX", l.covered.x, 0.2f, 0.8f);
        visit(prefix + "EndX", l.end.x, 0.2f, 0.8f);
    }
}
}

class TitleCloudTransition::State {
public:
    struct LayerLayout {
        const LayerSettings& layer;
        float layerHeight;
        float viewportHeight;
        float margin;
    };
    struct LayerMotion {
        Vector2 center;
        float rotation;
    };

    virtual ~State() = default;
    virtual const State& Update(TitleCloudTransition&) const { return *this; }
    virtual const State& OnSceneChanged(TitleCloudTransition&) const { return *this; }
    virtual const State& OnFramePresented(TitleCloudTransition&) const { return *this; }
    virtual bool CanSwitch(const TitleCloudTransition&) const { return false; }
    virtual void Draw(TitleCloudTransition& transition) const {
        if (transition.DrawLayers()) transition.coveredDrawn_ = false;
    }
    virtual float GetBackingAlpha(const TitleCloudTransition&) const { return 0.0f; }
    virtual LayerMotion GetLayerMotion(const TitleCloudTransition&, const LayerLayout& layout) const {
        return {layout.layer.covered, layout.layer.rotation};
    }

    Phase GetPhase() const { return phase_; }
    const char* GetName() const { return name_; }
    static const State& For(Phase phase);

protected:
    State(Phase phase, const char* name) : phase_(phase), name_(name) {}

private:
    class Idle;
    class Entering;
    class Opaque;
    class Covered;
    class Waiting;
    class Exiting;
    const Phase phase_;
    const char* const name_;
};

class TitleCloudTransition::State::Idle final : public State {
public:
    Idle() : State(Phase::Idle, "待機") {}
    void Draw(TitleCloudTransition&) const override {}
};

class TitleCloudTransition::State::Entering final : public State {
public:
    Entering() : State(Phase::Entering, "進入") {}

    const State& Update(TitleCloudTransition& transition) const override {
        if (transition.elapsed_ >= transition.activeSettings_.enterSeconds) {
            transition.elapsed_ -= transition.activeSettings_.enterSeconds;
            return For(Phase::Covered);
        }
        return *this;
    }

    float GetBackingAlpha(const TitleCloudTransition& transition) const override {
        const auto& settings = transition.activeSettings_;
        const float p = static_cast<float>(transition.elapsed_ / settings.enterSeconds);
        // Missing PNGs use the same full-screen opaque fade, without holes.
        return Smooth(transition.imagesAvailable_ ? (p - settings.backingEnterStart) / (1 - settings.backingEnterStart) : p);
    }

    LayerMotion GetLayerMotion(const TitleCloudTransition& transition, const LayerLayout& layout) const override {
        const auto& layer = layout.layer;
        Vector2 start = layer.start;
        start.y = (std::max)(start.y, 1 + layout.margin);
        if (transition.enterAtLowerEdge_) {
            // Advance the clear-only entry past the transparent margins of the cloud PNGs.
            start.y = 1.0f + layout.layerHeight / (2.0f * layout.viewportHeight) - 0.115f;
        }
        const float p = Smooth(static_cast<float>((transition.elapsed_ - layer.enterDelay)
            / (transition.activeSettings_.enterSeconds - layer.enterDelay)));
        return {{start.x + (layer.covered.x-start.x)*p, start.y + (layer.covered.y-start.y)*p},
            layer.rotation * p};
    }
};

class TitleCloudTransition::State::Opaque : public State {
public:
    float GetBackingAlpha(const TitleCloudTransition&) const override { return 1.0f; }
    void Draw(TitleCloudTransition& transition) const override {
        if (transition.DrawLayers()) transition.coveredDrawn_ = true;
    }

protected:
    Opaque(Phase phase, const char* name) : State(phase, name) {}
};

class TitleCloudTransition::State::Covered final : public Opaque {
public:
    Covered() : Opaque(Phase::Covered, "遮蔽・交換待ち") {}

    bool CanSwitch(const TitleCloudTransition& transition) const override {
        return transition.coveredPresented_ && transition.elapsed_ >= transition.activeSettings_.holdSeconds * 0.5;
    }

    const State& OnSceneChanged(TitleCloudTransition& transition) const override {
        if (!CanSwitch(transition)) return *this;
        transition.newScenePresented_ = false;
        transition.coveredDrawn_ = false;
        return For(Phase::Waiting);
    }

    const State& OnFramePresented(TitleCloudTransition& transition) const override {
        if (transition.coveredDrawn_) transition.coveredPresented_ = true;
        return *this;
    }
};

class TitleCloudTransition::State::Waiting final : public Opaque {
public:
    Waiting() : Opaque(Phase::Waiting, "新シーン表示待ち") {}

    const State& Update(TitleCloudTransition& transition) const override {
        if (transition.newScenePresented_ && transition.elapsed_ >= transition.activeSettings_.holdSeconds) {
            // Do not apply synchronous load time to the reveal animation.
            transition.elapsed_ = 0;
            return For(Phase::Exiting);
        }
        return *this;
    }

    const State& OnFramePresented(TitleCloudTransition& transition) const override {
        if (transition.coveredDrawn_) transition.newScenePresented_ = true;
        return *this;
    }
};

class TitleCloudTransition::State::Exiting final : public State {
public:
    Exiting() : State(Phase::Exiting, "退出") {}

    void Draw(TitleCloudTransition& transition) const override {
        if (transition.elapsed_ >= transition.activeSettings_.exitSeconds) {
            transition.clearDrawn_ = true;
            return;
        }
        State::Draw(transition);
    }

    const State& OnFramePresented(TitleCloudTransition& transition) const override {
        if (!transition.clearDrawn_) return *this;
        transition.ResetProgress();
        return For(Phase::Idle);
    }

    float GetBackingAlpha(const TitleCloudTransition& transition) const override {
        const auto& settings = transition.activeSettings_;
        return 1 - Smooth(static_cast<float>(transition.elapsed_ / settings.exitSeconds)
            / (transition.imagesAvailable_ ? settings.backingExitEnd : 1.0f));
    }

    LayerMotion GetLayerMotion(const TitleCloudTransition& transition, const LayerLayout& layout) const override {
        const auto& layer = layout.layer;
        Vector2 end = layer.end;
        end.y = (std::max)(end.y, 1 + layout.margin);
        const float p = Smooth(static_cast<float>((transition.elapsed_ - layer.exitDelay)
            / (transition.activeSettings_.exitSeconds - layer.exitDelay)));
        return {{layer.covered.x + (end.x-layer.covered.x)*p, layer.covered.y + (end.y-layer.covered.y)*p},
            layer.rotation};
    }
};

const TitleCloudTransition::State& TitleCloudTransition::State::For(Phase phase) {
    static const Idle idle;
    static const Entering entering;
    static const Covered covered;
    static const Waiting waiting;
    static const Exiting exiting;
    switch (phase) {
    case Phase::Idle: return idle;
    case Phase::Entering: return entering;
    case Phase::Covered: return covered;
    case Phase::Waiting: return waiting;
    case Phase::Exiting: return exiting;
    }
    return idle;
}

TitleCloudTransition::TitleCloudTransition() : state_(&State::For(Phase::Idle)) {}
TitleCloudTransition::~TitleCloudTransition() = default;
TitleCloudTransition::Settings& TitleCloudTransition::GetSettings() { static Settings s; return s; }
bool TitleCloudTransition::LoadSettings(const char* path) {
    std::ifstream file(path);
    if (!file) return false;
    const std::string json((std::istreambuf_iterator<char>(file)), {});
    if (json.find('{') == std::string::npos) return false;
    Settings next;
    bool valid = true;
    Fields(next, [&](const std::string& key, float& value, float low, float high) {
        std::smatch match;
        const std::regex pattern("\"" + key + "\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)");
        if (std::regex_search(json, match, pattern)) {
            try { const float v = std::stof(match[1].str()); if (std::isfinite(v)) value = std::clamp(v, low, high); }
            catch (...) { valid = false; }
        }
    });
    if (valid) GetSettings() = next;
    return valid;
}
bool TitleCloudTransition::SaveSettings(const char* path) {
    std::error_code error;
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, error);
    if (error) return false;
    std::ofstream file(path);
    if (!file) return false;
    file.precision(9);
    file << "{\n  \"version\": 1";
    Fields(GetSettings(), [&](const std::string& key, float& value, float, float) { file << ",\n  \"" << key << "\": " << value; });
    file << "\n}\n";
    return static_cast<bool>(file);
}

void TitleCloudTransition::Initialize(SpriteCommon* common) {
    if (common_) return;
    common_ = common;
    static const bool loaded = LoadSettings(kSettingsPath); (void)loaded;
    backing_ = std::make_unique<Sprite>();
    backing_->Initialize(common_);
    backing_->SetTexture("resources/human/white.png");
    Sprite::WhiteGradientSettings gradient;
    gradient.enabled = true;
    gradient.startColor = {Linear(230.0f/255), Linear(240.0f/255), Linear(253.0f/255)};
    gradient.endColor = {Linear(217.0f/255), Linear(233.0f/255), Linear(252.0f/255)};
    backing_->SetWhiteGradient(gradient);
    backing_->SetTextureSampling(true);
    imagesAvailable_ = true;
    // Validate before the existing loader's assert path, once while the title loads.
    for (const auto* path : kTextures) {
        DirectX::ScratchImage image;
        if (FAILED(DirectX::LoadFromWICFile(std::filesystem::path(path).c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image))) {
            imagesAvailable_ = false;
            break;
        }
    }
    if (imagesAvailable_) {
        for (size_t i = 0; i < clouds_.size(); ++i) {
            clouds_[i] = std::make_unique<Sprite>();
            clouds_[i]->Initialize(common_);
            clouds_[i]->SetTexture(kTextures[i]);
            clouds_[i]->SetTextureSampling(true);
        }
    }
}

bool TitleCloudTransition::Start() {
    if (IsActive()) return false;
    activeSettings_ = GetSettings();
    Fields(activeSettings_, [](const std::string&, float& v, float low, float high) { v = std::isfinite(v) ? std::clamp(v, low, high) : low; });
    state_ = &State::For(Phase::Entering);
    ResetProgress();
    enterAtLowerEdge_ = false;
    return true;
}
bool TitleCloudTransition::StartAtLowerEdge() {
    if (!Start()) return false;
    enterAtLowerEdge_ = true;
    return true;
}
void TitleCloudTransition::Cancel() {
    state_ = &State::For(Phase::Idle);
    ResetProgress();
}
void TitleCloudTransition::ResetProgress() {
    elapsed_ = 0;
    coveredPresented_ = newScenePresented_ = coveredDrawn_ = clearDrawn_ = false;
}
void TitleCloudTransition::Update(float dt) {
    coveredDrawn_ = clearDrawn_ = false;
    if (!IsActive()) return;
    if (std::isfinite(dt) && dt > 0) elapsed_ += dt;
    state_ = &state_->Update(*this);
}
bool TitleCloudTransition::CanSwitch() const {
    return state_->CanSwitch(*this);
}
bool TitleCloudTransition::CanFinishTestLoad() const {
    return CanSwitch() && elapsed_ >= activeSettings_.holdSeconds * 0.5 + testLoadDelay;
}
bool TitleCloudTransition::IsCovering() const { return GetPhase() == Phase::Entering; }
TitleCloudTransition::Phase TitleCloudTransition::GetPhase() const { return state_->GetPhase(); }
void TitleCloudTransition::OnSceneChanged() {
    state_ = &state_->OnSceneChanged(*this);
}
void TitleCloudTransition::OnFramePresented() {
    // Draw(), then Present + fence, then this notification. Update alone is insufficient.
    state_ = &state_->OnFramePresented(*this);
    coveredDrawn_ = clearDrawn_ = false;
}
float TitleCloudTransition::GetBackingAlpha() const {
    return state_->GetBackingAlpha(*this);
}
TitleCloudTransition::LayerPose TitleCloudTransition::GetLayerPose(size_t i, Vector2 viewport) const {
    const auto& l = activeSettings_.layers.at(i);
    const float w = (std::max)(viewport.x, 1.0f), h = (std::max)(viewport.y, 1.0f);
    const float cover = (std::max)(w / 1672.0f, h / 941.0f) * l.width;
    const Vector2 size{1672.0f * cover, 941.0f * cover};
    // Keep both endpoints outside even for very wide aspect ratios and rotated corners.
    const float margin = (size.y + std::abs(l.rotation) * size.x) / (2 * h) + 2 / h;
    const auto motion = state_->GetLayerMotion(*this, {l, size.y, h, margin});
    return {{motion.center.x*w, motion.center.y*h}, size, motion.rotation};
}
void TitleCloudTransition::Draw() {
    if (!common_ || !backing_) return;
    state_->Draw(*this);
}
bool TitleCloudTransition::DrawLayers() {
    auto* dx = common_->GetDxCommon();
    auto* cmd = dx->GetCommandList();
    const auto viewport = dx->GetBackBufferViewport();
    const auto scissor = dx->GetBackBufferScissorRect();
    if (viewport.Width <= 0 || viewport.Height <= 0) return false;
    cmd->RSSetViewports(1, &viewport);
    cmd->RSSetScissorRects(1, &scissor);
    SrvManager::GetInstance()->PreDraw();
    common_->CommonDrawSetting(true);
    const Vector2 canvas{viewport.Width, viewport.Height};
    backing_->SetCanvasSize(canvas);
    backing_->SetPosition({-2, -2});
    backing_->SetSize({canvas.x+4, canvas.y+4});
    backing_->SetColor({1,1,1,GetBackingAlpha()});
    backing_->Update();
    backing_->Draw();
    if (imagesAvailable_) for (size_t i = 0; i < clouds_.size(); ++i) {
        const auto pose = GetLayerPose(i, canvas);
        auto& cloud = *clouds_[i];
        // Sprite uses its upper-left pivot; compensate to rotate around image centre.
        const float c = std::cos(pose.rotation), s = std::sin(pose.rotation);
        cloud.SetPosition({pose.center.x-(c*pose.size.x-s*pose.size.y)*0.5f,
            pose.center.y-(s*pose.size.x+c*pose.size.y)*0.5f});
        cloud.SetCanvasSize(canvas);
        cloud.SetSize(pose.size);
        cloud.SetRotation(pose.rotation);
        cloud.SetTextureSampling(true, activeSettings_.edgeFade);
        cloud.Update();
        cloud.Draw();
    }
    return true;
}

void TitleCloudTransition::ShowSettings() { settingsVisible = true; }
bool TitleCloudTransition::DrawImGui() {
    bool test = false;
#ifdef USE_IMGUI
    if (!settingsVisible) return false;
    DebugUiWorkspace::TrackWindow("titleTransition", "雲の画面遷移・調整");
    if (ImGui::Begin("雲の画面遷移・調整", &settingsVisible)) {
        auto& s = GetSettings();
        if (ImGui::Button("設定を保存")) status = SaveSettings(kSettingsPath) ? "保存しました。" : "保存できませんでした。";
        ImGui::SameLine();
        if (ImGui::Button("設定を読み込む")) status = LoadSettings(kSettingsPath) ? "読み込みました。" : "ファイルがないか読み込めませんでした。";
        ImGui::SameLine();
        if (ImGui::Button("採用値へ戻す")) s = Settings{};
        ImGui::TextWrapped("%s", status);
        ImGui::Text("画像: %s / 状態: %s", imagesAvailable_ ? "3枚を読み込み済み" : "欠落のため全画面Fade", state_->GetName());
        ImGui::SliderFloat("進入時間(実秒)", &s.enterSeconds, .2f, 8);
        ImGui::SliderFloat("最低保持時間(実秒)", &s.holdSeconds, .05f, 8);
        ImGui::SliderFloat("退出時間(実秒)", &s.exitSeconds, .2f, 8);
        ImGui::SliderFloat("画像上下のAlpha端フェード", &s.edgeFade, 0, .2f);
        ImGui::SliderFloat("下地を出し始める進入割合", &s.backingEnterStart, .5f, .95f);
        ImGui::SliderFloat("下地が消える退出割合", &s.backingExitEnd, .1f, .8f);
        ImGui::TextWrapped("下地: 上RGB(230,240,253) / 下RGB(217,233,252)。遮蔽時は常に不透明、外周2px余白。上下反転なし。描画順2→1→3。");
        const char* labels[]{"厚い雲・2", "主層・1", "斜め前景・3"};
        for (size_t i = 0; i < s.layers.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            auto& l = s.layers[i];
            if (ImGui::CollapsingHeader(labels[i])) {
                ImGui::SliderFloat("進入開始の遅れ(実秒)", &l.enterDelay, 0, s.enterSeconds*.8f);
                ImGui::SliderFloat("退出開始の遅れ(実秒)", &l.exitDelay, 0, s.exitSeconds*.8f);
                ImGui::SliderFloat("coverサイズ倍率", &l.width, 1.05f, 2.5f);
                ImGui::SliderFloat("開始中心x", &l.start.x, .2f, .8f);
                ImGui::SliderFloat("遮蔽中心x", &l.covered.x, .2f, .8f);
                ImGui::SliderFloat("終了中心x", &l.end.x, .2f, .8f);
            }
            ImGui::PopID();
        }
        ImGui::BeginDisabled(IsActive());
        ImGui::SliderFloat("テストの追加ロード待ち(実秒)", &testLoadDelay, 0, 5);
        test = ImGui::Button("テスト再生(シーン交換なし)");
        ImGui::EndDisabled();
    }
    ImGui::End();
#endif
    return test;
}
