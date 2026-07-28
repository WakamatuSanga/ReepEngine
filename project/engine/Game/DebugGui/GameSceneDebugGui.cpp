#include "GameSceneDebugGui.h"
#include "GameScene.h"

#include "Engine/Core/DirectXCommon.h"
#include "Engine/Core/GameViewport.h"
#include "Engine/Core/RuntimeModeController.h"
#include "Engine/Editor/BlenderSync/BlenderLiveSync.h"
#include "Engine/Editor/Camera/EditorCameraController.h"
#include "Engine/Editor/SkinningEditor.h"
#include "Engine/Game/Camera/CameraShakeController.h"
#include "Engine/Game/Camera/RailShooterCameraRig.h"
#include "Engine/Game/Collision/PlayerBulletEnemyCollision.h"
#include "Engine/Game/Collision/PlayerEnemyBulletCollision.h"
#include "Engine/Game/Effect/CombatEffectController.h"
#include "Engine/Game/Effect/CombatSlowMotionController.h"
#include "Engine/Game/Effect/ImpactDistortionController.h"
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
#include "Engine/Graphics/Cloud/VolumetricCloudPass.h"
#include "Engine/Graphics/Model/GltfSkinnedModel.h"
#include "Engine/Graphics/Particle/GpuParticleSystem.h"
#include "Engine/Graphics/Shadow/ScreenSpaceFakeShadowPass.h"
#include "Engine/Level/LevelSceneRuntime.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void GameSceneDebugGui::Initialize(const GameSceneDebugGuiContext& context) {
    scene_ = context.scene;
}

void GameSceneDebugGui::Initialize(GameScene* scene) {
    Initialize(GameSceneDebugGuiContext{ scene });
}

void GameSceneDebugGui::DrawImGui(DirectXCommon* dxCommon, bool showDebugUi, VolumetricCloudPass* volumetricCloudPass) {
#ifdef USE_IMGUI
    if (!scene_) {
        return;
    }

    if (showDebugUi) {
        DrawGameViewImGui(dxCommon);
        DrawManagerDebugWindows();
        DrawSceneToolWindows(dxCommon, volumetricCloudPass);
    } else {
        ClearGameViewDebugState();
    }
#else
    (void)dxCommon;
    (void)showDebugUi;
    (void)volumetricCloudPass;
#endif
}

#ifdef USE_IMGUI
void GameSceneDebugGui::ClearGameViewDebugState() {
    scene_->gameViewTopLeft_ = { 0.0f, 0.0f };
    scene_->gameViewSize_ = { 0.0f, 0.0f };
    scene_->gameViewMouseLocal_ = { 0.0f, 0.0f };
    scene_->isGameViewHovered_ = false;
    scene_->isGameViewFocused_ = false;
    if (scene_->gameViewport_) {
        scene_->gameViewport_->ClearImGuiGameViewRect();
    }
    if (scene_->skinningEditor_) {
        scene_->skinningEditor_->ClearGameViewRect();
    }
    if (scene_->levelSceneRuntime_) {
        scene_->levelSceneRuntime_->ClearGameViewRect();
    }
}

