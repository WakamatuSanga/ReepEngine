#include "GameScene.h"
#include "SceneManager.h"
#include "TitleScene.h"
#include "MyGame.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "Audio.h"
#include "GltfAnimationLoader.h"
#include "GltfSkinnedModel.h"
#include "GltfSkeletonLoader.h"
#include "SkinningEditor.h"
#include "Skeleton.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kRingInnerRadius = 0.45f;
    constexpr float kRingOuterRadius = 1.0f;
    constexpr uint32_t kRingSubdivision = 32u;

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

    float WrapDegrees(float degrees) {
        while (degrees < 0.0f) {
            degrees += 360.0f;
        }
        while (degrees >= 360.0f) {
            degrees -= 360.0f;
        }
        return degrees;
    }

    std::unique_ptr<Skeleton> MakeHumanoidPreviewSkeleton() {
        auto skeleton = std::make_unique<Skeleton>();
        skeleton->name = "Humanoid Preview";
        skeleton->joints.resize(8);

        skeleton->joints[0].name = "Root";
        skeleton->joints[0].parentIndex = -1;
        skeleton->joints[0].children = { 1 };
        skeleton->joints[0].localTranslate = { 2.0f, 0.0f, 0.0f };

        skeleton->joints[1].name = "Spine";
        skeleton->joints[1].parentIndex = 0;
        skeleton->joints[1].children = { 2, 4, 6 };
        skeleton->joints[1].localTranslate = { 0.0f, 1.2f, 0.0f };

        skeleton->joints[2].name = "Chest";
        skeleton->joints[2].parentIndex = 1;
        skeleton->joints[2].children = { 3 };
        skeleton->joints[2].localTranslate = { 0.0f, 0.9f, 0.0f };

        skeleton->joints[3].name = "Head";
        skeleton->joints[3].parentIndex = 2;
        skeleton->joints[3].localTranslate = { 0.0f, 0.7f, 0.0f };

        skeleton->joints[4].name = "Arm.L";
        skeleton->joints[4].parentIndex = 1;
        skeleton->joints[4].children = { 5 };
        skeleton->joints[4].localTranslate = { -0.8f, 0.6f, 0.0f };

        skeleton->joints[5].name = "Fore.L";
        skeleton->joints[5].parentIndex = 4;
        skeleton->joints[5].localTranslate = { -0.7f, 0.0f, 0.0f };

        skeleton->joints[6].name = "Arm.R";
        skeleton->joints[6].parentIndex = 1;
        skeleton->joints[6].children = { 7 };
        skeleton->joints[6].localTranslate = { 0.8f, 0.6f, 0.0f };

        skeleton->joints[7].name = "Fore.R";
        skeleton->joints[7].parentIndex = 6;
        skeleton->joints[7].localTranslate = { 0.7f, 0.0f, 0.0f };

        UpdateSkeletonWorldTransforms(*skeleton);
        return skeleton;
    }

    std::unique_ptr<Skeleton> MakeChainPreviewSkeleton() {
        auto skeleton = std::make_unique<Skeleton>();
        skeleton->name = "Chain Preview";
        skeleton->joints.resize(5);

        skeleton->joints[0].name = "Root";
        skeleton->joints[0].parentIndex = -1;
        skeleton->joints[0].children = { 1 };
        skeleton->joints[0].localTranslate = { -2.0f, 0.0f, 0.0f };

        skeleton->joints[1].name = "Joint01";
        skeleton->joints[1].parentIndex = 0;
        skeleton->joints[1].children = { 2 };
        skeleton->joints[1].localTranslate = { 0.0f, 1.0f, 0.0f };

        skeleton->joints[2].name = "Joint02";
        skeleton->joints[2].parentIndex = 1;
        skeleton->joints[2].children = { 3 };
        skeleton->joints[2].localTranslate = { 0.6f, 0.8f, 0.0f };

        skeleton->joints[3].name = "Joint03";
        skeleton->joints[3].parentIndex = 2;
        skeleton->joints[3].children = { 4 };
        skeleton->joints[3].localTranslate = { 0.4f, 0.8f, 0.0f };

        skeleton->joints[4].name = "Tip";
        skeleton->joints[4].parentIndex = 3;
        skeleton->joints[4].localTranslate = { 0.2f, 0.6f, 0.0f };

        UpdateSkeletonWorldTransforms(*skeleton);
        return skeleton;
    }

    struct RadialBlurPreset {
        const char* name;
        uint32_t enabled;
        float strength;
        std::array<float, 2> center;
        uint32_t sampleCount;
    };

    struct DissolvePreset {
        const char* name;
        uint32_t enabled;
        float threshold;
        float edgeWidth;
        std::array<float, 4> edgeColor;
    };

    struct OutlinePreset {
        const char* name;
        uint32_t outlineMode;
        uint32_t hybridColorSource;
        float hybridColorWeight;
        float hybridDepthWeight;
        float hybridNormalWeight;
        float outlineStrength;
        float outlineThickness;
        float outlineThreshold;
        float outlineSoftness;
        float outlineDepthThreshold;
        float outlineDepthStrength;
        float outlineNormalThreshold;
        float outlineNormalStrength;
        std::array<float, 4> outlineColor;
    };

    constexpr OutlinePreset kOutlinePresets[] = {
        { "Balanced", 4u, 2u, 1.00f, 1.00f, 1.00f, 2.40f, 1.10f, 0.050f, 0.025f, 0.0020f, 10.0f, 0.10f, 4.0f, { 0.02f, 0.02f, 0.02f, 1.0f } },
        { "Color Emphasis", 4u, 2u, 1.35f, 0.45f, 1.00f, 2.80f, 1.15f, 0.055f, 0.025f, 0.0020f, 10.0f, 0.10f, 4.0f, { 0.03f, 0.03f, 0.03f, 1.0f } },
        { "Depth Emphasis", 4u, 1u, 0.55f, 1.45f, 1.00f, 2.60f, 1.20f, 0.060f, 0.030f, 0.0015f, 14.0f, 0.10f, 4.0f, { 0.01f, 0.01f, 0.01f, 1.0f } },
        { "Soft Outline", 4u, 1u, 0.85f, 0.75f, 1.00f, 1.70f, 1.60f, 0.035f, 0.100f, 0.0020f, 10.0f, 0.10f, 4.0f, { 0.08f, 0.08f, 0.08f, 1.0f } },
        { "FinalHybrid Balanced", 6u, 2u, 1.00f, 1.00f, 1.00f, 2.40f, 1.10f, 0.050f, 0.025f, 0.0015f, 14.0f, 0.10f, 4.0f, { 0.02f, 0.02f, 0.02f, 1.0f } },
        { "FinalHybrid Color Emphasis", 6u, 2u, 1.45f, 0.55f, 0.65f, 2.80f, 1.15f, 0.055f, 0.025f, 0.0025f, 10.0f, 0.12f, 3.5f, { 0.02f, 0.02f, 0.02f, 1.0f } },
        { "FinalHybrid Depth Emphasis", 6u, 1u, 0.55f, 1.55f, 0.70f, 2.60f, 1.20f, 0.060f, 0.030f, 0.0010f, 18.0f, 0.12f, 3.5f, { 0.02f, 0.02f, 0.02f, 1.0f } },
        { "FinalHybrid Normal Emphasis", 6u, 2u, 0.65f, 0.75f, 1.65f, 2.50f, 1.15f, 0.050f, 0.025f, 0.0015f, 12.0f, 0.08f, 5.5f, { 0.02f, 0.02f, 0.02f, 1.0f } },
    };

    constexpr RadialBlurPreset kRadialBlurPresets[] = {
        { "Weak", 1u, 0.010f, { 0.5f, 0.5f }, 6u },
        { "Medium", 1u, 0.020f, { 0.5f, 0.5f }, 8u },
        { "Strong", 1u, 0.040f, { 0.5f, 0.5f }, 12u },
        { "Dramatic", 1u, 0.060f, { 0.5f, 0.5f }, 16u },
    };

    constexpr DissolvePreset kDissolvePresets[] = {
        { "Weak", 1u, 0.20f, 0.02f, { 1.0f, 0.6f, 0.2f, 1.0f } },
        { "Medium", 1u, 0.45f, 0.04f, { 1.0f, 0.5f, 0.1f, 1.0f } },
        { "Strong", 1u, 0.65f, 0.06f, { 1.0f, 0.4f, 0.0f, 1.0f } },
        { "Dramatic", 1u, 0.82f, 0.08f, { 0.4f, 0.9f, 1.0f, 1.0f } },
    };
}

GameScene::GameScene() = default;

GameScene::~GameScene() = default;

