#include "RuntimeModeController.h"
#include "Engine/Core/FrameTimer.h"
#include "Engine/Input/Input.h"
#include "Engine/Core/WinApp.h"
#include "Engine/Graphics/Camera/Camera.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <cmath>

RuntimeModeController* RuntimeModeController::activeController_ = nullptr;

RuntimeModeController::RuntimeModeController() = default;

RuntimeModeController::~RuntimeModeController() {
#ifdef USE_IMGUI
    RequestWindowFullscreen(false);
#endif
    if (activeController_ == this) {
        activeController_ = nullptr;
    }
}

void RuntimeModeController::Initialize(WinApp* winApp) {
    winApp_ = winApp;
    activeController_ = this;
#ifdef USE_IMGUI
    mode_ = RuntimeMode::Debug;
    RequestWindowFullscreen(false);
#else
    mode_ = RuntimeMode::Game;
    RequestWindowFullscreen(true);
#endif
}

void RuntimeModeController::Update(Input* input) {
#ifdef USE_IMGUI
    if (input && input->TriggerKey(DIK_F1)) {
        SetMode(IsDebugMode() ? RuntimeMode::Game : RuntimeMode::Debug);
    }
#else
    mode_ = RuntimeMode::Game;
    RequestWindowFullscreen(true);
#endif
}

void RuntimeModeController::BeginCameraModeSwitchDiagnostics(const Camera* camera) {
    modeBeforeCameraDiagnostics_ = GetMode();
    cameraPoseBeforeModeSwitch_ = CaptureCameraPose(camera);
    cameraModeSwitchChangedThisFrame_ = false;
    cameraProjectionUpdatedOnModeSwitch_ = false;
    cameraPoseRestoredOnModeSwitch_ = false;
}

void RuntimeModeController::EndCameraModeSwitchDiagnostics(Camera* camera, bool projectionUpdated) {
    cameraPoseAfterModeSwitch_ = CaptureCameraPose(camera);
    cameraModeSwitchChangedThisFrame_ = modeBeforeCameraDiagnostics_ != GetMode();
    cameraProjectionUpdatedOnModeSwitch_ = projectionUpdated;

    if (!cameraModeSwitchChangedThisFrame_ || !preserveCameraPoseOnModeSwitch_) {
        return;
    }

    if (HasCameraPoseChanged(cameraPoseBeforeModeSwitch_, cameraPoseAfterModeSwitch_)) {
        RestoreCameraPose(camera, cameraPoseBeforeModeSwitch_);
        cameraPoseRestoredOnModeSwitch_ = true;
        cameraPoseAfterModeSwitch_ = CaptureCameraPose(camera);
    }
}

