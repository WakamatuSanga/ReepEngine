#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"

#include "Engine/Animation/Skeleton.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Graphics/Camera/Camera.h"
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

    float Distance(const Vector3& lhs, const Vector3& rhs) {
        const float x = lhs.x - rhs.x;
        const float y = lhs.y - rhs.y;
        const float z = lhs.z - rhs.z;
        return std::sqrt(x * x + y * y + z * z);
    }

    Vector3 Normalize(const Vector3& value) {
        const float length = Distance(value, {});
        if (!std::isfinite(length) || length <= 0.00001f) {
            return {};
        }
        return { value.x / length, value.y / length, value.z / length };
    }

    Vector3 TransformPosition(
        const Vector3& value,
        const Matrix4x4& matrix) {
        return {
            value.x * matrix.m[0][0] + value.y * matrix.m[1][0] +
                value.z * matrix.m[2][0] + matrix.m[3][0],
            value.x * matrix.m[0][1] + value.y * matrix.m[1][1] +
                value.z * matrix.m[2][1] + matrix.m[3][1],
            value.x * matrix.m[0][2] + value.y * matrix.m[1][2] +
                value.z * matrix.m[2][2] + matrix.m[3][2],
        };
    }

    Vector3 ClosestPointOnSegment(
        const Vector3& point,
        const Vector3& start,
        const Vector3& end) {
        const Vector3 segment{
            end.x - start.x, end.y - start.y, end.z - start.z };
        const Vector3 fromStart{
            point.x - start.x, point.y - start.y, point.z - start.z };
        const float lengthSquared = segment.x * segment.x +
            segment.y * segment.y + segment.z * segment.z;
        const float projection = lengthSquared > 0.000001f
            ? std::clamp(
                (fromStart.x * segment.x + fromStart.y * segment.y +
                    fromStart.z * segment.z) / lengthSquared,
                0.0f, 1.0f)
            : 0.0f;
        return {
            start.x + segment.x * projection,
            start.y + segment.y * projection,
            start.z + segment.z * projection };
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

void KrakenTentacleMidbossController::Impl::RefreshPlacementDiagnostics() {
    const std::uint64_t facingApplyCount =
        placementDiagnostics.facingApplyCount;
    const float lastFacingYaw = placementDiagnostics.lastFacingYaw;
    const std::uint32_t reachMask =
        placementDiagnostics.attackReachChainMask;
    placementDiagnostics = {};
    placementDiagnostics.facingApplyCount = facingApplyCount;
    placementDiagnostics.lastFacingYaw = lastFacingYaw;
    placementDiagnostics.attackReachChainMask = reachMask;

    placementDiagnostics.modelForward = Normalize({
        worldMatrix.m[2][0], worldMatrix.m[2][1], worldMatrix.m[2][2] });
    if (camera) {
        const Matrix4x4& cameraWorld = camera->GetWorldMatrix();
        placementDiagnostics.cameraForward = Normalize({
            cameraWorld.m[2][0], cameraWorld.m[2][1],
            cameraWorld.m[2][2] });
        placementDiagnostics.bossCameraDistance =
            Distance(worldPosition, camera->GetTranslate());
    }
    if (collisionPlayer) {
        const Vector3 playerPosition = collisionPlayer->GetWorldPosition();
        Vector3 direction{
            playerPosition.x - worldPosition.x,
            0.0f,
            playerPosition.z - worldPosition.z };
        placementDiagnostics.bossPlayerDistance =
            Distance(worldPosition, playerPosition);
        placementDiagnostics.bossToPlayerDirection = Normalize(direction);
        const Vector3& modelForward = placementDiagnostics.modelForward;
        const Vector3& toPlayer =
            placementDiagnostics.bossToPlayerDirection;
        placementDiagnostics.forwardDot =
            modelForward.x * toPlayer.x + modelForward.z * toPlayer.z;
        placementDiagnostics.facingValid =
            IsFinite(modelForward) && IsFinite(toPlayer) &&
            Distance(toPlayer, {}) > 0.0f;
    }

    if (diagnostics.skinnedBounds.valid) {
        const Vector3 localMin = diagnostics.skinnedBounds.minimum;
        const Vector3 localMax = diagnostics.skinnedBounds.maximum;
        const std::array<Vector3, 8> corners = {{
            { localMin.x, localMin.y, localMin.z },
            { localMax.x, localMin.y, localMin.z },
            { localMin.x, localMax.y, localMin.z },
            { localMax.x, localMax.y, localMin.z },
            { localMin.x, localMin.y, localMax.z },
            { localMax.x, localMin.y, localMax.z },
            { localMin.x, localMax.y, localMax.z },
            { localMax.x, localMax.y, localMax.z },
        }};
        Vector3 worldMin = TransformPosition(corners[0], worldMatrix);
        Vector3 worldMax = worldMin;
        Vector3 screenMin{ 1280.0f, 720.0f, 1.0f };
        Vector3 screenMax{};
        float minimumViewDepth = 1000000.0f;
        bool projectedAll = camera != nullptr;
        for (const Vector3& corner : corners) {
            const Vector3 world = TransformPosition(corner, worldMatrix);
            worldMin.x = (std::min)(worldMin.x, world.x);
            worldMin.y = (std::min)(worldMin.y, world.y);
            worldMin.z = (std::min)(worldMin.z, world.z);
            worldMax.x = (std::max)(worldMax.x, world.x);
            worldMax.y = (std::max)(worldMax.y, world.y);
            worldMax.z = (std::max)(worldMax.z, world.z);
            if (!camera) {
                continue;
            }
            const Vector3 view =
                TransformPosition(world, camera->GetViewMatrix());
            minimumViewDepth = (std::min)(minimumViewDepth, view.z);
            const Matrix4x4& viewProjection =
                camera->GetViewProjectionMatrix();
            const float clipX = world.x * viewProjection.m[0][0] +
                world.y * viewProjection.m[1][0] +
                world.z * viewProjection.m[2][0] +
                viewProjection.m[3][0];
            const float clipY = world.x * viewProjection.m[0][1] +
                world.y * viewProjection.m[1][1] +
                world.z * viewProjection.m[2][1] +
                viewProjection.m[3][1];
            const float clipW = world.x * viewProjection.m[0][3] +
                world.y * viewProjection.m[1][3] +
                world.z * viewProjection.m[2][3] +
                viewProjection.m[3][3];
            if (!std::isfinite(clipW) || clipW <= 0.00001f) {
                projectedAll = false;
                continue;
            }
            const float screenX = (clipX / clipW + 1.0f) * 640.0f;
            const float screenY = (1.0f - clipY / clipW) * 360.0f;
            screenMin.x = (std::min)(screenMin.x, screenX);
            screenMin.y = (std::min)(screenMin.y, screenY);
            screenMax.x = (std::max)(screenMax.x, screenX);
            screenMax.y = (std::max)(screenMax.y, screenY);
        }
        placementDiagnostics.worldBounds = {
            IsFinite(worldMin) && IsFinite(worldMax), worldMin, worldMax,
            { worldMax.x - worldMin.x, worldMax.y - worldMin.y,
                worldMax.z - worldMin.z },
            { (worldMin.x + worldMax.x) * 0.5f,
                (worldMin.y + worldMax.y) * 0.5f,
                (worldMin.z + worldMax.z) * 0.5f } };
        placementDiagnostics.screenBoundsValid = projectedAll;
        placementDiagnostics.screenBoundsMinimum = screenMin;
        placementDiagnostics.screenBoundsMaximum = screenMax;
        placementDiagnostics.screenHeightOccupancy =
            (screenMax.y - screenMin.y) / 720.0f;
        placementDiagnostics.rootSideScreenY = screenMax.y;
        placementDiagnostics.rootSideHidden = screenMax.y >= 720.0f;
        if (camera) {
            placementDiagnostics.nearPlaneDistance =
                minimumViewDepth - camera->GetNearClip();
            placementDiagnostics.nearPlaneWarning =
                placementDiagnostics.nearPlaneDistance <= 0.25f;
        }
    }

    const auto attack = std::find_if(
        capsuleSnapshots.begin(), capsuleSnapshots.end(),
        [this](const KrakenTentacleMidbossCapsuleSnapshot& snapshot) {
            return snapshot.valid &&
                snapshot.role == KrakenColliderPreviewRole::Attack &&
                snapshot.chainIndex == selectedAttackChainIndex;
        });
    if (attack == capsuleSnapshots.end() ||
        !diagnostics.playerCollisionSnapshotValid) {
        return;
    }
    placementDiagnostics.attackCapsuleStart = attack->worldStart;
    placementDiagnostics.attackCapsuleEnd = attack->worldEnd;
    placementDiagnostics.attackCapsuleRadius = attack->worldRadius;
    placementDiagnostics.playerSphereCenter =
        diagnostics.playerCollisionCenter;
    placementDiagnostics.playerSphereRadius =
        diagnostics.playerCollisionRadius;
    const Vector3 closest = ClosestPointOnSegment(
        placementDiagnostics.playerSphereCenter,
        attack->worldStart, attack->worldEnd);
    placementDiagnostics.attackClosestDistance = Distance(
        placementDiagnostics.playerSphereCenter, closest);
    placementDiagnostics.attackCombinedRadius =
        attack->worldRadius + diagnostics.playerCollisionRadius;
    placementDiagnostics.attackOverlap =
        placementDiagnostics.attackClosestDistance <=
        placementDiagnostics.attackCombinedRadius;
    placementDiagnostics.attackReachValid = true;
    if (attack->phaseActive && placementDiagnostics.attackOverlap &&
        selectedAttackChainIndex < 32) {
        placementDiagnostics.attackReachChainMask |=
            std::uint32_t{ 1 } << selectedAttackChainIndex;
    }
}