void GameScene::Initialize() {
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

    ringEffectPlaneModel_ = modelManager->CreatePlane("RingEffectPlane");
    if (ringEffectPlaneModel_) {
        ringEffectPlaneModel_->SetTextureIndex(texManager->GetTextureIndexByFilePath("resources/particle/circle2.png"));
        if (auto* material = ringEffectPlaneModel_->GetMaterialData()) {
            material->enableLighting = 0;
            material->color = {
                ringEffectPlaneColor_[0],
                ringEffectPlaneColor_[1],
                ringEffectPlaneColor_[2],
                ringEffectPlaneColor_[3]
            };
        }
    }

    ringEffectModel_ = modelManager->CreateRing("RingEffectRing", 32, 0.45f, 1.0f);
    if (ringEffectModel_) {
        ringEffectModel_->SetTextureIndex(texManager->GetTextureIndexByFilePath("resources/particle/gradationLine.png"));
        if (auto* material = ringEffectModel_->GetMaterialData()) {
            material->enableLighting = 0;
            material->color = {
                ringEffectColor_[0],
                ringEffectColor_[1],
                ringEffectColor_[2],
                ringEffectColor_[3]
            };
        }
    }

    effectCylinderModel_ = modelManager->CreateEffectCylinder("EffectCylinder", 32);
    if (effectCylinderModel_) {
        effectCylinderModel_->SetTextureIndex(texManager->GetTextureIndexByFilePath("resources/particle/gradationLine.png"));
        if (auto* material = effectCylinderModel_->GetMaterialData()) {
            material->enableLighting = 0;
            material->alphaReference = 0.0f;
        }
    }

    ringEffectPlane_ = std::make_unique<Object3d>();
    ringEffectPlane_->Initialize(object3dCommon);
    ringEffectPlane_->SetModel(ringEffectPlaneModel_);
    ringEffectPlane_->SetCamera(camera_.get());
    ringEffectPlane_->SetEnvironmentMapEnabled(false);

    ringEffect_ = std::make_unique<Object3d>();
    ringEffect_->Initialize(object3dCommon);
    ringEffect_->SetModel(ringEffectModel_);
    ringEffect_->SetCamera(camera_.get());
    ringEffect_->SetEnvironmentMapEnabled(false);
    ringEffect_->SetRingAppearanceEnabled(isRingAppearancePreviewEnabled_);
    ringEffect_->SetRingUVDirection(currentRingUVDirection_);
    ringEffect_->SetRingInnerRadiusRatio(0.45f);
    ringEffect_->SetRingStartAlpha(ringStartAlpha_);
    ringEffect_->SetRingEndAlpha(ringEndAlpha_);
    ringEffect_->SetRingStartFadeRange(ringStartFadeRange_);
    ringEffect_->SetRingEndFadeRange(ringEndFadeRange_);
    ringEffect_->SetRingInnerColor({ ringInnerColor_[0], ringInnerColor_[1], ringInnerColor_[2], ringInnerColor_[3] });
    ringEffect_->SetRingOuterColor({ ringOuterColor_[0], ringOuterColor_[1], ringOuterColor_[2], ringOuterColor_[3] });

    effectCylinder_ = std::make_unique<Object3d>();
    effectCylinder_->Initialize(object3dCommon);
    effectCylinder_->SetModel(effectCylinderModel_);
    effectCylinder_->SetCamera(camera_.get());
    effectCylinder_->SetEnvironmentMapEnabled(false);
    effectCylinder_->SetTranslate({ 3.0f, -1.0f, 3.0f });
    effectCylinder_->SetScale({ 1.5f, 1.5f, 1.5f });

    auto createRingEffectPreviewObject = [&](std::unique_ptr<Object3d>& object, Model* model) {
        object = std::make_unique<Object3d>();
        object->Initialize(object3dCommon);
        object->SetModel(model);
        object->SetCamera(camera_.get());
        object->SetEnvironmentMapEnabled(false);
    };

    createRingEffectPreviewObject(ringEffectComparePlaneBillboard_, ringEffectPlaneModel_);
    createRingEffectPreviewObject(ringEffectCompareBillboard_, ringEffectModel_);
    createRingEffectPreviewObject(ringEffectComparePlaneWorld_, ringEffectPlaneModel_);
    createRingEffectPreviewObject(ringEffectCompareWorld_, ringEffectModel_);

    debugSprite_ = std::make_unique<Sprite>();
    debugSprite_->Initialize(spriteCommon);
    debugSprite_->SetTexture("resources/obj/axis/uvChecker.png");
    debugSprite_->SetPosition({ 100.0f, 100.0f });
    debugSprite_->SetSize({ 100.0f, 100.0f });

    particleManager->SetTexture(particleTexturePath_);

    previewSkeleton_ = MakeHumanoidPreviewSkeleton();
    previewSkeletonSecondary_ = MakeChainPreviewSkeleton();
    walkSkeleton_ = GltfSkeletonLoader::LoadFromFile("resources/human/walk.gltf");
    sneakWalkSkeleton_ = GltfSkeletonLoader::LoadFromFile("resources/human/sneakWalk.gltf");
    skinningEditor_ = std::make_unique<SkinningEditor>();
    skinningEditor_->RegisterTarget("InternalSphere (preview)", previewSkeleton_.get());
    skinningEditor_->RegisterTarget("Fence (preview)", previewSkeletonSecondary_.get());
    if (walkSkeleton_) {
        AnimationClip walkClip{};
        const bool hasWalkClip = GltfAnimationLoader::LoadFirstClipFromFile(
            "resources/human/walk.gltf",
            *walkSkeleton_,
            walkClip);
        skinningEditor_->RegisterTarget("walk.gltf", walkSkeleton_.get(), hasWalkClip ? &walkClip : nullptr);

        walkSkinnedModel_ = std::make_unique<GltfSkinnedModel>();
        if (walkSkinnedModel_->Initialize(modelManager->GetModelCommon(), walkSkeleton_.get(), "resources/human/walk.gltf")) {
            walkSkinnedObject_ = std::make_unique<Object3d>();
            walkSkinnedObject_->Initialize(object3dCommon);
            walkSkinnedObject_->SetModel(walkSkinnedModel_->GetModel());
            walkSkinnedObject_->SetCamera(camera_.get());
            walkSkinnedObject_->SetEnvironmentMapEnabled(false);
        } else {
            walkSkinnedModel_.reset();
        }
    }

    if (sneakWalkSkeleton_) {
        AnimationClip sneakWalkClip{};
        const bool hasSneakWalkClip = GltfAnimationLoader::LoadFirstClipFromFile(
            "resources/human/sneakWalk.gltf",
            *sneakWalkSkeleton_,
            sneakWalkClip);
        skinningEditor_->RegisterTarget(
            "sneakWalk.gltf",
            sneakWalkSkeleton_.get(),
            hasSneakWalkClip ? &sneakWalkClip : nullptr);

        sneakWalkSkinnedModel_ = std::make_unique<GltfSkinnedModel>();
        if (sneakWalkSkinnedModel_->Initialize(
            modelManager->GetModelCommon(),
            sneakWalkSkeleton_.get(),
            "resources/human/sneakWalk.gltf")) {
            sneakWalkSkinnedObject_ = std::make_unique<Object3d>();
            sneakWalkSkinnedObject_->Initialize(object3dCommon);
            sneakWalkSkinnedObject_->SetModel(sneakWalkSkinnedModel_->GetModel());
            sneakWalkSkinnedObject_->SetCamera(camera_.get());
            sneakWalkSkinnedObject_->SetEnvironmentMapEnabled(false);
        } else {
            sneakWalkSkinnedModel_.reset();
        }
    }
}

void GameScene::Finalize() {
    if (skinningEditor_) {
        skinningEditor_->ClearTargets();
    }
}

