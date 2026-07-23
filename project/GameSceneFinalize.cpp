#include "GameScene.h"
#include "Engine/Animation/Skeleton.h"
#include "Engine/Core/GameViewport.h"
#include "Engine/Core/RuntimeModeController.h"
#include "Engine/Editor/BlenderSync/BlenderLiveSync.h"
#include "Engine/Editor/Camera/EditorCameraController.h"
#include "Engine/Editor/SkinningEditor.h"
#include "Engine/Game/Camera/CameraShakeController.h"
#include "Engine/Game/Camera/RailShooterCameraRig.h"
#include "Engine/Game/Collision/PlayerBulletEnemyCollision.h"
#include "Engine/Game/Collision/PlayerEnemyBulletCollision.h"
#include "Engine/Game/DebugGui/GameSceneDebugGui.h"
#include "Engine/Game/Effect/CombatEffectController.h"
#include "Engine/Game/Effect/CombatSlowMotionController.h"
#include "Engine/Game/Effect/ImpactDistortionController.h"
#include "Engine/Game/Effect/GpuParticleEffectPlayer.h"
#include "Engine/Game/Effect/PostEffectController.h"
#include "Engine/Game/Enemy/EnemyAttackController.h"
#include "Engine/Game/Enemy/EnemyBulletManager.h"
#include "Engine/Game/Enemy/EnemyDefeatEffectController.h"
#include "Engine/Game/Enemy/EnemyLaserTelegraphController.h"
#include "Engine/Game/Enemy/EnemyManager.h"
#include "Engine/Game/Field/InfluenceFieldManager.h"
#include "Engine/Game/GameState/GameOverFlowController.h"
#include "Engine/Game/GameState/PlayerDeathSequenceController.h"
#include "Engine/Game/Player/BoostController.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Game/Player/PlayerDamageFeedbackController.h"
#include "Engine/Game/Player/PlayerBarrelRollRingController.h"
#include "Engine/Game/Player/PlayerBulletCancelEffectController.h"
#include "Engine/Game/Player/PlayerBulletManager.h"
#include "Engine/Game/Player/PlayerChargeFeedbackController.h"
#include "Engine/Game/Player/PlayerChargeGatherEffectController.h"
#include "Engine/Game/Player/PlayerJetExhaustController.h"
#include "Engine/Game/Player/PlayerRailController.h"
#include "Engine/Game/Player/PlayerRailFlightVisualTiltController.h"
#include "Engine/Game/Player/PlayerSonicBoostRingController.h"
#include "Engine/Game/RailShooter/EnemySpawnActionBridge.h"
#include "Engine/Game/RailShooter/EnemyWaveManager.h"
#include "Engine/Game/RailShooter/EventActionDispatcher.h"
#include "Engine/Game/RailShooter/PlayerEventTriggerBridge.h"
#include "Engine/Game/RailShooter/ProjectileRailMotionAdapter.h"
#include "Engine/Game/RailShooter/PostEffectActionBridge.h"
#include "Engine/Game/RailShooter/RailShooterEventActionBridge.h"
#include "Engine/Game/RailShooter/StartupEnemySpawnController.h"
#include "Engine/Game/Targeting/AimCorridorTargetingController.h"
#include "Engine/Game/UI/AimCorridorVisualController.h"
#include "Engine/Game/UI/PlayerHudController.h"
#include "Engine/Game/UI/WarningUIController.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Cloud/CloudVolume.h"
#include "Engine/Graphics/Effect/PrimitiveEffectSystem.h"
#include "Engine/Graphics/Model/GltfSkinnedModel.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Particle/GpuParticleSystem.h"
#include "Engine/Graphics/Shadow/ScreenSpaceFakeShadowPass.h"
#include "Engine/Graphics/Skybox/Skybox.h"
#include "Engine/Graphics/Sprite/Sprite.h"
#include "Engine/Level/LevelSceneRuntime.h"

GameScene::GameScene() = default;

GameScene::~GameScene() = default;

