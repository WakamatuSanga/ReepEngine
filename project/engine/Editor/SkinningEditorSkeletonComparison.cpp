#include "SkinningEditorSkeletonComparison.h"

#include "Engine/Animation/Skeleton.h"
#include "Engine/Graphics/Model/GltfSkinnedModel.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {
    constexpr float kMatrixTolerance = 1.0e-5f;
    constexpr float kPositionTolerance = 1.0e-4f;

    float MaximumMatrixDifference(
        const Matrix4x4& lhs,
        const Matrix4x4& rhs) {
        float maximum = 0.0f;
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                if (!std::isfinite(lhs.m[row][column]) ||
                    !std::isfinite(rhs.m[row][column])) {
                    return std::numeric_limits<float>::infinity();
                }
                maximum = (std::max)(
                    maximum,
                    std::fabs(lhs.m[row][column] - rhs.m[row][column]));
            }
        }
        return maximum;
    }

    float MaximumVectorDifference(const Vector3& lhs, const Vector3& rhs) {
        if (!std::isfinite(lhs.x) || !std::isfinite(lhs.y) ||
            !std::isfinite(lhs.z) || !std::isfinite(rhs.x) ||
            !std::isfinite(rhs.y) || !std::isfinite(rhs.z)) {
            return std::numeric_limits<float>::infinity();
        }
        return (std::max)({
            std::fabs(lhs.x - rhs.x),
            std::fabs(lhs.y - rhs.y),
            std::fabs(lhs.z - rhs.z),
        });
    }

    void UpdateMaximum(float value, float& maximum) {
        maximum = (std::max)(value, maximum);
    }
}

SkinningEditorSkeletonSnapshot CaptureSkinningEditorSkeletonSnapshot(
    const Skeleton* skeleton,
    const GltfSkinnedModel* model) {
    SkinningEditorSkeletonSnapshot snapshot{};
    if (!skeleton || !model) {
        return snapshot;
    }

    snapshot.rootIndex = skeleton->root;
    if (snapshot.rootIndex >= 0 &&
        snapshot.rootIndex < static_cast<int>(skeleton->joints.size())) {
        snapshot.rootName = skeleton->joints[
            static_cast<std::size_t>(snapshot.rootIndex)].name;
    }
    snapshot.joints.reserve(skeleton->joints.size());
    for (const Joint& joint : skeleton->joints) {
        SkinningEditorSkeletonJointSnapshot jointSnapshot{};
        jointSnapshot.name = joint.name;
        jointSnapshot.index = joint.index;
        jointSnapshot.parentIndex = joint.parentIndex;
        jointSnapshot.localTranslation = joint.localTranslate;
        jointSnapshot.localRotation = joint.localRotate;
        jointSnapshot.localScale = joint.localScale;
        jointSnapshot.localMatrix = joint.localMatrix;
        jointSnapshot.globalMatrix = joint.skeletonSpaceMatrix;
        jointSnapshot.globalPosition = joint.worldTranslate;
        snapshot.joints.push_back(std::move(jointSnapshot));
    }
    snapshot.paletteMatrices =
        model->GetSkinningDiagnostics().paletteMatrices;
    snapshot.valid =
        !snapshot.joints.empty() &&
        snapshot.paletteMatrices.size() == snapshot.joints.size();
    return snapshot;
}

SkinningEditorSkeletonComparisonResult CompareSkinningEditorSkeletons(
    const SkinningEditorSkeletonSnapshot& original,
    const SkinningEditorSkeletonSnapshot& compatible) {
    SkinningEditorSkeletonComparisonResult result{};
    result.executed = true;
    result.originalJointCount = original.joints.size();
    result.compatibleJointCount = compatible.joints.size();
    if (!original.valid || !compatible.valid) {
        result.errorMessage =
            "元アセットと互換Previewを一度ずつ正常に読み込んでください。";
        return result;
    }

    const std::size_t sharedJointCount =
        (std::min)(original.joints.size(), compatible.joints.size());
    float maximumDifference = 0.0f;
    for (std::size_t jointIndex = 0;
         jointIndex < sharedJointCount;
         ++jointIndex) {
        const SkinningEditorSkeletonJointSnapshot& originalJoint =
            original.joints[jointIndex];
        const SkinningEditorSkeletonJointSnapshot& compatibleJoint =
            compatible.joints[jointIndex];
        const float localDifference = MaximumMatrixDifference(
            originalJoint.localMatrix,
            compatibleJoint.localMatrix);
        const float globalDifference = MaximumMatrixDifference(
            originalJoint.globalMatrix,
            compatibleJoint.globalMatrix);
        const float positionDifference = MaximumVectorDifference(
            originalJoint.globalPosition,
            compatibleJoint.globalPosition);
        const float translationDifference = MaximumVectorDifference(
            originalJoint.localTranslation,
            compatibleJoint.localTranslation);
        const float scaleDifference = MaximumVectorDifference(
            originalJoint.localScale,
            compatibleJoint.localScale);
        float paletteDifference = 0.0f;
        if (jointIndex < original.paletteMatrices.size() &&
            jointIndex < compatible.paletteMatrices.size()) {
            paletteDifference = MaximumMatrixDifference(
                original.paletteMatrices[jointIndex],
                compatible.paletteMatrices[jointIndex]);
        }

        const std::string comparisonName = originalJoint.name.empty()
            ? ("Joint " + std::to_string(jointIndex))
            : originalJoint.name;
        UpdateMaximum(
            localDifference,
            result.maxLocalMatrixDifference);
        UpdateMaximum(
            globalDifference,
            result.maxGlobalMatrixDifference);
        UpdateMaximum(
            positionDifference,
            result.maxPositionDifference);
        UpdateMaximum(
            paletteDifference,
            result.maxPaletteMatrixDifference);
        const float jointMaximumDifference = (std::max)({
            localDifference,
            globalDifference,
            positionDifference,
            paletteDifference,
        });
        if (jointMaximumDifference > maximumDifference) {
            maximumDifference = jointMaximumDifference;
            result.maxErrorJointName = comparisonName;
        }

        const bool metadataMismatch =
            originalJoint.name != compatibleJoint.name ||
            originalJoint.index != compatibleJoint.index ||
            originalJoint.parentIndex != compatibleJoint.parentIndex;
        const bool transformMismatch =
            localDifference > kMatrixTolerance ||
            globalDifference > kMatrixTolerance ||
            positionDifference > kPositionTolerance ||
            translationDifference > kMatrixTolerance ||
            scaleDifference > kMatrixTolerance ||
            paletteDifference > kMatrixTolerance;
        result.mismatchJointCount +=
            metadataMismatch || transformMismatch ? 1u : 0u;
    }

    result.mismatchJointCount +=
        (std::max)(original.joints.size(), compatible.joints.size()) -
        sharedJointCount;
    const bool rootMatches =
        original.rootIndex == compatible.rootIndex &&
        original.rootName == compatible.rootName;
    const bool paletteCountMatches =
        original.paletteMatrices.size() == compatible.paletteMatrices.size();
    if (!rootMatches) {
        result.errorMessage = "Root Jointが一致しません。";
    } else if (!paletteCountMatches) {
        result.errorMessage = "Palette Matrix数が一致しません。";
    } else if (result.mismatchJointCount != 0) {
        result.errorMessage = "許容誤差を超えるJoint差を検出しました。";
    }
    result.succeeded =
        rootMatches &&
        paletteCountMatches &&
        result.mismatchJointCount == 0;
    return result;
}
