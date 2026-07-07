#include "GameScene.h"

void GameScene::Initialize() {
    InitializeSceneResources();
}

void GameScene::Update() {
    UpdateSceneRuntime();
}

void GameScene::Draw() {
    DrawSceneRender();
}

void GameScene::Finalize() {
    FinalizeSceneResources();
}
