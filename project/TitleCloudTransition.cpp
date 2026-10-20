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

TitleCloudTransition::TitleCloudTransition() = default;
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
    phase_ = Phase::Entering;
    elapsed_ = 0;
    coveredPresented_ = newScenePresented_ = coveredDrawn_ = clearDrawn_ = false;
    return true;
}
void TitleCloudTransition::Cancel() {
    phase_ = Phase::Idle;
    elapsed_ = 0;
    coveredPresented_ = newScenePresented_ = coveredDrawn_ = clearDrawn_ = false;
}
void TitleCloudTransition::Update(float dt) {
    coveredDrawn_ = clearDrawn_ = false;
    if (!IsActive()) return;
    if (std::isfinite(dt) && dt > 0) elapsed_ += dt;
    if (phase_ == Phase::Entering && elapsed_ >= activeSettings_.enterSeconds) {
        elapsed_ -= activeSettings_.enterSeconds;
        phase_ = Phase::Covered;
    } else if (phase_ == Phase::Waiting && newScenePresented_ && elapsed_ >= activeSettings_.holdSeconds) {
        // Do not apply synchronous load time to the reveal animation.
        elapsed_ = 0;
        phase_ = Phase::Exiting;
    }
}
bool TitleCloudTransition::CanSwitch() const {
    return phase_ == Phase::Covered && coveredPresented_ && elapsed_ >= activeSettings_.holdSeconds * 0.5;
}
bool TitleCloudTransition::CanFinishTestLoad() const {
    return CanSwitch() && elapsed_ >= activeSettings_.holdSeconds * 0.5 + testLoadDelay;
}
bool TitleCloudTransition::IsCovering() const { return phase_ == Phase::Entering; }
void TitleCloudTransition::OnSceneChanged() {
    if (!CanSwitch()) return;
    phase_ = Phase::Waiting;
    newScenePresented_ = false;
    coveredDrawn_ = false;
}
void TitleCloudTransition::OnFramePresented() {
    // Draw(), then Present + fence, then this notification. Update alone is insufficient.
    if (coveredDrawn_) {
        if (phase_ == Phase::Covered) coveredPresented_ = true;
        if (phase_ == Phase::Waiting) newScenePresented_ = true;
    }
    if (phase_ == Phase::Exiting && clearDrawn_) Cancel();
    coveredDrawn_ = clearDrawn_ = false;
}
float TitleCloudTransition::GetBackingAlpha() const {
    if (phase_ == Phase::Idle) return 0;
    if (phase_ == Phase::Covered || phase_ == Phase::Waiting) return 1;
    if (phase_ == Phase::Entering) {
        const float p = static_cast<float>(elapsed_ / activeSettings_.enterSeconds);
        // Missing PNGs use the same full-screen opaque fade, without holes.
        return Smooth(imagesAvailable_ ? (p - activeSettings_.backingEnterStart) / (1 - activeSettings_.backingEnterStart) : p);
    }
    return 1 - Smooth(static_cast<float>(elapsed_ / activeSettings_.exitSeconds) / (imagesAvailable_ ? activeSettings_.backingExitEnd : 1.0f));
}
TitleCloudTransition::LayerPose TitleCloudTransition::GetLayerPose(size_t i, Vector2 viewport) const {
    const auto& l = activeSettings_.layers.at(i);
    const float w = (std::max)(viewport.x, 1.0f), h = (std::max)(viewport.y, 1.0f);
    const float cover = (std::max)(w / 1672.0f, h / 941.0f) * l.width;
    const Vector2 size{1672.0f * cover, 941.0f * cover};
    // Keep both endpoints outside even for very wide aspect ratios and rotated corners.
    const float margin = (size.y + std::abs(l.rotation) * size.x) / (2 * h) + 2 / h;
    Vector2 start = l.start, end = l.end;
    start.y = (std::max)(start.y, 1 + margin);
    end.y = (std::max)(end.y, 1 + margin);
    Vector2 center = l.covered;
    float rotation = l.rotation;
    if (phase_ == Phase::Entering) {
        const float p = Smooth(static_cast<float>((elapsed_ - l.enterDelay) / (activeSettings_.enterSeconds - l.enterDelay)));
        center = {start.x + (l.covered.x-start.x)*p, start.y + (l.covered.y-start.y)*p};
        rotation *= p;
    } else if (phase_ == Phase::Exiting) {
        const float p = Smooth(static_cast<float>((elapsed_ - l.exitDelay) / (activeSettings_.exitSeconds - l.exitDelay)));
        center = {l.covered.x + (end.x-l.covered.x)*p, l.covered.y + (end.y-l.covered.y)*p};
    }
    return {{center.x*w, center.y*h}, size, rotation};
}
void TitleCloudTransition::Draw() {
    if (!IsActive() || !common_ || !backing_) return;
    if (phase_ == Phase::Exiting && elapsed_ >= activeSettings_.exitSeconds) { clearDrawn_ = true; return; }
    auto* dx = common_->GetDxCommon();
    auto* cmd = dx->GetCommandList();
    const auto viewport = dx->GetBackBufferViewport();
    const auto scissor = dx->GetBackBufferScissorRect();
    if (viewport.Width <= 0 || viewport.Height <= 0) return;
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
    coveredDrawn_ = (phase_ == Phase::Covered || phase_ == Phase::Waiting) && GetBackingAlpha() == 1;
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
        ImGui::Text("画像: %s / 状態: %s", imagesAvailable_ ? "3枚を読み込み済み" : "欠落のため全画面Fade", phase_ == Phase::Idle ? "待機" : phase_ == Phase::Entering ? "進入" : phase_ == Phase::Covered ? "遮蔽・交換待ち" : phase_ == Phase::Waiting ? "新シーン表示待ち" : "退出");
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
