#include "GameScene.h"

#include "MyGame.h"
#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossController.h"
#include "Engine/Graphics/Model/ModelManager.h"

void GameScene::InitializeKrakenTentacleMidboss() {
    FinalizeKrakenTentacleMidboss();

    auto* game = MyGame::GetInstance();
    auto* modelManager = ModelManager::GetInstance();
    if (!game || !modelManager) {
        return;
    }

    krakenTentacleMidboss_ =
        std::make_unique<KrakenTentacleMidbossController>();
    krakenTentacleMidboss_->Initialize(
        modelManager->GetModelCommon(),
        game->GetObject3dCommon());
    krakenTentacleMidboss_->SetCamera(camera_.get());
    krakenTentacleMidboss_->SetCollisionQueryContext(
        player_.get(), playerBulletManager_.get());
}

void GameScene::UpdateKrakenTentacleMidboss(float scaledDeltaTime) {
    if (!krakenTentacleMidboss_) {
        return;
    }
    krakenTentacleMidboss_->SetCamera(camera_.get());
    krakenTentacleMidboss_->Update(scaledDeltaTime);
}

void GameScene::DrawKrakenTentacleMidboss() {
    if (krakenTentacleMidboss_) {
        krakenTentacleMidboss_->Draw();
    }
}

void GameScene::DrawKrakenTentacleMidbossDebug() {
#ifdef USE_IMGUI
    if (krakenTentacleMidboss_) {
        krakenTentacleMidboss_->DrawDebug(
            gameViewTopLeft_[0],
            gameViewTopLeft_[1],
            gameViewSize_[0],
            gameViewSize_[1]);
    }
#endif
}

void GameScene::DrawKrakenTentacleMidbossImGui() {
    if (krakenTentacleMidboss_) {
        krakenTentacleMidboss_->DrawImGui();
    }
}

void GameScene::FinalizeKrakenTentacleMidboss() {
    if (krakenTentacleMidboss_) {
        krakenTentacleMidboss_->Finalize();
    }
    krakenTentacleMidboss_.reset();
}
