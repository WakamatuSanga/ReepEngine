#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"

#include "Engine/Animation/Skeleton.h"
#include "Engine/Graphics/Model/GltfSkinnedModel.h"
#include "Engine/Graphics/Model/GltfSkinnedModelMaterialData.h"
#include "Engine/Graphics/Model/GltfSkinnedModelPrimitiveData.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
    constexpr float kMaximumBoundsRatio = 100.0f;
    constexpr std::size_t kExpectedMeshCount = 1;
    constexpr std::size_t kExpectedPrimitiveCount = 3;
    constexpr std::size_t kExpectedMaterialCount = 3;
    constexpr std::size_t kExpectedVertexCount = 11648;
    constexpr std::size_t kExpectedIndexCount = 52752;
    constexpr std::size_t kExpectedTriangleCount = 17584;
    constexpr std::size_t kExpectedJointCount = 41;
    constexpr std::array<const char*, 3> kExpectedMaterialNames = {
        "KrakenSkin",
        "KrakenSucker",
        "KrakenSuckerInner",
    };

    bool IsFinite(const Vector3& value) {
        return std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
    }

    KrakenTentacleMidbossBoundsSnapshot ToSnapshot(
        const GltfSkinnedModel::Bounds& source) {
        return {
            source.isValid,
            source.min,
            source.max,
            source.size,
            source.center,
        };
    }

    bool IsBoundsAbnormal(
        const KrakenTentacleMidbossBoundsSnapshot& bounds) {
        if (!bounds.valid || !IsFinite(bounds.minimum) ||
            !IsFinite(bounds.maximum) || !IsFinite(bounds.size) ||
            !IsFinite(bounds.center)) {
            return true;
        }
        return bounds.minimum.x > bounds.maximum.x ||
            bounds.minimum.y > bounds.maximum.y ||
            bounds.minimum.z > bounds.maximum.z ||
            bounds.size.x < 0.0f || bounds.size.y < 0.0f ||
            bounds.size.z < 0.0f;
    }

    bool IsBoundsAbnormal(
        const KrakenTentacleMidbossBoundsSnapshot& source,
        const KrakenTentacleMidbossBoundsSnapshot& skinned) {
        if (IsBoundsAbnormal(source) || IsBoundsAbnormal(skinned)) {
            return true;
        }
        const float sourceExtent = (std::max)({
            std::fabs(source.size.x), std::fabs(source.size.y),
            std::fabs(source.size.z), 0.0001f });
        const float skinnedExtent = (std::max)({
            std::fabs(skinned.size.x), std::fabs(skinned.size.y),
            std::fabs(skinned.size.z) });
        const float sourceCoordinateExtent = (std::max)({
            std::fabs(source.minimum.x), std::fabs(source.minimum.y),
            std::fabs(source.minimum.z), std::fabs(source.maximum.x),
            std::fabs(source.maximum.y), std::fabs(source.maximum.z),
            0.0001f });
        const float skinnedCoordinateExtent = (std::max)({
            std::fabs(skinned.minimum.x), std::fabs(skinned.minimum.y),
            std::fabs(skinned.minimum.z), std::fabs(skinned.maximum.x),
            std::fabs(skinned.maximum.y), std::fabs(skinned.maximum.z) });
        return skinnedExtent >= sourceExtent * kMaximumBoundsRatio ||
            skinnedCoordinateExtent >=
                sourceCoordinateExtent * kMaximumBoundsRatio;
    }

    bool HasExpectedPrimitiveMaterialMap(
        const GltfSkinnedPrimitiveDiagnostics& diagnostics) {
        if (diagnostics.primitives.size() != kExpectedPrimitiveCount) {
            return false;
        }
        for (std::size_t index = 0;
            index < diagnostics.primitives.size(); ++index) {
            const GltfSkinnedPrimitiveDiagnosticEntry& primitive =
                diagnostics.primitives[index];
            if (!primitive.valid ||
                primitive.sourcePrimitiveIndex != index ||
                primitive.materialIndex != static_cast<int>(index)) {
                return false;
            }
        }
        return true;
    }

    bool HasExpectedMaterials(
        const GltfSkinnedMaterialDiagnostics& diagnostics) {
        if (diagnostics.materials.size() != kExpectedMaterialCount ||
            diagnostics.bindings.size() != kExpectedPrimitiveCount) {
            return false;
        }
        for (std::size_t index = 0; index < kExpectedMaterialCount; ++index) {
            const GltfSkinnedMaterialData& material =
                diagnostics.materials[index];
            const GltfSkinnedMaterialBindingDiagnostic& binding =
                diagnostics.bindings[index];
            if (!material.valid || material.sourceMaterialIndex !=
                    static_cast<int>(index) ||
                material.name != kExpectedMaterialNames[index] ||
                material.usingFallbackTexture ||
                binding.sourcePrimitiveIndex != index ||
                binding.sourceMaterialIndex != static_cast<int>(index) ||
                binding.materialName != kExpectedMaterialNames[index]) {
                return false;
            }
        }
        return true;
    }
}

