#include "GameScene.h"

#include "Engine/Game/Boss/Kraken/KrakenTentacleWaveEncounterController.h"

void GameScene::InitializeKrakenTentacleWaveEncounter() {
    FinalizeKrakenTentacleWaveEncounter();
    krakenTentacleWaveEncounter_ =
        std::make_unique<KrakenTentacleWaveEncounterController>();
    krakenTentacleWaveEncounter_->Initialize(
        enemyWaveManager_.get(),
        krakenTentacleMidboss_.get(),
        railShooterCameraRig_.get(),
        camera_.get());
}

void GameScene::PreUpdateKrakenTentacleWaveEncounter(
    float scaledDeltaTime) {
    if (krakenTentacleWaveEncounter_) {
        krakenTentacleWaveEncounter_->PreRailUpdate(scaledDeltaTime);
    }
}

void GameScene::PostKrakenUpdateKrakenTentacleWaveEncounter() {
    if (krakenTentacleWaveEncounter_) {
        krakenTentacleWaveEncounter_->PostKrakenUpdate();
    }
}

void GameScene::PostWaveUpdateKrakenTentacleWaveEncounter(
    float scaledDeltaTime) {
    if (krakenTentacleWaveEncounter_) {
        krakenTentacleWaveEncounter_->PostWaveUpdate(scaledDeltaTime);
    }
}

void GameScene::DrawKrakenTentacleWaveEncounterImGui() {
    if (krakenTentacleWaveEncounter_) {
        krakenTentacleWaveEncounter_->DrawImGui();
    }
}

void GameScene::ResetKrakenTentacleWaveEncounter() {
    if (krakenTentacleWaveEncounter_) {
        krakenTentacleWaveEncounter_->Reset();
    }
}

void GameScene::FinalizeKrakenTentacleWaveEncounter() {
    if (krakenTentacleWaveEncounter_) {
        krakenTentacleWaveEncounter_->Finalize();
    }
    krakenTentacleWaveEncounter_.reset();
}