void RuntimeModeController::DrawImGui() {
#ifdef USE_IMGUI
    if (!ShouldDrawDebugUi()) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(440.0f, 560.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("実行モード / 性能確認 (Runtime Mode / Performance Debug)")) {
        ImGui::End();
        return;
    }

    FrameTimer& timer = FrameTimer::GetInstance();
    bool useClamp = timer.IsDeltaTimeClampEnabled();
    if (ImGui::Checkbox("DeltaTimeを制限する (Use DeltaTime Clamp)", &useClamp)) {
        timer.SetDeltaTimeClampEnabled(useClamp);
    }
    float maxDeltaTime = timer.GetMaxDeltaTime();
    if (ImGui::DragFloat("最大DeltaTime (Max DeltaTime)", &maxDeltaTime, 0.001f, 1.0f / 240.0f, 0.25f, "%.4f")) {
        timer.SetMaxDeltaTime(maxDeltaTime);
    }
    ImGui::Text("Runtime Mode: %s", IsDebugMode() ? "Debug" : "Game");
    ImGui::Text("FPS: %.1f", timer.GetFps());
    ImGui::Text("Frame Time ms: %.3f", timer.GetFrameTimeMs());
    ImGui::Text("Raw DeltaTime: %.5f", timer.GetRawDeltaTime());
    ImGui::Text("Clamped / Gameplay DeltaTime: %.5f", timer.GetGameplayDeltaTime());
    ImGui::Text("Frame Index: %llu", static_cast<unsigned long long>(timer.GetFrameIndex()));

    ImGui::SeparatorText("Game Mode Render Scale");
    ImGui::Checkbox("Game Mode描画倍率を使う (Use Game Mode Render Scale)", &useGameModeRenderScale_);
    const char* scaleNames[] = { "1.0", "0.75", "0.5", "0.25" };
    const float scales[] = { 1.0f, 0.75f, 0.5f, 0.25f };
    int scaleIndex = 0;
    for (int i = 0; i < IM_ARRAYSIZE(scales); ++i) {
        if (std::abs(gameModeRenderScale_ - scales[i]) < 0.001f) {
            scaleIndex = i;
        }
    }
    if (ImGui::Combo("Game Mode描画倍率 (Game Mode Render Scale)", &scaleIndex, scaleNames, IM_ARRAYSIZE(scaleNames))) {
        gameModeRenderScale_ = scales[scaleIndex];
    }
    if (ImGui::Button("描画倍率をリセット (Reset Render Scale)")) {
        useGameModeRenderScale_ = true;
        gameModeRenderScale_ = 1.0f;
    }
    ImGui::Checkbox("Game Mode軽量プリセット自動適用 (Auto Apply Game Mode Performance Preset)", &autoApplyGameModePerformancePreset_);
    ImGui::Text("Game Mode Internal Resolution: %u x %u",
        performanceStats_.renderTextureWidth,
        performanceStats_.renderTextureHeight);
    ImGui::Text("Internal Render Scale: %.2f", performanceStats_.internalRenderScale);
    ImGui::Text("BackBuffer Size: %d x %d", performanceStats_.windowWidth, performanceStats_.windowHeight);
    ImGui::Text("Internal Render Size: %u x %u", performanceStats_.renderTextureWidth, performanceStats_.renderTextureHeight);
    ImGui::Text("Scene RT Size: %u x %u", performanceStats_.renderTextureWidth, performanceStats_.renderTextureHeight);
    ImGui::Text("Depth RT Size: %u x %u", performanceStats_.depthTextureWidth, performanceStats_.depthTextureHeight);
    ImGui::Text("Current Viewport Size: %.1f x %.1f", performanceStats_.currentViewportWidth, performanceStats_.currentViewportHeight);
    ImGui::Text("Current Scissor Size: %d x %d", performanceStats_.currentScissorWidth, performanceStats_.currentScissorHeight);
    ImGui::Text("BackBuffer Viewport Size: %.1f x %.1f", performanceStats_.backBufferViewportWidth, performanceStats_.backBufferViewportHeight);
    ImGui::Text("BackBuffer Scissor Size: %d x %d", performanceStats_.backBufferScissorWidth, performanceStats_.backBufferScissorHeight);

    ImGui::SeparatorText("カメラ切り替え診断 (Camera Mode Switch Diagnostics)");
    ImGui::Checkbox("カメラ姿勢を保持する (Preserve Camera Pose On Mode Switch)", &preserveCameraPoseOnModeSwitch_);
    ImGui::Text("Mode Changed This Frame: %s", cameraModeSwitchChangedThisFrame_ ? "true" : "false");
    ImGui::Text("Projection Updated: %s", cameraProjectionUpdatedOnModeSwitch_ ? "true" : "false");
    ImGui::Text("Camera Pose Restored: %s", cameraPoseRestoredOnModeSwitch_ ? "true" : "false");
    if (cameraPoseBeforeModeSwitch_.valid && cameraPoseAfterModeSwitch_.valid) {
        ImGui::Text("Camera Position Before: %.3f, %.3f, %.3f",
            cameraPoseBeforeModeSwitch_.position[0],
            cameraPoseBeforeModeSwitch_.position[1],
            cameraPoseBeforeModeSwitch_.position[2]);
        ImGui::Text("Camera Position After : %.3f, %.3f, %.3f",
            cameraPoseAfterModeSwitch_.position[0],
            cameraPoseAfterModeSwitch_.position[1],
            cameraPoseAfterModeSwitch_.position[2]);
        ImGui::Text("Rotation Before: %.3f, %.3f, %.3f",
            cameraPoseBeforeModeSwitch_.rotation[0],
            cameraPoseBeforeModeSwitch_.rotation[1],
            cameraPoseBeforeModeSwitch_.rotation[2]);
        ImGui::Text("Rotation After : %.3f, %.3f, %.3f",
            cameraPoseAfterModeSwitch_.rotation[0],
            cameraPoseAfterModeSwitch_.rotation[1],
            cameraPoseAfterModeSwitch_.rotation[2]);
        ImGui::Text("Forward Before: %.3f, %.3f, %.3f",
            cameraPoseBeforeModeSwitch_.forward[0],
            cameraPoseBeforeModeSwitch_.forward[1],
            cameraPoseBeforeModeSwitch_.forward[2]);
        ImGui::Text("Forward After : %.3f, %.3f, %.3f",
            cameraPoseAfterModeSwitch_.forward[0],
            cameraPoseAfterModeSwitch_.forward[1],
            cameraPoseAfterModeSwitch_.forward[2]);
        ImGui::Text("FOV Before / After: %.4f / %.4f",
            cameraPoseBeforeModeSwitch_.fovY,
            cameraPoseAfterModeSwitch_.fovY);
        ImGui::Text("Aspect Before / After: %.4f / %.4f",
            cameraPoseBeforeModeSwitch_.aspectRatio,
            cameraPoseAfterModeSwitch_.aspectRatio);
    }

    ImGui::SeparatorText("Frame / Viewport");
    ImGui::Text("Window Size: %d x %d", performanceStats_.windowWidth, performanceStats_.windowHeight);
    ImGui::Text("Render Target Size: %u x %u", performanceStats_.renderTextureWidth, performanceStats_.renderTextureHeight);
    ImGui::Text("Game Viewport Size: %.1f x %.1f", performanceStats_.gameViewportWidth, performanceStats_.gameViewportHeight);
    ImGui::Text("Present Interval / VSync: %u / %s",
        performanceStats_.presentInterval,
        performanceStats_.presentInterval > 0 ? "ON" : "OFF");
    ImGui::Text("Fixed FPS Wait: %s", performanceStats_.fixedFpsWaitEnabled ? "ON" : "OFF");

    ImGui::SeparatorText("Runtime Counts");
    ImGui::Text("Active Enemy Count: %zu", performanceStats_.activeEnemyCount);
    ImGui::Text("Enemy Bullet Count: %zu", performanceStats_.enemyBulletCount);
    ImGui::Text("Player Bullet Count: %zu", performanceStats_.playerBulletCount);
    ImGui::Text("Primitive Effect Count: %zu", performanceStats_.primitiveEffectCount);
    ImGui::Text("GPU Particle Active Estimate: %u", performanceStats_.gpuParticleActiveEstimate);
    ImGui::TextDisabled("Draw Call Count: not collected yet");

    ImGui::SeparatorText("Cloud");
    ImGui::Text("Cloud Enabled: %s", performanceStats_.cloudEnabled ? "true" : "false");
    ImGui::Text("Low Resolution Cloud: %s", performanceStats_.lowResolutionCloudEnabled ? "true" : "false");
    ImGui::Text("Cloud Composite: %s", performanceStats_.cloudCompositeEnabled ? "true" : "false");
    ImGui::Text("Depth-aware Upsample: %s", performanceStats_.depthAwareCloudUpsampleEnabled ? "true" : "false");
    ImGui::Text("Cloud Resolution Scale: %.3f", performanceStats_.cloudResolutionScale);

    ImGui::SeparatorText("影っぽい表示の確認用 (Shadow-like Debug)");
    ImGui::TextWrapped("画面が影っぽく見える原因を切り分けるための一時スイッチです。見た目を恒久的に消すものではありません。");
    ImGui::SeparatorText("RenderScale Artifact Debug");
    ImGui::Checkbox("Clear Color Test", &shadowLikeDebugSettings_.clearColorTest);
    ImGui::TextWrapped("ONにすると内部RTのClear色を派手な色にして、未Clear領域や前フレーム残りを見つけやすくします。");
    ImGui::Checkbox("雲を無効化 (Disable Clouds)", &shadowLikeDebugSettings_.disableClouds);
    ImGui::Checkbox("雲合成を無効化 (Disable Cloud Composite)", &shadowLikeDebugSettings_.disableCloudComposite);
    ImGui::Checkbox("深度にじみ抑制を無効化 (Disable Depth-aware Upsample)", &shadowLikeDebugSettings_.disableDepthAwareUpsample);
    ImGui::Checkbox("ポストエフェクトを無効化 (Disable PostEffects)", &shadowLikeDebugSettings_.disablePostEffects);
    ImGui::Checkbox("Fake Shadowを無効化 (Disable Fake Shadow)", &shadowLikeDebugSettings_.disableFakeShadow);
    ImGui::Checkbox("エフェクト/パーティクルを無効化 (Disable Effects)", &shadowLikeDebugSettings_.disableEffects);
    ImGui::Checkbox("PrimitiveEffectを無効化 (Disable PrimitiveEffect)", &shadowLikeDebugSettings_.disablePrimitiveEffect);
    ImGui::Checkbox("GPU Particleを無効化 (Disable GPU Particle)", &shadowLikeDebugSettings_.disableGpuParticle);
    ImGui::Checkbox("PBRライティングを無効化 (Disable PBR Lighting)", &shadowLikeDebugSettings_.disablePbrLighting);
    ImGui::TextWrapped("Current Suspected Source: %s", GetCurrentSuspectedShadowSource());
    ImGui::End();
#endif
}

