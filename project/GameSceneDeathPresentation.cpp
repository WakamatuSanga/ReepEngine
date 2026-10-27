#include "GameScene.h"
#include "MyGame.h"
#include "Engine/Core/FrameTimer.h"
#include "Engine/Core/GameViewport.h"
#include "Engine/Core/RuntimeModeController.h"
#include "Engine/Game/DebugGui/GameSceneDebugGui.h"
#include "Engine/Game/Effect/CombatEffectController.h"
#include "Engine/Game/Effect/GameplaySpeedLineController.h"
#include "Engine/Game/GameState/PlayerDeathSequenceController.h"
#include "Engine/Game/GameState/GameOverFlowController.h"
#include "Engine/Game/GameState/StageCombatState.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Game/Player/PlayerDamageFeedbackController.h"
#include "Engine/Game/Player/PlayerBulletManager.h"
#include "Engine/Game/Player/PlayerJetExhaustController.h"
#include "Engine/Game/Player/PlayerSonicBoostRingController.h"
#include "Engine/Game/Player/PlayerBarrelRollRingController.h"
#include "Engine/Game/Player/PlayerBulletCancelEffectController.h"
#include "Engine/Game/Enemy/EnemyDefeatEffectController.h"
#include "Engine/Game/Targeting/AimCorridorTargetingController.h"
#include "Engine/Game/UI/AimCorridorVisualController.h"
#include "Engine/Game/UI/PlayerHudController.h"
#include "Engine/Game/UI/WarningUIController.h"
#include "Engine/Graphics/Cloud/CloudVolume.h"
#include "Engine/Graphics/Cloud/VolumetricCloudPass.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Skybox/Skybox.h"
#include "Engine/Graphics/Effect/PrimitiveEffectSystem.h"

void GameScene::UpdateDeathPresentation() {
    const float raw=FrameTimer::GetInstance().GetGameplayDeltaTime();
    const float dt=playerDeathSequenceController_->GetPresentationDeltaTime(raw);
    if (stageCombatState_) stageCombatState_->SetGameplayActive(false);
    if (!deathPresentationEntered_) {
        deathPresentationEntered_=true;
        if (gameplaySpeedLineController_) gameplaySpeedLineController_->Reset();
        if (playerJetExhaustController_) playerJetExhaustController_->ResetDisplayState();
        if (warningUIController_) warningUIController_->HideWarning();
    }
    if (runtimeModeController_) runtimeModeController_->Update(MyGame::GetInstance()->GetInput());
    if (gameViewport_) gameViewport_->BeginFrame(runtimeModeController_->IsGameMode());
    if (player_) player_->SetGameViewInputActive(false);
    if (playerBulletManager_) playerBulletManager_->SetGameViewInputActive(false);
    if (aimCorridorTargetingController_) aimCorridorTargetingController_->SetPlayerAlive(false);
    if (aimCorridorVisualController_) aimCorridorVisualController_->SetPlayerAlive(false);
    playerDeathSequenceController_->Update(raw);
    if (playerDamageFeedbackController_) playerDamageFeedbackController_->Update(dt);
    if (combatEffectController_) combatEffectController_->Update(dt,camera_.get());
    if (primitiveEffectSystem_) primitiveEffectSystem_->Update(dt);
    if (playerSonicBoostRingController_) playerSonicBoostRingController_->Update(dt);
    if (playerBarrelRollRingController_) playerBarrelRollRingController_->Update(dt);
    if (playerBulletCancelEffectController_) playerBulletCancelEffectController_->Update(dt);
    if (enemyDefeatEffectController_) enemyDefeatEffectController_->Update(dt);
    if (cloudVolume_) cloudVolume_->Update(dt);
    if (auto* clouds=MyGame::GetInstance()->GetVolumetricCloudPass()) {
        clouds->SetExternalFlowMultiplier(1);clouds->SetExternalBoostExtraFlowSpeed(0);
    }
    if (playerHudController_) playerHudController_->Update(dt);
    if (gameOverFlowController_) gameOverFlowController_->Update();
#ifdef USE_IMGUI
    if (debugGui_) debugGui_->DrawImGui(MyGame::GetInstance()->GetDxCommon(),runtimeModeController_->ShouldDrawDebugUi(),MyGame::GetInstance()->GetVolumetricCloudPass());
#endif
    if (skybox_) {skybox_->SetTranslate(camera_->GetViewTranslate());skybox_->Update();}
    cloudProjectedBounds_=MyGame::GetInstance()->GetVolumetricCloudPass()->BuildProjectedBounds(camera_.get(),cloudVolume_.get());
}
