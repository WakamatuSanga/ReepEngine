#include "GameScene.h"
#include "SceneManager.h"
#include "TitleScene.h"
#include "MyGame.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Graphics/Model/Model.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Particle/ParticleManager.h"
#include "Engine/Graphics/Shadow/ScreenSpaceFakeShadowPass.h"
#include "Engine/Graphics/Skybox/Skybox.h"
#include "Engine/Graphics/Sprite/Sprite.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Animation/Skeleton.h"
#include "Engine/Graphics/Model/GltfAnimationLoader.h"
#include "Engine/Graphics/Model/GltfSkinnedModel.h"
#include "Engine/Graphics/Model/GltfSkeletonLoader.h"
#include "Engine/Graphics/Effect/PrimitiveEffectSystem.h"
#include "Engine/Graphics/Particle/GpuParticleSystem.h"
#include "Engine/Utility/Logger.h"
#include "Engine/Editor/SkinningEditor.h"
#include "Engine/Editor/Camera/EditorCameraController.h"
#include "Engine/Editor/BlenderSync/BlenderLiveSync.h"
#include "Engine/Level/LevelSceneRuntime.h"
#include "Engine/Game/Camera/CameraShakeController.h"
#include "Engine/Game/Camera/RailShooterCameraRig.h"
#include "Engine/Game/Collision/PlayerEnemyBulletCollision.h"
#include "Engine/Game/Effect/CombatEffectController.h"
#include "Engine/Game/Effect/GpuParticleEffectPlayer.h"
#include "Engine/Game/Effect/PostEffectController.h"
#include "Engine/Game/Enemy/EnemyAttackController.h"
#include "Engine/Game/Enemy/EnemyBulletManager.h"
#include "Engine/Game/Enemy/EnemyManager.h"
#include "Engine/Game/GameState/GameOverFlowController.h"
#include "Engine/Game/GameState/PlayerDeathSequenceController.h"
#include "Engine/Game/Player/BoostController.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Game/Player/PlayerBulletManager.h"
#include "Engine/Game/Player/PlayerRailController.h"
#include "Engine/Game/Collision/PlayerBulletEnemyCollision.h"
#include "Engine/Game/RailShooter/EventActionDispatcher.h"
#include "Engine/Game/RailShooter/EnemySpawnActionBridge.h"
#include "Engine/Game/RailShooter/PlayerEventTriggerBridge.h"
#include "Engine/Game/RailShooter/PostEffectActionBridge.h"
#include "Engine/Game/RailShooter/RailShooterEventActionBridge.h"
#include "Engine/Game/RailShooter/StartupEnemySpawnController.h"
#include "Engine/Core/FrameTimer.h"
#include "Engine/Core/GameViewport.h"
#include "Engine/Core/RuntimeModeController.h"
#include "Engine/Core/SrvManager.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <numbers>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr bool kEnableSkinningPreviewTargets = false;
    constexpr bool kEnableSimpleSkinSkinningTarget = false;

    std::string FindResourcePathCandidate(const std::string& path) {
        const std::array<std::filesystem::path, 6> basePaths = {
            std::filesystem::path{},
            std::filesystem::path{ "project" },
            std::filesystem::path{ ".." } / "project",
            std::filesystem::path{ ".." } / ".." / "project",
            std::filesystem::path{ ".." } / ".." / ".." / "project",
            std::filesystem::path{ ".." } / ".." / ".." / ".." / "project",
        };

        for (const std::filesystem::path& basePath : basePaths) {
            const std::filesystem::path candidate = basePath.empty()
                ? std::filesystem::path(path)
                : basePath / path;
            if (std::filesystem::exists(candidate)) {
                return candidate.generic_string();
            }
        }

        return {};
    }

    std::string ResolveResourcePath(const std::string& preferredPath, const std::string& fallbackPath = {}) {
        if (std::string resolvedPath = FindResourcePathCandidate(preferredPath); !resolvedPath.empty()) {
            Logger::Log("[GameScene] Resolved resource: " + preferredPath + " -> " + resolvedPath);
            return resolvedPath;
        }

        if (!fallbackPath.empty()) {
            if (std::string resolvedPath = FindResourcePathCandidate(fallbackPath); !resolvedPath.empty()) {
                Logger::Log("[GameScene] Resolved resource fallback: " + preferredPath + " -> " + resolvedPath);
                return resolvedPath;
            }
        }

        Logger::Log(
            "[GameScene] Resource missing: preferred=" + preferredPath +
            " fallback=" + fallbackPath +
            " cwd=" + std::filesystem::current_path().generic_string());
        return preferredPath;
    }

    std::string MakeDisplayPath(const std::string& path) {
        try {
            return std::filesystem::absolute(std::filesystem::path(path)).lexically_normal().generic_string();
        } catch (...) {
            return path;
        }
    }

    Vector3 ExtractMatrixScale(const Matrix4x4& matrix) {
        return {
            std::sqrt(
                (matrix.m[0][0] * matrix.m[0][0]) +
                (matrix.m[0][1] * matrix.m[0][1]) +
                (matrix.m[0][2] * matrix.m[0][2])),
            std::sqrt(
                (matrix.m[1][0] * matrix.m[1][0]) +
                (matrix.m[1][1] * matrix.m[1][1]) +
                (matrix.m[1][2] * matrix.m[1][2])),
            std::sqrt(
                (matrix.m[2][0] * matrix.m[2][0]) +
                (matrix.m[2][1] * matrix.m[2][1]) +
                (matrix.m[2][2] * matrix.m[2][2]))
        };
    }

    std::string FormatVector3(const Vector3& value) {
        return
            "(" + std::to_string(value.x) +
            ", " + std::to_string(value.y) +
            ", " + std::to_string(value.z) + ")";
    }

    Vector3 RadiansToDegrees(const Vector3& radians) {
        constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>;
        return {
            radians.x * kRadToDeg,
            radians.y * kRadToDeg,
            radians.z * kRadToDeg
        };
    }

    SkinningEditor::BoundsInfo ToBoundsInfo(const GltfSkinnedModel::Bounds& bounds) {
        SkinningEditor::BoundsInfo result{};
        result.isValid = bounds.isValid;
        result.min = bounds.min;
        result.max = bounds.max;
        result.size = bounds.size;
        result.center = bounds.center;
        return result;
    }

    SkinningEditor::TargetPreviewInfo BuildTargetPreviewInfo(
        const Skeleton* skeleton,
        const GltfSkinnedModel* skinnedModel,
        float previewScale,
        const Vector3& previewRotation) {
        SkinningEditor::TargetPreviewInfo previewInfo{};
        previewInfo.previewScale = previewScale;
        previewInfo.previewRotation = previewRotation;
        previewInfo.defaultPreviewRotation = previewRotation;
        if (skinnedModel) {
            previewInfo.sourceBounds = ToBoundsInfo(skinnedModel->GetSourceBounds());
            previewInfo.skinnedBounds = ToBoundsInfo(skinnedModel->GetSkinnedBounds());
            const GltfSkinnedModel::TextureDebugInfo& textureInfo = skinnedModel->GetTextureDebugInfo();
            previewInfo.materialTexturePath = textureInfo.materialTexturePath;
            previewInfo.resolvedTexturePath = textureInfo.resolvedTexturePath;
            previewInfo.textureIndex = textureInfo.textureIndex;
            previewInfo.usingWhiteFallback = textureInfo.usingWhiteFallback;
            previewInfo.usingUvCheckerFallback = textureInfo.usingUvCheckerFallback;
            previewInfo.missingTextureCount = textureInfo.missingTextureCount;
        }
        if (skeleton &&
            skeleton->root >= 0 &&
            skeleton->root < static_cast<int32_t>(skeleton->joints.size())) {
            const Joint& rootJoint = skeleton->joints[static_cast<size_t>(skeleton->root)];
            previewInfo.rootNodeScale = rootJoint.sourceNodeScale;
            previewInfo.rootNodeTranslation = rootJoint.sourceNodeTranslation;
            previewInfo.skeletonRootWorldScale = ExtractMatrixScale(rootJoint.worldMatrix);
            previewInfo.skeletonRootWorldTranslation = rootJoint.worldTranslate;
        }
        return previewInfo;
    }

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

    const std::string walkGltfPath = ResolveResourcePath("resources/human/walk.gltf");
    const std::string sneakWalkGltfPath = ResolveResourcePath("resources/human/sneakWalk.gltf");
    const std::string walkGltfDisplayPath = MakeDisplayPath(walkGltfPath);
    const std::string sneakWalkGltfDisplayPath = MakeDisplayPath(sneakWalkGltfPath);

    previewSkeleton_ = MakeHumanoidPreviewSkeleton();
    previewSkeletonSecondary_ = MakeChainPreviewSkeleton();
    skinningEditor_ = std::make_unique<SkinningEditor>();
    runtimeModeController_ = std::make_unique<RuntimeModeController>();
    runtimeModeController_->Initialize(MyGame::GetInstance()->GetWinApp());
    gameViewport_ = std::make_unique<GameViewport>();
    gameViewport_->Initialize(MyGame::GetInstance()->GetWinApp());
    editorCameraController_ = std::make_unique<EditorCameraController>();
    editorCameraController_->Initialize(camera_.get());
    player_ = std::make_unique<Player>();
    player_->Initialize(object3dCommon, camera_.get());
    boostController_ = std::make_unique<BoostController>();
    boostController_->Initialize(MyGame::GetInstance()->GetVolumetricCloudPass());
    playerBulletManager_ = std::make_unique<PlayerBulletManager>();
    playerBulletManager_->Initialize(object3dCommon, camera_.get(), player_.get());
    playerBulletManager_->SetGameViewport(gameViewport_.get());
    gpuParticleEffectPlayer_ = std::make_unique<GpuParticleEffectPlayer>();
    gpuParticleEffectPlayer_->Initialize(MyGame::GetInstance()->GetDxCommon(), SrvManager::GetInstance());
    combatEffectController_ = std::make_unique<CombatEffectController>();
    combatEffectController_->Initialize(primitiveEffectSystem_.get(), gpuParticleEffectPlayer_.get(), player_.get());
    enemyManager_ = std::make_unique<EnemyManager>();
    enemyManager_->Initialize(object3dCommon, camera_.get());
    enemyManager_->SetPlayer(player_.get());
    if (screenSpaceFakeShadowPass_) {
        screenSpaceFakeShadowPass_->SetTargets(player_.get(), enemyManager_.get());
    }
    enemyBulletManager_ = std::make_unique<EnemyBulletManager>();
    enemyBulletManager_->Initialize(object3dCommon, camera_.get());
    player_->SetBarrelRollDependencies(enemyBulletManager_.get(), combatEffectController_.get());
    cameraShakeController_ = std::make_unique<CameraShakeController>();
    cameraShakeController_->Initialize();
    postEffectController_ = std::make_unique<PostEffectController>();
    postEffectController_->Initialize(MyGame::GetInstance()->GetDxCommon(), spriteCommon);
    playerDeathSequenceController_ = std::make_unique<PlayerDeathSequenceController>();
    playerDeathSequenceController_->Initialize(MyGame::GetInstance()->GetDxCommon(), spriteCommon, cameraShakeController_.get());
    levelSceneRuntime_ = std::make_unique<LevelSceneRuntime>();
    levelSceneRuntime_->Initialize(object3dCommon, camera_.get());
    enemyAttackController_ = std::make_unique<EnemyAttackController>();
    enemyAttackController_->Initialize(enemyManager_.get(), enemyBulletManager_.get(), player_.get(), playerDeathSequenceController_.get());
    playerBulletEnemyCollision_ = std::make_unique<PlayerBulletEnemyCollision>();
    playerBulletEnemyCollision_->Initialize(playerBulletManager_.get(), enemyManager_.get(), combatEffectController_.get());
    playerEnemyBulletCollision_ = std::make_unique<PlayerEnemyBulletCollision>();
    playerEnemyBulletCollision_->Initialize(player_.get(), enemyBulletManager_.get(), playerDeathSequenceController_.get(), combatEffectController_.get());
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
    startupEnemySpawnController_ = std::make_unique<StartupEnemySpawnController>();
    startupEnemySpawnController_->Initialize(enemyManager_.get(), levelSceneRuntime_.get(), camera_.get());
    postEffectActionBridge_ = std::make_unique<PostEffectActionBridge>();
    postEffectActionBridge_->Initialize(postEffectController_.get());
    eventActionDispatcher_ = std::make_unique<EventActionDispatcher>();
    eventActionDispatcher_->Initialize(
        levelSceneRuntime_->GetEventRuntime(),
        railShooterEventActionBridge_.get(),
        enemySpawnActionBridge_.get(),
        postEffectActionBridge_.get(),
        primitiveEffectSystem_.get(),
        levelSceneRuntime_.get());
    blenderLiveSync_ = std::make_unique<BlenderLiveSync>();
    blenderLiveSync_->Initialize(levelSceneRuntime_.get());

    std::string skinningLoadStatus;
    auto appendSkinningStatus = [&](const std::string& message) {
        if (!skinningLoadStatus.empty()) {
            skinningLoadStatus += "\n";
        }
        skinningLoadStatus += message;
        Logger::Log("[Skinning] " + message);
    };

    auto appendTargetStatus = [](std::string& targetStatus, const std::string& message) {
        if (!targetStatus.empty()) {
            targetStatus += "\n";
        }
        targetStatus += message;
    };

    auto registerGltfTarget = [&](
        const std::string& label,
        const std::string& gltfPath,
        const std::string& displayPath,
        std::unique_ptr<Skeleton>& skeleton,
        std::unique_ptr<GltfSkinnedModel>& skinnedModel,
        std::unique_ptr<Object3d>& skinnedObject,
        float initialPreviewScale,
        const Vector3& initialPreviewRotation) {
        std::string targetStatus;
        appendTargetStatus(targetStatus, "resolved path: " + displayPath);
        appendTargetStatus(
            targetStatus,
            std::filesystem::exists(std::filesystem::path(gltfPath))
                ? "path exists: yes"
                : "path exists: no");

        skeleton = GltfSkeletonLoader::LoadFromFile(gltfPath);
        if (skeleton) {
            appendTargetStatus(targetStatus, "skeleton: loaded");
            appendTargetStatus(targetStatus, "bones: " + std::to_string(skeleton->joints.size()));
        } else {
            appendTargetStatus(targetStatus, "skeleton: failed");
        }

        AnimationClip clip{};
        bool hasClip = false;
        if (skeleton) {
            hasClip = GltfAnimationLoader::LoadFirstClipFromFile(gltfPath, *skeleton, clip);
            appendTargetStatus(
                targetStatus,
                hasClip
                    ? ("clip: " + clip.name)
                    : "clip: failed or not found");
            appendTargetStatus(targetStatus, std::string("clip count: ") + (hasClip ? "1" : "0"));
        } else {
            appendTargetStatus(targetStatus, "clip: skipped because skeleton failed");
            appendTargetStatus(targetStatus, "clip count: 0");
        }

        bool skinnedMeshLoaded = false;
        if (skeleton) {
            skinnedModel = std::make_unique<GltfSkinnedModel>();
            if (skinnedModel->Initialize(modelManager->GetModelCommon(), skeleton.get(), gltfPath)) {
                skinnedObject = std::make_unique<Object3d>();
                skinnedObject->Initialize(object3dCommon);
                skinnedObject->SetModel(skinnedModel->GetModel());
                skinnedObject->SetCamera(camera_.get());
                skinnedObject->SetScale({ initialPreviewScale, initialPreviewScale, initialPreviewScale });
                skinnedObject->SetRotate(initialPreviewRotation);
                skinnedObject->SetEnvironmentMapEnabled(false);
                skinnedMeshLoaded = true;
                appendTargetStatus(targetStatus, "skinned mesh: loaded");
            } else {
                skinnedModel.reset();
                skinnedObject.reset();
                appendTargetStatus(targetStatus, "skinned mesh: failed");
            }
        } else {
            skinnedModel.reset();
            skinnedObject.reset();
            appendTargetStatus(targetStatus, "skinned mesh: skipped because skeleton failed");
        }

        const SkinningEditor::TargetPreviewInfo previewInfo = BuildTargetPreviewInfo(
            skeleton.get(),
            skinnedModel.get(),
            initialPreviewScale,
            initialPreviewRotation);
        if (previewInfo.sourceBounds.isValid) {
            appendTargetStatus(targetStatus, "local min: " + FormatVector3(previewInfo.sourceBounds.min));
            appendTargetStatus(targetStatus, "local max: " + FormatVector3(previewInfo.sourceBounds.max));
            appendTargetStatus(targetStatus, "local size: " + FormatVector3(previewInfo.sourceBounds.size));
            appendTargetStatus(targetStatus, "local center: " + FormatVector3(previewInfo.sourceBounds.center));
        }
        if (previewInfo.skinnedBounds.isValid) {
            appendTargetStatus(targetStatus, "initial skinned size: " + FormatVector3(previewInfo.skinnedBounds.size));
        }
        appendTargetStatus(targetStatus, "root node scale: " + FormatVector3(previewInfo.rootNodeScale));
        appendTargetStatus(targetStatus, "root node translation: " + FormatVector3(previewInfo.rootNodeTranslation));
        appendTargetStatus(targetStatus, "skeleton root world scale: " + FormatVector3(previewInfo.skeletonRootWorldScale));
        appendTargetStatus(targetStatus, "skeleton root world translation: " + FormatVector3(previewInfo.skeletonRootWorldTranslation));
        appendTargetStatus(targetStatus, "final world scale: " + std::to_string(previewInfo.previewScale));
        appendTargetStatus(targetStatus, "preview rotation degrees: " + FormatVector3(RadiansToDegrees(previewInfo.previewRotation)));

        const bool isCriticalLoadOk = skeleton && skinnedMeshLoaded;
        skinningEditor_->RegisterTarget(
            label,
            skeleton.get(),
            hasClip ? &clip : nullptr,
            isCriticalLoadOk ? "gltf" : "failed",
            displayPath,
            targetStatus,
            skeleton ? static_cast<int>(skeleton->joints.size()) : 0,
            skinnedMeshLoaded,
            previewInfo);

        appendSkinningStatus(label + "\n" + targetStatus);
    };

    registerGltfTarget(
        "walk.gltf",
        walkGltfPath,
        walkGltfDisplayPath,
        walkSkeleton_,
        walkSkinnedModel_,
        walkSkinnedObject_,
        0.01f,
        { std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f });
    registerGltfTarget(
        "sneakWalk.gltf",
        sneakWalkGltfPath,
        sneakWalkGltfDisplayPath,
        sneakWalkSkeleton_,
        sneakWalkSkinnedModel_,
        sneakWalkSkinnedObject_,
        0.01f,
        { std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f });

    if (kEnableSimpleSkinSkinningTarget) {
        const std::string simpleSkinGltfPath = ResolveResourcePath("resources/simpleSkin/simpleSkin.gltf");
        const std::string simpleSkinDisplayPath = MakeDisplayPath(simpleSkinGltfPath);
        registerGltfTarget(
            "simpleSkin.gltf",
            simpleSkinGltfPath,
            simpleSkinDisplayPath,
            simpleSkinSkeleton_,
            simpleSkinSkinnedModel_,
            simpleSkinSkinnedObject_,
            1.0f,
            { 0.0f, 0.0f, 0.0f });
    }

    if (kEnableSkinningPreviewTargets) {
        skinningEditor_->RegisterTarget("InternalSphere (preview)", previewSkeleton_.get());
        skinningEditor_->RegisterTarget("Fence (preview)", previewSkeletonSecondary_.get());
    }

    skinningEditor_->SelectTargetByLabel("walk.gltf");
    skinningEditor_->SetStatusMessage(skinningLoadStatus);
}