void RuntimeModeController::DrawMenuBarImGui() {
#ifdef USE_IMGUI
    ImGui::Separator();
    ImGui::TextDisabled("Runtime: %s", IsDebugMode() ? "Debug" : "Game");
    ImGui::SameLine();
    if (ImGui::SmallButton("Debug Mode")) {
        SetMode(RuntimeMode::Debug);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Game Mode")) {
        SetMode(RuntimeMode::Game);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("F1 Toggle");
#endif
}

RuntimeMode RuntimeModeController::GetMode() const {
#ifdef USE_IMGUI
    return mode_;
#else
    return RuntimeMode::Game;
#endif
}

bool RuntimeModeController::IsDebugMode() const {
    return GetMode() == RuntimeMode::Debug;
}

bool RuntimeModeController::IsGameMode() const {
    return GetMode() == RuntimeMode::Game;
}

bool RuntimeModeController::ShouldDrawDebugUi() const {
    return IsDebugMode();
}

bool RuntimeModeController::ShouldDrawLevelDebug() const {
    return IsDebugMode();
}

bool RuntimeModeController::ShouldDrawEventDebug() const {
    return IsDebugMode();
}

bool RuntimeModeController::ShouldDrawRailDebug() const {
    return IsDebugMode();
}

bool RuntimeModeController::ShouldKeepDockSpaceAlive() const {
    return true;
}

bool RuntimeModeController::ShouldUseGameViewFullscreenPanel() const {
    return false;
}

bool RuntimeModeController::ShouldUseWindowFullscreen() const {
    return IsGameMode();
}

void RuntimeModeController::RequestWindowFullscreen(bool fullscreen) {
    requestedWindowFullscreen_ = fullscreen;
    if (winApp_) {
        winApp_->SetFullscreen(fullscreen);
    }
}

void RuntimeModeController::SetPerformanceStats(const PerformanceStats& stats) {
    performanceStats_ = stats;
}

float RuntimeModeController::GetDesiredRenderScale() const {
    if (IsGameMode() && useGameModeRenderScale_) {
        return std::clamp(gameModeRenderScale_, 0.25f, 1.0f);
    }
    return 1.0f;
}

RuntimeModeController::CameraPoseSnapshot RuntimeModeController::CaptureCameraPose(const Camera* camera) const {
    CameraPoseSnapshot pose{};
    if (!camera) {
        return pose;
    }

    const Vector3& position = camera->GetTranslate();
    const Vector3& rotation = camera->GetRotate();
    const Matrix4x4& world = camera->GetWorldMatrix();
    pose.position[0] = position.x;
    pose.position[1] = position.y;
    pose.position[2] = position.z;
    pose.rotation[0] = rotation.x;
    pose.rotation[1] = rotation.y;
    pose.rotation[2] = rotation.z;
    pose.forward[0] = world.m[2][0];
    pose.forward[1] = world.m[2][1];
    pose.forward[2] = world.m[2][2];
    pose.fovY = camera->GetFovY();
    pose.aspectRatio = camera->GetAspectRatio();
    pose.nearClip = camera->GetNearClip();
    pose.farClip = camera->GetFarClip();
    pose.valid = true;
    return pose;
}

bool RuntimeModeController::HasCameraPoseChanged(const CameraPoseSnapshot& before, const CameraPoseSnapshot& after) const {
    if (!before.valid || !after.valid) {
        return false;
    }

    constexpr float kPoseEpsilon = 0.0001f;
    for (int i = 0; i < 3; ++i) {
        if (std::abs(before.position[i] - after.position[i]) > kPoseEpsilon) {
            return true;
        }
        if (std::abs(before.rotation[i] - after.rotation[i]) > kPoseEpsilon) {
            return true;
        }
    }
    return
        std::abs(before.fovY - after.fovY) > kPoseEpsilon ||
        std::abs(before.nearClip - after.nearClip) > kPoseEpsilon ||
        std::abs(before.farClip - after.farClip) > kPoseEpsilon;
}

void RuntimeModeController::RestoreCameraPose(Camera* camera, const CameraPoseSnapshot& pose) const {
    if (!camera || !pose.valid) {
        return;
    }

    camera->SetTranslate({ pose.position[0], pose.position[1], pose.position[2] });
    camera->SetRotate({ pose.rotation[0], pose.rotation[1], pose.rotation[2] });
    camera->SetFovY(pose.fovY);
    camera->SetNearClip(pose.nearClip);
    camera->SetFarClip(pose.farClip);
    camera->Update();
}

const char* RuntimeModeController::GetCurrentSuspectedShadowSource() const {
    if (shadowLikeDebugSettings_.disableClouds) {
        return "Clouds hidden for test";
    }
    if (shadowLikeDebugSettings_.disableCloudComposite) {
        return "Cloud composite hidden for test";
    }
    if (shadowLikeDebugSettings_.disableDepthAwareUpsample) {
        return "Depth-aware upsample hidden for test";
    }
    if (shadowLikeDebugSettings_.disablePostEffects) {
        return "PostEffects hidden for test";
    }
    if (shadowLikeDebugSettings_.disableFakeShadow) {
        return "Fake Shadow hidden for test";
    }
    if (shadowLikeDebugSettings_.disableEffects) {
        return "Primitive/GPU effects hidden for test";
    }
    if (shadowLikeDebugSettings_.disablePrimitiveEffect) {
        return "PrimitiveEffect hidden for test";
    }
    if (shadowLikeDebugSettings_.disableGpuParticle) {
        return "GPU Particle hidden for test";
    }
    if (shadowLikeDebugSettings_.disablePbrLighting) {
        return "PBR lighting disabled for test";
    }
    return "Unknown - toggle one diagnostic switch at a time";
}

RuntimeModeController* RuntimeModeController::GetActiveController() {
    return activeController_;
}

void RuntimeModeController::DrawActiveMenuBarImGui() {
    if (activeController_) {
        activeController_->DrawMenuBarImGui();
    }
}

void RuntimeModeController::SetMode(RuntimeMode mode) {
#ifdef USE_IMGUI
    mode_ = mode;
    RequestWindowFullscreen(ShouldUseWindowFullscreen());
#else
    mode_ = RuntimeMode::Game;
    RequestWindowFullscreen(true);
#endif
}
