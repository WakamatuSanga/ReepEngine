#include "Engine/Game/GameState/StageCombatState.h"
#include "GameSceneDebugGui.h"
#include "GameScene.h"
#include "SceneManager.h"
#include "TitleScene.h"
#include "Engine/Graphics/Effect/PrimitiveEffectSystem.h"
#include "Engine/Core/DebugUiWorkspace.h"
#include <vector>

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
#include "Engine/Game/GameState/StageStartSequenceController.h"
#include "Engine/Game/Player/BoostController.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Game/Player/FlightTuning.h"
#include "Engine/Game/Player/PlayerDamageFeedbackController.h"
#include "Engine/Game/Player/PlayerBarrelRollRingController.h"
#include "Engine/Game/Player/PlayerBulletCancelEffectController.h"
#include "Engine/Game/Player/PlayerBulletManager.h"
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
#include "Engine/Game/Targeting/PlayerLockOnTargetProvider.h"
#include "Engine/Game/UI/AimCorridorVisualController.h"
#include "Engine/Game/UI/PlayerHudController.h"
#include "Engine/Game/UI/WarningUIController.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Cloud/VolumetricCloudPass.h"
#include "Engine/Graphics/Model/GltfSkinnedModel.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Particle/GpuParticleSystem.h"
#include "Engine/Graphics/Shadow/ScreenSpaceFakeShadowPass.h"
#include "Engine/Level/LevelSceneRuntime.h"

#ifdef USE_IMGUI
#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossController.h"
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
        DrawManagerDebugWindows(dxCommon, volumetricCloudPass);
        // Keep the existing debug HP propagation independent of panel visibility.
        if (scene_->enemyManager_ && scene_->krakenTentacleMidboss_) scene_->krakenTentacleMidboss_->SetDebugHpOne(EnemyManager::IsDebugHpOneEnabled());
        if (scene_->skinningEditor_) {
            KrakenPreviewAssetMode requestedAssetMode{};
            if (scene_->skinningEditor_->ConsumeKrakenGltfPreviewLoadRequest(
                requestedAssetMode)) {
                scene_->skinningPreviewAssetMode_ = requestedAssetMode;
                scene_->InitializeSkinningEditorPreview();
            }
        }
        RefreshSkinningPreviewAfterEditorInput(true);
        DrawGameViewImGui(dxCommon);
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
void GameSceneDebugGui::RefreshSkinningPreviewAfterEditorInput(
    bool refreshObjectTransform) {
    if (!scene_ || !scene_->skinningEditor_) {
        return;
    }

    if (refreshObjectTransform &&
        scene_->skinningPreviewObject_ &&
        scene_->skinningPreviewSkeleton_ &&
        scene_->skinningEditor_->GetTargetSkeleton() ==
            scene_->skinningPreviewSkeleton_.get()) {
        const float previewScale =
            scene_->skinningEditor_->GetActivePreviewScale();
        scene_->skinningPreviewObject_->SetScale({
            previewScale, previewScale, previewScale });
        scene_->skinningPreviewObject_->SetRotate(
            scene_->skinningEditor_->GetActivePreviewRotation());
        scene_->skinningPreviewObject_->Update();
    }

    if (scene_->skinningPreviewModel_) {
        scene_->skinningEditor_->RefreshKrakenMotionPreviewDiagnostics();
    }
}

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

    DebugUiWorkspace::PrepareGameView();
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
                if (scene_->skinningEditor_->IsGizmoInteracting()) {
                    RefreshSkinningPreviewAfterEditorInput(false);
                }
                scene_->skinningEditor_->DrawDebugOverlay(scene_->camera_.get());
            }
            scene_->DrawKrakenTentacleMidbossDebug();
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

