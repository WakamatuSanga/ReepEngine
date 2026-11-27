#include "GameScene.h"

#include "MyGame.h"
#include "Engine/Game/Boss/Kraken/KrakenChasePreview.h"
#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossController.h"
#include "Engine/Game/Boss/Kraken/KrakenTentacleWaveEncounterController.h"
#include "Engine/Game/Enemy/EnemyBulletManager.h"
#include "Engine/Game/Enemy/EnemyManager.h"
#include "Engine/Game/GameState/StageCombatState.h"
#include "Engine/Game/Player/BoostController.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Game/Player/PlayerBulletManager.h"
#include "Engine/Graphics/Model/ModelManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

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
#ifdef USE_IMGUI
    ImGui::Checkbox("中ボス撃破後に本体追撃を試す", &krakenChaseAfterMidboss_);
    if (ImGui::Button(IsKrakenChasePreviewActive() ? "本体追撃をもう一度 (F7)" : "本体追撃を試走 (F7)")) {
        krakenChaseStartRequested_ = true;
    }
    if (krakenChasePreview_) krakenChasePreview_->DrawImGui();
    ImGui::Separator();
#endif
    if (krakenTentacleWaveEncounter_) {
        krakenTentacleWaveEncounter_->DrawImGui();
    }
}

bool GameScene::TryBeginKrakenChaseAfterMidboss() {
    return krakenChaseAfterMidboss_ && stageCombatState_ && stageCombatState_->IsGameplayActive()
        && krakenTentacleWaveEncounter_
        && krakenTentacleWaveEncounter_->GetState() == KrakenTentacleWaveEncounterState::Active
        && krakenTentacleMidboss_ && krakenTentacleMidboss_->IsDefeatStarted()
        && StartKrakenChasePreview();
}

bool GameScene::StartKrakenChasePreview() {
    if (!camera_ || !player_ || !railShooterCameraRig_) return false;
    auto* modelCommon = ModelManager::GetInstance()->GetModelCommon();
    auto* objectCommon = MyGame::GetInstance()->GetObject3dCommon();
    if (!modelCommon || !objectCommon) return false;
    if (!krakenChasePreview_) krakenChasePreview_ = std::make_unique<KrakenChasePreview>();
    if (!krakenChasePreview_->Start(*modelCommon, *objectCommon, *camera_, *railShooterCameraRig_,
        player_->GetWorldPosition())) return false;

    krakenChaseAfterMidboss_ = false;
    ResetKrakenTentacleWaveEncounter();
    if (boostController_) boostController_->Cancel();
    if (stageCombatState_) stageCombatState_->SetGameplayActive(false);
    if (playerBulletManager_) playerBulletManager_->DeleteAllBullets();
    if (enemyBulletManager_) enemyBulletManager_->DeleteAllBullets();
    if (enemyManager_) enemyManager_->DeleteAllEnemies();
    return true;
}

bool GameScene::IsKrakenChasePreviewActive() const {
    return krakenChasePreview_ && krakenChasePreview_->IsActive();
}

void GameScene::UpdateKrakenChasePreview(float deltaTime) {
    krakenChasePreview_->Update(deltaTime);
}

void GameScene::DrawKrakenChasePreview() const {
    if (krakenChasePreview_) krakenChasePreview_->Draw();
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
