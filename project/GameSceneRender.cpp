#include "GameScene.h"
#include "MyGame.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Core/RuntimeModeController.h"
#include "Engine/Editor/SkinningEditor.h"
#include "Engine/Game/Effect/CombatEffectController.h"
#include "Engine/Game/Effect/ImpactDistortionController.h"
#include "Engine/Game/Effect/PostEffectController.h"
#include "Engine/Game/Enemy/EnemyBulletManager.h"
#include "Engine/Game/Enemy/EnemyDefeatEffectController.h"
#include "Engine/Game/Enemy/EnemyLaserTelegraphController.h"
#include "Engine/Game/Enemy/EnemyManager.h"
#include "Engine/Game/Field/InfluenceFieldManager.h"
#include "Engine/Game/GameState/PlayerDeathSequenceController.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Game/Player/PlayerDamageFeedbackController.h"
#include "Engine/Game/Player/PlayerBarrelRollRingController.h"
#include "Engine/Game/Player/PlayerBulletCancelEffectController.h"
#include "Engine/Game/Player/PlayerBulletManager.h"
#include "Engine/Game/Player/PlayerChargeFeedbackController.h"
#include "Engine/Game/Player/PlayerChargeGatherEffectController.h"
#include "Engine/Game/Player/PlayerJetExhaustController.h"
#include "Engine/Game/Player/PlayerSonicBoostRingController.h"
#include "Engine/Game/UI/PlayerHudController.h"
#include "Engine/Game/UI/WarningUIController.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Cloud/CloudVolume.h"
#include "Engine/Graphics/Cloud/VolumetricCloudPass.h"
#include "Engine/Graphics/Effect/PrimitiveEffectSystem.h"
#include "Engine/Graphics/Model/GltfSkinnedModel.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Particle/GpuParticleSystem.h"
#include "Engine/Graphics/Particle/ParticleManager.h"
#include "Engine/Graphics/Shadow/ScreenSpaceFakeShadowPass.h"
#include "Engine/Graphics/Skybox/Skybox.h"
#include "Engine/Graphics/Sprite/Sprite.h"
#include "Engine/Level/LevelSceneRuntime.h"

