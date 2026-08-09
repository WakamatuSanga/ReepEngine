#include "GameScene.h"
#include "MyGame.h"
#include "Engine/Animation/Skeleton.h"
#include "Engine/Editor/SkinningEditor.h"
#include "Engine/Editor/KrakenPreviewAssetMode.h"
#include "Engine/Editor/SkinningEditorSkeletonComparison.h"
#include "Engine/Graphics/Model/GltfSkinnedModel.h"
#include "Engine/Graphics/Model/GltfSkinnedModelMaterialData.h"
#include "Engine/Graphics/Model/GltfSkinnedModelPrimitiveData.h"
#include "Engine/Graphics/Model/GltfSkeletonLoader.h"
#include "Engine/Graphics/Model/GltfNodeMatrixDiagnostics.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Utility/Logger.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <string>

namespace {
    constexpr const char* kMidbossTargetLabel =
        "\U000030AF\U000030E9\U000030FC\U000030B1\U000030F3\U00004E2D\U000030DC\U000030B9\U000089E6\U0000624B";
    constexpr const char* kOriginalMidbossGltfPath =
        "resources/midboss/kraken_midboss_tentacles.gltf";
    constexpr const char* kCompatibleMidbossGltfPath =
        "resources/midboss/editor_compat/kraken_midboss_tentacles_editor_compat.gltf";
    constexpr const char* kExpectedRootName = "Kraken_Tentacle_Rig_Root";
    constexpr std::size_t kExpectedJointCount = 41;
    constexpr float kInitialPreviewScale = 0.5f;

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

    std::string ResolveResourcePath(const std::string& path) {
        if (std::string resolvedPath = FindResourcePathCandidate(path); !resolvedPath.empty()) {
            Logger::Log("[GameScene] Resolved resource: " + path + " -> " + resolvedPath);
            return resolvedPath;
        }

        Logger::Log(
            "[GameScene] Resource missing: " + path +
            " cwd=" + std::filesystem::current_path().generic_string());
        return path;
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
            const Joint& rootJoint = skeleton->joints[static_cast<std::size_t>(skeleton->root)];
            previewInfo.rootNodeScale = rootJoint.sourceNodeScale;
            previewInfo.rootNodeTranslation = rootJoint.sourceNodeTranslation;
            previewInfo.skeletonRootWorldScale = ExtractMatrixScale(rootJoint.worldMatrix);
            previewInfo.skeletonRootWorldTranslation = rootJoint.worldTranslate;
        }
        return previewInfo;
    }

    void AppendStatus(std::string& status, const std::string& message) {
        if (!status.empty()) {
            status += "\n";
        }
        status += message;
    }
}

