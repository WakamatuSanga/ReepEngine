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

    bool IsFiniteVector2(const Vector2& value) {
        return std::isfinite(value.x) &&
            std::isfinite(value.y);
    }

    bool IsFiniteVector3(const Vector3& value) {
        return std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
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
                value.z * matrix.m[2][2] + matrix.m[3][2]
        };
    }

    Vector3 TransformDirection(
        const Vector3& value,
        const Matrix4x4& matrix) {
        return {
            value.x * matrix.m[0][0] + value.y * matrix.m[1][0] +
                value.z * matrix.m[2][0],
            value.x * matrix.m[0][1] + value.y * matrix.m[1][1] +
                value.z * matrix.m[2][1],
            value.x * matrix.m[0][2] + value.y * matrix.m[1][2] +
                value.z * matrix.m[2][2]
        };
    }

    Vector3 NormalizeSkinnedNormal(const Vector3& value) {
        const float length = std::sqrt(
            value.x * value.x + value.y * value.y + value.z * value.z);
        if (length <= kPositiveWeightEpsilon) {
            return { 0.0f, 1.0f, 0.0f };
        }
        const float inverseLength = 1.0f / length;
        return { value.x * inverseLength,
            value.y * inverseLength,
            value.z * inverseLength };
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
        Vector3 skinnedPosition{};
        Vector3 skinnedNormal{};
        float accumulatedSkinningWeight = 0.0f;

        for (size_t influenceIndex = 0; influenceIndex < vertex.weights.size(); ++influenceIndex) {
            const float weight = vertex.weights[influenceIndex];
            const uint32_t jointIndex = vertex.joints[influenceIndex];
            if (!(weight <= kPositiveWeightEpsilon ||
                jointIndex >= jointPalette_.size())) {
                const Matrix4x4& jointMatrix = jointPalette_[jointIndex];
                const Vector3 transformedPosition =
                    TransformPosition(vertex.position, jointMatrix);
                const Vector3 transformedNormal =
                    TransformDirection(vertex.normal, jointMatrix);
                skinnedPosition.x += transformedPosition.x * weight;
                skinnedPosition.y += transformedPosition.y * weight;
                skinnedPosition.z += transformedPosition.z * weight;
                skinnedNormal.x += transformedNormal.x * weight;
                skinnedNormal.y += transformedNormal.y * weight;
                skinnedNormal.z += transformedNormal.z * weight;
                accumulatedSkinningWeight += weight;
            }

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

        if (accumulatedSkinningWeight <= kPositiveWeightEpsilon) {
            skinnedPosition = vertex.position;
            skinnedNormal = vertex.normal;
        } else {
            skinnedNormal = NormalizeSkinnedNormal(skinnedNormal);
        }
        if (!IsFiniteVector3(skinnedPosition) ||
            !IsFiniteVector3(skinnedNormal) ||
            !IsFiniteVector2(vertex.texcoord)) {
            ++diagnostics.nonFiniteSkinnedVertexCount;
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
