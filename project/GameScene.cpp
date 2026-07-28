#include "GameScene.h"
#include "Engine/Game/Enemy/EnemyBulletManager.h"
#include "Engine/Game/Player/PlayerBulletManager.h"
#include "Engine/Game/RailShooter/ProjectileRailMotionAdapter.h"

void GameScene::Initialize() {
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
    UpdateSceneRuntime();
}

void GameScene::Draw() {
    DrawSceneRender();
}

void GameScene::Finalize() {
    if (projectileRailMotionAdapter_) {
        projectileRailMotionAdapter_->Finalize();
    }
    FinalizeSceneResources();
    projectileRailMotionAdapter_.reset();
}