void GameSceneDebugGui::DrawGameViewImGui(DirectXCommon* dxCommon) {
    if (!dxCommon || !dxCommon->GetFinalOutputTextureResource()) {
        ClearGameViewDebugState();
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(640.0f, 400.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Game View")) {
        const D3D12_RESOURCE_DESC desc = dxCommon->GetFinalOutputTextureResource()->GetDesc();
        const float textureWidth = static_cast<float>(desc.Width);
        const float textureHeight = static_cast<float>(desc.Height);
        if (scene_->gameViewport_) {
            scene_->gameViewport_->SetRenderTargetSize(textureWidth, textureHeight);
        }
        ImVec2 availableSize = ImGui::GetContentRegionAvail();

        if (textureWidth > 0.0f && textureHeight > 0.0f && availableSize.x > 1.0f && availableSize.y > 1.0f) {
            ImVec2 imageSize = availableSize;
            const float textureAspect = textureWidth / textureHeight;
            if (imageSize.x / imageSize.y > textureAspect) {
                imageSize.x = imageSize.y * textureAspect;
            } else {
                imageSize.y = imageSize.x / textureAspect;
            }

            const ImVec2 imageTopLeft = ImGui::GetCursorScreenPos();
            scene_->gameViewTopLeft_ = { imageTopLeft.x, imageTopLeft.y };
            scene_->gameViewSize_ = { imageSize.x, imageSize.y };
            if (imageSize.y > 0.0f) {
                scene_->camera_->SetAspectRatio(imageSize.x / imageSize.y);
                scene_->camera_->Update();
            }

            const ImTextureID textureId = static_cast<ImTextureID>(dxCommon->GetFinalOutputTextureSRVGPUHandle().ptr);
            ImGui::Image(textureId, imageSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));

            const ImGuiIO& io = ImGui::GetIO();
            scene_->isGameViewHovered_ = ImGui::IsItemHovered();
            scene_->isGameViewFocused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
            if (scene_->gameViewport_) {
                scene_->gameViewport_->SetImGuiGameViewRect(
                    imageTopLeft.x,
                    imageTopLeft.y,
                    imageSize.x,
                    imageSize.y,
                    scene_->isGameViewHovered_,
                    scene_->isGameViewFocused_);
            }
            scene_->gameViewMouseLocal_ = {
                io.MousePos.x - imageTopLeft.x,
                io.MousePos.y - imageTopLeft.y
            };
            if (scene_->skinningEditor_) {
                scene_->skinningEditor_->SetGameViewRect(imageTopLeft.x, imageTopLeft.y, imageSize.x, imageSize.y);
                scene_->skinningEditor_->DrawGizmo(scene_->camera_.get());
                scene_->skinningEditor_->DrawDebugOverlay(scene_->camera_.get());
            }
            if (scene_->levelSceneRuntime_) {
                scene_->levelSceneRuntime_->SetGameViewRect(imageTopLeft.x, imageTopLeft.y, imageSize.x, imageSize.y);
            }
        } else {
            scene_->isGameViewFocused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
            scene_->gameViewTopLeft_ = { 0.0f, 0.0f };
            scene_->gameViewSize_ = { 0.0f, 0.0f };
            scene_->gameViewMouseLocal_ = { 0.0f, 0.0f };
            scene_->isGameViewHovered_ = false;
            if (scene_->gameViewport_) {
                scene_->gameViewport_->ClearImGuiGameViewRect();
            }
            if (scene_->skinningEditor_) {
                scene_->skinningEditor_->ClearGameViewRect();
            }
            if (scene_->levelSceneRuntime_) {
                scene_->levelSceneRuntime_->ClearGameViewRect();
            }
            ImGui::TextDisabled("RenderTexture is not ready.");
        }
    } else {
        ClearGameViewDebugState();
    }
    ImGui::End();
}