void GameScene::Update() {
    auto input = MyGame::GetInstance()->GetInput();
    auto particleManager = ParticleManager::GetInstance();
    auto volumetricCloudPass = MyGame::GetInstance()->GetVolumetricCloudPass();
    auto dxCommon = MyGame::GetInstance()->GetDxCommon();
    auto modelManager = ModelManager::GetInstance();
    auto& postEffectParams = dxCommon->GetPostEffectParameters();
    auto& hitEffectParams = particleManager->GetHitEffectParams();
    auto& fireballEffectParams = particleManager->GetFireballEffectParams();
    auto& windEffectParams = particleManager->GetWindEffectParams();
    auto audio = Audio::GetInstance();
    auto texManager = TextureManager::GetInstance();

    if (input->PushKey(DIK_T)) {
        SceneManager::GetInstance()->ChangeScene(std::make_unique<TitleScene>());
        return;
    }

#ifdef _DEBUG
    if (!ImGui::GetIO().WantCaptureMouse && input->MouseDown(Input::MouseLeft)) {
#else
    if (input->MouseDown(Input::MouseLeft)) {
#endif
        Vector3 rot = camera_->GetRotate();
        rot.y += input->MouseDeltaX() * 0.0025f;
        rot.x += input->MouseDeltaY() * 0.0025f;
        camera_->SetRotate(rot);
    }

    Vector3 move = { 0,0,0 };
    if (input->PushKey(DIK_W)) move.z += 1.0f;
    if (input->PushKey(DIK_S)) move.z -= 1.0f;
    if (input->PushKey(DIK_D)) move.x += 1.0f;
    if (input->PushKey(DIK_A)) move.x -= 1.0f;

    if (move.x != 0 || move.z != 0) {
        float speed = 0.1f;
        Vector3 rot = camera_->GetRotate();
        Vector3 trans = camera_->GetTranslate();
        trans.x += (move.x * std::cos(rot.y) + move.z * std::sin(rot.y)) * speed;
        trans.z += (move.x * -std::sin(rot.y) + move.z * std::cos(rot.y)) * speed;
        camera_->SetTranslate(trans);
    }

    if (input->PushKey(DIK_0)) audio->PlayAudio("resources/sounds/Alarm01.mp3");

    camera_->Update();
    if (cloudVolume_) {
        // TODO: Replace this fixed timestep with the engine's shared delta time when that API is available.
        cloudVolume_->Update(1.0f / 60.0f);
    }
    if (volumetricCloudPass && cloudVolume_) {
        cloudProjectedBounds_ = volumetricCloudPass->BuildProjectedBounds(camera_.get(), cloudVolume_.get());
    } else {
        cloudProjectedBounds_ = {};
    }
    objectRandomTime_ += 0.016f;
    ringAnimationTime_ += 0.016f;
    if (animatedCubeObject_ && hasAnimatedCubeAnimation_) {
        constexpr float kAnimationDeltaTime = 1.0f / 60.0f;
        animatedCubeAnimationTime_ += kAnimationDeltaTime;
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

    if (isRingAnimationEnabled_) {
        if (isRingAlphaAnimationEnabled_) {
            float alphaWave = (std::sin(ringAnimationTime_ * ringAlphaAnimationSpeed_) + 1.0f) * 0.5f;
            float animatedAlpha = std::lerp(ringAlphaAnimationMin_, ringAlphaAnimationMax_, alphaWave);
            ringStartAlpha_ = animatedAlpha;
            ringEndAlpha_ = animatedAlpha;
        }

        if (isRingRadiusAnimationEnabled_) {
            float startWave = (std::sin(ringAnimationTime_ * ringRadiusAnimationSpeed_) + 1.0f) * 0.5f;
            float endWave = (std::sin(ringAnimationTime_ * ringRadiusAnimationSpeed_ + std::numbers::pi_v<float> * 0.5f) + 1.0f) * 0.5f;
            ringShapeStartRadius_ = std::lerp(ringRadiusAnimationStartMin_, ringRadiusAnimationStartMax_, startWave);
            ringShapeEndRadius_ = std::lerp(ringRadiusAnimationEndMin_, ringRadiusAnimationEndMax_, endWave);
            isRingEffectModelDirty_ = true;
        }

        if (isRingAngleAnimationEnabled_) {
            float span = std::clamp(ringAngleAnimationSpan_, 0.0f, 360.0f);
            float maxStart = (std::max)(0.0f, 360.0f - span);
            float animatedStart = WrapDegrees(ringAngleAnimationBase_ + ringAnimationTime_ * ringAngleAnimationSpeed_ * 60.0f);
            if (maxStart > 0.0f) {
                animatedStart = std::fmod(animatedStart, maxStart);
            } else {
                animatedStart = 0.0f;
            }
            ringShapeStartAngle_ = animatedStart;
            ringShapeEndAngle_ = animatedStart + span;
            isRingEffectModelDirty_ = true;
        }
    }

    if (isRingEffectModelDirty_ && ringEffect_) {
        float startAngleRadians = ringShapeStartAngle_ * std::numbers::pi_v<float> / 180.0f;
        float endAngleRadians = ringShapeEndAngle_ * std::numbers::pi_v<float> / 180.0f;
        ringEffectModel_ = modelManager->CreateRing(
            "RingEffectRing",
            kRingSubdivision,
            kRingInnerRadius,
            kRingOuterRadius,
            startAngleRadians,
            endAngleRadians,
            ringShapeStartRadius_,
            ringShapeEndRadius_);
        if (ringEffectModel_) {
            ringEffectModel_->SetTextureIndex(texManager->GetTextureIndexByFilePath("resources/particle/gradationLine.png"));
            if (auto* material = ringEffectModel_->GetMaterialData()) {
                material->enableLighting = 0;
                material->color = {
                    ringEffectColor_[0],
                    ringEffectColor_[1],
                    ringEffectColor_[2],
                    ringEffectColor_[3]
                };
            }
            ringEffect_->SetModel(ringEffectModel_);
            if (ringEffectCompareBillboard_) {
                ringEffectCompareBillboard_->SetModel(ringEffectModel_);
            }
            if (ringEffectCompareWorld_) {
                ringEffectCompareWorld_->SetModel(ringEffectModel_);
            }
        }
        isRingEffectModelDirty_ = false;
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
    if (walkSkinnedObject_) {
        walkSkinnedObject_->Update();
    }
    if (sneakWalkSkinnedObject_) {
        sneakWalkSkinnedObject_->Update();
    }
    if (effectCylinder_) {
        effectCylinderTime_ += 0.016f;
        if (effectCylinderModel_) {
            if (auto* material = effectCylinderModel_->GetMaterialData()) {
                material->color = {
                    effectCylinderColor_[0],
                    effectCylinderColor_[1],
                    effectCylinderColor_[2],
                    effectCylinderColor_[3]
                };
                if (isEffectCylinderUVScrollEnabled_) {
                    material->uvTransform = MatrixMath::MakeAffine(
                        { 1.0f, 1.0f, 1.0f },
                        { 0.0f, 0.0f, 0.0f },
                        { effectCylinderTime_ * effectCylinderUVScrollSpeedX_, effectCylinderTime_ * effectCylinderUVScrollSpeedY_, 0.0f });
                } else {
                    material->uvTransform = MatrixMath::MakeIdentity4x4();
                }
            }
        }
        effectCylinder_->Update();
    }
    for (auto& primitivePreviewObject : primitivePreviewObjects_) {
        primitivePreviewObject->Update();
    }
    if (ringEffect_ || ringEffectCompareBillboard_ || ringEffectCompareWorld_) {
        if (ringEffectModel_) {
            Matrix4x4 uvTransform = MatrixMath::MakeIdentity4x4();
            if (isRingUVScrollEnabled_) {
                uvTransform = MatrixMath::MakeAffine(
                    { 1.0f, 1.0f, 1.0f },
                    { 0.0f, 0.0f, 0.0f },
                    { ringAnimationTime_ * ringUVScrollSpeedX_, ringAnimationTime_ * ringUVScrollSpeedY_, 0.0f });
            }
            if (auto* material = ringEffectModel_->GetMaterialData()) {
                material->color = {
                    ringEffectColor_[0],
                    ringEffectColor_[1],
                    ringEffectColor_[2],
                    ringEffectColor_[3]
                };
                material->uvTransform = uvTransform;
            }
        }

        if (ringEffectPlaneModel_) {
            if (auto* material = ringEffectPlaneModel_->GetMaterialData()) {
                material->color = {
                    ringEffectPlaneColor_[0],
                    ringEffectPlaneColor_[1],
                    ringEffectPlaneColor_[2],
                    ringEffectPlaneColor_[3]
                };
            }
        }

        auto updateRingEffectObject = [&](Object3d* ringObject, Object3d* planeObject, const Vector3& translate, bool useBillboard, bool useAppearancePreview) {
            if (!ringObject) {
                return;
            }

            Vector3 effectRotation = ringEffectRotate_;
            if (useBillboard) {
                Vector3 cameraRotation = camera_->GetRotate();
                effectRotation.x += cameraRotation.x;
                effectRotation.y += cameraRotation.y;
                effectRotation.z += cameraRotation.z;
            }

            ringObject->SetTranslate(translate);
            ringObject->SetRotate(effectRotation);
            ringObject->SetScale(ringEffectScale_);
            ringObject->SetRingAppearanceEnabled(useAppearancePreview);
            ringObject->SetRingUVDirection(currentRingUVDirection_);
            ringObject->SetRingInnerRadiusRatio(0.45f);
            ringObject->SetRingStartAlpha(ringStartAlpha_);
            ringObject->SetRingEndAlpha(ringEndAlpha_);
            ringObject->SetRingStartFadeRange(ringStartFadeRange_);
            ringObject->SetRingEndFadeRange(ringEndFadeRange_);
            ringObject->SetRingInnerColor({ ringInnerColor_[0], ringInnerColor_[1], ringInnerColor_[2], ringInnerColor_[3] });
            ringObject->SetRingOuterColor({ ringOuterColor_[0], ringOuterColor_[1], ringOuterColor_[2], ringOuterColor_[3] });
            ringObject->Update();

            if (planeObject) {
                Vector3 planeTranslate = translate;
                planeTranslate.z += 0.01f;
                Vector3 planeRotation = effectRotation;
                planeRotation.x += std::numbers::pi_v<float> * 0.5f;

                planeObject->SetTranslate(planeTranslate);
                planeObject->SetRotate(planeRotation);
                planeObject->SetScale({
                    ringEffectScale_.x * ringEffectPlaneScale_,
                    ringEffectScale_.y * ringEffectPlaneScale_,
                    1.0f
                    });
                planeObject->Update();
            }
        };

        updateRingEffectObject(
            ringEffect_.get(),
            ringEffectPlane_.get(),
            ringEffectTranslate_,
            isRingBillboardEnabled_,
            isRingAppearancePreviewEnabled_);

        if (isRingEffectCompareVisible_) {
            Vector3 compareBillboardTranslate = ringEffectTranslate_;
            compareBillboardTranslate.x -= ringEffectCompareSpacing_ * 0.5f;
            Vector3 compareWorldTranslate = ringEffectTranslate_;
            compareWorldTranslate.x += ringEffectCompareSpacing_ * 0.5f;

            updateRingEffectObject(
                ringEffectCompareBillboard_.get(),
                ringEffectComparePlaneBillboard_.get(),
                compareBillboardTranslate,
                true,
                false);
            updateRingEffectObject(
                ringEffectCompareWorld_.get(),
                ringEffectComparePlaneWorld_.get(),
                compareWorldTranslate,
                false,
                false);
        }
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
    if (skinningEditor_) {
        if (previewSkeleton_) {
            UpdateSkeletonWorldTransforms(*previewSkeleton_);
        }
        if (previewSkeletonSecondary_) {
            UpdateSkeletonWorldTransforms(*previewSkeletonSecondary_);
        }
        if (walkSkeleton_) {
            UpdateSkeletonWorldTransforms(*walkSkeleton_);
        }
        if (sneakWalkSkeleton_) {
            UpdateSkeletonWorldTransforms(*sneakWalkSkeleton_);
        }
        skinningEditor_->Update();
        if (walkSkinnedModel_) {
            walkSkinnedModel_->UpdateSkinning();
        }
        if (sneakWalkSkinnedModel_) {
            sneakWalkSkinnedModel_->UpdateSkinning();
        }
    }

#ifdef _DEBUG
    if (skinningEditor_) {
        skinningEditor_->DrawImGui();
        skinningEditor_->DrawGizmo(camera_.get());
        if (walkSkinnedModel_) {
            walkSkinnedModel_->UpdateSkinning();
        }
        if (sneakWalkSkinnedModel_) {
            sneakWalkSkinnedModel_->UpdateSkinning();
        }
        skinningEditor_->DrawDebugOverlay(camera_.get());
    }
    ImGui::SetNextWindowSize(ImVec2(500, 200), ImGuiCond_Once);
    ImGui::Begin("DebugText");
    Vector2 spritePos = debugSprite_->GetPosition();
    ImGui::DragFloat2("Sprite Pos", &spritePos.x, 1.0f, -9999.0f, 9999.0f, "%4.1f");
    debugSprite_->SetPosition(spritePos);
    ImGui::End();

    auto DrawEffectParamsUI = [](const char* label, ParticleManager::EffectParams& params) {
        std::string prefix = label;

        int spawnCount = static_cast<int>(params.spawnCount);
        if (ImGui::DragInt((prefix + " Spawn Count").c_str(), &spawnCount, 1.0f, 1, 100)) {
            if (spawnCount < 1) {
                spawnCount = 1;
            }
            params.spawnCount = static_cast<uint32_t>(spawnCount);
        }

        ImGui::DragFloat2((prefix + " Scale X").c_str(), &params.scaleXRange.x, 0.01f, 0.01f, 4.0f, "%.2f");
        ImGui::DragFloat2((prefix + " Scale Y").c_str(), &params.scaleYRange.x, 0.01f, 0.01f, 6.0f, "%.2f");
        ImGui::DragFloat2((prefix + " Lifetime").c_str(), &params.lifeTimeRange.x, 0.01f, 0.01f, 3.0f, "%.2f");
        ImGui::DragFloat2((prefix + " Speed").c_str(), &params.speedRange.x, 0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat2((prefix + " Rotate Z").c_str(), &params.rotateZRange.x, 0.01f, -6.29f, 6.29f, "%.2f");

        Vector3 colorMin = { params.colorRRange.x, params.colorGRange.x, params.colorBRange.x };
        Vector3 colorMax = { params.colorRRange.y, params.colorGRange.y, params.colorBRange.y };
        if (ImGui::ColorEdit3((prefix + " Color Min").c_str(), &colorMin.x)) {
            params.colorRRange.x = colorMin.x;
            params.colorGRange.x = colorMin.y;
            params.colorBRange.x = colorMin.z;
        }
        if (ImGui::ColorEdit3((prefix + " Color Max").c_str(), &colorMax.x)) {
            params.colorRRange.y = colorMax.x;
            params.colorGRange.y = colorMax.y;
            params.colorBRange.y = colorMax.z;
        }
        };

    ImGui::Begin("Game Scene Menu");
    ImGui::Text("Press [T] to return to Title");
    ImGui::SeparatorText("Camera");
    Vector3 camTrans = camera_->GetTranslate();
    if (ImGui::DragFloat3("Cam Pos", &camTrans.x, 0.1f)) camera_->SetTranslate(camTrans);

    ImGui::SeparatorText("Skybox");
    ImGui::Checkbox("Show Skybox", &isSkyboxVisible_);
    ImGui::Checkbox("Follow Camera", &isSkyboxFollowCamera_);
    ImGui::DragFloat3("Skybox Scale", &skyboxScale_.x, 1.0f, 1.0f, 1000.0f, "%.1f");
    ImGui::TextWrapped("DDS: %s", skyboxTexturePath_.c_str());
    ImGui::Text("TextureIndex: %u", skyboxTextureIndex_);

    if (cloudVolume_) {
        auto& cloudParams = cloudVolume_->GetParameters();

        ImGui::SeparatorText("Volumetric Cloud");
        if (volumetricCloudPass) {
            bool isCloudPassEnabled = volumetricCloudPass->IsEnabled();
            if (ImGui::Checkbox("Cloud Pass Enabled", &isCloudPassEnabled)) {
                volumetricCloudPass->SetEnabled(isCloudPassEnabled);
                cloudProjectedBounds_ = volumetricCloudPass->BuildProjectedBounds(camera_.get(), cloudVolume_.get());
            }

            const char* cloudForceModeNames[] = {
                "None",
                "Force Skip",
                "Force Fullscreen",
                "Force Scissor",
                "Force Max Quality",
                "Force Aggressive LOD"
            };
            int cloudForceMode = static_cast<int>(volumetricCloudPass->GetForceMode());
            if (ImGui::Combo("Cloud Force Mode", &cloudForceMode, cloudForceModeNames, IM_ARRAYSIZE(cloudForceModeNames))) {
                volumetricCloudPass->SetForceMode(static_cast<VolumetricCloudPass::ForceMode>(cloudForceMode));
                cloudProjectedBounds_ = volumetricCloudPass->BuildProjectedBounds(camera_.get(), cloudVolume_.get());
            }
        }

        if (ImGui::Button("Reset Cloud Preset")) {
            cloudParams = MakeRecommendedCloudParameters();
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("Recommended visible starting point");
        ImGui::DragFloat3("Cloud Center", &cloudParams.center.x, 0.1f);
        ImGui::DragFloat3("Cloud HalfExtents", &cloudParams.halfExtents.x, 0.1f, 0.1f, 100.0f, "%.2f");
        ImGui::SliderFloat("Cloud Density", &cloudParams.density, 0.0f, 2.0f, "%.3f");
        ImGui::SliderFloat("Cloud Absorption", &cloudParams.absorption, 0.01f, 8.0f, "%.2f");
        ImGui::DragFloat3("Cloud Wind Dir", &cloudParams.windDirection.x, 0.01f, -1.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Cloud Wind Speed", &cloudParams.windSpeed, 0.0f, 5.0f, "%.2f");
        ImGui::DragFloat3("Cloud Sun Dir", &cloudParams.sunDirection.x, 0.01f, -1.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Cloud Light Absorption", &cloudParams.lightAbsorption, 0.0f, 8.0f, "%.2f");
        ImGui::ColorEdit4("Cloud Color (A = density scale)", &cloudParams.color.x);
        ImGui::SliderFloat("Cloud Noise Scale", &cloudParams.noiseScale, 0.01f, 2.0f, "%.3f");
        ImGui::SliderFloat("Cloud Detail Noise", &cloudParams.detailNoiseScale, 0.01f, 4.0f, "%.3f");
        ImGui::SliderFloat("Cloud Detail Weight", &cloudParams.detailWeight, 0.0f, 1.5f, "%.2f");
        ImGui::SliderFloat("Cloud Edge Fade", &cloudParams.edgeFade, 0.01f, 1.0f, "%.3f");
        ImGui::SliderFloat("Cloud Ambient", &cloudParams.ambientLighting, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("Cloud Sun Intensity", &cloudParams.sunIntensity, 0.0f, 4.0f, "%.2f");

        int cloudViewStepCount = static_cast<int>(cloudParams.viewStepCount);
        if (ImGui::SliderInt("Cloud View Steps", &cloudViewStepCount, 1, 256)) {
            cloudParams.viewStepCount = static_cast<uint32_t>(cloudViewStepCount);
        }

        int cloudLightStepCount = static_cast<int>(cloudParams.lightStepCount);
        if (ImGui::SliderInt("Cloud Light Steps", &cloudLightStepCount, 1, 32)) {
            cloudParams.lightStepCount = static_cast<uint32_t>(cloudLightStepCount);
        }

        if (volumetricCloudPass) {
            const char* cloudDebugViewNames[] = {
                "Final",
                "Alpha only",
                "Density only",
                "Light only"
            };
            int cloudDebugView = static_cast<int>(volumetricCloudPass->GetDebugViewMode());
            if (ImGui::Combo("Cloud Debug View", &cloudDebugView, cloudDebugViewNames, IM_ARRAYSIZE(cloudDebugViewNames))) {
                volumetricCloudPass->SetDebugViewMode(
                    static_cast<VolumetricCloudPass::DebugViewMode>(cloudDebugView));
            }
        }

        ImGui::SeparatorText("Cloud Optimization Debug");
        auto DrawCloudFlag = [](const char* label, bool value, const ImVec4& trueColor, const ImVec4& falseColor) {
            ImGui::TextUnformatted(label);
            ImGui::SameLine(220.0f);
            ImGui::TextColored(value ? trueColor : falseColor, value ? "true" : "false");
        };

        DrawCloudFlag("Cloud Visible", cloudProjectedBounds_.isVisible, ImVec4(0.30f, 1.00f, 0.35f, 1.0f), ImVec4(1.00f, 0.35f, 0.35f, 1.0f));
        DrawCloudFlag("Cloud Pass Skipped", cloudProjectedBounds_.isPassSkipped, ImVec4(1.00f, 0.35f, 0.35f, 1.0f), ImVec4(0.30f, 1.00f, 0.35f, 1.0f));
        DrawCloudFlag("Fullscreen Fallback", cloudProjectedBounds_.isFullScreenFallback, ImVec4(1.00f, 0.80f, 0.25f, 1.0f), ImVec4(0.45f, 0.85f, 1.00f, 1.0f));
        DrawCloudFlag("Use Fullscreen Scissor", cloudProjectedBounds_.useFullScreenScissor, ImVec4(1.00f, 0.80f, 0.25f, 1.0f), ImVec4(0.30f, 1.00f, 0.35f, 1.0f));
        DrawCloudFlag("Camera Inside Cloud", cloudProjectedBounds_.isCameraInsideCloud, ImVec4(1.00f, 0.80f, 0.25f, 1.0f), ImVec4(0.45f, 0.85f, 1.00f, 1.0f));
        DrawCloudFlag("Near Plane Crossing", cloudProjectedBounds_.isNearPlaneCrossing, ImVec4(1.00f, 0.80f, 0.25f, 1.0f), ImVec4(0.45f, 0.85f, 1.00f, 1.0f));

        const LONG scissorWidth = cloudProjectedBounds_.scissorRect.right - cloudProjectedBounds_.scissorRect.left;
        const LONG scissorHeight = cloudProjectedBounds_.scissorRect.bottom - cloudProjectedBounds_.scissorRect.top;
        ImGui::Text("Scissor Rect: L=%ld T=%ld R=%ld B=%ld", cloudProjectedBounds_.scissorRect.left, cloudProjectedBounds_.scissorRect.top, cloudProjectedBounds_.scissorRect.right, cloudProjectedBounds_.scissorRect.bottom);
        ImGui::Text("Scissor Size: %ld x %ld", scissorWidth, scissorHeight);
        ImGui::TextColored(
            (cloudProjectedBounds_.scissorAreaRatio >= 0.90f) ? ImVec4(1.00f, 0.45f, 0.35f, 1.0f) : ImVec4(0.35f, 1.00f, 0.45f, 1.0f),
            "Scissor Area Ratio: %.3f (%.1f%%)",
            cloudProjectedBounds_.scissorAreaRatio,
            cloudProjectedBounds_.scissorAreaRatio * 100.0f);
        ImGui::Text("Current ViewStep Scale: %.3f", cloudProjectedBounds_.currentViewStepScale);
        ImGui::Text("Current LightStep Scale: %.3f", cloudProjectedBounds_.currentLightStepScale);
        ImGui::Text("Estimated View Steps: %u", cloudProjectedBounds_.estimatedViewSteps);
        ImGui::Text("Estimated Light Steps: %u", cloudProjectedBounds_.estimatedLightSteps);
    }

    ImGui::SeparatorText("Environment Map");
    ImGui::Checkbox("Reflect Sphere", &isSphereEnvironmentMapEnabled_);
    ImGui::SliderFloat("Reflect Strength", &sphereEnvironmentMapIntensity_, 0.0f, 1.0f, "%.2f");
    ImGui::TextWrapped("Cubemap DDS: %s", skyboxTexturePath_.c_str());
    ImGui::Text("Cubemap TextureIndex: %u", skyboxTextureIndex_);

    ImGui::SeparatorText("Object Dissolve");
    ImGui::TextWrapped("Applies only to the sphere object for assignment verification.");
    ImGui::Checkbox("Enable Object Dissolve", &isObjectDissolveEnabled_);
    ImGui::SliderFloat("Object Dissolve Threshold", &objectDissolveThreshold_, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Object Dissolve Edge Width", &objectDissolveEdgeWidth_, 0.001f, 0.2f, "%.3f");
    ImGui::SliderFloat("Object Dissolve Edge Glow", &objectDissolveEdgeGlowStrength_, 0.0f, 4.0f, "%.2f");
    ImGui::SliderFloat("Object Dissolve Edge Noise", &objectDissolveEdgeNoiseStrength_, 0.0f, 1.0f, "%.2f");
    ImGui::ColorEdit4("Object Dissolve Edge Color", objectDissolveEdgeColor_.data());
    const char* objectDissolveMaskTextureNames[] = { "noise0", "noise1" };
    const char* objectDissolveMaskTexturePaths[] = {
        "resources/postEffect/noise0.png",
        "resources/postEffect/noise1.png"
    };
    if (ImGui::Combo("Object Dissolve Mask", &currentObjectDissolveMaskTexture_, objectDissolveMaskTextureNames, IM_ARRAYSIZE(objectDissolveMaskTextureNames))) {
        objectDissolveMaskTexturePath_ = objectDissolveMaskTexturePaths[currentObjectDissolveMaskTexture_];
        object3dSphere_->SetDissolveMaskTexture(objectDissolveMaskTexturePath_);
    }
    ImGui::TextWrapped("Mask Path: %s", objectDissolveMaskTexturePath_.c_str());

    ImGui::SeparatorText("Object Random Noise");
    ImGui::TextWrapped("Applies only to the sphere object for shader random verification.");
    ImGui::Checkbox("Enable Object Random", &isObjectRandomEnabled_);
    ImGui::Checkbox("Preview Object Random", &isObjectRandomPreview_);
    ImGui::SliderFloat("Object Random Intensity", &objectRandomIntensity_, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Object Random Time", &objectRandomTime_, 0.0f, 100.0f, "%.2f");

    ImGui::SeparatorText("Post Effects");
    auto DrawPostEffectUI = [](const char* label, uint32_t& enabled, float& intensity) {
        bool isEnabled = enabled != 0;
        if (ImGui::Checkbox(label, &isEnabled)) {
            enabled = isEnabled ? 1u : 0u;
        }
        std::string sliderLabel = std::string(label) + " Strength";
        ImGui::SliderFloat(sliderLabel.c_str(), &intensity, 0.0f, 1.0f, "%.2f");
        };
    auto ApplyRadialBlurPreset = [&](const RadialBlurPreset& preset) {
        postEffectParams.radialBlurEnabled = preset.enabled;
        postEffectParams.radialBlurStrength = preset.strength;
        postEffectParams.radialBlurCenter = preset.center;
        postEffectParams.radialBlurSampleCount = preset.sampleCount;
    };
    auto ApplyDissolvePreset = [&](const DissolvePreset& preset) {
        postEffectParams.dissolveEnabled = preset.enabled;
        postEffectParams.dissolveThreshold = preset.threshold;
        postEffectParams.dissolveEdgeWidth = preset.edgeWidth;
        postEffectParams.dissolveEdgeColor = preset.edgeColor;
    };
    auto ApplyOutlinePreset = [&](const OutlinePreset& preset) {
        postEffectParams.outlineMode = preset.outlineMode;
        postEffectParams.hybridColorSource = preset.hybridColorSource;
        postEffectParams.hybridColorWeight = preset.hybridColorWeight;
        postEffectParams.hybridDepthWeight = preset.hybridDepthWeight;
        postEffectParams.hybridNormalWeight = preset.hybridNormalWeight;
        postEffectParams.outlineIntensity = preset.outlineStrength;
        postEffectParams.outlineThickness = preset.outlineThickness;
        postEffectParams.outlineThreshold = preset.outlineThreshold;
        postEffectParams.outlineSoftness = preset.outlineSoftness;
        postEffectParams.outlineDepthThreshold = preset.outlineDepthThreshold;
        postEffectParams.outlineDepthStrength = preset.outlineDepthStrength;
        postEffectParams.outlineNormalThreshold = preset.outlineNormalThreshold;
        postEffectParams.outlineNormalStrength = preset.outlineNormalStrength;
        postEffectParams.outlineColor = preset.outlineColor;
    };
    bool gaussianEnabled = postEffectParams.gaussianEnabled != 0;
    if (ImGui::Checkbox("Gaussian", &gaussianEnabled)) {
        postEffectParams.gaussianEnabled = gaussianEnabled ? 1u : 0u;
    }
    ImGui::SliderFloat("Gaussian Strength", &postEffectParams.gaussianIntensity, 0.0f, 4.0f, "%.2f");
    bool radialBlurEnabled = postEffectParams.radialBlurEnabled != 0;
    if (ImGui::Checkbox("RadialBlur", &radialBlurEnabled)) {
        postEffectParams.radialBlurEnabled = radialBlurEnabled ? 1u : 0u;
    }
    static int radialBlurPresetIndex = 1;
    const char* radialBlurPresetNames[] = { "Weak", "Medium", "Strong", "Dramatic" };
    if (ImGui::Combo("RadialBlur Preset", &radialBlurPresetIndex, radialBlurPresetNames, IM_ARRAYSIZE(radialBlurPresetNames))) {
        ApplyRadialBlurPreset(kRadialBlurPresets[radialBlurPresetIndex]);
    }
    ImGui::SliderFloat("RadialBlur Strength", &postEffectParams.radialBlurStrength, 0.0f, 0.2f, "%.3f");
    ImGui::SliderFloat2("RadialBlur Center", postEffectParams.radialBlurCenter.data(), 0.0f, 1.0f, "%.2f");
    int radialBlurSampleCount = static_cast<int>(postEffectParams.radialBlurSampleCount);
    if (ImGui::SliderInt("RadialBlur Sample Count", &radialBlurSampleCount, 1, 32)) {
        postEffectParams.radialBlurSampleCount = static_cast<uint32_t>(radialBlurSampleCount);
    }
    const char* dissolveNoiseTextureNames[] = { "noise0", "noise1" };
    const char* dissolveNoiseTexturePaths[] = {
        "resources/postEffect/noise0.png",
        "resources/postEffect/noise1.png"
    };
    if (ImGui::Combo("Dissolve Noise Texture", &currentDissolveNoiseTexture_, dissolveNoiseTextureNames, IM_ARRAYSIZE(dissolveNoiseTextureNames))) {
        texManager->LoadTexture(dissolveNoiseTexturePaths[currentDissolveNoiseTexture_]);
        dxCommon->SetDissolveNoiseTextureIndex(
            texManager->GetTextureIndexByFilePath(dissolveNoiseTexturePaths[currentDissolveNoiseTexture_]));
    }
    bool dissolveEnabled = postEffectParams.dissolveEnabled != 0;
    if (ImGui::Checkbox("Dissolve", &dissolveEnabled)) {
        postEffectParams.dissolveEnabled = dissolveEnabled ? 1u : 0u;
    }
    static int dissolvePresetIndex = 1;
    const char* dissolvePresetNames[] = { "Weak", "Medium", "Strong", "Dramatic" };
    if (ImGui::Combo("Dissolve Preset", &dissolvePresetIndex, dissolvePresetNames, IM_ARRAYSIZE(dissolvePresetNames))) {
        ApplyDissolvePreset(kDissolvePresets[dissolvePresetIndex]);
    }
    ImGui::SliderFloat("Dissolve Threshold", &postEffectParams.dissolveThreshold, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Dissolve Edge Width", &postEffectParams.dissolveEdgeWidth, 0.001f, 0.2f, "%.3f");
    ImGui::ColorEdit4("Dissolve Edge Color", postEffectParams.dissolveEdgeColor.data());
    bool outlineEnabled = postEffectParams.outlineMode != 0;
    if (ImGui::Checkbox("Outline", &outlineEnabled)) {
        if (!outlineEnabled) {
            postEffectParams.outlineMode = 0;
        } else if (postEffectParams.outlineMode == 0) {
            postEffectParams.outlineMode = 1;
        }
    }
    const char* outlineModeNames[] = { "Off", "ColorDiff8", "Sobel", "Depth", "Hybrid", "Normal", "FinalHybrid" };
    int outlineMode = static_cast<int>(postEffectParams.outlineMode);
    if (ImGui::Combo("Outline Mode", &outlineMode, outlineModeNames, IM_ARRAYSIZE(outlineModeNames))) {
        postEffectParams.outlineMode = static_cast<uint32_t>(outlineMode);
    }
    static int outlinePresetIndex = 0;
    const char* outlinePresetNames[] = {
        "Balanced",
        "Color Emphasis",
        "Depth Emphasis",
        "Soft Outline",
        "FinalHybrid Balanced",
        "FinalHybrid Color Emphasis",
        "FinalHybrid Depth Emphasis",
        "FinalHybrid Normal Emphasis"
    };
    if (ImGui::Combo("Outline Preset", &outlinePresetIndex, outlinePresetNames, IM_ARRAYSIZE(outlinePresetNames))) {
        ApplyOutlinePreset(kOutlinePresets[outlinePresetIndex]);
    }
    ImGui::SliderFloat("Outline Strength", &postEffectParams.outlineIntensity, 0.0f, 10.0f, "%.2f");
    ImGui::SliderFloat("Outline Thickness", &postEffectParams.outlineThickness, 0.5f, 4.0f, "%.2f");
    ImGui::SliderFloat("Outline Threshold", &postEffectParams.outlineThreshold, 0.0f, 1.5f, "%.3f");
    ImGui::SliderFloat("Outline Softness", &postEffectParams.outlineSoftness, 0.001f, 1.0f, "%.3f");
    ImGui::SliderFloat("Outline Depth Threshold", &postEffectParams.outlineDepthThreshold, 0.0001f, 0.05f, "%.4f");
    ImGui::SliderFloat("Outline Depth Strength", &postEffectParams.outlineDepthStrength, 0.0f, 50.0f, "%.2f");
    ImGui::SliderFloat("Outline Normal Threshold", &postEffectParams.outlineNormalThreshold, 0.0f, 2.0f, "%.3f");
    ImGui::SliderFloat("Outline Normal Strength", &postEffectParams.outlineNormalStrength, 0.0f, 20.0f, "%.2f");
    if (postEffectParams.outlineMode == 4 || postEffectParams.outlineMode == 6) {
        const char* hybridColorSourceNames[] = { "ColorDiff8", "Sobel" };
        int hybridColorSourceIndex = (postEffectParams.hybridColorSource == 1u) ? 0 : 1;
        if (ImGui::Combo("Hybrid Color Source", &hybridColorSourceIndex, hybridColorSourceNames, IM_ARRAYSIZE(hybridColorSourceNames))) {
            postEffectParams.hybridColorSource = (hybridColorSourceIndex == 0) ? 1u : 2u;
        }
        ImGui::SliderFloat("Hybrid Color Weight", &postEffectParams.hybridColorWeight, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("Hybrid Depth Weight", &postEffectParams.hybridDepthWeight, 0.0f, 2.0f, "%.2f");
        if (postEffectParams.outlineMode == 6) {
            ImGui::SliderFloat("Hybrid Normal Weight", &postEffectParams.hybridNormalWeight, 0.0f, 2.0f, "%.2f");
        }
    }
    ImGui::ColorEdit4("Outline Color", postEffectParams.outlineColor.data());
    DrawPostEffectUI("Grayscale", postEffectParams.grayscaleEnabled, postEffectParams.grayscaleIntensity);
    DrawPostEffectUI("Sepia", postEffectParams.sepiaEnabled, postEffectParams.sepiaIntensity);
    DrawPostEffectUI("Invert", postEffectParams.invertEnabled, postEffectParams.invertIntensity);
    DrawPostEffectUI("Vignette", postEffectParams.vignetteEnabled, postEffectParams.vignetteIntensity);
    DrawPostEffectUI("Smoothing", postEffectParams.smoothingEnabled, postEffectParams.smoothingIntensity);

    ImGui::SeparatorText("Primitive Preview");
    ImGui::Checkbox("Show Primitive Preview", &isPrimitivePreviewVisible_);
    ImGui::Text("Front Row : Plane / Circle / Ring / Triangle");
    ImGui::Text("Back Row  : Box / Cylinder / Cone / Torus");
    ImGui::Text("Ring uses gradationLine.png (AddressV = CLAMP)");

    ImGui::SeparatorText("Ring Effect Preview");
    auto ResetRingShapePreview = [&]() {
        isRingBillboardEnabled_ = false;
        ringEffectTranslate_ = { 0.0f, 1.2f, 3.0f };
        ringEffectRotate_ = { 0.0f, 0.0f, 0.0f };
        ringEffectScale_ = { 1.6f, 1.6f, 1.0f };
        ringShapeStartAngle_ = 0.0f;
        ringShapeEndAngle_ = 360.0f;
        ringShapeStartRadius_ = 1.0f;
        ringShapeEndRadius_ = 1.0f;
        isRingEffectModelDirty_ = true;
    };
    auto ResetRingEffectPreview = [&]() {
        isRingAppearancePreviewEnabled_ = true;
        isRingBillboardEnabled_ = true;
        ringEffectPlaneScale_ = 1.35f;
        currentRingUVDirection_ = 0;
        ringInnerColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
        ringOuterColor_ = { 1.0f, 0.6f, 0.2f, 1.0f };
        ringStartAlpha_ = 1.0f;
        ringEndAlpha_ = 1.0f;
        ringStartFadeRange_ = 0.15f;
        ringEndFadeRange_ = 0.15f;
        ringEffectPlaneColor_ = { 0.45f, 0.65f, 1.0f, 0.55f };
        ringEffectColor_ = { 1.0f, 1.0f, 1.0f, 0.9f };
        isRingAnimationEnabled_ = false;
        isRingUVScrollEnabled_ = false;
        isRingAlphaAnimationEnabled_ = false;
        isRingRadiusAnimationEnabled_ = false;
        isRingAngleAnimationEnabled_ = false;
        ringUVScrollSpeedX_ = 0.25f;
        ringUVScrollSpeedY_ = 0.0f;
        ringAlphaAnimationSpeed_ = 1.5f;
        ringAlphaAnimationMin_ = 0.15f;
        ringAlphaAnimationMax_ = 1.0f;
        ringRadiusAnimationSpeed_ = 1.25f;
        ringRadiusAnimationStartMin_ = 0.8f;
        ringRadiusAnimationStartMax_ = 1.2f;
        ringRadiusAnimationEndMin_ = 0.8f;
        ringRadiusAnimationEndMax_ = 1.2f;
        ringAngleAnimationSpeed_ = 1.0f;
        ringAngleAnimationSpan_ = 180.0f;
        ringAngleAnimationBase_ = 0.0f;
    };
    ImGui::Checkbox("Show Ring Effect", &isRingEffectVisible_);
    ImGui::Checkbox("Show Effect Plane", &isRingEffectPlaneVisible_);
    ImGui::Checkbox("Show Slide Compare", &isRingEffectCompareVisible_);
    if (ImGui::Button("Preset Slide Billboard")) {
        isRingEffectVisible_ = true;
        isRingEffectPlaneVisible_ = true;
        isRingEffectCompareVisible_ = true;
        isRingBillboardEnabled_ = true;
        isRingAppearancePreviewEnabled_ = false;
        ringEffectTranslate_ = { 0.0f, 1.2f, 3.0f };
        ringEffectRotate_ = { 0.0f, 0.0f, 0.0f };
        ringEffectScale_ = { 1.6f, 1.6f, 1.0f };
        ringEffectPlaneScale_ = 1.35f;
        ringEffectCompareSpacing_ = 4.2f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Preset Slide World")) {
        isRingEffectVisible_ = true;
        isRingEffectPlaneVisible_ = true;
        isRingEffectCompareVisible_ = true;
        isRingBillboardEnabled_ = false;
        isRingAppearancePreviewEnabled_ = false;
        ringEffectTranslate_ = { 0.0f, 1.2f, 3.0f };
        ringEffectRotate_ = { 1.05f, 0.0f, 0.35f };
        ringEffectScale_ = { 1.6f, 1.6f, 1.0f };
        ringEffectPlaneScale_ = 1.35f;
        ringEffectCompareSpacing_ = 4.2f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Shape Preview")) {
        ResetRingShapePreview();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Effect Preview")) {
        ResetRingEffectPreview();
    }
    ImGui::SliderFloat("Slide Compare Spacing", &ringEffectCompareSpacing_, 1.0f, 8.0f, "%.2f");
    ImGui::Text("Ring : gradationLine.png");
    ImGui::Text("Plane: circle2.png");
    ImGui::Text("Slide Compare = left billboard / right world");
    ImGui::SeparatorText("Shape Preview");
    ImGui::TextWrapped("Shape confirms start/end angle and radius. Billboard is usually OFF here so rotation stays readable.");
    ImGui::DragFloat3("Ring Position", &ringEffectTranslate_.x, 0.05f);
    if (isRingBillboardEnabled_) {
        ImGui::BeginDisabled();
    }
    ImGui::DragFloat3("Ring Rotation", &ringEffectRotate_.x, 0.01f);
    if (isRingBillboardEnabled_) {
        ImGui::EndDisabled();
        ImGui::TextDisabled("Rotation is disabled while Ring Billboard is ON.");
    }
    ImGui::SliderFloat3("Ring Scale", &ringEffectScale_.x, 0.2f, 3.0f, "%.2f");
    if (ImGui::SliderFloat("Ring Start Angle", &ringShapeStartAngle_, 0.0f, 360.0f, "%.1f")) {
        isRingEffectModelDirty_ = true;
    }
    if (ImGui::SliderFloat("Ring End Angle", &ringShapeEndAngle_, 0.0f, 360.0f, "%.1f")) {
        isRingEffectModelDirty_ = true;
    }
    if (ImGui::SliderFloat("Ring Start Radius", &ringShapeStartRadius_, 0.2f, 2.0f, "%.2f")) {
        isRingEffectModelDirty_ = true;
    }
    if (ImGui::SliderFloat("Ring End Radius", &ringShapeEndRadius_, 0.2f, 2.0f, "%.2f")) {
        isRingEffectModelDirty_ = true;
    }
    ImGui::SeparatorText("Effect Preview");
    ImGui::TextWrapped("Effect confirms billboard, appearance extensions, color/alpha and animation. Turn extensions ON when checking the expanded look.");
    ImGui::Checkbox("Ring Billboard", &isRingBillboardEnabled_);
    ImGui::Checkbox("Use Ring Appearance Extensions", &isRingAppearancePreviewEnabled_);
    ImGui::SliderFloat("Effect Plane Scale", &ringEffectPlaneScale_, 0.5f, 3.0f, "%.2f");
    ImGui::ColorEdit4("Effect Plane Color", ringEffectPlaneColor_.data());
    ImGui::ColorEdit4("Ring Tint", ringEffectColor_.data());
    const char* ringUVDirectionNames[] = { "Horizontal", "Vertical" };
    if (!isRingAppearancePreviewEnabled_) {
        ImGui::BeginDisabled();
    }
    ImGui::Combo("Ring UV Direction", &currentRingUVDirection_, ringUVDirectionNames, IM_ARRAYSIZE(ringUVDirectionNames));
    ImGui::ColorEdit4("Ring Inner Color", ringInnerColor_.data());
    ImGui::ColorEdit4("Ring Outer Color", ringOuterColor_.data());
    ImGui::SliderFloat("Ring Start Alpha", &ringStartAlpha_, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Ring End Alpha", &ringEndAlpha_, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Ring Start Fade", &ringStartFadeRange_, 0.001f, 1.0f, "%.3f");
    ImGui::SliderFloat("Ring End Fade", &ringEndFadeRange_, 0.001f, 1.0f, "%.3f");
    ImGui::Checkbox("Ring Animation", &isRingAnimationEnabled_);
    ImGui::Checkbox("Ring UV Scroll", &isRingUVScrollEnabled_);
    ImGui::SliderFloat2("Ring UV Scroll Speed", &ringUVScrollSpeedX_, -2.0f, 2.0f, "%.2f");
    ImGui::Checkbox("Ring Alpha Animation", &isRingAlphaAnimationEnabled_);
    ImGui::SliderFloat("Ring Alpha Speed", &ringAlphaAnimationSpeed_, 0.0f, 10.0f, "%.2f");
    ImGui::SliderFloat("Ring Alpha Min", &ringAlphaAnimationMin_, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Ring Alpha Max", &ringAlphaAnimationMax_, 0.0f, 1.0f, "%.2f");
    ImGui::Checkbox("Ring Radius Animation", &isRingRadiusAnimationEnabled_);
    ImGui::SliderFloat("Ring Radius Speed", &ringRadiusAnimationSpeed_, 0.0f, 10.0f, "%.2f");
    ImGui::SliderFloat("Ring Start Radius Min", &ringRadiusAnimationStartMin_, 0.0f, 2.0f, "%.2f");
    ImGui::SliderFloat("Ring Start Radius Max", &ringRadiusAnimationStartMax_, 0.0f, 2.0f, "%.2f");
    ImGui::SliderFloat("Ring End Radius Min", &ringRadiusAnimationEndMin_, 0.0f, 2.0f, "%.2f");
    ImGui::SliderFloat("Ring End Radius Max", &ringRadiusAnimationEndMax_, 0.0f, 2.0f, "%.2f");
    ImGui::Checkbox("Ring Angle Animation", &isRingAngleAnimationEnabled_);
    ImGui::SliderFloat("Ring Angle Speed", &ringAngleAnimationSpeed_, 0.0f, 10.0f, "%.2f");
    ImGui::SliderFloat("Ring Angle Base", &ringAngleAnimationBase_, 0.0f, 360.0f, "%.1f");
    ImGui::SliderFloat("Ring Angle Span", &ringAngleAnimationSpan_, 0.0f, 360.0f, "%.1f");
    if (!isRingAppearancePreviewEnabled_) {
        ImGui::EndDisabled();
        ImGui::TextDisabled("Enable Ring Appearance Extensions to edit UV/color/alpha/animation settings.");
    }

    ImGui::SeparatorText("Cylinder Portal Effect");
    if (effectCylinder_) {
        ImGui::ColorEdit4("Portal Color", effectCylinderColor_.data());
        ImGui::Checkbox("Portal UV Scroll", &isEffectCylinderUVScrollEnabled_);
        ImGui::SliderFloat2("Portal Scroll Speed", &effectCylinderUVScrollSpeedX_, -2.0f, 2.0f, "%.2f");
        
        Transform& cylTf = effectCylinder_->GetTransform();
        ImGui::DragFloat3("Portal Pos", &cylTf.translate.x, 0.1f);
        ImGui::DragFloat3("Portal Rot", &cylTf.rotate.x, 0.01f);
        ImGui::DragFloat3("Portal Scl", &cylTf.scale.x, 0.1f);
    }

    ImGui::SeparatorText("Particle Texture");
    const char* particleTextureNames[] = { "uvChecker", "Circle2", "Fence" };
    const char* particleTexturePaths[] = {
        "resources/obj/axis/uvChecker.png",
        "resources/particle/circle2.png",
        "resources/obj/fence/fence.png"
    };
    if (ImGui::Combo("Particle Texture", &currentParticleTexture_, particleTextureNames, IM_ARRAYSIZE(particleTextureNames))) {
        particleTexturePath_ = particleTexturePaths[currentParticleTexture_];
        particleManager->SetTexture(particleTexturePath_);
    }
    ImGui::TextWrapped("Particle Texture Path: %s", particleTexturePath_.c_str());

    ImGui::SeparatorText("Hit Effect");
    ImGui::TextWrapped("Main submission target: plane billboard particles stretched into hit streaks.");
    if (ImGui::Button("Emit Hit")) {
        particleManager->Emit("Hit", object3dSphere_->GetTransform().translate, hitEffectParams.spawnCount);
    }
    ImGui::Text("Hit Trigger: [Space] / [H]");

    ImGui::SeparatorText("Hit Params");
    DrawEffectParamsUI("Hit", hitEffectParams);

    if (ImGui::CollapsingHeader("Other Effects (Optional)")) {
        if (ImGui::Button("Emit Fireball")) {
            particleManager->Emit("Fireball", object3dSphere_->GetTransform().translate, fireballEffectParams.spawnCount);
        }
        ImGui::SameLine();
        if (ImGui::Button("Emit Wind")) {
            particleManager->Emit("Wind", object3dSphere_->GetTransform().translate, windEffectParams.spawnCount);
        }

        ImGui::SeparatorText("Fireball Params");
        DrawEffectParamsUI("Fireball", fireballEffectParams);

        ImGui::SeparatorText("Wind Params");
        DrawEffectParamsUI("Wind", windEffectParams);
    }

    ImGui::SeparatorText("Particle Smoke Test");
    if (ImGui::Button("Emit Basic Particle")) {
        particleManager->Emit(particleTexturePath_, object3dSphere_->GetTransform().translate, 1);
    }
    ImGui::Text("Trigger: [P]");

    ImGui::SeparatorText("Target Object Selection");
    ImGui::Combo("Target", &targetObjectIndex_, "Fence\0Sphere\0");

    Object3d* targetObj = (targetObjectIndex_ == 0) ? object3d_.get() : object3dSphere_.get();

    ImGui::SeparatorText("Model Transform");
    Transform& tf = targetObj->GetTransform();
    ImGui::DragFloat3("Pos", &tf.translate.x, 0.1f);
    ImGui::DragFloat3("Rot", &tf.rotate.x, 0.01f);
    ImGui::DragFloat3("Scl", &tf.scale.x, 0.1f);

    ImGui::SeparatorText("Model Texture");
    const char* modelTextureNames[] = { "uvChecker", "FenceTexture", "MonsterBall" };
    if (ImGui::Combo("Texture", &currentModelTexture_, modelTextureNames, IM_ARRAYSIZE(modelTextureNames))) {
        Model* targetModel = (targetObjectIndex_ == 0) ? modelFence_ : modelSphere_;
        if (targetModel) {
            if (currentModelTexture_ == 0) targetModel->SetTextureIndex(texIndexUvChecker_);
            else if (currentModelTexture_ == 1) targetModel->SetTextureIndex(texIndexFence_);
            else if (currentModelTexture_ == 2) targetModel->SetTextureIndex(texIndexMonsterBall_);
        }
    }

    ImGui::SeparatorText("Lighting & Material");
    auto* lightData = targetObj->GetDirectionalLightData();
    if (lightData) {
        if (ImGui::SliderFloat3("LightDir", &lightData->direction.x, -1.0f, 1.0f)) {
            float len = std::sqrt(lightData->direction.x * lightData->direction.x +
                lightData->direction.y * lightData->direction.y +
                lightData->direction.z * lightData->direction.z);
            if (len > 0.0f) {
                lightData->direction.x /= len;
                lightData->direction.y /= len;
                lightData->direction.z /= len;
            }
        }
        ImGui::ColorEdit3("LightColor", &lightData->color.x);
        ImGui::DragFloat("Intensity", &lightData->intensity, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Shininess", &lightData->shininess, 1.0f, 1.0f, 256.0f, "%.1f");
    }

    ImGui::SeparatorText("Blend Mode");
    ImGui::Combo("Blend", &currentBlendMode_, blendModeNames_, IM_ARRAYSIZE(blendModeNames_));
    ImGui::End();

    ImGui::SetNextWindowSize(ImVec2(320, 520), ImGuiCond_Once);
    ImGui::Begin("Scene Visibility");
    auto setAllVisibility = [this](bool isVisible) {
        isSkyboxVisible_ = isVisible;
        isFenceVisible_ = isVisible;
        isSphereVisible_ = isVisible;
        isAnimatedCubeVisible_ = isVisible;
        isSkinnedModelVisible_ = isVisible;
        isPrimitivePreviewVisible_ = isVisible;
        isEffectCylinderVisible_ = isVisible;
        isRingEffectVisible_ = isVisible;
        isRingEffectPlaneVisible_ = isVisible;
        isRingEffectCompareVisible_ = isVisible;
        isParticleVisible_ = isVisible;
        isVolumetricCloudVisible_ = isVisible;
        isDebugSpriteVisible_ = isVisible;
        };

    if (ImGui::Button("Show All")) {
        setAllVisibility(true);
    }
    ImGui::SameLine();
    if (ImGui::Button("Hide All")) {
        setAllVisibility(false);
    }

    ImGui::SeparatorText("Models");
    ImGui::Checkbox("Skybox", &isSkyboxVisible_);
    ImGui::Checkbox("Fence", &isFenceVisible_);
    ImGui::Checkbox("Sphere", &isSphereVisible_);
    ImGui::Checkbox("AnimatedCube", &isAnimatedCubeVisible_);
    ImGui::Checkbox("Active Skinned Model", &isSkinnedModelVisible_);
    ImGui::Checkbox("Primitive Preview", &isPrimitivePreviewVisible_);

    ImGui::SeparatorText("Particles / Effects");
    ImGui::Checkbox("ParticleManager", &isParticleVisible_);
    ImGui::Checkbox("Ring Effect", &isRingEffectVisible_);
    ImGui::Checkbox("Ring Effect Plane", &isRingEffectPlaneVisible_);
    ImGui::Checkbox("Ring Compare", &isRingEffectCompareVisible_);
    ImGui::Checkbox("Cylinder Portal", &isEffectCylinderVisible_);
    ImGui::Checkbox("Volumetric Cloud", &isVolumetricCloudVisible_);

    ImGui::SeparatorText("Debug");
    ImGui::Checkbox("Debug Sprite", &isDebugSpriteVisible_);
    ImGui::End();
#endif

    if (volumetricCloudPass && cloudVolume_) {
        cloudProjectedBounds_ = volumetricCloudPass->BuildProjectedBounds(camera_.get(), cloudVolume_.get());
    } else {
        cloudProjectedBounds_ = {};
    }
    }

void GameScene::Draw() {
    auto dxCommon = MyGame::GetInstance()->GetDxCommon();
    auto object3dCommon = MyGame::GetInstance()->GetObject3dCommon();
    auto skyboxCommon = MyGame::GetInstance()->GetSkyboxCommon();
    auto volumetricCloudPass = MyGame::GetInstance()->GetVolumetricCloudPass();
    auto particleManager = ParticleManager::GetInstance();
    auto spriteCommon = MyGame::GetInstance()->GetSpriteCommon();

    if (isSkyboxVisible_) {
        skyboxCommon->CommonDrawSetting();
        skybox_->Draw();
    }

    object3dCommon->CommonDrawSetting((Object3dCommon::BlendMode)currentBlendMode_);

    if (isFenceVisible_) {
        object3d_->Draw();
    }
    if (isSphereVisible_) {
        object3dSphere_->Draw();
    }
    if (isAnimatedCubeVisible_ && animatedCubeObject_) {
        animatedCubeObject_->Draw();
    }
    const Skeleton* activeSkinningTarget = skinningEditor_ ? skinningEditor_->GetTargetSkeleton() : nullptr;
    if (isSkinnedModelVisible_ && walkSkinnedObject_ && activeSkinningTarget == walkSkeleton_.get()) {
        walkSkinnedObject_->Draw();
    }
    if (isSkinnedModelVisible_ && sneakWalkSkinnedObject_ && activeSkinningTarget == sneakWalkSkeleton_.get()) {
        sneakWalkSkinnedObject_->Draw();
    }
    if (isEffectCylinderVisible_ && effectCylinder_) {
        object3dCommon->CommonDrawSetting(Object3dCommon::BlendMode::kAdd);
        effectCylinder_->Draw();
        object3dCommon->CommonDrawSetting((Object3dCommon::BlendMode)currentBlendMode_);
    }
    if (isPrimitivePreviewVisible_) {
        for (auto& primitivePreviewObject : primitivePreviewObjects_) {
            primitivePreviewObject->Draw();
        }
    }
    if (isRingEffectVisible_) {
        if (isRingEffectPlaneVisible_ && ringEffectPlane_) {
            ringEffectPlane_->Draw();
        }
        if (ringEffect_) {
            ringEffect_->Draw();
        }
    }
    if (isRingEffectCompareVisible_) {
        if (ringEffectComparePlaneBillboard_) {
            ringEffectComparePlaneBillboard_->Draw();
        }
        if (ringEffectCompareBillboard_) {
            ringEffectCompareBillboard_->Draw();
        }
        if (ringEffectComparePlaneWorld_) {
            ringEffectComparePlaneWorld_->Draw();
        }
        if (ringEffectCompareWorld_) {
            ringEffectCompareWorld_->Draw();
        }
    }

    if (isVolumetricCloudVisible_ && volumetricCloudPass && cloudVolume_) {
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

    if (isParticleVisible_) {
        particleManager->Draw();
    }

    spriteCommon->CommonDrawSetting();
    if (isDebugSpriteVisible_) {
        debugSprite_->Draw();
    }
}