#ifdef USE_IMGUI
void GameScene::ClearGameViewDebugState() {
    gameViewTopLeft_ = { 0.0f, 0.0f };
    gameViewSize_ = { 0.0f, 0.0f };
    gameViewMouseLocal_ = { 0.0f, 0.0f };
    isGameViewHovered_ = false;
    isGameViewFocused_ = false;
    if (gameViewport_) {
        gameViewport_->ClearImGuiGameViewRect();
    }
    if (skinningEditor_) {
        skinningEditor_->ClearGameViewRect();
    }
    if (levelSceneRuntime_) {
        levelSceneRuntime_->ClearGameViewRect();
    }
}

void GameScene::DrawGameViewImGui(DirectXCommon* dxCommon) {
    if (!dxCommon || !dxCommon->GetFinalOutputTextureResource()) {
        ClearGameViewDebugState();
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(640.0f, 400.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Game View")) {
        const D3D12_RESOURCE_DESC desc = dxCommon->GetFinalOutputTextureResource()->GetDesc();
        const float textureWidth = static_cast<float>(desc.Width);
        const float textureHeight = static_cast<float>(desc.Height);
        if (gameViewport_) {
            gameViewport_->SetRenderTargetSize(textureWidth, textureHeight);
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
            gameViewTopLeft_ = { imageTopLeft.x, imageTopLeft.y };
            gameViewSize_ = { imageSize.x, imageSize.y };
            if (imageSize.y > 0.0f) {
                camera_->SetAspectRatio(imageSize.x / imageSize.y);
                camera_->Update();
            }

            const ImTextureID textureId = static_cast<ImTextureID>(dxCommon->GetFinalOutputTextureSRVGPUHandle().ptr);
            ImGui::Image(textureId, imageSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));

            const ImGuiIO& io = ImGui::GetIO();
            isGameViewHovered_ = ImGui::IsItemHovered();
            isGameViewFocused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
            if (gameViewport_) {
                gameViewport_->SetImGuiGameViewRect(
                    imageTopLeft.x,
                    imageTopLeft.y,
                    imageSize.x,
                    imageSize.y,
                    isGameViewHovered_,
                    isGameViewFocused_);
            }
            gameViewMouseLocal_ = {
                io.MousePos.x - imageTopLeft.x,
                io.MousePos.y - imageTopLeft.y
            };
            if (skinningEditor_) {
                skinningEditor_->SetGameViewRect(imageTopLeft.x, imageTopLeft.y, imageSize.x, imageSize.y);
                skinningEditor_->DrawGizmo(camera_.get());
                skinningEditor_->DrawDebugOverlay(camera_.get());
            }
            if (levelSceneRuntime_) {
                levelSceneRuntime_->SetGameViewRect(imageTopLeft.x, imageTopLeft.y, imageSize.x, imageSize.y);
            }
        } else {
            isGameViewFocused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
            gameViewTopLeft_ = { 0.0f, 0.0f };
            gameViewSize_ = { 0.0f, 0.0f };
            gameViewMouseLocal_ = { 0.0f, 0.0f };
            isGameViewHovered_ = false;
            if (gameViewport_) {
                gameViewport_->ClearImGuiGameViewRect();
            }
            if (skinningEditor_) {
                skinningEditor_->ClearGameViewRect();
            }
            if (levelSceneRuntime_) {
                levelSceneRuntime_->ClearGameViewRect();
            }
            ImGui::TextDisabled("RenderTexture is not ready.");
        }
    } else {
        ClearGameViewDebugState();
    }
    ImGui::End();
}
#endif