bool KrakenTentacleMidbossController::Impl::ValidateLoadedAsset() {
    if (!skeleton || !model || !model->IsValid()) {
        lastError = "Skinned ModelまたはSkeletonが無効です。";
        return false;
    }
    const GltfSkinnedPrimitiveDiagnostics& primitive =
        model->GetPrimitiveDiagnostics();
    const GltfSkinnedMaterialDiagnostics material =
        model->GetMaterialDiagnostics();
    const GltfSkinnedModel::SkinningDiagnostics skinning =
        model->GetSkinningDiagnostics();

    const bool primitiveValid = primitive.loadSucceeded &&
        primitive.sourceMeshCount == kExpectedMeshCount &&
        primitive.sourceMeshIndex == 0 &&
        primitive.sourcePrimitiveCount == kExpectedPrimitiveCount &&
        primitive.validPrimitiveCount == kExpectedPrimitiveCount &&
        primitive.invalidPrimitiveCount == 0 &&
        primitive.vertexCount == kExpectedVertexCount &&
        primitive.totalIndexCount == kExpectedIndexCount &&
        primitive.triangleCount == kExpectedTriangleCount &&
        primitive.rangeCount == kExpectedPrimitiveCount &&
        primitive.drawCallCount == kExpectedPrimitiveCount &&
        primitive.sharedVertexStream &&
        primitive.requiredAttributeAccessorsMatch &&
        primitive.multiPrimitiveSupported &&
        primitive.multiMaterialSupported &&
        !primitive.multiMeshSupported &&
        HasExpectedPrimitiveMaterialMap(primitive);
    if (!primitiveValid) {
        lastError = "Mesh / Primitive / Vertex / Index構成が期待値と一致しません。";
        return false;
    }

    const bool materialValid = material.loadSucceeded &&
        material.sourceMaterialCount == kExpectedMaterialCount &&
        material.loadedMaterialCount == kExpectedMaterialCount &&
        material.validMaterialCount == kExpectedMaterialCount &&
        material.invalidMaterialCount == 0 &&
        material.materialConstantBufferCount == kExpectedMaterialCount &&
        material.primitiveCount == kExpectedPrimitiveCount &&
        material.multiPrimitiveSupported &&
        material.multiMaterialSupported &&
        !material.multiMeshSupported &&
        HasExpectedMaterials(material);
    if (!materialValid) {
        lastError = "3 MaterialまたはPrimitiveとの対応が期待値と一致しません。";
        return false;
    }

    const bool skinningValid =
        skeleton->joints.size() == kExpectedJointCount &&
        model->GetVertexCount() == kExpectedVertexCount &&
        model->GetPaletteCount() == kExpectedJointCount &&
        skinning.paletteCount == kExpectedJointCount &&
        skinning.vertexCount == kExpectedVertexCount &&
        skinning.nonFinitePaletteMatrixCount == 0 &&
        skinning.nonFiniteSkinnedVertexCount == 0 &&
        skinning.weightlessVertexCount == 0 &&
        skinning.invalidJointInfluenceCount == 0 &&
        skinning.nonFiniteWeightCount == 0 &&
        skinning.abnormalWeightSumVertexCount == 0 &&
        model->HasComputeSkinningResources() &&
        model->IsUsingComputeOutputVertices();
    if (!skinningValid) {
        lastError = "41 Joint SkinningまたはCompute Resourceが期待値と一致しません。";
        return false;
    }
    const KrakenTentacleMidbossBoundsSnapshot source =
        ToSnapshot(model->GetSourceBounds());
    const KrakenTentacleMidbossBoundsSnapshot skinned =
        ToSnapshot(model->GetSkinnedBounds());
    if (IsBoundsAbnormal(source, skinned)) {
        lastError = "Model Boundsが不正です。";
        return false;
    }
    RefreshSkinningDiagnostics();
    return true;
}

void KrakenTentacleMidbossController::Impl::RefreshSkinningDiagnostics() {
    if (!model) {
        return;
    }
    const GltfSkinnedPrimitiveDiagnostics& primitive =
        model->GetPrimitiveDiagnostics();
    const GltfSkinnedMaterialDiagnostics material =
        model->GetMaterialDiagnostics();
    const GltfSkinnedModel::SkinningDiagnostics skinning =
        model->GetSkinningDiagnostics();
    diagnostics.meshCount = primitive.sourceMeshCount;
    diagnostics.primitiveCount = primitive.validPrimitiveCount;
    diagnostics.materialCount = material.validMaterialCount;
    diagnostics.vertexCount = skinning.vertexCount;
    diagnostics.indexCount = primitive.totalIndexCount;
    diagnostics.triangleCount = primitive.triangleCount;
    diagnostics.jointCount = skeleton ? skeleton->joints.size() : 0;
    diagnostics.paletteCount = skinning.paletteCount;
    diagnostics.nonFinitePaletteCount =
        skinning.nonFinitePaletteMatrixCount;
    diagnostics.nonFiniteSkinnedVertexCount =
        skinning.nonFiniteSkinnedVertexCount;
    diagnostics.weightlessVertexCount = skinning.weightlessVertexCount;
    diagnostics.invalidJointInfluenceCount =
        skinning.invalidJointInfluenceCount;
    diagnostics.sourceBounds = ToSnapshot(skinning.sourceBounds);
    diagnostics.skinnedBounds = ToSnapshot(skinning.skinnedBounds);
    diagnostics.boundsAbnormal = IsBoundsAbnormal(
        diagnostics.sourceBounds, diagnostics.skinnedBounds);
}

void KrakenTentacleMidbossController::Impl::RefreshDrawDiagnostics() {
    if (!model) {
        return;
    }
    const GltfSkinnedMaterialDiagnostics material =
        model->GetMaterialDiagnostics();
    diagnostics.drawCallCount = material.drawCallCount;
    diagnostics.materialBindingCount = material.materialBindingCount;
    if (material.drawCallCount != kExpectedPrimitiveCount ||
        material.materialBindingCount != kExpectedPrimitiveCount ||
        material.bindingFailureCount != 0) {
        EnterHidden(
            "3 Primitive / 3 Materialの描画Bindingに失敗しました。",
            true);
    }
}
