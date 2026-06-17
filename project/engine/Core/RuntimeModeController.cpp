#include "RuntimeModeController.h"
#include "Engine/Input/Input.h"
#include "Engine/Core/WinApp.h"

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

RuntimeModeController* RuntimeModeController::activeController_ = nullptr;

RuntimeModeController::RuntimeModeController() = default;

RuntimeModeController::~RuntimeModeController() {
#ifdef _DEBUG
    RequestWindowFullscreen(false);
#endif
    if (activeController_ == this) {
        activeController_ = nullptr;
    }
}

void RuntimeModeController::Initialize(WinApp* winApp) {
    winApp_ = winApp;
    activeController_ = this;
#ifdef _DEBUG
    mode_ = RuntimeMode::Debug;
    RequestWindowFullscreen(false);
#else
    mode_ = RuntimeMode::Game;
    RequestWindowFullscreen(true);
#endif
}

void RuntimeModeController::Update(Input* input) {
#ifdef _DEBUG
    if (input && input->TriggerKey(DIK_F1)) {
        SetMode(IsDebugMode() ? RuntimeMode::Game : RuntimeMode::Debug);
    }
#else
    mode_ = RuntimeMode::Game;
    RequestWindowFullscreen(true);
#endif
}

void RuntimeModeController::DrawImGui() {
    DrawMenuBarImGui();
}

void RuntimeModeController::DrawMenuBarImGui() {
#ifdef _DEBUG
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
#ifdef _DEBUG
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

RuntimeModeController* RuntimeModeController::GetActiveController() {
    return activeController_;
}

void RuntimeModeController::DrawActiveMenuBarImGui() {
    if (activeController_) {
        activeController_->DrawMenuBarImGui();
    }
}

void RuntimeModeController::SetMode(RuntimeMode mode) {
#ifdef _DEBUG
    mode_ = mode;
    RequestWindowFullscreen(ShouldUseWindowFullscreen());
#else
    mode_ = RuntimeMode::Game;
    RequestWindowFullscreen(true);
#endif
}