void GameSceneDebugGui::DrawManagerDebugWindows() {
    if (scene_->skinningEditor_) {
        scene_->skinningEditor_->DrawImGui();
    }
    if (scene_->gpuParticleSystem_) {
        scene_->gpuParticleSystem_->DrawImGui();
    }
    if (scene_->editorCameraController_) {
        scene_->editorCameraController_->DrawImGui();
    }
    if (scene_->player_) {
        scene_->player_->DrawImGui();
    }
    if (scene_->playerRailFlightVisualTiltController_) {
        scene_->playerRailFlightVisualTiltController_->DrawImGui();
    }
    if (scene_->playerDamageFeedbackController_) {
        scene_->playerDamageFeedbackController_->DrawImGui();
    }
    if (scene_->playerHudController_) {
        scene_->playerHudController_->DrawImGui();
    }
    if (scene_->aimCorridorVisualController_) {
        scene_->aimCorridorVisualController_->DrawImGui();
    }
    if (scene_->aimCorridorTargetingController_) {
        scene_->aimCorridorTargetingController_->DrawImGui();
    }
    if (scene_->boostController_) {
        scene_->boostController_->DrawImGui();
    }
    if (scene_->playerJetExhaustController_) {
        scene_->playerJetExhaustController_->DrawImGui();
    }
    if (scene_->playerSonicBoostRingController_) {
        scene_->playerSonicBoostRingController_->DrawImGui();
    }
    if (scene_->playerBarrelRollRingController_) {
        scene_->playerBarrelRollRingController_->DrawImGui();
    }
    if (scene_->playerBulletCancelEffectController_) {
        scene_->playerBulletCancelEffectController_->DrawImGui();
    }
    if (scene_->enemyDefeatEffectController_) {
        scene_->enemyDefeatEffectController_->DrawImGui();
    }
    if (scene_->influenceFieldManager_) {
        scene_->influenceFieldManager_->DrawImGui();
    }
    if (scene_->projectileRailMotionAdapter_) {
        scene_->projectileRailMotionAdapter_->DrawImGui(
            scene_->playerBulletManager_ ? scene_->playerBulletManager_->GetActiveCount() : 0,
            scene_->enemyBulletManager_ ? scene_->enemyBulletManager_->GetActiveCount() : 0);
    }
    if (scene_->playerBulletManager_) {
        scene_->playerBulletManager_->DrawImGui();
    }
    if (scene_->playerChargeFeedbackController_) {
        scene_->playerChargeFeedbackController_->DrawImGui();
    }
    if (scene_->playerChargeGatherEffectController_) {
        scene_->playerChargeGatherEffectController_->DrawImGui();
    }
    if (scene_->enemyManager_) {
        scene_->enemyManager_->DrawImGui();
    }
    if (scene_->enemyBulletManager_) {
        scene_->enemyBulletManager_->DrawImGui();
    }
    if (scene_->enemyAttackController_) {
        scene_->enemyAttackController_->DrawImGui();
    }
    if (scene_->playerEnemyBulletCollision_) {
        scene_->playerEnemyBulletCollision_->DrawImGui();
    }
    if (scene_->playerBulletEnemyCollision_) {
        scene_->playerBulletEnemyCollision_->DrawImGui();
    }
    if (scene_->combatEffectController_) {
        scene_->combatEffectController_->DrawImGui();
    }
    if (scene_->combatSlowMotionController_) {
        scene_->combatSlowMotionController_->DrawImGui();
    }
    if (scene_->impactDistortionController_) {
        scene_->impactDistortionController_->DrawImGui();
    }
    if (scene_->playerDeathSequenceController_) {
        scene_->playerDeathSequenceController_->DrawImGui();
    }
    if (scene_->cameraShakeController_) {
        scene_->cameraShakeController_->DrawImGui();
    }
    if (scene_->gameOverFlowController_) {
        scene_->gameOverFlowController_->DrawImGui();
    }
    if (scene_->playerRailController_) {
        scene_->playerRailController_->DrawImGui();
    }
    if (scene_->playerEventTriggerBridge_) {
        scene_->playerEventTriggerBridge_->DrawImGui();
    }
    if (scene_->railShooterCameraRig_) {
        scene_->railShooterCameraRig_->DrawImGui();
    }
    if (scene_->gameViewport_) {
        scene_->gameViewport_->DrawImGui();
    }
    if (scene_->runtimeModeController_) {
        scene_->runtimeModeController_->DrawImGui();
    }
    if (scene_->screenSpaceFakeShadowPass_) {
        scene_->screenSpaceFakeShadowPass_->DrawImGui();
    }
    if (scene_->railShooterEventActionBridge_) {
        scene_->railShooterEventActionBridge_->DrawImGui();
    }
    if (scene_->enemySpawnActionBridge_) {
        scene_->enemySpawnActionBridge_->DrawImGui();
    }
    if (scene_->enemyWaveManager_) {
        scene_->enemyWaveManager_->DrawImGui();
    }
    if (scene_->enemyLaserTelegraphController_) {
        scene_->enemyLaserTelegraphController_->DrawImGui();
    }
    if (scene_->startupEnemySpawnController_) {
        scene_->startupEnemySpawnController_->DrawImGui();
    }
    if (scene_->postEffectActionBridge_) {
        scene_->postEffectActionBridge_->DrawImGui();
    }
    if (scene_->eventActionDispatcher_) {
        scene_->eventActionDispatcher_->DrawImGui();
    }
    if (scene_->postEffectController_) {
        scene_->postEffectController_->DrawImGui();
    }
    if (scene_->warningUIController_) {
        scene_->warningUIController_->DrawImGui();
    }
    if (scene_->levelSceneRuntime_) {
        scene_->levelSceneRuntime_->DrawImGui();
    }
    if (scene_->blenderLiveSync_) {
        scene_->blenderLiveSync_->DrawImGui();
    }
    if (scene_->skinningPreviewModel_) {
        scene_->skinningPreviewModel_->UpdateSkinning();
    }


}
#endif
