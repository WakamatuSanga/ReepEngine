#include "GameScene.h"
#include "MyGame.h"
#include "Engine/Core/GameViewport.h"
#include "Engine/Core/RuntimeModeController.h"
#include "Engine/Core/SrvManager.h"
#include "Engine/Editor/BlenderSync/BlenderLiveSync.h"
#include "Engine/Editor/Camera/EditorCameraController.h"
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
#include "Engine/Game/RailShooter/EnemySpawnActionBridge.h"
#include "Engine/Game/RailShooter/EnemyWaveManager.h"
#include "Engine/Game/RailShooter/EventActionDispatcher.h"
#include "Engine/Game/RailShooter/PlayerEventTriggerBridge.h"
#include "Engine/Game/RailShooter/PostEffectActionBridge.h"
#include "Engine/Game/RailShooter/RailShooterEventActionBridge.h"
#include "Engine/Game/RailShooter/StartupEnemySpawnController.h"
#include "Engine/Game/UI/PlayerHudController.h"
#include "Engine/Game/UI/WarningUIController.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Cloud/CloudVolume.h"
#include "Engine/Graphics/Effect/PrimitiveEffectSystem.h"
#include "Engine/Graphics/Model/GltfAnimationLoader.h"
#include "Engine/Graphics/Model/GltfSkinnedModel.h"
#include "Engine/Graphics/Model/Model.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Particle/GpuParticleSystem.h"
#include "Engine/Graphics/Particle/ParticleManager.h"
#include "Engine/Graphics/Shadow/ScreenSpaceFakeShadowPass.h"
#include "Engine/Graphics/Skybox/Skybox.h"
#include "Engine/Graphics/Sprite/Sprite.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Level/LevelSceneRuntime.h"
#include <utility>

namespace {
    CloudVolume::Parameters MakeRecommendedCloudParameters() {
        CloudVolume::Parameters parameters{};
        parameters.center = { 0.0f, 4.5f, 8.0f };
        parameters.halfExtents = { 12.0f, 4.5f, 12.0f };
        parameters.density = 0.85f;
        parameters.absorption = 1.15f;
        parameters.windDirection = { 1.0f, 0.0f, 0.25f };
        parameters.windSpeed = 0.20f;
        parameters.sunDirection = { 0.35f, -1.0f, 0.15f };
        parameters.lightAbsorption = 0.75f;
        parameters.color = { 0.98f, 0.99f, 1.00f, 1.00f };
        parameters.noiseScale = 0.12f;
        parameters.detailNoiseScale = 0.42f;
        parameters.detailWeight = 0.20f;
        parameters.edgeFade = 0.30f;
        parameters.ambientLighting = 0.18f;
        parameters.sunIntensity = 1.05f;
        parameters.viewStepCount = 72;
        parameters.lightStepCount = 8;
        return parameters;
    }

}

