#include "GameScene.h"
#include "MyGame.h"
#include "SceneManager.h"
#include "TitleScene.h"
#include "Engine/Animation/Skeleton.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Core/FrameTimer.h"
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
#include "Engine/Game/Player/PlayerSonicBoostRingController.h"
#include "Engine/Game/RailShooter/EnemyWaveManager.h"
#include "Engine/Game/RailShooter/EventActionDispatcher.h"
#include "Engine/Game/RailShooter/PlayerEventTriggerBridge.h"
#include "Engine/Game/RailShooter/StartupEnemySpawnController.h"
#include "Engine/Game/UI/PlayerHudController.h"
#include "Engine/Game/UI/WarningUIController.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Cloud/CloudVolume.h"
#include "Engine/Graphics/Cloud/VolumetricCloudPass.h"
#include "Engine/Graphics/Model/GltfSkinnedModel.h"
#include "Engine/Graphics/Effect/PrimitiveEffectSystem.h"
#include "Engine/Graphics/Model/Model.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Particle/GpuParticleSystem.h"
#include "Engine/Graphics/Particle/ParticleManager.h"
#include "Engine/Graphics/Shadow/ScreenSpaceFakeShadowPass.h"
#include "Engine/Graphics/Skybox/Skybox.h"
#include "Engine/Graphics/Sprite/Sprite.h"
#include "Engine/Level/LevelSceneRuntime.h"
#include <algorithm>
#include <cmath>
#include <memory>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void GameScene::UpdateSceneRuntime() {
    auto input = MyGame::GetInstance()->GetInput();
    auto particleManager = ParticleManager::GetInstance();
    auto volumetricCloudPass = MyGame::GetInstance()->GetVolumetricCloudPass();
    auto dxCommon = MyGame::GetInstance()->GetDxCommon();
    auto& hitEffectParams = particleManager->GetHitEffectParams();
    auto audio = Audio::GetInstance();
    const FrameTimer& frameTimer = FrameTimer::GetInstance();
    const float unscaledDeltaTime = frameTimer.GetGameplayDeltaTime();
    float gameplayDeltaTime = unscaledDeltaTime;
    RuntimeModeController::ShadowLikeDebugSettings shadowDebugSettings{};
    bool cameraProjectionUpdated = false;

    if (input->PushKey(DIK_T)) {
        SceneManager::GetInstance()->ChangeScene(std::make_unique<TitleScene>());
        return;
    }
    if (runtimeModeController_) {
        runtimeModeController_->BeginCameraModeSwitchDiagnostics(camera_.get());
        runtimeModeController_->Update(input);
        shadowDebugSettings = runtimeModeController_->GetShadowLikeDebugSettings();
        if (dxCommon) {
            dxCommon->SetOffscreenRenderScale(runtimeModeController_->GetDesiredRenderScale());
            dxCommon->SetRenderScaleClearColorTestEnabled(shadowDebugSettings.clearColorTest);
        }
    }
    if (blenderLiveSync_) {
        blenderLiveSync_->Update();
    }
    if (cameraShakeController_) {
        cameraShakeController_->BeginFrame(camera_.get());
    }

    const bool isCameraRigActive = railShooterCameraRig_ && railShooterCameraRig_->IsCameraRigActive();
    const bool isDeathSequenceActive = playerDeathSequenceController_ && playerDeathSequenceController_->IsActiveOrFinished();
    const bool isGameMode = runtimeModeController_ ? runtimeModeController_->IsGameMode() : true;
    if (enemyWaveManager_) {
        enemyWaveManager_->SetGameModeActive(isGameMode);
    }
    if (isGameMode && runtimeModeController_ && runtimeModeController_->ShouldAutoApplyGameModePerformancePreset() && volumetricCloudPass) {
        volumetricCloudPass->ApplyGameModePerformancePreset();
    }
    if (volumetricCloudPass) {
        volumetricCloudPass->SetDiagnosticDisableComposite(shadowDebugSettings.disableCloudComposite);
        volumetricCloudPass->SetDiagnosticDisableDepthAwareUpsample(shadowDebugSettings.disableDepthAwareUpsample);
    }
    if (primitiveEffectSystem_) {
        primitiveEffectSystem_->SetDiagnosticSuppressed(
            shadowDebugSettings.disableEffects || shadowDebugSettings.disablePrimitiveEffect);
    }
    if (postEffectController_) {
        postEffectController_->SetDiagnosticSuppressed(shadowDebugSettings.disablePostEffects);
    }
    Model::SetGlobalPbrLightingDisabled(shadowDebugSettings.disablePbrLighting);
    const bool shouldDrawLevelDebug = runtimeModeController_ ? runtimeModeController_->ShouldDrawLevelDebug() : false;
    const bool shouldDrawEventDebug = runtimeModeController_ ? runtimeModeController_->ShouldDrawEventDebug() : false;
    const bool shouldDrawRailDebug = runtimeModeController_ ? runtimeModeController_->ShouldDrawRailDebug() : false;
    if (gameViewport_) {
        if (dxCommon && dxCommon->GetFinalOutputTextureResource()) {
            const D3D12_RESOURCE_DESC desc = dxCommon->GetFinalOutputTextureResource()->GetDesc();
            gameViewport_->SetRenderTargetSize(static_cast<float>(desc.Width), static_cast<float>(desc.Height));
        }
        gameViewport_->BeginFrame(isGameMode);
        const GameViewport::Rect& viewportRect = gameViewport_->GetGameViewportRect();
        if (viewportRect.height > 1.0f) {
            camera_->SetAspectRatio(viewportRect.width / viewportRect.height);
            camera_->Update();
            cameraProjectionUpdated = true;
        }
    }
    if (screenSpaceFakeShadowPass_) {
        if (dxCommon) {
            screenSpaceFakeShadowPass_->SetRenderTargetSize(
                static_cast<float>(dxCommon->GetRenderTextureWidth()),
                static_cast<float>(dxCommon->GetRenderTextureHeight()));
        }
        screenSpaceFakeShadowPass_->SetDebugMode(runtimeModeController_ ? runtimeModeController_->IsDebugMode() : false);
    }
