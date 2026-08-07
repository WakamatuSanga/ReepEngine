#include "GltfSkinnedModel.h"
#include <cmath>

namespace {
    constexpr float kPositiveWeightEpsilon = 0.000001f;
    constexpr float kWeightSumTolerance = 0.0001f;
    constexpr float kIdentityMatrixTolerance = 0.0001f;

    bool IsFiniteMatrix(const Matrix4x4& matrix) {
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                if (!std::isfinite(matrix.m[row][column])) {
                    return false;
                }
            }
        }
        return true;
    }

    bool IsIdentityMatrix(const Matrix4x4& matrix) {
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                const float expected = (row == column) ? 1.0f : 0.0f;
                if (std::fabs(matrix.m[row][column] - expected) > kIdentityMatrixTolerance) {
                    return false;
                }
            }
        }
        return true;
    }
}

GltfSkinnedModel::SkinningDiagnostics GltfSkinnedModel::GetSkinningDiagnostics() const {
    SkinningDiagnostics diagnostics{};
    diagnostics.paletteMatrices = jointPalette_;
    diagnostics.paletteCount = static_cast<uint32_t>(jointPalette_.size());
    diagnostics.vertexCount = static_cast<uint32_t>(sourceVertices_.size());
    diagnostics.sourceBounds = sourceBounds_;
    diagnostics.skinnedBounds = skinnedBounds_;

    for (const Matrix4x4& matrix : jointPalette_) {
        if (!IsFiniteMatrix(matrix)) {
            ++diagnostics.nonFinitePaletteMatrixCount;
            continue;
        }
        if (IsIdentityMatrix(matrix)) {
            ++diagnostics.identityPaletteMatrixCount;
        }
    }

    std::vector<bool> referencedJoints(jointPalette_.size(), false);
    for (const SourceVertex& vertex : sourceVertices_) {
        float weightSum = 0.0f;
        uint32_t positiveInfluenceCount = 0;
        bool hasNonFiniteWeight = false;

        for (size_t influenceIndex = 0; influenceIndex < vertex.weights.size(); ++influenceIndex) {
            const float weight = vertex.weights[influenceIndex];
            const uint32_t jointIndex = vertex.joints[influenceIndex];
            if (jointIndex >= jointPalette_.size()) {
                ++diagnostics.invalidJointInfluenceCount;
            }
            if (!std::isfinite(weight)) {
                ++diagnostics.nonFiniteWeightCount;
                hasNonFiniteWeight = true;
                continue;
            }

            weightSum += weight;
            if (weight > kPositiveWeightEpsilon) {
                ++positiveInfluenceCount;
                if (jointIndex < referencedJoints.size()) {
                    referencedJoints[jointIndex] = true;
                }
            }
        }

        if (positiveInfluenceCount == 0) {
            ++diagnostics.weightlessVertexCount;
        }
        if (hasNonFiniteWeight || std::fabs(weightSum - 1.0f) > kWeightSumTolerance) {
            ++diagnostics.abnormalWeightSumVertexCount;
        }
        if (positiveInfluenceCount > diagnostics.maxPositiveInfluenceCount) {
            diagnostics.maxPositiveInfluenceCount = positiveInfluenceCount;
        }
    }

    for (bool isReferenced : referencedJoints) {
        diagnostics.referencedJointCount += isReferenced ? 1u : 0u;
    }
    return diagnostics;
}