void GameScene::DrawSceneRender() {
    auto dxCommon = MyGame::GetInstance()->GetDxCommon();
    auto object3dCommon = MyGame::GetInstance()->GetObject3dCommon();
    auto skyboxCommon = MyGame::GetInstance()->GetSkyboxCommon();
    auto volumetricCloudPass = MyGame::GetInstance()->GetVolumetricCloudPass();
    auto particleManager = ParticleManager::GetInstance();
    auto spriteCommon = MyGame::GetInstance()->GetSpriteCommon();
    const bool shouldDrawDebugVisuals =
        runtimeModeController_ ? runtimeModeController_->ShouldDrawLevelDebug() : false;
    const RuntimeModeController::ShadowLikeDebugSettings shadowDebugSettings =
        runtimeModeController_ ? runtimeModeController_->GetShadowLikeDebugSettings() : RuntimeModeController::ShadowLikeDebugSettings{};

    if (isSkyboxVisible_) {
        skyboxCommon->CommonDrawSetting();
        skybox_->Draw();
    }

    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();
    if (shouldDrawDebugVisuals && simpleSkinSkinnedModel_) {
        simpleSkinSkinnedModel_->DispatchComputeSkinning(commandList);
    }
    if (shouldDrawDebugVisuals && walkSkinnedModel_) {
        walkSkinnedModel_->DispatchComputeSkinning(commandList);
    }
    if (shouldDrawDebugVisuals && sneakWalkSkinnedModel_) {
        sneakWalkSkinnedModel_->DispatchComputeSkinning(commandList);
    }

    object3dCommon->CommonDrawSetting((Object3dCommon::BlendMode)currentBlendMode_);

    if (shouldDrawDebugVisuals && isFenceVisible_) {
        object3d_->Draw();
    }
    if (shouldDrawDebugVisuals && isSphereVisible_) {
        object3dSphere_->Draw();
    }
    if (shouldDrawDebugVisuals && isAnimatedCubeVisible_ && animatedCubeObject_) {
        animatedCubeObject_->Draw();
    }
    const Skeleton* activeSkinningTarget = skinningEditor_ ? skinningEditor_->GetTargetSkeleton() : nullptr;
    if (shouldDrawDebugVisuals && isSkinnedModelVisible_ && simpleSkinSkinnedObject_ && activeSkinningTarget == simpleSkinSkeleton_.get()) {
        simpleSkinSkinnedObject_->Draw();
    }
    if (shouldDrawDebugVisuals && isSkinnedModelVisible_ && walkSkinnedObject_ && activeSkinningTarget == walkSkeleton_.get()) {
        walkSkinnedObject_->Draw();
    }
    if (shouldDrawDebugVisuals && isSkinnedModelVisible_ && sneakWalkSkinnedObject_ && activeSkinningTarget == sneakWalkSkeleton_.get()) {
        sneakWalkSkinnedObject_->Draw();
    }
    if (shouldDrawDebugVisuals && isPrimitivePreviewVisible_) {
        for (auto& primitivePreviewObject : primitivePreviewObjects_) {
            primitivePreviewObject->Draw();
        }
    }
    if (shouldDrawDebugVisuals && levelSceneRuntime_) {
        levelSceneRuntime_->Draw();
    }
    if (player_) {
        player_->Draw();
    }
    if (playerBulletManager_) {
        playerBulletManager_->Draw();
    }
    if (enemyManager_) {
        enemyManager_->Draw();
    }
    if (enemyBulletManager_) {
        enemyBulletManager_->Draw();
    }
    if (enemyLaserTelegraphController_ && !shadowDebugSettings.disableEffects) {
        enemyLaserTelegraphController_->Draw();
    }
    if (influenceFieldManager_) {
        influenceFieldManager_->DrawDebug();
    }
    if (screenSpaceFakeShadowPass_ && !shadowDebugSettings.disableFakeShadow) {
        screenSpaceFakeShadowPass_->Draw(camera_.get());
    }
    if (primitiveEffectSystem_ && !shadowDebugSettings.disableEffects && !shadowDebugSettings.disablePrimitiveEffect) {
        primitiveEffectSystem_->Draw();
    }
    if (combatEffectController_ && !shadowDebugSettings.disableEffects && !shadowDebugSettings.disableGpuParticle) {
        combatEffectController_->Draw();
    }
    if (playerJetExhaustController_ && !shadowDebugSettings.disableEffects && !shadowDebugSettings.disableGpuParticle) {
        playerJetExhaustController_->Draw();
    }
    if (playerSonicBoostRingController_ && !shadowDebugSettings.disableEffects) {
        playerSonicBoostRingController_->Draw();
    }
    if (playerBarrelRollRingController_ && !shadowDebugSettings.disableEffects) {
        playerBarrelRollRingController_->Draw();
    }
    if (playerBulletCancelEffectController_ && !shadowDebugSettings.disableEffects) {
        playerBulletCancelEffectController_->Draw();
    }
    if (playerChargeFeedbackController_ && !shadowDebugSettings.disableEffects) {
        playerChargeFeedbackController_->Draw();
    }
    if (enemyDefeatEffectController_ && !shadowDebugSettings.disableEffects) {
        enemyDefeatEffectController_->Draw();
    }

    if (isVolumetricCloudVisible_ && !shadowDebugSettings.disableClouds && volumetricCloudPass && cloudVolume_) {
        if (cloudProjectedBounds_.isVisible && !cloudProjectedBounds_.isPassSkipped) {
            dxCommon->TransitionDepthBuffer(
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            volumetricCloudPass->Render(camera_.get(), cloudVolume_.get(), cloudProjectedBounds_);
            dxCommon->TransitionDepthBuffer(
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_DEPTH_WRITE);

            ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();
            D3D12_CPU_DESCRIPTOR_HANDLE sceneRTV = dxCommon->GetRenderTextureRTV();
            D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = dxCommon->GetDepthStencilView();
            commandList->OMSetRenderTargets(1, &sceneRTV, FALSE, &depthStencilView);
        }
    }

    if (playerJetExhaustController_ && !shadowDebugSettings.disableEffects && !shadowDebugSettings.disableGpuParticle) {
        playerJetExhaustController_->DrawAfterCloud();
    }
    if (playerSonicBoostRingController_ && !shadowDebugSettings.disableEffects) {
        playerSonicBoostRingController_->DrawAfterCloud();
    }
    if (playerBarrelRollRingController_ && !shadowDebugSettings.disableEffects) {
        playerBarrelRollRingController_->DrawAfterCloud();
    }
    if (playerBulletCancelEffectController_ && !shadowDebugSettings.disableEffects) {
        playerBulletCancelEffectController_->DrawAfterCloud();
    }
    if (playerChargeFeedbackController_ && !shadowDebugSettings.disableEffects) {
        playerChargeFeedbackController_->DrawAfterCloud();
    }
    if (playerChargeGatherEffectController_ && !shadowDebugSettings.disableEffects && !shadowDebugSettings.disableGpuParticle) {
        playerChargeGatherEffectController_->Draw();
    }
    if (enemyDefeatEffectController_ && !shadowDebugSettings.disableEffects) {
        enemyDefeatEffectController_->DrawAfterCloud();
    }
    if (enemyLaserTelegraphController_ && !shadowDebugSettings.disableEffects) {
        enemyLaserTelegraphController_->DrawAfterCloud();
    }

    if (shouldDrawDebugVisuals && !shadowDebugSettings.disableEffects && !shadowDebugSettings.disableGpuParticle && gpuParticleSystem_) {
        gpuParticleSystem_->Draw();
    }

    if (isParticleVisible_ && !shadowDebugSettings.disableEffects) {
        particleManager->Draw();
    }

    if (impactDistortionController_ && !shadowDebugSettings.disableEffects && !shadowDebugSettings.disablePostEffects) {
        impactDistortionController_->Draw();
    }

    spriteCommon->CommonDrawSetting();
    if (playerDamageFeedbackController_) {
        playerDamageFeedbackController_->Draw();
    }
    if (playerHudController_) {
        playerHudController_->Draw();
    }
    if (shouldDrawDebugVisuals && isDebugSpriteVisible_) {
        debugSprite_->Draw();
    }
    if (playerDeathSequenceController_) {
        playerDeathSequenceController_->Draw();
    }
    if (postEffectController_) {
        postEffectController_->Draw();
    }
    if (warningUIController_) {
        warningUIController_->Draw();
    }
}
