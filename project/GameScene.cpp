#include "Engine/Game/GameState/StageCombatState.h"
#include "Engine/Graphics/Particle/GpuParticleSystem.h"
#include "GameScene.h"
#include "MyGame.h"
#include "SceneManager.h"
#include "TitleScene.h"
#include "Engine/Game/GameState/StageStartSequenceController.h"
#include "Engine/Game/Enemy/EnemyBulletManager.h"
#include "Engine/Game/Player/PlayerBulletManager.h"
#include "Engine/Game/RailShooter/ProjectileRailMotionAdapter.h"

void GameScene::Initialize() {
    presentationPrepared_ = false;
    InitializeSceneResources();
    projectileRailMotionAdapter_ = std::make_unique<ProjectileRailMotionAdapter>();
    projectileRailMotionAdapter_->Initialize(railShooterCameraRig_.get());
    if (playerBulletManager_) {
        playerBulletManager_->SetProjectileRailMotionAdapter(projectileRailMotionAdapter_.get());
    }
    if (enemyBulletManager_) {
        enemyBulletManager_->SetProjectileRailMotionAdapter(projectileRailMotionAdapter_.get());
    }
}

void GameScene::Update() {
    if (stageStart_ && stageStart_->IsActive()) {
        if (MyGame::GetInstance()->GetInput()->TriggerKey(DIK_T)) {
            SceneManager::GetInstance()->ChangeScene(std::make_unique<TitleScene>());
            return;
        }
        UpdateStageStartPresentation();
        return;
    }
    if (stageCombatState_) {
        auto* input = MyGame::GetInstance()->GetInput();
        if (input->TriggerKey(DIK_PAUSE)) stageCombatState_->SetPaused(!stageCombatState_->IsPaused());
        GpuParticleSystem::SetSimulationPaused(stageCombatState_->IsPaused());
        if (stageCombatState_->IsPaused()) {
            if (input->TriggerKey(DIK_T)) SceneManager::GetInstance()->ChangeScene(std::make_unique<TitleScene>());
            return;
        }
    }
    if (auto* clouds = MyGame::GetInstance()->GetVolumetricCloudPass(); clouds && clouds->UpdateResolutionComparison()) return;
    UpdateSceneRuntime();
}

void GameScene::Draw() {
    DrawSceneRender();
}

void GameScene::Finalize() {
    GpuParticleSystem::SetSimulationPaused(false);
    if (stageCombatState_) stageCombatState_->SetGameplayActive(false);
    if (stageStart_) stageStart_->Cancel();
    stageStart_.reset();
    presentationPrepared_ = false;
    if (auto* clouds = MyGame::GetInstance()->GetVolumetricCloudPass()) clouds->EndResolutionComparison();
    if (projectileRailMotionAdapter_) {
        projectileRailMotionAdapter_->Finalize();
    }
    FinalizeSceneResources();
    projectileRailMotionAdapter_.reset();
    stageCombatState_.reset();
}
