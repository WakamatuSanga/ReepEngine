#include "GameOverFlowController.h"
#include "GameOverScene.h"
#include "SceneManager.h"
#include "Engine/Game/GameState/PlayerDeathSequenceController.h"
#include <memory>

#ifdef _DEBUG
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
#ifdef _DEBUG
    ImGui::SetNextWindowSize(ImVec2(300.0f, 120.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("GameOver遷移確認 (Game Over Flow Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Transition Requested: %s", hasRequestedTransition_ ? "true" : "false");
    ImGui::End();
#endif
}