#ifdef USE_IMGUI
    const bool shouldShowPlayerActionDebugVisuals =
        !runtimeModeController_ || runtimeModeController_->ShouldDrawDebugUi();
    const ImGuiIO& imguiIO = ImGui::GetIO();
    const bool isGizmoInteracting = skinningEditor_ && skinningEditor_->IsGizmoInteracting();
    const bool isImGuiInputActive = imguiIO.WantTextInput || ImGui::IsAnyItemActive();
    const bool isGameViewportMouse = gameViewport_ && gameViewport_->IsMouseInGameViewport();
    const bool isGameplayInputEditing = imguiIO.WantTextInput || (ImGui::IsAnyItemActive() && !isGameViewportMouse);
    const bool gameViewHovered = gameViewport_ ? gameViewport_->IsGameViewHovered() : isGameMode;
    const bool gameViewFocused = gameViewport_ ? gameViewport_->IsGameViewFocused() : isGameMode;
    if (editorCameraController_) {
        editorCameraController_->Update(
            unscaledDeltaTime,
            input,
            gameViewHovered,
            gameViewFocused,
            isImGuiInputActive,
            isGizmoInteracting,
            isCameraRigActive || isGameMode);
    }
    const bool canUseGameInput =
        gameViewport_
            ? gameViewport_->IsGameInputActive(
                isGameplayInputEditing,
                editorCameraController_ && editorCameraController_->IsRightMouseFlyActive(),
                isGizmoInteracting || isDeathSequenceActive)
            : ((isGameMode || gameViewHovered || gameViewFocused) && !isGizmoInteracting && !isDeathSequenceActive);
    if (player_) {
        player_->SetGameViewInputActive(canUseGameInput);
        player_->SetActionDebugVisualsEnabled(shouldShowPlayerActionDebugVisuals);
    }
    if (playerBulletManager_) {
        playerBulletManager_->SetGameViewInputActive(canUseGameInput);
    }
    if (boostController_) {
        boostController_->SetGameViewInputActive(canUseGameInput);
    }
#else
    if (gameViewport_) {
        gameViewport_->BeginFrame(true);
        const GameViewport::Rect& viewportRect = gameViewport_->GetGameViewportRect();
        if (viewportRect.height > 1.0f) {
            camera_->SetAspectRatio(viewportRect.width / viewportRect.height);
            camera_->Update();
            cameraProjectionUpdated = true;
        }
    }
    if (editorCameraController_) {
        editorCameraController_->Update(
            unscaledDeltaTime,
            input,
            true,
            true,
            false,
            false,
            true);
    }
    const bool canUseGameInput =
        gameViewport_
            ? gameViewport_->IsGameInputActive(
                false,
                editorCameraController_ && editorCameraController_->IsRightMouseFlyActive(),
                isDeathSequenceActive)
            : (!(editorCameraController_ && editorCameraController_->IsRightMouseFlyActive()) && !isDeathSequenceActive);
    if (player_) {
        player_->SetGameViewInputActive(canUseGameInput);
        player_->SetActionDebugVisualsEnabled(false);
    }
    if (playerBulletManager_) {
        playerBulletManager_->SetGameViewInputActive(canUseGameInput);
    }
    if (boostController_) {
        boostController_->SetGameViewInputActive(canUseGameInput);
    }
