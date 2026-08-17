#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"

#include "Engine/Animation/Skeleton.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Model/GltfSkeletonLoader.h"
#include "Engine/Graphics/Model/GltfSkinnedModel.h"
#include "Engine/Graphics/Object3d/Object3d.h"

#include <array>
#include <filesystem>
#include <utility>

namespace {
    constexpr const char* kOriginalAssetPath =
        "resources/midboss/kraken_midboss_tentacles.gltf";

    std::string ResolveResourcePath(const std::string& path) {
        const std::array<std::filesystem::path, 6> basePaths = {
            std::filesystem::path{},
            std::filesystem::path{ "project" },
            std::filesystem::path{ ".." } / "project",
            std::filesystem::path{ ".." } / ".." / "project",
            std::filesystem::path{ ".." } / ".." / ".." / "project",
            std::filesystem::path{ ".." } / ".." / ".." / ".." /
                "project",
        };
        for (const std::filesystem::path& basePath : basePaths) {
            const std::filesystem::path candidate = basePath.empty()
                ? std::filesystem::path(path)
                : basePath / path;
            std::error_code error;
            if (std::filesystem::exists(candidate, error) && !error) {
                return candidate.lexically_normal().generic_string();
            }
        }
        return {};
    }
}

KrakenTentacleMidbossController::Impl::Impl() {
    requestedAssetPath = kOriginalAssetPath;
    worldMatrix = MatrixMath::MakeAffine(
        worldScale, worldRotation, worldPosition);
}

KrakenTentacleMidbossController::Impl::~Impl() {
    Finalize();
}

bool KrakenTentacleMidbossController::Impl::FailInitialization(
    const std::string& message) {
    ++diagnostics.modelLoadFailureCount;
    lastError = message;
    state = KrakenTentacleMidbossState::Hidden;
    initialized = false;
    modelLoaded = false;
    skeletonValid = false;
    pendingCommand = KrakenTentacleMidbossPendingCommand::None;
    object.reset();
    model.reset();
    skeleton.reset();
    bindPose.clear();
    bindLocalEulerRadians.clear();
    chains.clear();
    colliderDefinitions.clear();
    boneSnapshots.clear();
    capsuleSnapshots.clear();
    tipSnapshots.clear();
    return false;
}

bool KrakenTentacleMidbossController::Impl::Initialize(
    ModelCommon* modelCommon,
    Object3dCommon* object3dCommonValue) {
    Finalize();
    requestedAssetPath = kOriginalAssetPath;
    object3dCommon = object3dCommonValue;
    if (!modelCommon || !object3dCommon) {
        return FailInitialization(
            "Runtime初期化に必要な描画共通データがありません。");
    }

    resolvedAssetPath = ResolveResourcePath(requestedAssetPath);
    if (resolvedAssetPath.empty()) {
        return FailInitialization(
            "元midbossのglTFアセットが見つかりません。");
    }

    skeleton = GltfSkeletonLoader::LoadFromFile(resolvedAssetPath);
    if (!skeleton) {
        return FailInitialization(
            "元midbossのSkeleton読込に失敗しました。");
    }
    UpdateSkeletonWorldTransforms(*skeleton);
    if (!CaptureBindPoseAndChains()) {
        const std::string error = lastError.empty()
            ? "Bind Poseまたは触手Chainを検証できませんでした。"
            : lastError;
        return FailInitialization(error);
    }

    model = std::make_unique<GltfSkinnedModel>();
    if (!model->Initialize(modelCommon, skeleton.get(), resolvedAssetPath)) {
        return FailInitialization(
            "元midbossのSkinned Model読込に失敗しました。");
    }
    model->SetUseComputeOutputVertices(true);
    if (!ValidateLoadedAsset()) {
        const std::string error = lastError.empty()
            ? "元midbossのモデル構成が期待値と一致しません。"
            : lastError;
        return FailInitialization(error);
    }

    object = std::make_unique<Object3d>();
    object->Initialize(object3dCommon);
    if (!object->IsValid()) {
        return FailInitialization(
            "Runtime描画Objectの初期化に失敗しました。");
    }
    object->SetModel(model->GetModel());
    object->SetEnvironmentMapEnabled(false);
    object->SetCamera(camera);

    initialized = true;
    modelLoaded = true;
    skeletonValid = true;
    state = KrakenTentacleMidbossState::Hidden;
    safetyStopped = false;
    lastError.clear();
    lastWarning.clear();
    UpdateObjectTransform();
    RefreshSkinningDiagnostics();
    RefreshColliderSnapshots();
    RefreshBoneSnapshots();
    return true;
}

void KrakenTentacleMidbossController::Impl::SetCamera(Camera* value) {
    camera = value;
    if (object) {
        object->SetCamera(camera);
    }
    if (!camera && IsVisible()) {
        EnterHidden(
            "Gameplay Cameraが無効になったため表示を停止しました。",
            true);
    }
}