void GameSceneDebugGui::DrawManagerDebugWindows(DirectXCommon* dxCommon, VolumetricCloudPass* clouds) {
    using C = DebugUiWorkspace::Category;
    std::vector<DebugUiWorkspace::Panel> panels;
    panels.push_back({ "game", "シーン移動・実行モード", C::Game, [this] {
        ImGui::TextUnformatted("F1: Game / Debug、T: タイトルへ戻る");
        ImGui::TextUnformatted("モード切替は上部メニューまたはF1を使用します。");
        if (ImGui::Button("実行・性能設定を開く")) DebugUiWorkspace::SelectPanel("runtimeModeController");
        if (ImGui::Button("雲の画面遷移を調整")) SceneManager::GetInstance()->ShowTitleCloudTransitionSettings();
        if (ImGui::Button("Restart")) SceneManager::GetInstance()->ChangeScene(std::make_unique<GameScene>());
        ImGui::SameLine();
        if (ImGui::Button("タイトルへ戻る")) SceneManager::GetInstance()->ChangeScene(std::make_unique<TitleScene>());
        ImGui::TextWrapped("各機能は上の分類から選びます。独立エディタの開閉と配置だけを保存し、調整値は担当クラスが保持します。");
    } });
    if (scene_->stageStart_) panels.push_back({ "stageStart", "ステージ1・開始演出", C::Game, [this] {
        scene_->stageStart_->DrawImGui();
        if (ImGui::Button("調整値でRestart")) SceneManager::GetInstance()->ChangeScene(std::make_unique<GameScene>());
    } });
    panels.push_back({ "kraken", "クラーケン中ボスの演出・配置", C::Enemy, [this] { scene_->DrawKrakenTentacleMidbossImGui(); }, true });
    panels.push_back({ "krakenWave", "クラーケン交戦ウェーブ", C::Enemy, [this] { scene_->DrawKrakenTentacleWaveEncounterImGui(); } });
    if (scene_->boostController_) panels.push_back({ "flightTuning", "飛行・ロール・風の採用値", C::Player, [] { FlightTuning::DrawImGui(); } });
    if (scene_->skinningEditor_) panels.push_back({ "skinningEditor", "スキニング・クラーケン編集", C::Enemy, [this] { scene_->skinningEditor_->SetOpen(true); scene_->skinningEditor_->DrawImGui(); }, true });
    if (scene_->gpuParticleSystem_) panels.push_back({ "gpuParticleSystem", "GPU粒子エディタ", C::Rendering, [this] { scene_->gpuParticleSystem_->DrawImGui(); }, true });
    if (scene_->editorCameraController_) panels.push_back({ "editorCameraController", "エディタカメラ", C::Player, [this] { scene_->editorCameraController_->DrawImGui(); }, false });
    if (scene_->player_) panels.push_back({ "player", "プレイヤー・HP・ロール", C::Player, [this] { scene_->player_->DrawImGui(scene_->playerDamageFeedbackController_.get()); }, false });
    if (scene_->playerRailFlightVisualTiltController_) panels.push_back({ "playerRailFlightVisualTiltController", "レール飛行の見た目傾き", C::Player, [this] { scene_->playerRailFlightVisualTiltController_->DrawImGui(); }, false });
    if (scene_->playerDamageFeedbackController_) panels.push_back({ "playerDamageFeedbackController", "被弾の点滅・火花", C::Player, [this] { scene_->playerDamageFeedbackController_->DrawImGui(); }, false });
    if (scene_->playerHudController_) panels.push_back({ "playerHudController", "HUD・スコア・ロック弾・スロー回避", C::Player, [this] { scene_->playerHudController_->DrawImGui(); }, false });
    if (scene_->aimCorridorVisualController_) panels.push_back({ "aimCorridorVisualController", "照準コリドー表示", C::Player, [this] { scene_->aimCorridorVisualController_->DrawImGui(); }, false });
    if (scene_->aimCorridorTargetingController_) panels.push_back({ "aimCorridorTargetingController", "照準コリドー判定", C::Player, [this] { scene_->aimCorridorTargetingController_->DrawImGui(); }, false });
    if (scene_->playerLockOnTargetProvider_) panels.push_back({ "playerLockOnTargetProvider", "ロックオン対象・ミサイル", C::Player, [this] { scene_->playerLockOnTargetProvider_->DrawImGui(
            scene_->aimCorridorTargetingController_.get(),
            scene_->playerBulletManager_.get()); }, false });
    if (scene_->boostController_) panels.push_back({ "boostController", "Boost操作・状態", C::Player, [this] { scene_->boostController_->DrawImGui();
 }, false });
    if (scene_->playerJetExhaustController_) panels.push_back({ "playerJetExhaustController", "機体のジェット排気", C::Rendering, [this] { scene_->playerJetExhaustController_->DrawImGui(); }, false });
    if (scene_->playerSonicBoostRingController_) panels.push_back({ "playerSonicBoostRingController", "Boostリング", C::Rendering, [this] { scene_->playerSonicBoostRingController_->DrawImGui(); }, false });
    if (scene_->playerBarrelRollRingController_) panels.push_back({ "playerBarrelRollRingController", "ロールリング", C::Rendering, [this] { scene_->playerBarrelRollRingController_->DrawImGui(); }, false });
    if (scene_->playerBulletCancelEffectController_) panels.push_back({ "playerBulletCancelEffectController", "弾消去エフェクト", C::Rendering, [this] { scene_->playerBulletCancelEffectController_->DrawImGui(); }, false });
    if (scene_->enemyDefeatEffectController_) panels.push_back({ "enemyDefeatEffectController", "敵撃破エフェクト", C::Rendering, [this] { scene_->enemyDefeatEffectController_->DrawImGui(); }, false });
    if (scene_->influenceFieldManager_) panels.push_back({ "influenceFieldManager", "風圧・雲の影響場", C::Rendering, [this] { scene_->influenceFieldManager_->DrawImGui(); }, false });
    if (scene_->projectileRailMotionAdapter_) panels.push_back({ "projectileRailMotionAdapter", "弾のレール移動診断", C::Stage, [this] { scene_->projectileRailMotionAdapter_->DrawImGui(
            scene_->playerBulletManager_ ? scene_->playerBulletManager_->GetActiveCount() : 0,
            scene_->enemyBulletManager_ ? scene_->enemyBulletManager_->GetActiveCount() : 0); }, false });
    if (scene_->playerBulletManager_) panels.push_back({ "playerBulletManager", "射撃・通常弾・ミサイル", C::Player, [this] { scene_->playerBulletManager_->DrawImGui(); }, false });
    if (scene_->enemyManager_) panels.push_back({ "enemyManager", "敵HP・出現・配置", C::Enemy, [this] { scene_->enemyManager_->DrawImGui(); }, false });
    if (scene_->enemyBulletManager_) panels.push_back({ "enemyBulletManager", "敵弾", C::Enemy, [this] { scene_->enemyBulletManager_->DrawImGui(); }, false });
    if (scene_->enemyAttackController_) panels.push_back({ "enemyAttackController", "敵攻撃", C::Enemy, [this] { scene_->enemyAttackController_->DrawImGui(); }, false });
    if (scene_->playerEnemyBulletCollision_) panels.push_back({ "playerEnemyBulletCollision", "敵弾とプレイヤーの衝突", C::Player, [this] { scene_->playerEnemyBulletCollision_->DrawImGui(); }, false });
    if (scene_->playerBulletEnemyCollision_) panels.push_back({ "playerBulletEnemyCollision", "プレイヤー弾の命中判定", C::Enemy, [this] { scene_->playerBulletEnemyCollision_->DrawImGui(); }, false });
    if (scene_->combatEffectController_) panels.push_back({ "combatEffectController", "戦闘エフェクト", C::Rendering, [this] { scene_->combatEffectController_->DrawImGui(); }, false });
    if (scene_->combatSlowMotionController_) panels.push_back({ "combatSlowMotionController", "戦闘スロー", C::Rendering, [this] { scene_->combatSlowMotionController_->DrawImGui(); }, false });
    if (scene_->impactDistortionController_) panels.push_back({ "impactDistortionController", "命中時の歪み", C::Rendering, [this] { scene_->impactDistortionController_->DrawImGui(); }, false });
    if (scene_->playerDeathSequenceController_) panels.push_back({ "playerDeathSequenceController", "プレイヤー死亡演出", C::Game, [this] { scene_->playerDeathSequenceController_->DrawImGui(); }, false });
    if (scene_->cameraShakeController_) panels.push_back({ "cameraShakeController", "カメラシェイク", C::Rendering, [this] { scene_->cameraShakeController_->DrawImGui(); }, false });
    if (scene_->gameOverFlowController_) panels.push_back({ "gameOverFlowController", "GameOver遷移状態", C::Game, [this] { scene_->gameOverFlowController_->DrawImGui(); }, false });
    if (scene_->playerRailController_) panels.push_back({ "playerRailController", "プレイヤーのレール基準", C::Stage, [this] { scene_->playerRailController_->DrawImGui(); }, false });
    if (scene_->playerEventTriggerBridge_) panels.push_back({ "playerEventTriggerBridge", "プレイヤーイベント判定", C::Stage, [this] { scene_->playerEventTriggerBridge_->DrawImGui(); }, false });
    if (scene_->railShooterCameraRig_) panels.push_back({ "railShooterCameraRig", "通常カメラ・レールRig", C::Player, [this] { scene_->railShooterCameraRig_->DrawImGui(); }, false });
    if (scene_->gameViewport_) panels.push_back({ "gameViewport", "Game Viewの入力診断", C::Diagnostics, [this] { scene_->gameViewport_->DrawImGui(); }, false });
    if (scene_->runtimeModeController_) panels.push_back({ "runtimeModeController", "CPU・実行モード・描画倍率", C::Diagnostics, [this] { scene_->runtimeModeController_->DrawImGui(); }, false });
    if (scene_->screenSpaceFakeShadowPass_) panels.push_back({ "screenSpaceFakeShadowPass", "スクリーンスペース簡易影", C::Rendering, [this] { scene_->screenSpaceFakeShadowPass_->DrawImGui(); }, false });
    if (scene_->railShooterEventActionBridge_) panels.push_back({ "railShooterEventActionBridge", "レールイベント接続", C::Stage, [this] { scene_->railShooterEventActionBridge_->DrawImGui(); }, false });
    if (scene_->enemySpawnActionBridge_) panels.push_back({ "enemySpawnActionBridge", "敵出現アクション接続", C::Stage, [this] { scene_->enemySpawnActionBridge_->DrawImGui(); }, false });
    if (scene_->enemyWaveManager_) panels.push_back({ "enemyWaveManager", "敵ウェーブ・進行エディタ", C::Enemy, [this] { scene_->enemyWaveManager_->DrawImGui(); }, true });
    if (scene_->enemyLaserTelegraphController_) panels.push_back({ "enemyLaserTelegraphController", "敵レーザー予告", C::Enemy, [this] { scene_->enemyLaserTelegraphController_->DrawImGui(); }, false });
    if (scene_->startupEnemySpawnController_) panels.push_back({ "startupEnemySpawnController", "開幕の敵出現", C::Enemy, [this] { scene_->startupEnemySpawnController_->DrawImGui(); }, false });
    if (scene_->postEffectActionBridge_) panels.push_back({ "postEffectActionBridge", "PostEffectイベント接続", C::Stage, [this] { scene_->postEffectActionBridge_->DrawImGui(); }, false });
    if (scene_->eventActionDispatcher_) panels.push_back({ "eventActionDispatcher", "イベントアクション振り分け", C::Stage, [this] { scene_->eventActionDispatcher_->DrawImGui(); }, false });
    if (scene_->postEffectController_) panels.push_back({ "postEffectController", "PostEffectの演出トリガー・Boost", C::Rendering, [this] { scene_->postEffectController_->DrawImGui(); }, false });
    if (scene_->warningUIController_) panels.push_back({ "warningUIController", "WARNING表示", C::Rendering, [this] { scene_->warningUIController_->DrawImGui(); }, false });
    if (scene_->levelSceneRuntime_) panels.push_back({ "levelSceneRuntime", "レベル・レールエディタ", C::Stage, [this] { scene_->levelSceneRuntime_->DrawImGui(); }, true });
    if (scene_->blenderLiveSync_) panels.push_back({ "blenderLiveSync", "Blenderライブ同期", C::Stage, [this] { scene_->blenderLiveSync_->DrawImGui(); }, true });
    auto add = [&](const char* id, const char* label, C category, SceneTool tool) {
        panels.push_back({ id, label, category, [this, dxCommon, clouds, tool] { DrawSceneToolContents(dxCommon, clouds, tool); } });
    };
    add("cameraPose", "通常カメラ位置の直接編集", C::Player, SceneTool::Camera);
    add("scene", "空・背景", C::Rendering, SceneTool::Scene);
    add("cloud", "雲の設定・GPU・比較・候補", C::Rendering, SceneTool::Cloud);
    add("postSettings", "PostEffectの描画設定", C::Rendering, SceneTool::Post);
    add("particles", "既存粒子・Hit／Fireball／Wind", C::Rendering, SceneTool::Particles);
    add("objects", "検証オブジェクト・素材", C::Rendering, SceneTool::Object);
    add("visibility", "シーンの表示切替", C::Rendering, SceneTool::Visibility);
    add("skinningStatus", "GPUスキニング診断", C::Diagnostics, SceneTool::Skinning);
    add("sprite", "検証用スプライト位置", C::Diagnostics, SceneTool::Sprite);
    if (scene_->primitiveEffectSystem_) panels.push_back({ "primitive", "プリミティブエフェクト", C::Rendering, [this] { scene_->primitiveEffectSystem_->DrawImGui(); } });
    panels.push_back({ "cloudDiagnosticLink", "雲GPU・固定比較・候補への入口", C::Diagnostics, [] {
        ImGui::TextWrapped("雲の設定と比較・候補は同じ所有者のパネルにまとめています。固定比較はゲーム更新を固定し、候補試用は通常更新を続けます。");
        if (ImGui::Button("雲の設定・GPU・比較・候補を開く")) DebugUiWorkspace::SelectPanel("cloud");
    } });
    DebugUiWorkspace::Draw(panels);
}
#endif
