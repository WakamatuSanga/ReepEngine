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

void GameScene::PrepareForPresentation() {
    // SceneManager calls this only after the fully covered frame was presented.
    // Upload commands complete with the existing first GameScene frame/fence,
    // before the Loading -> Reveal notification; no extra wait or dummy shot.
    if (playerBulletManager_) playerBulletManager_->PrepareForPresentation();
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
}