void KrakenTentacleMidbossController::Impl::Reset() {
    ResetCollisionQueryState(true);
    state = KrakenTentacleMidbossState::Hidden;
    selectedAttackChainIndex = 0;
    stateElapsedTime = 0.0f;
    totalActiveTime = 0.0f;
    idleTime = 0.0f;
    attackElapsedTime = 0.0f;
    lastScaledDeltaTime = 0.0f;
    worldPosition = {};
    worldRotation = {};
    worldScale = { 0.5f, 0.5f, 0.5f };
    cameraForwardOffset = 35.0f;
    cameraRightOffset = 0.0f;
    cameraUpOffset = -2.0f;
    colliderRadiusScale = 1.0f;
    colliderGlobalRadiusScale = 1.0f;
    idleSettings = {};
    attackSettings = {};
    colliderPhaseSettings = {};
    idleSwayEnabled = true;
    showBones = false;
    showJoints = false;
    showColliders = false;
    showAttackColliders = true;
    showDamageColliders = true;
    showWeakPoints = true;
    showSelectedChainOnly = false;
    collisionQueryEnabled = true;
    safetyStopped = false;
    lastError.clear();
    lastWarning.clear();
    pendingCommand = KrakenTentacleMidbossPendingCommand::None;

    const std::size_t loadFailures = diagnostics.modelLoadFailureCount;
    diagnostics = {};
    diagnostics.modelLoadFailureCount = loadFailures;
    RestoreBindPose();
    if (skeleton) {
        UpdateSkeletonWorldTransforms(*skeleton);
    }
    UpdateObjectTransform();
    RefreshSkinningDiagnostics();
    RefreshColliderSnapshots();
    RefreshBoneSnapshots();
}

void KrakenTentacleMidbossController::Impl::Finalize() {
    ResetCollisionQueryState(true);
    state = KrakenTentacleMidbossState::Hidden;
    if (skeleton && bindPose.size() == skeleton->joints.size()) {
        RestoreBindPose();
        UpdateSkeletonWorldTransforms(*skeleton);
    }
    object.reset();
    model.reset();
    skeleton.reset();
    bindPose.clear();
    bindLocalEulerRadians.clear();
    chains.clear();
    colliderDefinitions.clear();
    boneSnapshots.clear();
    capsuleSnapshots.clear();
    tipSnapshots.clear();
    object3dCommon = nullptr;
    camera = nullptr;
    collisionPlayer = nullptr;
    collisionPlayerBulletManager = nullptr;
    diagnostics = {};
    requestedAssetPath = kOriginalAssetPath;
    resolvedAssetPath.clear();
    rootName.clear();
    lastError.clear();
    lastWarning.clear();
    lastCollisionQueryWarning.clear();
    initialized = false;
    modelLoaded = false;
    skeletonValid = false;
    safetyStopped = false;
    collisionQueryEnabled = true;
    pendingCommand = KrakenTentacleMidbossPendingCommand::None;
    stateElapsedTime = 0.0f;
    totalActiveTime = 0.0f;
    idleTime = 0.0f;
    attackElapsedTime = 0.0f;
    lastScaledDeltaTime = 0.0f;
    collisionRegistrationGeneration = 1;
}

KrakenTentacleMidbossController::KrakenTentacleMidbossController()
    : impl_(std::make_unique<Impl>()) {}

KrakenTentacleMidbossController::~KrakenTentacleMidbossController() = default;

bool KrakenTentacleMidbossController::Initialize(
    ModelCommon* modelCommon,
    Object3dCommon* object3dCommon) {
    return impl_ && impl_->Initialize(modelCommon, object3dCommon);
}

void KrakenTentacleMidbossController::SetCamera(Camera* camera) {
    if (impl_) {
        impl_->SetCamera(camera);
    }
}

void KrakenTentacleMidbossController::SetCollisionQueryContext(
    const Player* player,
    const PlayerBulletManager* playerBulletManager) {
    if (impl_) {
        impl_->SetCollisionQueryContext(player, playerBulletManager);
    }
}

void KrakenTentacleMidbossController::Update(float scaledDeltaTime) {
    if (impl_) {
        impl_->Update(scaledDeltaTime);
        impl_->UpdateCollisionQuery();
    }
}

void KrakenTentacleMidbossController::Draw() {
    if (impl_) {
        impl_->Draw();
    }
}

void KrakenTentacleMidbossController::DrawDebug(
    float viewX,
    float viewY,
    float viewWidth,
    float viewHeight) const {
    if (impl_) {
        impl_->DrawDebug(viewX, viewY, viewWidth, viewHeight);
    }
}

void KrakenTentacleMidbossController::DrawImGui() {
    if (impl_) {
        impl_->DrawImGui();
    }
}

void KrakenTentacleMidbossController::Reset() {
    if (impl_) {
        impl_->Reset();
    }
}

void KrakenTentacleMidbossController::Finalize() {
    if (impl_) {
        impl_->Finalize();
    }
}

bool KrakenTentacleMidbossController::IsInitialized() const {
    return impl_ && impl_->initialized;
}

bool KrakenTentacleMidbossController::IsVisible() const {
    return impl_ && impl_->IsVisible();
}

KrakenTentacleMidbossState
KrakenTentacleMidbossController::GetState() const {
    return impl_ ? impl_->state : KrakenTentacleMidbossState::Hidden;
}
