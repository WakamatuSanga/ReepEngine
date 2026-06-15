#include "TitleScene.h"
#include "SceneManager.h"
#include "GameScene.h" 
#include "MyGame.h"    
#include "Engine/Input/Input.h"

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

void TitleScene::Initialize() {}

void TitleScene::Finalize() {}

void TitleScene::Update() {
#ifdef _DEBUG
    ImGui::Begin("Title Scene");
    ImGui::Text("This is Title Scene.");
    ImGui::Text("Left click to Start Game!");
    ImGui::End();
#endif

    Input* input = MyGame::GetInstance()->GetInput();
#ifdef _DEBUG
    const ImGuiIO& io = ImGui::GetIO();
    const bool isImGuiCapturingMouse = io.WantCaptureMouse || ImGui::IsAnyItemActive();
#else
    const bool isImGuiCapturingMouse = false;
#endif

    if (input && input->MouseTrigger(Input::MouseLeft) && !isImGuiCapturingMouse) {
        SceneManager::GetInstance()->ChangeScene(std::make_unique<GameScene>());
    }
}

void TitleScene::Draw() {}
