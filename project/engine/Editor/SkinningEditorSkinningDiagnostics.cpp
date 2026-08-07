#include "SkinningEditorKrakenMotionPreview.h"
#include "Engine/Animation/Skeleton.h"
#include "Engine/Graphics/Model/GltfSkinnedModel.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr float kMatrixDifferenceTolerance = 0.0001f;
    constexpr float kMaximumBoundsRatio = 100.0f;

    bool IsFiniteVector(const Vector3& value) {
        return std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
    }

    bool MatrixDiffers(const Matrix4x4& lhs, const Matrix4x4& rhs) {
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                if (std::fabs(lhs.m[row][column] - rhs.m[row][column]) >
                    kMatrixDifferenceTolerance) {
                    return true;
                }
            }
        }
        return false;
    }

    bool IsFiniteBounds(
        const SkinningEditorKrakenMotionPreview::BoundsSnapshot& bounds) {
        return bounds.isValid &&
            IsFiniteVector(bounds.min) &&
            IsFiniteVector(bounds.max) &&
            IsFiniteVector(bounds.size);
    }

    SkinningEditorKrakenMotionPreview::BoundsSnapshot ToBoundsSnapshot(
        const GltfSkinnedModel::Bounds& bounds) {
        SkinningEditorKrakenMotionPreview::BoundsSnapshot result{};
        result.isValid = bounds.isValid;
        result.min = bounds.min;
        result.max = bounds.max;
        result.size = bounds.size;
        return result;
    }
}

void SkinningEditorKrakenMotionPreview::CaptureBindPalette() {
    bindPalette_.clear();
    if (model_) {
        bindPalette_ =
            model_->GetSkinningDiagnostics().paletteMatrices;
    }
}

uint32_t SkinningEditorKrakenMotionPreview::CountChangedPaletteMatrices(
    const std::vector<Matrix4x4>& palette) const {
    uint32_t changedCount = 0;
    const std::size_t sharedCount =
        (std::min)(bindPalette_.size(), palette.size());
    for (std::size_t index = 0; index < sharedCount; ++index) {
        changedCount +=
            MatrixDiffers(bindPalette_[index], palette[index])
            ? 1u
            : 0u;
    }
    changedCount += static_cast<uint32_t>(
        (std::max)(bindPalette_.size(), palette.size()) -
        sharedCount);
    return changedCount;
}

bool SkinningEditorKrakenMotionPreview::IsBoundsAbnormal(
    const BoundsSnapshot& source,
    const BoundsSnapshot& skinned) const {
    if (!IsFiniteBounds(source) || !IsFiniteBounds(skinned)) {
        return true;
    }

    const float sourceExtent = (std::max)({
        std::fabs(source.size.x),
        std::fabs(source.size.y),
        std::fabs(source.size.z),
        0.0001f,
        });
    const float skinnedExtent = (std::max)({
        std::fabs(skinned.size.x),
        std::fabs(skinned.size.y),
        std::fabs(skinned.size.z),
        });
    const float sourceCoordinateExtent = (std::max)({
        std::fabs(source.min.x), std::fabs(source.min.y),
        std::fabs(source.min.z), std::fabs(source.max.x),
        std::fabs(source.max.y), std::fabs(source.max.z), 0.0001f });
    const float skinnedCoordinateExtent = (std::max)({
        std::fabs(skinned.min.x), std::fabs(skinned.min.y),
        std::fabs(skinned.min.z), std::fabs(skinned.max.x),
        std::fabs(skinned.max.y), std::fabs(skinned.max.z) });
    return skinnedExtent > sourceExtent * kMaximumBoundsRatio ||
        skinnedCoordinateExtent >
            sourceCoordinateExtent * kMaximumBoundsRatio;
}

void SkinningEditorKrakenMotionPreview::RefreshDiagnostics() {
    const bool recovered =
        diagnostics_.safetyRecoveryOccurred;
    diagnostics_ = {};
    diagnostics_.safetyRecoveryOccurred = recovered;
    diagnostics_.skeletonEnabled = skeleton_ != nullptr;
    diagnostics_.boneOverlaySynchronized = ValidateCurrentPose();
    if (!model_) {
        return;
    }

    const GltfSkinnedModel::SkinningDiagnostics source =
        model_->GetSkinningDiagnostics();
    diagnostics_.paletteMatrixCount = source.paletteCount;
    diagnostics_.nonFinitePaletteMatrixCount =
        source.nonFinitePaletteMatrixCount;
    diagnostics_.identityPaletteMatrixCount =
        source.identityPaletteMatrixCount;
    diagnostics_.changedPaletteMatrixCount =
        CountChangedPaletteMatrices(source.paletteMatrices);
    diagnostics_.weightReferencedJointCount =
        source.referencedJointCount;
    diagnostics_.skinnedVertexCount = source.vertexCount;
    diagnostics_.verticesWithoutWeights =
        source.weightlessVertexCount;
    diagnostics_.invalidJointInfluenceCount =
        source.invalidJointInfluenceCount;
    diagnostics_.nonFiniteWeightCount =
        source.nonFiniteWeightCount;
    diagnostics_.invalidWeightSumVertexCount =
        source.abnormalWeightSumVertexCount;
    diagnostics_.maxPositiveInfluences =
        source.maxPositiveInfluenceCount;
    diagnostics_.sourceBounds =
        ToBoundsSnapshot(source.sourceBounds);
    diagnostics_.skinnedBounds =
        ToBoundsSnapshot(source.skinnedBounds);
    diagnostics_.abnormalBoundsDetected = IsBoundsAbnormal(
        diagnostics_.sourceBounds,
        diagnostics_.skinnedBounds);
    diagnostics_.paletteUpdateSucceeded =
        skeleton_ &&
        diagnostics_.paletteMatrixCount ==
            skeleton_->joints.size() &&
        diagnostics_.nonFinitePaletteMatrixCount == 0;
    diagnostics_.skinningUpdateSucceeded =
        diagnostics_.paletteUpdateSucceeded &&
        !diagnostics_.abnormalBoundsDetected &&
        diagnostics_.verticesWithoutWeights == 0 &&
        diagnostics_.invalidJointInfluenceCount == 0 &&
        diagnostics_.nonFiniteWeightCount == 0 &&
        diagnostics_.invalidWeightSumVertexCount == 0;
}

void SkinningEditorKrakenMotionPreview::RefreshDiagnosticsAndRecover() {
    if (!IsTarget(skeleton_) || !model_) {
        return;
    }

    RefreshDiagnostics();
    const bool requiresRecovery =
        diagnostics_.nonFinitePaletteMatrixCount > 0 ||
        diagnostics_.abnormalBoundsDetected;
    if (!requiresRecovery || recovering_) {
        return;
    }

    recovering_ = true;
    diagnostics_.safetyRecoveryOccurred = true;
    runtimeError_ =
        "\u30B9\u30AD\u30CB\u30F3\u30B0\u7570\u5E38\u3092\u691C\u51FA\u3057\u305F\u305F\u3081\u3001\u30D0\u30A4\u30F3\u30C9\u30DD\u30FC\u30BA\u3078\u5B89\u5168\u5FA9\u5E30\u3057\u307E\u3057\u305F\u3002";
    ReturnToBindPose(false);
    model_->UpdateSkinning();
    RefreshDiagnostics();
    diagnostics_.safetyRecoveryOccurred = true;
    recovering_ = false;
}