void GameScene::InitializeSceneResources() {
    auto modelManager = ModelManager::GetInstance();
    auto texManager = TextureManager::GetInstance();
    auto particleManager = ParticleManager::GetInstance();
    auto object3dCommon = MyGame::GetInstance()->GetObject3dCommon();
    auto spriteCommon = MyGame::GetInstance()->GetSpriteCommon();

    // --- モデル取得・生成 ---
    modelFence_ = modelManager->FindModel("resources/obj/fence/fence.obj");

    modelSphere_ = modelManager->CreateSphere("InternalSphere", 16);

    camera_ = std::make_unique<Camera>();
    camera_->SetTranslate({ 0.0f, 2.0f, -10.0f });
    camera_->SetRotate({ 0.1f, 0.0f, 0.0f });

    gpuParticleSystem_ = std::make_unique<GpuParticleSystem>();
    gpuParticleSystem_->Initialize(MyGame::GetInstance()->GetDxCommon(), SrvManager::GetInstance());

    cloudVolume_ = std::make_unique<CloudVolume>();
    cloudVolume_->GetParameters() = MakeRecommendedCloudParameters();

    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(MyGame::GetInstance()->GetSkyboxCommon());
    skybox_->SetCamera(camera_.get());
    skybox_->SetScale(skyboxScale_);
    skybox_->SetTexture(skyboxTexturePath_);
    skyboxTextureIndex_ = texManager->GetTextureIndexByFilePath(skyboxTexturePath_);
    skyboxTranslate_ = camera_->GetTranslate();
    skybox_->SetTranslate(skyboxTranslate_);

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCommon);
    object3d_->SetModel(modelFence_);
    object3d_->SetTranslate({ -2.0f, 0.0f, 0.0f });
    object3d_->SetCamera(camera_.get());
    object3d_->SetEnvironmentTextureIndex(skyboxTextureIndex_);
    object3d_->SetEnvironmentMapEnabled(false);

    object3dSphere_ = std::make_unique<Object3d>();
    object3dSphere_->Initialize(object3dCommon);
    object3dSphere_->SetModel(modelSphere_); // 生成した球体をセット
    object3dSphere_->SetTranslate({ 2.0f, 0.0f, 0.0f });
    object3dSphere_->SetCamera(camera_.get());
    object3dSphere_->SetEnvironmentTextureIndex(skyboxTextureIndex_);
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
    object3dSphere_->SetDissolveMaskTexture(objectDissolveMaskTexturePath_);
    object3dSphere_->SetRandomEnabled(isObjectRandomEnabled_);
    object3dSphere_->SetRandomPreview(isObjectRandomPreview_);
    object3dSphere_->SetRandomIntensity(objectRandomIntensity_);
    object3dSphere_->SetRandomTime(objectRandomTime_);

    animatedCubeModel_ = std::make_unique<GltfSkinnedModel>();
    if (animatedCubeModel_->InitializeStatic(modelManager->GetModelCommon(), "resources/AnimatedCube/AnimatedCube.gltf")) {
        animatedCubeObject_ = std::make_unique<Object3d>();
        animatedCubeObject_->Initialize(object3dCommon);
        animatedCubeObject_->SetModel(animatedCubeModel_->GetModel());
        animatedCubeObject_->SetCamera(camera_.get());
        animatedCubeObject_->SetTranslate({ 0.0f, 1.5f, 4.0f });
        animatedCubeObject_->SetScale({ 1.0f, 1.0f, 1.0f });
        animatedCubeObject_->SetEnvironmentMapEnabled(false);
    } else {
        animatedCubeModel_.reset();
    }
    hasAnimatedCubeAnimation_ = GltfAnimationLoader::LoadFirstNodeClipFromFile(
        "resources/AnimatedCube/AnimatedCube.gltf",
        animatedCubeClip_);

    texManager->LoadTexture("resources/obj/axis/uvChecker.png");
    texManager->LoadTexture("resources/obj/fence/fence.png");
    texManager->LoadTexture("resources/obj/monsterBall/monsterBall.png");
    texManager->LoadTexture("resources/particle/circle2.png");
    texManager->LoadTexture("resources/particle/gradationLine.png");

    texIndexUvChecker_ = texManager->GetTextureIndexByFilePath("resources/obj/axis/uvChecker.png");
    texIndexFence_ = texManager->GetTextureIndexByFilePath("resources/obj/fence/fence.png");
    texIndexMonsterBall_ = texManager->GetTextureIndexByFilePath("resources/obj/monsterBall/monsterBall.png");

    if (modelSphere_) {
        // 初期テクスチャをモンスターボールに設定
        modelSphere_->SetTextureIndex(texIndexMonsterBall_);
    }

    primitivePreviewObjects_.clear();
    primitivePreviewObjects_.reserve(8);

    auto createPrimitivePreview = [&](Model* model, const Vector3& translate, const Vector3& rotate, const Vector3& scale) {
        if (!model) {
            return;
        }

        auto preview = std::make_unique<Object3d>();
        preview->Initialize(object3dCommon);
        preview->SetModel(model);
        preview->SetCamera(camera_.get());
        preview->SetTranslate(translate);
            preview->SetRotate(rotate);
        preview->SetScale(scale);
        preview->SetEnvironmentMapEnabled(false);
        primitivePreviewObjects_.push_back(std::move(preview));
        };

    createPrimitivePreview(modelManager->CreatePlane("PrimitivePlane"), { -4.5f, -1.0f, 3.0f }, { 0.0f, 0.0f, 0.0f }, { 1.4f, 1.4f, 1.4f });
    createPrimitivePreview(modelManager->CreateCircle("PrimitiveCircle", 32), { -1.5f, -1.0f, 3.0f }, { 0.0f, 0.0f, 0.0f }, { 1.2f, 1.2f, 1.2f });
    Model* primitiveRingModel = modelManager->CreateRing("PrimitiveRing", 32, 0.45f, 1.0f);
    if (primitiveRingModel) {
        primitiveRingModel->SetTextureIndex(texManager->GetTextureIndexByFilePath("resources/particle/gradationLine.png"));
    }
    createPrimitivePreview(primitiveRingModel, { 1.5f, -1.0f, 3.0f }, { 0.0f, 0.0f, 0.0f }, { 1.5f, 1.5f, 1.5f });
    createPrimitivePreview(modelManager->CreateTriangle("PrimitiveTriangle"), { 4.5f, -1.0f, 3.0f }, { 0.0f, 0.0f, 0.0f }, { 1.4f, 1.4f, 1.4f });
    createPrimitivePreview(modelManager->CreateBox("PrimitiveBox"), { -4.5f, 0.9f, 6.0f }, { 0.35f, 0.45f, 0.0f }, { 0.9f, 0.9f, 0.9f });
    createPrimitivePreview(modelManager->CreateCylinder("PrimitiveCylinder", 32), { -1.5f, 0.9f, 6.0f }, { 0.1f, 0.35f, 0.0f }, { 0.85f, 0.85f, 0.85f });
    createPrimitivePreview(modelManager->CreateCone("PrimitiveCone", 32), { 1.5f, 0.9f, 6.0f }, { 0.1f, 0.35f, 0.0f }, { 0.85f, 0.85f, 0.85f });
    createPrimitivePreview(modelManager->CreateTorus("PrimitiveTorus", 32, 16), { 4.5f, 0.9f, 6.0f }, { 0.6f, 0.3f, 0.0f }, { 1.0f, 1.0f, 1.0f });

    primitiveEffectSystem_ = std::make_unique<PrimitiveEffectSystem>();
    primitiveEffectSystem_->Initialize(object3dCommon, camera_.get());
    screenSpaceFakeShadowPass_ = std::make_unique<ScreenSpaceFakeShadowPass>();
    screenSpaceFakeShadowPass_->Initialize(spriteCommon);

    debugSprite_ = std::make_unique<Sprite>();
    debugSprite_->Initialize(spriteCommon);
    debugSprite_->SetTexture("resources/obj/axis/uvChecker.png");
    debugSprite_->SetPosition({ 100.0f, 100.0f });
    debugSprite_->SetSize({ 100.0f, 100.0f });

    particleManager->SetTexture(particleTexturePath_);

    runtimeModeController_ = std::make_unique<RuntimeModeController>();
    runtimeModeController_->Initialize(MyGame::GetInstance()->GetWinApp());
    gameViewport_ = std::make_unique<GameViewport>();
    gameViewport_->Initialize(MyGame::GetInstance()->GetWinApp());
    debugGui_ = std::make_unique<GameSceneDebugGui>();
    debugGui_->Initialize(this);
    editorCameraController_ = std::make_unique<EditorCameraController>();
    editorCameraController_->Initialize(camera_.get());
    player_ = std::make_unique<Player>();
    player_->Initialize(object3dCommon, camera_.get());
    boostController_ = std::make_unique<BoostController>();
    boostController_->Initialize(MyGame::GetInstance()->GetVolumetricCloudPass());
    playerJetExhaustController_ = std::make_unique<PlayerJetExhaustController>();
    playerJetExhaustController_->Initialize(object3dCommon, camera_.get(), player_.get(), boostController_.get(), MyGame::GetInstance()->GetDxCommon(), SrvManager::GetInstance());
    playerSonicBoostRingController_ = std::make_unique<PlayerSonicBoostRingController>();
    playerSonicBoostRingController_->Initialize(MyGame::GetInstance()->GetDxCommon(), camera_.get(), player_.get(), boostController_.get());
    playerBarrelRollRingController_ = std::make_unique<PlayerBarrelRollRingController>();
    playerBarrelRollRingController_->Initialize(MyGame::GetInstance()->GetDxCommon(), camera_.get());
    playerBulletCancelEffectController_ = std::make_unique<PlayerBulletCancelEffectController>();
    playerBulletCancelEffectController_->Initialize(MyGame::GetInstance()->GetDxCommon(), camera_.get());
    playerBulletManager_ = std::make_unique<PlayerBulletManager>();
    playerBulletManager_->Initialize(object3dCommon, camera_.get(), player_.get());
    playerBulletManager_->SetGameViewport(gameViewport_.get());
    playerChargeFeedbackController_ = std::make_unique<PlayerChargeFeedbackController>();
    playerChargeFeedbackController_->Initialize(MyGame::GetInstance()->GetDxCommon(), camera_.get(), player_.get(), playerBulletManager_.get());
    playerChargeGatherEffectController_ = std::make_unique<PlayerChargeGatherEffectController>();
    playerChargeGatherEffectController_->Initialize(MyGame::GetInstance()->GetDxCommon(), SrvManager::GetInstance(), camera_.get(), player_.get(), playerBulletManager_.get());
    gpuParticleEffectPlayer_ = std::make_unique<GpuParticleEffectPlayer>();
    gpuParticleEffectPlayer_->Initialize(MyGame::GetInstance()->GetDxCommon(), SrvManager::GetInstance());
    combatEffectController_ = std::make_unique<CombatEffectController>();
    combatEffectController_->Initialize(primitiveEffectSystem_.get(), gpuParticleEffectPlayer_.get(), player_.get());
    playerDamageFeedbackController_ = std::make_unique<PlayerDamageFeedbackController>();
    playerDamageFeedbackController_->Initialize(spriteCommon, player_.get(), combatEffectController_.get());
    playerHudController_ = std::make_unique<PlayerHudController>();
    playerHudController_->Initialize(spriteCommon, playerDamageFeedbackController_.get(), playerBulletManager_.get(), boostController_.get());
    combatSlowMotionController_ = std::make_unique<CombatSlowMotionController>();
    combatSlowMotionController_->Initialize();
    impactDistortionController_ = std::make_unique<ImpactDistortionController>();
    impactDistortionController_->Initialize(MyGame::GetInstance()->GetDxCommon(), SrvManager::GetInstance(), camera_.get());
    enemyDefeatEffectController_ = std::make_unique<EnemyDefeatEffectController>();
    enemyDefeatEffectController_->Initialize(MyGame::GetInstance()->GetDxCommon(), camera_.get());
    enemyManager_ = std::make_unique<EnemyManager>();
    enemyManager_->Initialize(object3dCommon, camera_.get());
    enemyManager_->SetPlayer(player_.get());
    influenceFieldManager_ = std::make_unique<InfluenceFieldManager>();
    influenceFieldManager_->Initialize(object3dCommon, camera_.get());
    influenceFieldManager_->SetTargets(player_.get(), enemyManager_.get(), boostController_.get());
    influenceFieldManager_->SetConsumers(gpuParticleSystem_.get(), MyGame::GetInstance()->GetVolumetricCloudPass());
    if (screenSpaceFakeShadowPass_) {
        screenSpaceFakeShadowPass_->SetTargets(player_.get(), enemyManager_.get());
    }
    enemyBulletManager_ = std::make_unique<EnemyBulletManager>();
    enemyBulletManager_->Initialize(object3dCommon, camera_.get());
    enemyLaserTelegraphController_ = std::make_unique<EnemyLaserTelegraphController>();
    enemyLaserTelegraphController_->Initialize(MyGame::GetInstance()->GetDxCommon(), camera_.get());
    player_->SetBarrelRollDependencies(enemyBulletManager_.get(), combatEffectController_.get());
    player_->SetBarrelRollEffectControllers(playerBarrelRollRingController_.get(), playerBulletCancelEffectController_.get());
    player_->SetBarrelRollSlowMotionController(combatSlowMotionController_.get());
    cameraShakeController_ = std::make_unique<CameraShakeController>();
    cameraShakeController_->Initialize();
    postEffectController_ = std::make_unique<PostEffectController>();
    postEffectController_->Initialize(MyGame::GetInstance()->GetDxCommon(), spriteCommon);
    warningUIController_ = std::make_unique<WarningUIController>();
    warningUIController_->Initialize(spriteCommon);
    if (boostController_) {
        boostController_->SetPostEffectContext(player_.get(), camera_.get(), postEffectController_.get());
    }
    playerDeathSequenceController_ = std::make_unique<PlayerDeathSequenceController>();
    playerDeathSequenceController_->Initialize(MyGame::GetInstance()->GetDxCommon(), spriteCommon, cameraShakeController_.get());
    levelSceneRuntime_ = std::make_unique<LevelSceneRuntime>();
    levelSceneRuntime_->Initialize(object3dCommon, camera_.get());
    enemyAttackController_ = std::make_unique<EnemyAttackController>();
    enemyAttackController_->Initialize(enemyManager_.get(), enemyBulletManager_.get(), player_.get(), playerDeathSequenceController_.get(), camera_.get());
    playerBulletEnemyCollision_ = std::make_unique<PlayerBulletEnemyCollision>();
    playerBulletEnemyCollision_->Initialize(playerBulletManager_.get(), enemyManager_.get(), combatEffectController_.get());
    playerBulletEnemyCollision_->SetEnemyDefeatEffectController(enemyDefeatEffectController_.get());
    playerBulletEnemyCollision_->SetImpactDistortionController(impactDistortionController_.get());
    playerEnemyBulletCollision_ = std::make_unique<PlayerEnemyBulletCollision>();
    playerEnemyBulletCollision_->Initialize(player_.get(), enemyBulletManager_.get(), playerDeathSequenceController_.get(), combatEffectController_.get());
    playerEnemyBulletCollision_->SetBulletCancelEffectController(playerBulletCancelEffectController_.get());
    playerEnemyBulletCollision_->SetSlowMotionController(combatSlowMotionController_.get());
    playerEnemyBulletCollision_->SetImpactDistortionController(impactDistortionController_.get());
    playerEnemyBulletCollision_->SetDamageFeedbackController(playerDamageFeedbackController_.get());
    gameOverFlowController_ = std::make_unique<GameOverFlowController>();
    gameOverFlowController_->Initialize(playerDeathSequenceController_.get());
    playerRailController_ = std::make_unique<PlayerRailController>();
    playerRailController_->Initialize(player_.get(), levelSceneRuntime_->GetRailRuntime());
    playerEventTriggerBridge_ = std::make_unique<PlayerEventTriggerBridge>();
    playerEventTriggerBridge_->Initialize(player_.get(), levelSceneRuntime_->GetEventRuntime());
    railShooterCameraRig_ = std::make_unique<RailShooterCameraRig>();
    railShooterCameraRig_->Initialize(camera_.get(), levelSceneRuntime_->GetRailRuntime());
    railShooterCameraRig_->SetLevelSceneRuntime(levelSceneRuntime_.get());
    railShooterEventActionBridge_ = std::make_unique<RailShooterEventActionBridge>();
    railShooterEventActionBridge_->Initialize(levelSceneRuntime_->GetEventRuntime(), railShooterCameraRig_.get());
    enemySpawnActionBridge_ = std::make_unique<EnemySpawnActionBridge>();
    enemySpawnActionBridge_->Initialize(enemyManager_.get(), levelSceneRuntime_.get());
    enemyWaveManager_ = std::make_unique<EnemyWaveManager>();
    enemyWaveManager_->Initialize(enemyManager_.get(), camera_.get());
    enemyWaveManager_->SetPlayer(player_.get());
    enemyWaveManager_->SetLaserTelegraphController(enemyLaserTelegraphController_.get());
    enemyWaveManager_->SetWarningUIController(warningUIController_.get());
    startupEnemySpawnController_ = std::make_unique<StartupEnemySpawnController>();
    startupEnemySpawnController_->Initialize(enemyManager_.get(), levelSceneRuntime_.get(), camera_.get());
    postEffectActionBridge_ = std::make_unique<PostEffectActionBridge>();
    postEffectActionBridge_->Initialize(postEffectController_.get());
    eventActionDispatcher_ = std::make_unique<EventActionDispatcher>();
    eventActionDispatcher_->Initialize(
        levelSceneRuntime_->GetEventRuntime(),
        railShooterEventActionBridge_.get(),
        enemySpawnActionBridge_.get(),
        enemyWaveManager_.get(),
        postEffectActionBridge_.get(),
        primitiveEffectSystem_.get(),
        levelSceneRuntime_.get(),
        warningUIController_.get());
    blenderLiveSync_ = std::make_unique<BlenderLiveSync>();
    blenderLiveSync_->Initialize(levelSceneRuntime_.get());

    InitializeSkinningEditorPreview();
}