void GameScene::InitializeSkinningEditorPreview() {
    const Vector3 initialPreviewRotation{ 0.0f, 0.0f, 0.0f };

    if (!skinningEditor_) {
        skinningEditor_ = std::make_unique<SkinningEditor>();
    } else {
        skinningEditor_->ClearTargets();
    }
    skinningPreviewObject_.reset();
    skinningPreviewModel_.reset();
    skinningPreviewSkeleton_.reset();

    auto* modelManager = ModelManager::GetInstance();
    auto* object3dCommon = MyGame::GetInstance()->GetObject3dCommon();
    const bool usesOriginalAsset =
        skinningPreviewAssetMode_ ==
        KrakenPreviewAssetMode::OriginalMatrix;
    const char* requestedAssetPath = usesOriginalAsset
        ? kOriginalMidbossGltfPath
        : kCompatibleMidbossGltfPath;
    const std::string gltfPath = ResolveResourcePath(requestedAssetPath);
    const std::string displayPath = MakeDisplayPath(gltfPath);
    std::string targetStatus;

    AppendStatus(
        targetStatus,
        std::string("Preview\u30A2\u30BB\u30C3\u30C8: ") +
            (usesOriginalAsset
                ? "\u5143\u30A2\u30BB\u30C3\u30C8\uFF08matrix\u6B63\u5F0F\u5BFE\u5FDC\uFF09"
                : "\u4E92\u63DBPreview\uFF08TRS\u6BD4\u8F03\uFF09"));
    AppendStatus(
        targetStatus,
        usesOriginalAsset
            ? "\u5143\u30a2\u30bb\u30c3\u30c8\u306e3 Primitive\u3092\u5171\u6709Skinning\u7d50\u679c\u3067\u5168\u63cf\u753b\u3057\u307e\u3059\u3002"
            : "\u4e92\u63dbPreview\u306e1 Primitive\u3092\u5171\u6709Skinning\u7d4c\u8def\u3067\u63cf\u753b\u3057\u307e\u3059\u3002");
    AppendStatus(targetStatus, "Skeleton\u3068Skin Weight\u306e\u78ba\u8a8d\u7528\u3067\u3059\u3002");
    AppendStatus(
        targetStatus,
        "スキン付き複数プリミティブ・複数マテリアルへ対応済みです。各プリミティブへ元アセットのマテリアルを割り当てます。");
    AppendStatus(
        targetStatus,
        "複数メッシュは未対応です。");
    AppendStatus(targetStatus, "\U00008AAD\U00008FBC\U000030A2\U000030BB\U000030C3\U000030C8: " + displayPath);
    AppendStatus(
        targetStatus,
        std::filesystem::exists(std::filesystem::path(gltfPath))
            ? "\U000030D1\U000030B9\U000078BA\U00008A8D: \U00005B58\U00005728\U00003057\U0000307E\U00003059"
            : "\U000030D1\U000030B9\U000078BA\U00008A8D: \U00005B58\U00005728\U00003057\U0000307E\U0000305B\U00003093");

    GltfNodeMatrixDiagnostics nodeDiagnostics{};
    skinningPreviewSkeleton_ =
        GltfSkeletonLoader::LoadFromFile(gltfPath, &nodeDiagnostics);
    int loadedJointCount = 0;
    std::string loadedRootName;
    bool skeletonCompatible = false;
    if (skinningPreviewSkeleton_) {
        loadedJointCount = static_cast<int>(skinningPreviewSkeleton_->joints.size());
        const int32_t rootIndex = skinningPreviewSkeleton_->root;
        if (rootIndex >= 0 && rootIndex < loadedJointCount) {
            loadedRootName = skinningPreviewSkeleton_->joints[static_cast<std::size_t>(rootIndex)].name;
        }
        skeletonCompatible =
            loadedJointCount == static_cast<int>(kExpectedJointCount) &&
            rootIndex == 0 &&
            loadedRootName == kExpectedRootName;
        AppendStatus(targetStatus, "Skeleton: \U00008AAD\U00008FBC\U00006210\U0000529F");
        AppendStatus(targetStatus, "Joint\U00006570: " + std::to_string(loadedJointCount));
        AppendStatus(
            targetStatus,
            "Root Bone: " +
            (loadedRootName.empty()
                ? std::string{ "\U000053D6\U00005F97\U00004E0D\U000053EF" }
                : loadedRootName));
        AppendStatus(
            targetStatus,
            skeletonCompatible
                ? "Skeleton\U00004E92\U000063DB\U00006027: \U00006B63\U00005E38"
                : "Skeleton\U00004E92\U000063DB\U00006027: \U00005931\U00006557\U0000FF08\U0000671F\U00005F85\U00005024\U0000306FRoot index 0 / Joint 41\U0000FF09");
    } else {
        AppendStatus(targetStatus, "Skeleton: \U00008AAD\U00008FBC\U00005931\U00006557");
        AppendStatus(targetStatus, "Joint\U00006570: 0");
        AppendStatus(targetStatus, "Root Bone: \U000053D6\U00005F97\U00004E0D\U000053EF");
        if (!nodeDiagnostics.errorMessage.empty()) {
            AppendStatus(targetStatus, "Node\u5909\u63DB\u30A8\u30E9\u30FC: " +
                nodeDiagnostics.errorMessage);
        }
    }

    if (!skeletonCompatible) {
        skinningPreviewSkeleton_.reset();
    }

    AppendStatus(targetStatus, "Animation: \U0000306A\U00003057\U0000FF08Bind Pose\U0000FF09");
    AppendStatus(targetStatus, "Animation\U00006570: 0");

    GltfSkinnedPrimitiveDiagnostics primitiveDiagnostics{};
    primitiveDiagnostics.sourcePath = gltfPath;
    const GltfSkinnedModel* primitiveDiagnosticsModel = nullptr;
    GltfSkinnedMaterialDiagnostics materialDiagnostics{};
    materialDiagnostics.sourcePath = gltfPath;
    const GltfSkinnedModel* materialDiagnosticsModel = nullptr;
    bool skinnedMeshLoaded = false;
    if (skinningPreviewSkeleton_) {
        skinningPreviewModel_ = std::make_unique<GltfSkinnedModel>();
        if (skinningPreviewModel_->Initialize(
            modelManager->GetModelCommon(),
            skinningPreviewSkeleton_.get(),
            gltfPath)) {
            primitiveDiagnostics =
                skinningPreviewModel_->GetPrimitiveDiagnostics();
            primitiveDiagnosticsModel = skinningPreviewModel_.get();
            materialDiagnostics =
                skinningPreviewModel_->GetMaterialDiagnostics();
            materialDiagnosticsModel = skinningPreviewModel_.get();
            skinningPreviewObject_ = std::make_unique<Object3d>();
            skinningPreviewObject_->Initialize(object3dCommon);
            skinningPreviewObject_->SetModel(
                skinningPreviewModel_->GetModel());
            skinningPreviewObject_->SetCamera(camera_.get());
            skinningPreviewObject_->SetScale({
                kInitialPreviewScale,
                kInitialPreviewScale,
                kInitialPreviewScale });
            skinningPreviewObject_->SetRotate(initialPreviewRotation);
            skinningPreviewObject_->SetEnvironmentMapEnabled(false);
            skinnedMeshLoaded = true;
            AppendStatus(targetStatus, "Skin Mesh: \u8aad\u8fbc\u6210\u529f");
            AppendStatus(
                targetStatus,
                "Source Primitive\u6570: " +
                    std::to_string(
                        primitiveDiagnostics.sourcePrimitiveCount));
            AppendStatus(
                targetStatus,
                "Total Index\u6570: " +
                    std::to_string(
                        primitiveDiagnostics.totalIndexCount));
            AppendStatus(
                targetStatus,
                "Draw Call\u6570: " +
                    std::to_string(
                        primitiveDiagnostics.drawCallCount));
            AppendStatus(
                targetStatus,
                "元マテリアル数: " +
                    std::to_string(
                        materialDiagnostics.sourceMaterialCount));
            AppendStatus(
                targetStatus,
                "マテリアルバインド数: " +
                    std::to_string(
                        materialDiagnostics.materialBindingCount));
            AppendStatus(
                targetStatus,
                "代替テクスチャ数: " +
                    std::to_string(materialDiagnostics.fallbackTextureCount));
        } else {
            primitiveDiagnostics =
                skinningPreviewModel_->GetPrimitiveDiagnostics();
            materialDiagnostics =
                skinningPreviewModel_->GetMaterialDiagnostics();
            if (!primitiveDiagnostics.errorMessage.empty()) {
                AppendStatus(
                    targetStatus,
                    "Primitive\u8aad\u8fbc\u30a8\u30e9\u30fc: " +
                        primitiveDiagnostics.errorMessage);
            }
            if (!materialDiagnostics.errorMessage.empty()) {
                AppendStatus(
                    targetStatus,
                    "マテリアル読込エラー: " +
                        materialDiagnostics.errorMessage);
            }
            skinningPreviewModel_.reset();
            skinningPreviewObject_.reset();
            skinningPreviewSkeleton_.reset();
            AppendStatus(targetStatus, "Skin Mesh: \u8aad\u8fbc\u5931\u6557");
        }
    } else {
        materialDiagnostics.errorMessage =
            "スケルトン読込失敗のためマテリアル診断を実行できません。";
        primitiveDiagnostics.errorMessage =
            "Skeleton\u8aad\u8fbc\u5931\u6557\u306e\u305f\u3081Primitive\u8a3a\u65ad\u3092\u5b9f\u884c\u3067\u304d\u307e\u305b\u3093\u3002";
        AppendStatus(
            targetStatus,
            "Skin Mesh: Skeleton\u4e92\u63db\u6027\u5931\u6557\u306e\u305f\u3081\u672a\u8aad\u8fbc");
    }

    const SkinningEditor::TargetPreviewInfo previewInfo = BuildTargetPreviewInfo(
        skinningPreviewSkeleton_.get(),
        skinningPreviewModel_.get(),
        kInitialPreviewScale,
        initialPreviewRotation);
    if (previewInfo.sourceBounds.isValid) {
        AppendStatus(targetStatus, "\U000030ED\U000030FC\U000030AB\U000030EB\U00006700\U00005C0F\U00005024: " + FormatVector3(previewInfo.sourceBounds.min));
        AppendStatus(targetStatus, "\U000030ED\U000030FC\U000030AB\U000030EB\U00006700\U00005927\U00005024: " + FormatVector3(previewInfo.sourceBounds.max));
        AppendStatus(targetStatus, "\U000030ED\U000030FC\U000030AB\U000030EB\U000030B5\U000030A4\U000030BA: " + FormatVector3(previewInfo.sourceBounds.size));
        AppendStatus(targetStatus, "\U000030ED\U000030FC\U000030AB\U000030EB\U00004E2D\U00005FC3: " + FormatVector3(previewInfo.sourceBounds.center));
    }
    if (previewInfo.skinnedBounds.isValid) {
        AppendStatus(targetStatus, "\U0000521D\U0000671FSkin\U00005F8C\U000030B5\U000030A4\U000030BA: " + FormatVector3(previewInfo.skinnedBounds.size));
    }
    if (skinnedMeshLoaded) {
        AppendStatus(targetStatus, "Material Texture: " + previewInfo.materialTexturePath);
        AppendStatus(targetStatus, "\U000089E3\U00006C7A\U00006E08\U0000307FTexture: " + previewInfo.resolvedTexturePath);
        AppendStatus(
            targetStatus,
            previewInfo.usingWhiteFallback || previewInfo.usingUvCheckerFallback || previewInfo.missingTextureCount > 0
                ? "Texture: \U000030D5\U000030A9\U000030FC\U000030EB\U000030D0\U000030C3\U000030AF\U00003092\U0000691C\U000051FA"
                : "Texture: kraken_albedo.png \U00008AAD\U00008FBC\U00006210\U0000529F");
    }
    AppendStatus(targetStatus, "Root Node Scale: " + FormatVector3(previewInfo.rootNodeScale));
    AppendStatus(targetStatus, "Root Node Position: " + FormatVector3(previewInfo.rootNodeTranslation));
    AppendStatus(targetStatus, "Preview Scale: " + std::to_string(previewInfo.previewScale));
    AppendStatus(targetStatus, "Bone\U00008868\U0000793A: \U000089AA\U00005B50\U00007DDAON / Joint ON / Bone\U0000540DOFF\U0000FF08\U000065E2\U00005B582D Overlay\U0000FF09");

    const bool loadSucceeded = skinningPreviewSkeleton_ && skinnedMeshLoaded;
    const SkinningEditorSkeletonSnapshot comparisonSnapshot =
        CaptureSkinningEditorSkeletonSnapshot(
            skinningPreviewSkeleton_.get(),
            skinningPreviewModel_.get());
    skinningEditor_->RegisterTarget(
        kMidbossTargetLabel,
        skinningPreviewSkeleton_.get(),
        nullptr,
        loadSucceeded
            ? (usesOriginalAsset
                ? "\u5143\u30A2\u30BB\u30C3\u30C8\uFF08matrix\u6B63\u5F0F\u5BFE\u5FDC\uFF09"
                : "\u4E92\u63DBPreview\uFF08TRS\u6BD4\u8F03\uFF09")
            : "Preview\uFF08\u8AAD\u8FBC\u5931\u6557\uFF09",
        displayPath,
        targetStatus,
        loadedJointCount,
        skinnedMeshLoaded,
        previewInfo);
    skinningEditor_->SelectTargetByLabel(kMidbossTargetLabel);
    skinningEditor_->SetKrakenMotionPreviewTarget(
        skinningPreviewSkeleton_.get(),
        skinningPreviewModel_.get());
    skinningEditor_->SetKrakenGltfPreviewLoadResult(
        skinningPreviewAssetMode_,
        nodeDiagnostics,
        comparisonSnapshot);
    skinningEditor_->SetKrakenSkinnedPrimitiveLoadResult(
        primitiveDiagnostics,
        primitiveDiagnosticsModel);
    skinningEditor_->SetKrakenSkinnedMaterialLoadResult(
        materialDiagnostics,
        materialDiagnosticsModel);
    skinningEditor_->SetStatusMessage(targetStatus);
    Logger::Log(std::string("[Skinning] ") + kMidbossTargetLabel + "\n" + targetStatus);
}

void GameScene::FinalizeSkinningEditorPreview() {
    if (skinningEditor_) {
        skinningEditor_->ClearTargets();
    }
    skinningPreviewObject_.reset();
    skinningPreviewModel_.reset();
    skinningPreviewSkeleton_.reset();
    skinningEditor_.reset();
}
