#include "GameScene.h"

#include "MyGame.h"
#include "Engine/Core/FrameTimer.h"
#include "Engine/Core/GameViewport.h"
#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossController.h"
#include "Engine/Game/Boss/Kraken/KrakenTentacleWaveEncounterController.h"
#include "Engine/Game/Camera/CameraShakeController.h"
#include "Engine/Game/Effect/CombatEffectController.h"
#include "Engine/Game/Effect/GameplaySpeedLineController.h"
#include "Engine/Game/Effect/ImpactDistortionController.h"
#include "Engine/Game/Effect/PostEffectController.h"
#include "Engine/Game/Enemy/EnemyBulletManager.h"
#include "Engine/Game/Enemy/EnemyDefeatEffectController.h"
#include "Engine/Game/Enemy/EnemyManager.h"
#include "Engine/Game/GameState/StageClearSequenceController.h"
#include "Engine/Game/GameState/StageCombatState.h"
#include "Engine/Game/Player/BoostController.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Game/Player/PlayerBulletManager.h"
#include "Engine/Game/Player/PlayerJetExhaustController.h"
#include "Engine/Game/RailShooter/EnemyWaveManager.h"
#include "Engine/Game/Targeting/AimCorridorTargetingController.h"
#include "Engine/Game/UI/WarningUIController.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Cloud/CloudVolume.h"
#include "Engine/Graphics/Cloud/VolumetricCloudPass.h"
#include "Engine/Graphics/Effect/PrimitiveEffectSystem.h"
#include "Engine/Graphics/Skybox/Skybox.h"

bool GameScene::TryBeginStageClear() {
    if (!stageClear_ || stageClear_->IsActive() || !StageClearSequenceController::IsMidbossClearEnabled() ||
        !stageCombatState_ || !stageCombatState_->IsGameplayActive() || !krakenTentacleWaveEncounter_ ||
        krakenTentacleWaveEncounter_->GetState() != KrakenTentacleWaveEncounterState::Active ||
        !krakenTentacleMidboss_ || !krakenTentacleMidboss_->IsDefeatStarted()) {
        return false;
    }

    // The projectile damage code has already awarded the final hit at this point.
    stageClear_->Begin(stageCombatState_->GetScore(), stageCombatState_->GetDefeatCount());
    stageCombatState_->SetGameplayActive(false);
    krakenTentacleMidboss_->BeginStageClearRetreat();
    if (player_) player_->SetGameViewInputActive(false);
    if (boostController_) boostController_->Cancel();
    if (postEffectController_) postEffectController_->ClearBoostEffects();
    if (cameraShakeController_) cameraShakeController_->SetGameplayEventsEnabled(false);
    if (playerBulletManager_) {
        playerBulletManager_->SetGameViewInputActive(false);
        playerBulletManager_->DeleteAllBullets();
    }
    if (enemyBulletManager_) enemyBulletManager_->DeleteAllBullets();
    if (enemyWaveManager_) enemyWaveManager_->StopAllWaves();
    if (enemyManager_) enemyManager_->DeleteAllEnemies();
    if (aimCorridorTargetingController_) aimCorridorTargetingController_->Reset();
    if (gameplaySpeedLineController_) gameplaySpeedLineController_->Reset();
    if (warningUIController_) warningUIController_->HideWarning();
    if (gameViewport_) gameViewport_->SetGameplayCursorHidden(false);
    return true;
}

void GameScene::UpdateStageClearPresentation() {
    const float dt = FrameTimer::GetInstance().GetGameplayDeltaTime();
    if (gameViewport_) gameViewport_->BeginFrame(true);
    if (stageClear_->NeedsTentacleUpdate()) {
        UpdateKrakenTentacleMidboss(dt);
    }
    stageClear_->Update(dt, !krakenTentacleMidboss_ || !krakenTentacleMidboss_->IsVisible());
    if (stageClear_->NeedsTentacleUpdate() && playerJetExhaustController_) playerJetExhaustController_->Update(dt);
    if (stageClear_->IsAircraftVisible()) {
        if (combatEffectController_) combatEffectController_->Update(dt, camera_.get());
        if (primitiveEffectSystem_) primitiveEffectSystem_->Update(dt);
        if (impactDistortionController_) impactDistortionController_->Update(dt);
        if (enemyDefeatEffectController_) enemyDefeatEffectController_->Update(dt);
    }
    if (skybox_) {
        skybox_->SetTranslate(camera_->GetViewTranslate());
        skybox_->Update();
    }
    if (cloudVolume_) cloudVolume_->Update(dt);
    if (auto* clouds = MyGame::GetInstance()->GetVolumetricCloudPass()) {
        clouds->SetGameplayFlowActive(true);
        clouds->SetExternalFlowMultiplier(1.0f);
        clouds->SetExternalBoostExtraFlowSpeed(0.0f);
        cloudProjectedBounds_ = clouds->BuildProjectedBounds(camera_.get(), cloudVolume_.get());
    }
}
