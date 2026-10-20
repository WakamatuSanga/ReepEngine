#include "Engine/Core/DebugUiWorkspace.h"
#include "GameOverFlowController.h"
#include "GameOverScene.h"
#include "SceneManager.h"
#include "Engine/Game/GameState/PlayerDeathSequenceController.h"
#include <memory>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

GameOverFlowController::GameOverFlowController() = default;

GameOverFlowController::~GameOverFlowController() = default;

void GameOverFlowController::Initialize(PlayerDeathSequenceController* deathSequence) {
    deathSequence_ = deathSequence;
    hasRequestedTransition_ = false;
}

void GameOverFlowController::Finalize() {
    deathSequence_ = nullptr;
    hasRequestedTransition_ = false;
}

void GameOverFlowController::Update() {
    if (hasRequestedTransition_ || !deathSequence_) {
        return;
    }

    if (deathSequence_->ConsumeFinished()) {
        hasRequestedTransition_ = true;
        SceneManager::GetInstance()->ChangeScene(std::make_unique<GameOverScene>());
    }
}

void GameOverFlowController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(300.0f, 120.0f), ImGuiCond_FirstUseEver);
    if (!DebugUiWorkspace::BeginPanel("GameOver遷移確認 (Game Over Flow Debug)")) {
        DebugUiWorkspace::EndPanel();
        return;
    }

    ImGui::Text("Transition Requested: %s", hasRequestedTransition_ ? "true" : "false");
    DebugUiWorkspace::EndPanel();
#endif
}