void GameScene::Finalize() {
    if (skinningEditor_) {
        skinningEditor_->ClearTargets();
    }
    if (editorCameraController_) {
        editorCameraController_->Finalize();
    }
    editorCameraController_.reset();
    runtimeModeController_.reset();
    gameViewport_.reset();
    blenderLiveSync_.reset();
    if (eventActionDispatcher_) {
        eventActionDispatcher_->Finalize();
    }
    eventActionDispatcher_.reset();
    if (startupEnemySpawnController_) {
        startupEnemySpawnController_->Finalize();
    }
    startupEnemySpawnController_.reset();
    if (enemySpawnActionBridge_) {
        enemySpawnActionBridge_->Finalize();
    }
    enemySpawnActionBridge_.reset();
    if (postEffectActionBridge_) {
        postEffectActionBridge_->Finalize();
    }
    postEffectActionBridge_.reset();
    if (railShooterEventActionBridge_) {
        railShooterEventActionBridge_->Finalize();
    }
    railShooterEventActionBridge_.reset();
    if (railShooterCameraRig_) {
        railShooterCameraRig_->Finalize();
    }
    railShooterCameraRig_.reset();
    if (playerRailController_) {
        playerRailController_->Finalize();
    }
    playerRailController_.reset();
    if (playerEventTriggerBridge_) {
        playerEventTriggerBridge_->Finalize();
    }
    playerEventTriggerBridge_.reset();
    levelSceneRuntime_.reset();
    if (gameOverFlowController_) {
        gameOverFlowController_->Finalize();
    }
    gameOverFlowController_.reset();
    if (playerEnemyBulletCollision_) {
        playerEnemyBulletCollision_->Finalize();
    }
    playerEnemyBulletCollision_.reset();
    if (playerBulletEnemyCollision_) {
        playerBulletEnemyCollision_->Finalize();
    }
    playerBulletEnemyCollision_.reset();
    if (combatEffectController_) {
        combatEffectController_->Finalize();
    }
    combatEffectController_.reset();
    if (gpuParticleEffectPlayer_) {
        gpuParticleEffectPlayer_->Finalize();
    }
    gpuParticleEffectPlayer_.reset();
    if (enemyAttackController_) {
        enemyAttackController_->Finalize();
    }
    enemyAttackController_.reset();
    if (enemyBulletManager_) {
        enemyBulletManager_->Finalize();
    }
    enemyBulletManager_.reset();
    if (playerDeathSequenceController_) {
        playerDeathSequenceController_->Finalize();
    }
    playerDeathSequenceController_.reset();
    if (postEffectController_) {
        postEffectController_->Finalize();
    }
    postEffectController_.reset();
    screenSpaceFakeShadowPass_.reset();
    if (cameraShakeController_) {
        cameraShakeController_->Reset(camera_.get());
        cameraShakeController_->Finalize();
    }
    cameraShakeController_.reset();
    if (enemyManager_) {
        enemyManager_->Finalize();
    }
    enemyManager_.reset();
    if (playerBulletManager_) {
        playerBulletManager_->Finalize();
    }
    playerBulletManager_.reset();
    if (player_) {
        player_->Finalize();
    }
    player_.reset();
    if (boostController_) {
        boostController_->Finalize();
    }
    boostController_.reset();
}

