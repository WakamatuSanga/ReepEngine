#include "GameScene.h"
#include "MyGame.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Skybox/Skybox.h"
#include "Engine/Graphics/Sprite/Sprite.h"
#include "Engine/Graphics/Particle/GpuParticleSystem.h"
#include "Engine/Game/Player/PlayerJetExhaustController.h"
#include "Engine/Game/Player/PlayerBulletManager.h"
#include "Engine/Game/UI/PlayerHudController.h"
#include "Engine/Level/LevelSceneRuntime.h"
#include "Engine/Game/Enemy/EnemyManager.h"
#include "Engine/Game/Enemy/EnemyBulletManager.h"
#include "Engine/Game/RailShooter/EnemyWaveManager.h"
#include "Engine/Game/Effect/CombatEffectController.h"
#include "Engine/Game/Camera/RailShooterCameraRig.h"
#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossController.h"
#include "Engine/Game/Targeting/AimCorridorTargetingController.h"
#include "Engine/Game/UI/AimCorridorVisualController.h"
#include "Engine/Game/Player/PlayerRailFlightVisualTiltController.h"
#include <stdexcept>

void GameScene::PrepareForPresentation() {
    // Title transitions call this after a fully covered Present; Restart uses
    // the same preparation before its first normal Update.
    // Upload commands complete with the existing first GameScene frame/fence,
    // before the Loading -> Reveal notification; no extra wait or dummy shot.
    if (presentationPrepared_) return;
    if (!levelSceneRuntime_ || !levelSceneRuntime_->PrepareForPresentation())
        throw std::runtime_error("Stage structure: " + (levelSceneRuntime_ ? levelSceneRuntime_->GetLastLoadStatus() : "missing runtime"));
    if (!enemyWaveManager_ || !enemyWaveManager_->PrepareForPresentation(levelSceneRuntime_->GetSceneData()))
        throw std::runtime_error("Stage wave dependencies could not be prepared");
    if (!enemyManager_ || !enemyManager_->PrepareForPresentation())
        throw std::runtime_error("Stage enemy shared visual could not be prepared");
    if (!playerBulletManager_ || !playerBulletManager_->PrepareForPresentation()
        || !enemyBulletManager_ || !enemyBulletManager_->PrepareForPresentation())
        throw std::runtime_error("Stage projectile shared visuals could not be prepared");
    if (!combatEffectController_ || !combatEffectController_->PrepareForPresentation())
        throw std::runtime_error("Stage combat effects could not be prepared");
    if (!krakenTentacleMidboss_ || !krakenTentacleMidboss_->IsInitialized())
        throw std::runtime_error("Stage Kraken skinned model could not be prepared");
    if (railShooterCameraRig_) railShooterCameraRig_->PrepareForPresentation();
    // These owners used to be created during the first normal Update.
    if (!aimCorridorVisualController_) {
        aimCorridorVisualController_ = std::make_unique<AimCorridorVisualController>();
        if (!aimCorridorVisualController_->Initialize(MyGame::GetInstance()->GetDxCommon(), player_.get(), camera_.get()))
            throw std::runtime_error("Stage reticle could not be prepared");
    }
    if (!aimCorridorTargetingController_) {
        aimCorridorTargetingController_ = std::make_unique<AimCorridorTargetingController>();
        if (!aimCorridorTargetingController_->Initialize(MyGame::GetInstance()->GetDxCommon(), playerLockOnTargetProvider_.get(), camera_.get(), aimCorridorVisualController_.get()))
            throw std::runtime_error("Stage target markers could not be prepared");
    }
    if (!playerRailFlightVisualTiltController_) {
        playerRailFlightVisualTiltController_ = std::make_unique<PlayerRailFlightVisualTiltController>();
        playerRailFlightVisualTiltController_->Initialize(player_.get(), railShooterCameraRig_.get());
    }
    // Render transforms only: no input, waves, rail events, shooting or gameplay clocks.
    // Player::Initialize already prepares the player's world and render transforms.
    camera_->Update();
    skybox_->SetTranslate(camera_->GetTranslate());
    skybox_->Update();
    if (object3d_) object3d_->Update();
    if (object3dSphere_) object3dSphere_->Update();
    if (animatedCubeObject_) animatedCubeObject_->Update();
    if (skinningPreviewObject_) skinningPreviewObject_->Update();
    for (auto& object : primitivePreviewObjects_) object->Update();
    if (debugSprite_) debugSprite_->Update();
    if (gpuParticleSystem_) {
        gpuParticleSystem_->SetDeltaTime(0.0f);
        gpuParticleSystem_->Update(camera_.get());
    }
    if (playerJetExhaustController_) playerJetExhaustController_->Update(0.0f);
    if (playerHudController_) playerHudController_->Update(0.0f);
    cloudProjectedBounds_ = MyGame::GetInstance()->GetVolumetricCloudPass()->BuildProjectedBounds(
        camera_.get(), cloudVolume_.get());
    presentationPrepared_ = true;
}
