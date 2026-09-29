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
    FrameTimer::GetInstance().SetBulletMeasurementGameMode(IsGameMode());
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
    ImGui::TextWrapped("Game Mode Render Scale はゲーム画面全体の内部解像度です。Player / Enemy / Bullet も低解像度になります。雲だけ軽くしたい場合は Volumetric Cloud の Cloud Resolution Scale を下げてください。");
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
    ImGui::SameLine();
    if (ImGui::Button("画面品質優先 (Screen Quality)")) { useGameModeRenderScale_ = true; gameModeRenderScale_ = 1.0f; }
    ImGui::SameLine();
    if (ImGui::Button("全体軽量 (Overall Light)")) { useGameModeRenderScale_ = true; gameModeRenderScale_ = 0.75f; }
    ImGui::Checkbox("Game Mode軽量プリセット自動適用 (Auto Apply Game Mode Performance Preset)", &autoApplyGameModePerformancePreset_);
    ImGui::Text("Internal Render Scale: %.2f", performanceStats_.internalRenderScale);
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

    if (ImGui::CollapsingHeader("弾の負荷計測")) {
        bool measuring = timer.IsBulletMeasurementEnabled();
        if (ImGui::Checkbox("弾の計測を有効にする", &measuring)) {
            timer.SetBulletMeasurementEnabled(measuring);
        }
        ImGui::SameLine();
        if (ImGui::Button("計測区間をリセット")) {
            timer.ResetBulletMeasurement();
        }
#ifdef _DEBUG
        ImGui::TextUnformatted("ビルド構成：Debug x64");
#else
        ImGui::TextUnformatted("ビルド構成：Development x64");
#endif
        ImGui::Text("生存弾数：プレイヤー %zu / 敵 %zu（敵本体 %zu）",
            performanceStats_.playerBulletCount, performanceStats_.enemyBulletCount,
            performanceStats_.activeEnemyCount);
        const auto& report = timer.GetBulletReport();
        ImGui::Text("現在のRuntime %s：%.2f 秒 / %llu フレーム",
            IsGameMode() ? "Game" : "Debug", report.seconds,
            static_cast<unsigned long long>(report.frames));
        ImGui::TextWrapped("前フレームの値と、約1秒ごとに更新する1フレームあたりの平均・最大です。モード切り替え時は未完了区間と切り替えフレームを除外し、新しい区間を開始します。区間確定前は平均・最大が0です。");
        constexpr const char* labels[] = {
            "フレーム時間 (ms)", "生存プレイヤー弾 (発)", "生存敵弾 (発)",
            "プレイヤー弾生成 (発/フレーム)", "敵弾生成 (発/フレーム)",
            "プレイヤー弾生成CPU (ms)", "敵弾生成CPU (ms)",
            "プレイヤー弾更新CPU (ms)", "敵弾更新CPU (ms)",
            "弾→通常敵の衝突CPU (ms)", "敵弾→プレイヤーの衝突CPU (ms)",
            "Kraken衝突・命中処理CPU (ms)", "敵弾消去判定CPU (ms)",
            "プレイヤー弾描画命令CPU (ms)", "敵弾描画命令CPU (ms)",
            "プレイヤー弾モデルDraw (回/フレーム)", "敵弾モデルDraw (回/フレーム)",
            "全体更新CPU (ms)", "描画命令作成全体CPU (ms)",
            "描画終端・コマンド送信CPU (ms)", "Present呼び出しCPU (ms)",
            "既存GPU完了待ちCPU (ms)", "固定FPS制御CPU (ms)",
            "コマンド再設定CPU (ms)", "OSメッセージ処理CPU (ms)",
            "計測外・差分 (ms)"
        };
        static_assert(IM_ARRAYSIZE(labels) == static_cast<int>(FrameTimer::BulletMetric::Count));
        const auto drawReport = [&](const char* id, const FrameTimer::BulletReport& values, const char* lastLabel) {
            if (!ImGui::BeginTable(id, 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                return;
            }
            ImGui::TableSetupColumn("計測項目", ImGuiTableColumnFlags_WidthStretch, 3.0f);
            ImGui::TableSetupColumn(lastLabel, ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("区間平均", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("区間最大", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableHeadersRow();
            for (size_t i = 0; i < values.last.size(); ++i) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextWrapped("%s", labels[i]);
                ImGui::TableNextColumn(); ImGui::Text("%.3f", values.last[i]);
                ImGui::TableNextColumn(); ImGui::Text("%.3f", values.average[i]);
                ImGui::TableNextColumn(); ImGui::Text("%.3f", values.maximum[i]);
            }
            ImGui::EndTable();
        };
        drawReport("BulletPerformance", report, "前フレーム");
        ImGui::SeparatorText("直近のGame Mode計測結果");
        const auto& gameReport = timer.GetLastGameBulletReport();
        if (gameReport.frames == 0) {
            ImGui::TextUnformatted("未取得");
        } else {
            ImGui::Text("保存区間：%.2f 秒 / %llu フレーム",
                gameReport.seconds, static_cast<unsigned long long>(gameReport.frames));
            drawReport("GameBulletPerformance", gameReport, "区間末フレーム");
        }
        ImGui::TextWrapped("Game ModeではUI非表示でも計測が継続します。保存表の全項目は同じ完了区間の値で、Debug中は更新しません。計測リセット・ON/OFF切り替えで保存結果も解除します。");
        ImGui::TextWrapped("生成CPUは共通SpawnBullet内（確保・初期化・登録）です。照準計算・チャージ軌道準備・ミサイル追加設定は含みません。プレイヤー弾には通常弾・チャージ弾・ミサイルを含みます。");
        ImGui::TextWrapped("更新CPUは入力・生成・破棄・ミサイル排気更新を含むManager全体です。衝突CPUは命中後の処理を含み、Kraken欄は対Player攻撃判定も含みます。生成は更新に重複するため合算できません。各行の最大値も同じフレームとは限りません。");
        ImGui::TextWrapped("描画CPUは命令作成時間でありGPU時間ではありません。判定表示・ミサイル排気を含みます。Draw回数は弾モデルと判定表示の実描画命令のみで、GPU粒子の排気や命中エフェクトは除外します。");
        ImGui::TextWrapped("GPU時間：未計測。フレーム時間にはVSync・GPU同期・固定FPS待機を含みます。計測OFF時は時計取得・集計を省きます。");
        ImGui::TextWrapped("全体内訳はCPU側の経過時間です。フレーム時間はUpdate冒頭のBeginFrameから次回BeginFrameまで。全体更新は入力・ImGui・シーン更新、描画命令作成全体はPreDrawからImGui描画まで（PostDrawは除外）です。");
        ImGui::TextWrapped("PostDrawは終端Barrier・Close・Execute、Present、FenceのSignal・完了確認・既存待機、固定FPS制御、Allocator/ListのResetに分割しています。OSメッセージ処理は次回BeginFrame直前です。新たな待機は追加していません。");
        ImGui::TextWrapped("全体更新～OSメッセージ処理の8行は重複しません。平均の合計＋計測外・差分の平均＝フレーム平均です。弾の各時間は内訳に含まれるため二重に足さないでください。最大値同士は別フレームの場合があり合算できません。");
        ImGui::TextWrapped("計測外・差分はBeginFrame内の集計・区間間の処理・計測オーバーヘッド等です。CPU側の経過時間にはOSによる中断も含みます。PresentやFence待ちだけからGPU実行時間は断定できません。");
    }

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
    FrameTimer::GetInstance().SetBulletCounts(stats.playerBulletCount, stats.enemyBulletCount);
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
    FrameTimer::GetInstance().SetBulletMeasurementGameMode(IsGameMode());
}