void GameScene::Update() {
    auto input = MyGame::GetInstance()->GetInput();
    auto particleManager = ParticleManager::GetInstance();
    auto volumetricCloudPass = MyGame::GetInstance()->GetVolumetricCloudPass();
    auto dxCommon = MyGame::GetInstance()->GetDxCommon();
    auto& postEffectParams = dxCommon->GetPostEffectParameters();
    auto& hitEffectParams = particleManager->GetHitEffectParams();
    auto& fireballEffectParams = particleManager->GetFireballEffectParams();
    auto& windEffectParams = particleManager->GetWindEffectParams();
    auto audio = Audio::GetInstance();
    auto texManager = TextureManager::GetInstance();
    const FrameTimer& frameTimer = FrameTimer::GetInstance();
    const float gameplayDeltaTime = frameTimer.GetGameplayDeltaTime();
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
            gameplayDeltaTime,
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
            gameplayDeltaTime,
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
    if (player_) {
        player_->Update(gameplayDeltaTime);
    }
    if (boostController_) {
        boostController_->Update(gameplayDeltaTime);
    }
    if (playerBulletManager_) {
        playerBulletManager_->Update(gameplayDeltaTime);
    }
    if (startupEnemySpawnController_) {
        startupEnemySpawnController_->Update(gameplayDeltaTime);
    }
    if (enemyManager_) {
        enemyManager_->Update(gameplayDeltaTime);
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
    if (combatEffectController_) {
        combatEffectController_->Update(gameplayDeltaTime, camera_.get());
    }
    if (playerDeathSequenceController_) {
        playerDeathSequenceController_->Update(gameplayDeltaTime);
    }
    if (postEffectController_) {
        postEffectController_->Update(gameplayDeltaTime);
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
        primitiveEffectSystem_->Update(gameplayDeltaTime);
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
    if (showDebugUi) {
        DrawGameViewImGui(dxCommon);
    } else {
        ClearGameViewDebugState();
    }
    if (showDebugUi) {
    if (skinningEditor_) {
        skinningEditor_->DrawImGui();
    }
    if (gpuParticleSystem_) {
        gpuParticleSystem_->DrawImGui();
    }
    if (editorCameraController_) {
        editorCameraController_->DrawImGui();
    }
    if (player_) {
        player_->DrawImGui();
    }
    if (boostController_) {
        boostController_->DrawImGui();
    }
    if (playerBulletManager_) {
        playerBulletManager_->DrawImGui();
    }
    if (enemyManager_) {
        enemyManager_->DrawImGui();
    }
    if (enemyBulletManager_) {
        enemyBulletManager_->DrawImGui();
    }
    if (enemyAttackController_) {
        enemyAttackController_->DrawImGui();
    }
    if (playerEnemyBulletCollision_) {
        playerEnemyBulletCollision_->DrawImGui();
    }
    if (playerBulletEnemyCollision_) {
        playerBulletEnemyCollision_->DrawImGui();
    }
    if (combatEffectController_) {
        combatEffectController_->DrawImGui();
    }
    if (playerDeathSequenceController_) {
        playerDeathSequenceController_->DrawImGui();
    }
    if (cameraShakeController_) {
        cameraShakeController_->DrawImGui();
    }
    if (gameOverFlowController_) {
        gameOverFlowController_->DrawImGui();
    }
    if (playerRailController_) {
        playerRailController_->DrawImGui();
    }
    if (playerEventTriggerBridge_) {
        playerEventTriggerBridge_->DrawImGui();
    }
    if (railShooterCameraRig_) {
        railShooterCameraRig_->DrawImGui();
    }
    if (gameViewport_) {
        gameViewport_->DrawImGui();
    }
    if (runtimeModeController_) {
        runtimeModeController_->DrawImGui();
    }
    if (screenSpaceFakeShadowPass_) {
        screenSpaceFakeShadowPass_->DrawImGui();
    }
    if (railShooterEventActionBridge_) {
        railShooterEventActionBridge_->DrawImGui();
    }
    if (enemySpawnActionBridge_) {
        enemySpawnActionBridge_->DrawImGui();
    }
    if (startupEnemySpawnController_) {
        startupEnemySpawnController_->DrawImGui();
    }
    if (postEffectActionBridge_) {
        postEffectActionBridge_->DrawImGui();
    }
    if (eventActionDispatcher_) {
        eventActionDispatcher_->DrawImGui();
    }
    if (postEffectController_) {
        postEffectController_->DrawImGui();
    }
    if (levelSceneRuntime_) {
        levelSceneRuntime_->DrawImGui();
    }
    if (blenderLiveSync_) {
        blenderLiveSync_->DrawImGui();
    }
    if (simpleSkinSkinnedModel_) {
        simpleSkinSkinnedModel_->UpdateSkinning();
    }
    if (walkSkinnedModel_) {
        walkSkinnedModel_->UpdateSkinning();
    }
    if (sneakWalkSkinnedModel_) {
        sneakWalkSkinnedModel_->UpdateSkinning();
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

        ImGui::SeparatorText("ボリューメトリック雲 (Volumetric Cloud)");
        if (volumetricCloudPass) {
            bool isCloudPassEnabled = volumetricCloudPass->IsEnabled();
            if (ImGui::Checkbox("雲描画を有効化 (Cloud Pass Enabled)", &isCloudPassEnabled)) {
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
            if (ImGui::Combo("雲の強制モード (Cloud Force Mode)", &cloudForceMode, cloudForceModeNames, IM_ARRAYSIZE(cloudForceModeNames))) {
                volumetricCloudPass->SetForceMode(static_cast<VolumetricCloudPass::ForceMode>(cloudForceMode));
                cloudProjectedBounds_ = volumetricCloudPass->BuildProjectedBounds(camera_.get(), cloudVolume_.get());
            }
        }

        if (ImGui::Button("雲プリセットを初期化 (Reset Cloud Preset)")) {
            cloudParams = MakeRecommendedCloudParameters();
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("見えやすい初期値へ戻します");
        ImGui::DragFloat3("雲の中心 (Cloud Center)", &cloudParams.center.x, 0.1f);
        ImGui::DragFloat3("雲の半径範囲 (Cloud HalfExtents)", &cloudParams.halfExtents.x, 0.1f, 0.1f, 100.0f, "%.2f");
        ImGui::SliderFloat("雲の濃さ (Cloud Density)", &cloudParams.density, 0.0f, 2.0f, "%.3f");
        ImGui::SliderFloat("雲の吸収量 (Cloud Absorption)", &cloudParams.absorption, 0.01f, 8.0f, "%.2f");
        ImGui::DragFloat3("雲の流れる方向 (Cloud Wind Dir)", &cloudParams.windDirection.x, 0.01f, -1.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("雲の流れる速さ (Cloud Wind Speed)", &cloudParams.windSpeed, 0.0f, 5.0f, "%.2f");
        ImGui::DragFloat3("太陽方向 (Cloud Sun Dir)", &cloudParams.sunDirection.x, 0.01f, -1.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("光の吸収量 (Cloud Light Absorption)", &cloudParams.lightAbsorption, 0.0f, 8.0f, "%.2f");
        ImGui::ColorEdit4("雲の色 (A = density scale)", &cloudParams.color.x);
        ImGui::SliderFloat("雲ノイズ倍率 (Cloud Noise Scale)", &cloudParams.noiseScale, 0.01f, 2.0f, "%.3f");
        ImGui::SliderFloat("細部ノイズ倍率 (Cloud Detail Noise)", &cloudParams.detailNoiseScale, 0.01f, 4.0f, "%.3f");
        ImGui::SliderFloat("細部ノイズの強さ (Cloud Detail Weight)", &cloudParams.detailWeight, 0.0f, 1.5f, "%.2f");
        ImGui::SliderFloat("雲の端のぼかし (Cloud Edge Fade)", &cloudParams.edgeFade, 0.01f, 1.0f, "%.3f");
        ImGui::SliderFloat("環境光の強さ (Cloud Ambient)", &cloudParams.ambientLighting, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("太陽光の強さ (Cloud Sun Intensity)", &cloudParams.sunIntensity, 0.0f, 4.0f, "%.2f");

        int cloudViewStepCount = static_cast<int>(cloudParams.viewStepCount);
        if (ImGui::SliderInt("視線方向ステップ数 (Cloud View Steps)", &cloudViewStepCount, 1, 256)) {
            cloudParams.viewStepCount = static_cast<uint32_t>(cloudViewStepCount);
        }

        int cloudLightStepCount = static_cast<int>(cloudParams.lightStepCount);
        if (ImGui::SliderInt("光方向ステップ数 (Cloud Light Steps)", &cloudLightStepCount, 1, 32)) {
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
            if (ImGui::Combo("雲のデバッグ表示 (Cloud Debug View)", &cloudDebugView, cloudDebugViewNames, IM_ARRAYSIZE(cloudDebugViewNames))) {
                volumetricCloudPass->SetDebugViewMode(
                    static_cast<VolumetricCloudPass::DebugViewMode>(cloudDebugView));
            }

            volumetricCloudPass->DrawImGui();
        }

        ImGui::SeparatorText("雲の最適化診断 (Cloud Optimization Debug)");
        auto DrawCloudFlag = [](const char* label, bool value, const ImVec4& trueColor, const ImVec4& falseColor) {
            ImGui::TextUnformatted(label);
            ImGui::SameLine(220.0f);
            ImGui::TextColored(value ? trueColor : falseColor, value ? "true" : "false");
        };

        DrawCloudFlag("雲が表示範囲内 (Cloud Visible)", cloudProjectedBounds_.isVisible, ImVec4(0.30f, 1.00f, 0.35f, 1.0f), ImVec4(1.00f, 0.35f, 0.35f, 1.0f));
        DrawCloudFlag("雲描画をスキップ (Cloud Pass Skipped)", cloudProjectedBounds_.isPassSkipped, ImVec4(1.00f, 0.35f, 0.35f, 1.0f), ImVec4(0.30f, 1.00f, 0.35f, 1.0f));
        DrawCloudFlag("全画面へフォールバック (Fullscreen Fallback)", cloudProjectedBounds_.isFullScreenFallback, ImVec4(1.00f, 0.80f, 0.25f, 1.0f), ImVec4(0.45f, 0.85f, 1.00f, 1.0f));
        DrawCloudFlag("全画面シザー使用 (Use Fullscreen Scissor)", cloudProjectedBounds_.useFullScreenScissor, ImVec4(1.00f, 0.80f, 0.25f, 1.0f), ImVec4(0.30f, 1.00f, 0.35f, 1.0f));
        DrawCloudFlag("カメラが雲の中 (Camera Inside Cloud)", cloudProjectedBounds_.isCameraInsideCloud, ImVec4(1.00f, 0.80f, 0.25f, 1.0f), ImVec4(0.45f, 0.85f, 1.00f, 1.0f));
        DrawCloudFlag("Near Planeと交差 (Near Plane Crossing)", cloudProjectedBounds_.isNearPlaneCrossing, ImVec4(1.00f, 0.80f, 0.25f, 1.0f), ImVec4(0.45f, 0.85f, 1.00f, 1.0f));

        const LONG scissorWidth = cloudProjectedBounds_.scissorRect.right - cloudProjectedBounds_.scissorRect.left;
        const LONG scissorHeight = cloudProjectedBounds_.scissorRect.bottom - cloudProjectedBounds_.scissorRect.top;
        ImGui::Text("シザー矩形 (Scissor Rect): L=%ld T=%ld R=%ld B=%ld", cloudProjectedBounds_.scissorRect.left, cloudProjectedBounds_.scissorRect.top, cloudProjectedBounds_.scissorRect.right, cloudProjectedBounds_.scissorRect.bottom);
        ImGui::Text("シザーサイズ (Scissor Size): %ld x %ld", scissorWidth, scissorHeight);
        ImGui::TextColored(
            (cloudProjectedBounds_.scissorAreaRatio >= 0.90f) ? ImVec4(1.00f, 0.45f, 0.35f, 1.0f) : ImVec4(0.35f, 1.00f, 0.45f, 1.0f),
            "シザー面積比 (Scissor Area Ratio): %.3f (%.1f%%)",
            cloudProjectedBounds_.scissorAreaRatio,
            cloudProjectedBounds_.scissorAreaRatio * 100.0f);
        ImGui::Text("現在の視線ステップ倍率 (Current ViewStep Scale): %.3f", cloudProjectedBounds_.currentViewStepScale);
        ImGui::Text("現在の光ステップ倍率 (Current LightStep Scale): %.3f", cloudProjectedBounds_.currentLightStepScale);
        ImGui::Text("推定視線ステップ数 (Estimated View Steps): %u", cloudProjectedBounds_.estimatedViewSteps);
        ImGui::Text("推定光ステップ数 (Estimated Light Steps): %u", cloudProjectedBounds_.estimatedLightSteps);
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

    if (primitiveEffectSystem_) {
        primitiveEffectSystem_->DrawImGui();
    }

    ImGui::SetNextWindowSize(ImVec2(320, 520), ImGuiCond_Once);
    ImGui::Begin("Scene Visibility");
    auto setAllVisibility = [this](bool isVisible) {
        isSkyboxVisible_ = isVisible;
        isFenceVisible_ = isVisible;
        isSphereVisible_ = isVisible;
        isAnimatedCubeVisible_ = isVisible;
        isSkinnedModelVisible_ = isVisible;
        isPrimitivePreviewVisible_ = isVisible;
        if (primitiveEffectSystem_) {
            primitiveEffectSystem_->SetVisible(isVisible);
        }
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
    const Skeleton* visibleSkinningTarget = skinningEditor_ ? skinningEditor_->GetTargetSkeleton() : nullptr;
    GltfSkinnedModel* activeSkinnedModel = nullptr;
    const char* activeSkinnedModelName = "None";
    if (visibleSkinningTarget == simpleSkinSkeleton_.get()) {
        activeSkinnedModel = simpleSkinSkinnedModel_.get();
        activeSkinnedModelName = "simpleSkin";
    } else if (visibleSkinningTarget == walkSkeleton_.get()) {
        activeSkinnedModel = walkSkinnedModel_.get();
        activeSkinnedModelName = "walk.gltf";
    } else if (visibleSkinningTarget == sneakWalkSkeleton_.get()) {
        activeSkinnedModel = sneakWalkSkinnedModel_.get();
        activeSkinnedModelName = "sneakWalk.gltf";
    }
    ImGui::Checkbox("Primitive Preview", &isPrimitivePreviewVisible_);

    ImGui::SeparatorText("Particles / Effects");
    ImGui::Checkbox("ParticleManager", &isParticleVisible_);
    if (primitiveEffectSystem_) {
        primitiveEffectSystem_->DrawVisibilityImGui();
    }
    ImGui::Checkbox("ボリューメトリック雲 (Volumetric Cloud)", &isVolumetricCloudVisible_);

    ImGui::SeparatorText("Debug");
    ImGui::Checkbox("Debug Sprite", &isDebugSpriteVisible_);
    ImGui::End();

    ImGui::SetNextWindowSize(ImVec2(360, 260), ImGuiCond_Once);
    ImGui::Begin("Skinning Debug");
    ImGui::Text("Active Target: %s", activeSkinnedModelName);
    ImGui::Text("CPU Skinning Path: Enabled");
    if (activeSkinnedModel) {
        bool useComputeOutputVertices = activeSkinnedModel->IsUsingComputeOutputVertices();
        if (ImGui::Checkbox("GPU Skinning Output VBV", &useComputeOutputVertices)) {
            activeSkinnedModel->SetUseComputeOutputVertices(useComputeOutputVertices);
        }
        ImGui::SameLine();
        ImGui::TextDisabled(useComputeOutputVertices ? "ON" : "OFF");

        ImGui::SeparatorText("Resource Counts");
        ImGui::Text("Vertex Count    : %u", activeSkinnedModel->GetVertexCount());
        ImGui::Text("Influence Count : %u", activeSkinnedModel->GetInfluenceCount());
        ImGui::Text("Palette Count   : %u", activeSkinnedModel->GetPaletteCount());
        ImGui::Text("Dispatch Groups : %u", activeSkinnedModel->GetDispatchThreadGroupCount());
        ImGui::Text("Threads / Group : 1024");

        ImGui::SeparatorText("Resource State");
        ImGui::Text("Compute Resources: %s", activeSkinnedModel->HasComputeSkinningResources() ? "Ready" : "Missing");
        ImGui::TextDisabled("CPU/GPU max vertex delta: N/A (readback not implemented)");
        ImGui::TextWrapped("ON compares the compute output path visually. OFF keeps the CPU-updated vertex buffer path.");
    } else {
        ImGui::TextDisabled("Select a skinned target in Skinning Editor.");
    }
    ImGui::End();
    }
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
    if (screenSpaceFakeShadowPass_ && !shadowDebugSettings.disableFakeShadow) {
        screenSpaceFakeShadowPass_->Draw(camera_.get());
    }
    if (primitiveEffectSystem_ && !shadowDebugSettings.disableEffects && !shadowDebugSettings.disablePrimitiveEffect) {
        primitiveEffectSystem_->Draw();
    }
    if (combatEffectController_ && !shadowDebugSettings.disableEffects && !shadowDebugSettings.disableGpuParticle) {
        combatEffectController_->Draw();
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

    if (shouldDrawDebugVisuals && !shadowDebugSettings.disableEffects && !shadowDebugSettings.disableGpuParticle && gpuParticleSystem_) {
        gpuParticleSystem_->Draw();
    }

    if (isParticleVisible_ && !shadowDebugSettings.disableEffects) {
        particleManager->Draw();
    }

    spriteCommon->CommonDrawSetting();
    if (shouldDrawDebugVisuals && isDebugSpriteVisible_) {
        debugSprite_->Draw();
    }
    if (playerDeathSequenceController_) {
        playerDeathSequenceController_->Draw();
    }
    if (postEffectController_) {
        postEffectController_->Draw();
    }
}