#endif
    if (runtimeModeController_) {
        runtimeModeController_->EndCameraModeSwitchDiagnostics(camera_.get(), cameraProjectionUpdated);
    }

    if (combatSlowMotionController_) {
        combatSlowMotionController_->Update(unscaledDeltaTime);
        gameplayDeltaTime = unscaledDeltaTime * combatSlowMotionController_->GetTimeScale();
    }

    const float effectDeltaTime = (combatSlowMotionController_ && !combatSlowMotionController_->ShouldUseScaledDeltaForEffects())
        ? unscaledDeltaTime
        : gameplayDeltaTime;

    if (input->PushKey(DIK_0)) audio->PlayAudio("resources/sounds/Alarm01.mp3");

    if (railShooterCameraRig_) {
        railShooterCameraRig_->Update(gameplayDeltaTime);
    }
    if (cameraShakeController_) {
        cameraShakeController_->UpdateAndApply(gameplayDeltaTime, camera_.get());
    }
    if (levelSceneRuntime_) {
        const bool cameraRigActiveForDebug = railShooterCameraRig_ && railShooterCameraRig_->IsCameraRigActive();
        levelSceneRuntime_->SetCameraRigPreviewState(
            cameraRigActiveForDebug,
            !shouldDrawRailDebug || (railShooterCameraRig_ && railShooterCameraRig_->ShouldHideRailDebugWhileActive()),
            !shouldDrawRailDebug || (railShooterCameraRig_ && railShooterCameraRig_->ShouldHideRailPointsWhileActive()),
            !shouldDrawEventDebug || (railShooterCameraRig_ && railShooterCameraRig_->ShouldHideEventDebugWhileActive()),
            !shouldDrawLevelDebug || (railShooterCameraRig_ && railShooterCameraRig_->IsGameplayPreviewModeEnabled()));
    }
    camera_->Update();
    if (playerRailController_) {
        playerRailController_->Update(gameplayDeltaTime);
    }
    if (playerBulletCancelEffectController_) {
        playerBulletCancelEffectController_->BeginFrame();
    }
    if (enemyDefeatEffectController_) {
        enemyDefeatEffectController_->BeginFrame();
    }
    if (impactDistortionController_) {
        impactDistortionController_->BeginFrame();
    }
    if (player_) {
        player_->Update(gameplayDeltaTime);
    }
    if (playerDamageFeedbackController_) {
        playerDamageFeedbackController_->Update(unscaledDeltaTime);
    }
    if (boostController_) {
        boostController_->Update(gameplayDeltaTime);
    }
    if (playerSonicBoostRingController_) {
        playerSonicBoostRingController_->SetDebugVisualsEnabled(shouldDrawLevelDebug);
        playerSonicBoostRingController_->Update(effectDeltaTime);
    }
    if (playerBarrelRollRingController_) {
        playerBarrelRollRingController_->Update(effectDeltaTime);
    }
    if (playerBulletCancelEffectController_) {
        playerBulletCancelEffectController_->Update(effectDeltaTime);
    }
    if (playerJetExhaustController_) {
        playerJetExhaustController_->SetDebugVisualsEnabled(shouldDrawLevelDebug);
        playerJetExhaustController_->SetPlayerAlive(!(playerDeathSequenceController_ && playerDeathSequenceController_->IsActiveOrFinished()));
        playerJetExhaustController_->Update(effectDeltaTime);
    }
    if (playerBulletManager_) {
        playerBulletManager_->Update(gameplayDeltaTime);
    }
    if (playerChargeFeedbackController_) {
        playerChargeFeedbackController_->Update(effectDeltaTime);
    }
    if (playerChargeGatherEffectController_) {
        playerChargeGatherEffectController_->Update(effectDeltaTime);
    }
    if (startupEnemySpawnController_) {
        startupEnemySpawnController_->Update(gameplayDeltaTime);
    }
    if (enemyManager_) {
        enemyManager_->Update(gameplayDeltaTime);
    }
    if (influenceFieldManager_) {
        influenceFieldManager_->SetDebugVisualsEnabled(shouldDrawLevelDebug);
        influenceFieldManager_->Update(gameplayDeltaTime);
    }
    if (playerBulletEnemyCollision_) {
        playerBulletEnemyCollision_->Update();
    }
    if (enemyAttackController_) {
        enemyAttackController_->Update(gameplayDeltaTime);
    }
    if (enemyBulletManager_) {
        enemyBulletManager_->Update(gameplayDeltaTime);
    }
    if (playerEnemyBulletCollision_) {
        playerEnemyBulletCollision_->Update();
    }
    if (playerHudController_) {
        playerHudController_->SetGameModeActive(isGameMode);
        playerHudController_->Update(unscaledDeltaTime);
    }
    if (impactDistortionController_) {
        impactDistortionController_->Update(effectDeltaTime);
    }
    if (combatEffectController_) {
        combatEffectController_->Update(effectDeltaTime, camera_.get());
    }
    if (enemyDefeatEffectController_) {
        enemyDefeatEffectController_->Update(effectDeltaTime);
    }
    if (playerDeathSequenceController_) {
        playerDeathSequenceController_->Update(gameplayDeltaTime);
    }
    if (postEffectController_) {
        postEffectController_->Update(gameplayDeltaTime);
    }
    if (warningUIController_) {
        warningUIController_->Update(unscaledDeltaTime);
    }
    if (gameOverFlowController_) {
        gameOverFlowController_->Update();
    }
    if (playerEventTriggerBridge_) {
        playerEventTriggerBridge_->Update();
    }
    if (levelSceneRuntime_) {
        levelSceneRuntime_->Update();
    }
    if (eventActionDispatcher_) {
        eventActionDispatcher_->Update();
    }
    if (enemyWaveManager_) {
        enemyWaveManager_->SetCurrentBoostPower(boostController_ ? boostController_->GetCurrentBoostPower() : 0.0f);
        enemyWaveManager_->Update(gameplayDeltaTime);
    }
    if (enemyLaserTelegraphController_) {
        enemyLaserTelegraphController_->Update(gameplayDeltaTime);
    }
    if (cloudVolume_) {
        cloudVolume_->Update(gameplayDeltaTime);
    }
    if (volumetricCloudPass && cloudVolume_) {
        cloudProjectedBounds_ = volumetricCloudPass->BuildProjectedBounds(camera_.get(), cloudVolume_.get());
    } else {
        cloudProjectedBounds_ = {};
    }
    objectRandomTime_ += gameplayDeltaTime;
    if (animatedCubeObject_ && hasAnimatedCubeAnimation_) {
        animatedCubeAnimationTime_ += gameplayDeltaTime;
        const float duration = (std::max)(animatedCubeClip_.duration, 0.0001f);
        if (animatedCubeAnimationTime_ >= duration) {
            animatedCubeAnimationTime_ = std::fmod(animatedCubeAnimationTime_, duration);
        }

        if (const JointTrack* animatedCubeTrack = FindJointTrack(animatedCubeClip_, "AnimatedCube")) {
            if (!animatedCubeTrack->translate.keyframes.empty()) {
                animatedCubeObject_->SetTranslate(CalculateValue(animatedCubeTrack->translate.keyframes, animatedCubeAnimationTime_));
            }
            if (!animatedCubeTrack->rotate.keyframes.empty()) {
                Quaternion rotation = CalculateValue(animatedCubeTrack->rotate.keyframes, animatedCubeAnimationTime_);
                animatedCubeObject_->SetRotate(ConvertQuaternionToEulerXYZ(rotation));
            }
            if (!animatedCubeTrack->scale.keyframes.empty()) {
                animatedCubeObject_->SetScale(CalculateValue(animatedCubeTrack->scale.keyframes, animatedCubeAnimationTime_));
            }
        }
    }

    if (isSkyboxFollowCamera_) {
        skyboxTranslate_ = camera_->GetTranslate();
    }
    skybox_->SetCamera(camera_.get());
    skybox_->SetScale(skyboxScale_);
    skybox_->SetTranslate(skyboxTranslate_);
    skybox_->Update();
    object3dSphere_->SetEnvironmentMapEnabled(isSphereEnvironmentMapEnabled_);
    object3dSphere_->SetEnvironmentMapIntensity(sphereEnvironmentMapIntensity_);
    object3dSphere_->SetDissolveEnabled(isObjectDissolveEnabled_);
    object3dSphere_->SetDissolveThreshold(objectDissolveThreshold_);
    object3dSphere_->SetDissolveEdgeWidth(objectDissolveEdgeWidth_);
    object3dSphere_->SetDissolveEdgeGlowStrength(objectDissolveEdgeGlowStrength_);
    object3dSphere_->SetDissolveEdgeNoiseStrength(objectDissolveEdgeNoiseStrength_);
    object3dSphere_->SetDissolveEdgeColor({
        objectDissolveEdgeColor_[0],
        objectDissolveEdgeColor_[1],
        objectDissolveEdgeColor_[2],
        objectDissolveEdgeColor_[3]
        });
    object3dSphere_->SetRandomEnabled(isObjectRandomEnabled_);
    object3dSphere_->SetRandomPreview(isObjectRandomPreview_);
    object3dSphere_->SetRandomIntensity(objectRandomIntensity_);
    object3dSphere_->SetRandomTime(objectRandomTime_);
    object3d_->Update();
    object3dSphere_->Update();
    if (animatedCubeObject_) {
        animatedCubeObject_->Update();
    }
    const Skeleton* activeSkinningTarget = skinningEditor_ ? skinningEditor_->GetTargetSkeleton() : nullptr;
    const float activeSkinnedPreviewScale = skinningEditor_ ? skinningEditor_->GetActivePreviewScale() : 1.0f;
    const Vector3 activeSkinnedPreviewRotation = skinningEditor_ ? skinningEditor_->GetActivePreviewRotation() : Vector3{ 0.0f, 0.0f, 0.0f };
    auto applyActiveSkinnedPreviewTransform = [&](Object3d* object, const Skeleton* skeleton) {
        if (object && activeSkinningTarget == skeleton) {
            object->SetScale({ activeSkinnedPreviewScale, activeSkinnedPreviewScale, activeSkinnedPreviewScale });
            object->SetRotate(activeSkinnedPreviewRotation);
        }
    };
    applyActiveSkinnedPreviewTransform(simpleSkinSkinnedObject_.get(), simpleSkinSkeleton_.get());
    applyActiveSkinnedPreviewTransform(walkSkinnedObject_.get(), walkSkeleton_.get());
    applyActiveSkinnedPreviewTransform(sneakWalkSkinnedObject_.get(), sneakWalkSkeleton_.get());
    if (simpleSkinSkinnedObject_) {
        simpleSkinSkinnedObject_->Update();
    }
    if (walkSkinnedObject_) {
        walkSkinnedObject_->Update();
    }
    if (sneakWalkSkinnedObject_) {
        sneakWalkSkinnedObject_->Update();
    }
    for (auto& primitivePreviewObject : primitivePreviewObjects_) {
        primitivePreviewObject->Update();
    }
    if (primitiveEffectSystem_) {
        primitiveEffectSystem_->Update(effectDeltaTime);
    }
    debugSprite_->Update();

    if (input->PushKey(DIK_SPACE)) {
        particleManager->Emit("Hit", object3dSphere_->GetTransform().translate, hitEffectParams.spawnCount);
    }

    if (input->PushKey(DIK_H)) {
        particleManager->Emit("Hit", object3dSphere_->GetTransform().translate, hitEffectParams.spawnCount);
    }

    if (input->TriggerKey(DIK_P)) {
        particleManager->Emit(particleTexturePath_, object3dSphere_->GetTransform().translate, 1);
    }

    particleManager->Update(camera_.get());
    if (shouldDrawLevelDebug && !shadowDebugSettings.disableEffects && !shadowDebugSettings.disableGpuParticle && gpuParticleSystem_) {
        gpuParticleSystem_->Update(camera_.get());
    }
    if (shouldDrawLevelDebug && skinningEditor_) {
        if (previewSkeleton_) {
            UpdateSkeletonWorldTransforms(*previewSkeleton_);
        }
        if (previewSkeletonSecondary_) {
            UpdateSkeletonWorldTransforms(*previewSkeletonSecondary_);
        }
        if (simpleSkinSkeleton_) {
            UpdateSkeletonWorldTransforms(*simpleSkinSkeleton_);
        }
        if (walkSkeleton_) {
            UpdateSkeletonWorldTransforms(*walkSkeleton_);
        }
        if (sneakWalkSkeleton_) {
            UpdateSkeletonWorldTransforms(*sneakWalkSkeleton_);
        }
        skinningEditor_->Update();
        if (simpleSkinSkinnedModel_) {
            simpleSkinSkinnedModel_->UpdateSkinning();
        }
        if (walkSkinnedModel_) {
            walkSkinnedModel_->UpdateSkinning();
        }
        if (sneakWalkSkinnedModel_) {
            sneakWalkSkinnedModel_->UpdateSkinning();
        }
    }

    if (runtimeModeController_) {
        RuntimeModeController::PerformanceStats stats{};
        if (dxCommon) {
            stats.renderTextureWidth = dxCommon->GetRenderTextureWidth();
            stats.renderTextureHeight = dxCommon->GetRenderTextureHeight();
            stats.depthTextureWidth = dxCommon->GetDepthBufferWidth();
            stats.depthTextureHeight = dxCommon->GetDepthBufferHeight();
            stats.internalRenderScale = dxCommon->GetOffscreenRenderScale();
            stats.presentInterval = dxCommon->GetPresentInterval();
            stats.fixedFpsWaitEnabled = dxCommon->IsFixedFpsWaitEnabled();
            const D3D12_VIEWPORT offscreenViewport = dxCommon->GetOffscreenViewport();
            const D3D12_RECT offscreenScissor = dxCommon->GetOffscreenScissorRect();
            const D3D12_VIEWPORT backBufferViewport = dxCommon->GetBackBufferViewport();
            const D3D12_RECT backBufferScissor = dxCommon->GetBackBufferScissorRect();
            stats.currentViewportWidth = offscreenViewport.Width;
            stats.currentViewportHeight = offscreenViewport.Height;
            stats.currentScissorWidth = static_cast<int>(offscreenScissor.right - offscreenScissor.left);
            stats.currentScissorHeight = static_cast<int>(offscreenScissor.bottom - offscreenScissor.top);
            stats.backBufferViewportWidth = backBufferViewport.Width;
            stats.backBufferViewportHeight = backBufferViewport.Height;
            stats.backBufferScissorWidth = static_cast<int>(backBufferScissor.right - backBufferScissor.left);
            stats.backBufferScissorHeight = static_cast<int>(backBufferScissor.bottom - backBufferScissor.top);
        }
        if (gameViewport_) {
            const GameViewport::Rect& rect = gameViewport_->GetGameViewportRect();
            stats.gameViewportWidth = rect.width;
            stats.gameViewportHeight = rect.height;
        }
        if (volumetricCloudPass) {
            stats.cloudEnabled = volumetricCloudPass->IsEnabled() && isVolumetricCloudVisible_;
            stats.cloudResolutionScale = volumetricCloudPass->GetCloudResolutionScale();
            stats.lowResolutionCloudEnabled = volumetricCloudPass->IsLowResolutionCloudEnabled();
            stats.cloudCompositeEnabled = volumetricCloudPass->IsCloudCompositeEnabled();
            stats.depthAwareCloudUpsampleEnabled = volumetricCloudPass->IsDepthAwareUpsampleEnabled();
        }
        if (WinApp* winApp = MyGame::GetInstance()->GetWinApp()) {
            stats.windowWidth = winApp->GetClientWidth();
            stats.windowHeight = winApp->GetClientHeight();
        }
        stats.activeEnemyCount = enemyManager_ ? enemyManager_->GetActiveCount() : 0;
        stats.enemyBulletCount = enemyBulletManager_ ? enemyBulletManager_->GetActiveCount() : 0;
        stats.playerBulletCount = playerBulletManager_ ? playerBulletManager_->GetActiveCount() : 0;
        stats.primitiveEffectCount = primitiveEffectSystem_ ? primitiveEffectSystem_->GetEffectCount() : 0;
        stats.gpuParticleActiveEstimate =
            (gpuParticleSystem_ ? gpuParticleSystem_->GetActiveCountEstimate() : 0u) +
            (gpuParticleEffectPlayer_ ? gpuParticleEffectPlayer_->GetActiveParticleEstimate() : 0u);
        runtimeModeController_->SetPerformanceStats(stats);
    }

#ifdef USE_IMGUI
    const bool showDebugUi = !runtimeModeController_ || runtimeModeController_->ShouldDrawDebugUi();
    if (debugGui_) {
        debugGui_->DrawImGui(dxCommon, showDebugUi, volumetricCloudPass);
    }
#endif

    if (volumetricCloudPass && cloudVolume_) {
        cloudProjectedBounds_ = volumetricCloudPass->BuildProjectedBounds(camera_.get(), cloudVolume_.get());
    } else {
        cloudProjectedBounds_ = {};
    }
    }