void GameScene::FinalizeSceneResources() {
    if (skinningEditor_) {
        skinningEditor_->ClearTargets();
    }
    debugGui_.reset();
    if (editorCameraController_) {
        editorCameraController_->Finalize();
    }
    editorCameraController_.reset();
    runtimeModeController_.reset();
    gameViewport_.reset();
    blenderLiveSync_.reset();
    if (eventActionDispatcher_) {
        eventActionDispatcher_->Finalize();
    }
    eventActionDispatcher_.reset();
    if (startupEnemySpawnController_) {
        startupEnemySpawnController_->Finalize();
    }
    startupEnemySpawnController_.reset();
    if (enemyWaveManager_) {
        enemyWaveManager_->Finalize();
    }
    enemyWaveManager_.reset();
    if (enemyLaserTelegraphController_) {
        enemyLaserTelegraphController_->Finalize();
    }
    enemyLaserTelegraphController_.reset();
    if (enemySpawnActionBridge_) {
        enemySpawnActionBridge_->Finalize();
    }
    enemySpawnActionBridge_.reset();
    if (postEffectActionBridge_) {
        postEffectActionBridge_->Finalize();
    }
    postEffectActionBridge_.reset();
    if (railShooterEventActionBridge_) {
        railShooterEventActionBridge_->Finalize();
    }
    railShooterEventActionBridge_.reset();
    if (playerRailFlightVisualTiltController_) {
        playerRailFlightVisualTiltController_->Finalize();
    }
    playerRailFlightVisualTiltController_.reset();
    if (railShooterCameraRig_) {
        railShooterCameraRig_->Finalize();
    }
    railShooterCameraRig_.reset();
    if (playerRailController_) {
        playerRailController_->Finalize();
    }
    playerRailController_.reset();
    if (playerEventTriggerBridge_) {
        playerEventTriggerBridge_->Finalize();
    }
    playerEventTriggerBridge_.reset();
    levelSceneRuntime_.reset();
    if (gameOverFlowController_) {
        gameOverFlowController_->Finalize();
    }
    gameOverFlowController_.reset();
    if (playerEnemyBulletCollision_) {
        playerEnemyBulletCollision_->Finalize();
    }
    playerEnemyBulletCollision_.reset();
    if (playerBulletEnemyCollision_) {
        playerBulletEnemyCollision_->Finalize();
    }
    playerBulletEnemyCollision_.reset();
    if (enemyDefeatEffectController_) {
        enemyDefeatEffectController_->Finalize();
    }
    enemyDefeatEffectController_.reset();
    if (impactDistortionController_) {
        impactDistortionController_->Finalize();
    }
    impactDistortionController_.reset();
    if (combatSlowMotionController_) {
        combatSlowMotionController_->Finalize();
    }
    combatSlowMotionController_.reset();
    if (playerBulletManager_) {
        playerBulletManager_->ClearAimCorridorContext();
    }
    if (aimCorridorTargetingController_) {
        aimCorridorTargetingController_->Finalize();
    }
    aimCorridorTargetingController_.reset();
    if (aimCorridorVisualController_) {
        aimCorridorVisualController_->Finalize();
    }
    aimCorridorVisualController_.reset();
    if (playerHudController_) {
        playerHudController_->Finalize();
    }
    playerHudController_.reset();
    if (playerDamageFeedbackController_) {
        playerDamageFeedbackController_->Finalize();
    }
    playerDamageFeedbackController_.reset();
    if (combatEffectController_) {
        combatEffectController_->Finalize();
    }
    combatEffectController_.reset();
    if (gpuParticleEffectPlayer_) {
        gpuParticleEffectPlayer_->Finalize();
    }
    gpuParticleEffectPlayer_.reset();
    if (enemyAttackController_) {
        enemyAttackController_->Finalize();
    }
    enemyAttackController_.reset();
    if (enemyBulletManager_) {
        enemyBulletManager_->Finalize();
    }
    enemyBulletManager_.reset();
    if (playerDeathSequenceController_) {
        playerDeathSequenceController_->Finalize();
    }
    playerDeathSequenceController_.reset();
    if (boostController_) {
        boostController_->SetPostEffectContext(nullptr, nullptr, nullptr);
    }
    if (warningUIController_) {
        warningUIController_->Finalize();
    }
    warningUIController_.reset();
    if (postEffectController_) {
        postEffectController_->Finalize();
    }
    postEffectController_.reset();
    screenSpaceFakeShadowPass_.reset();
    if (influenceFieldManager_) {
        influenceFieldManager_->Finalize();
    }
    influenceFieldManager_.reset();
    if (cameraShakeController_) {
        cameraShakeController_->Reset(camera_.get());
        cameraShakeController_->Finalize();
    }
    cameraShakeController_.reset();
    if (enemyManager_) {
        enemyManager_->Finalize();
    }
    enemyManager_.reset();
    if (playerChargeGatherEffectController_) {
        playerChargeGatherEffectController_->Finalize();
    }
    playerChargeGatherEffectController_.reset();
    if (playerChargeFeedbackController_) {
        playerChargeFeedbackController_->Finalize();
    }
    playerChargeFeedbackController_.reset();
    if (playerBulletManager_) {
        playerBulletManager_->Finalize();
    }
    playerBulletManager_.reset();
    if (playerBulletCancelEffectController_) {
        playerBulletCancelEffectController_->Finalize();
    }
    playerBulletCancelEffectController_.reset();
    if (playerBarrelRollRingController_) {
        playerBarrelRollRingController_->Finalize();
    }
    playerBarrelRollRingController_.reset();
    if (playerSonicBoostRingController_) {
        playerSonicBoostRingController_->Finalize();
    }
    playerSonicBoostRingController_.reset();
    if (playerJetExhaustController_) {
        playerJetExhaustController_->Finalize();
    }
    playerJetExhaustController_.reset();
    if (player_) {
        player_->Finalize();
    }
    player_.reset();
    if (boostController_) {
        boostController_->Finalize();
    }
    boostController_.reset();
}

