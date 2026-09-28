#include "GltfNodeTransformParser.h"

#include "GltfNodeMatrixDiagnostics.h"
#include "Engine/Animation/AnimationClip.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>

namespace {
    constexpr float kAffineTolerance = 1.0e-5f;
    constexpr float kScaleTolerance = 1.0e-6f;
    constexpr float kShearTolerance = 1.0e-4f;
    constexpr float kReconstructionTolerance = 1.0e-5f;

    bool IsFinite(float value) {
        return std::isfinite(value);
    }

    bool IsFinite(const Vector3& value) {
        return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
    }

    bool IsFinite(const Quaternion& value) {
        return IsFinite(value.x) && IsFinite(value.y) &&
            IsFinite(value.z) && IsFinite(value.w);
    }

    bool IsFinite(const Matrix4x4& matrix) {
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                if (!IsFinite(matrix.m[row][column])) {
                    return false;
                }
            }
        }
        return true;
    }

    float Length3(float x, float y, float z) {
        return std::sqrt((x * x) + (y * y) + (z * z));
    }

    float Determinant3x3(const Matrix4x4& matrix) {
        return
            matrix.m[0][0] *
                ((matrix.m[1][1] * matrix.m[2][2]) -
                 (matrix.m[1][2] * matrix.m[2][1])) -
            matrix.m[0][1] *
                ((matrix.m[1][0] * matrix.m[2][2]) -
                 (matrix.m[1][2] * matrix.m[2][0])) +
            matrix.m[0][2] *
                ((matrix.m[1][0] * matrix.m[2][1]) -
                 (matrix.m[1][1] * matrix.m[2][0]));
    }

    Matrix4x4 MakeQuaternionRotationMatrix(const Quaternion& source) {
        Quaternion quaternion = source;
        const float length = std::sqrt(
            (quaternion.x * quaternion.x) +
            (quaternion.y * quaternion.y) +
            (quaternion.z * quaternion.z) +
            (quaternion.w * quaternion.w));
        if (length > kScaleTolerance) {
            const float inverseLength = 1.0f / length;
            quaternion.x *= inverseLength;
            quaternion.y *= inverseLength;
            quaternion.z *= inverseLength;
            quaternion.w *= inverseLength;
        } else {
            quaternion = {};
        }

        Matrix4x4 result = MatrixMath::MakeIdentity4x4();
        const float xx = quaternion.x * quaternion.x;
        const float yy = quaternion.y * quaternion.y;
        const float zz = quaternion.z * quaternion.z;
        const float xy = quaternion.x * quaternion.y;
        const float xz = quaternion.x * quaternion.z;
        const float yz = quaternion.y * quaternion.z;
        const float wx = quaternion.w * quaternion.x;
        const float wy = quaternion.w * quaternion.y;
        const float wz = quaternion.w * quaternion.z;

        result.m[0][0] = 1.0f - (2.0f * (yy + zz));
        result.m[0][1] = 2.0f * (xy + wz);
        result.m[0][2] = 2.0f * (xz - wy);
        result.m[1][0] = 2.0f * (xy - wz);
        result.m[1][1] = 1.0f - (2.0f * (xx + zz));
        result.m[1][2] = 2.0f * (yz + wx);
        result.m[2][0] = 2.0f * (xz + wy);
        result.m[2][1] = 2.0f * (yz - wx);
        result.m[2][2] = 1.0f - (2.0f * (xx + yy));
        return result;
    }

    Matrix4x4 MakeLocalMatrix(
        const Vector3& scale,
        const Quaternion& rotation,
        const Vector3& translation) {
        return MatrixMath::Multipty(
            MatrixMath::Multipty(
                MatrixMath::MakeScale(scale),
                MakeQuaternionRotationMatrix(rotation)),
            MatrixMath::MakeTranslate(translation));
    }

    float MaximumElementDifference(
        const Matrix4x4& lhs,
        const Matrix4x4& rhs) {
        float maximum = 0.0f;
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                maximum = (std::max)(
                    maximum,
                    std::fabs(lhs.m[row][column] - rhs.m[row][column]));
            }
        }
        return maximum;
    }

    std::string NodePrefix(const GltfNodeTransformInput& input) {
        const std::string name = input.nodeName.empty()
            ? ("Node " + std::to_string(input.nodeIndex))
            : ("ボーン「" + input.nodeName + "」");
        return name + "の";
    }

    GltfNodeTransformParseResult MakeError(
        const GltfNodeTransformInput& input,
        GltfNodeTransformError error,
        const std::string& reason,
        bool usedMatrix,
        bool usedTrs) {
        GltfNodeTransformParseResult result{};
        result.error = error;
        result.usedMatrix = usedMatrix;
        result.usedTrs = usedTrs;
        result.errorMessage = NodePrefix(input) + reason;
        return result;
    }

    GltfNodeTransformParseResult ParseMatrixTransform(
        const GltfNodeTransformInput& input) {
        if (!input.matrixIsArray) {
            return MakeError(
                input,
                GltfNodeTransformError::MatrixNotArray,
                "Node matrixが配列ではありません。",
                true,
                false);
        }
        if (input.matrixElementCount != 16) {
            return MakeError(
                input,
                GltfNodeTransformError::MatrixElementCount,
                "Node matrixの要素数が16ではありません。",
                true,
                false);
        }
        if (input.matrixNonNumericElementCount != 0) {
            return MakeError(
                input,
                GltfNodeTransformError::MatrixNonNumeric,
                "Node matrixに非数値要素があります。",
                true,
                false);
        }

        GltfNodeTransformParseResult result{};
        result.usedMatrix = true;
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                result.localMatrix.m[row][column] =
                    input.matrixValues[static_cast<std::size_t>(row * 4 + column)];
            }
        }
        if (!IsFinite(result.localMatrix)) {
            result.error = GltfNodeTransformError::MatrixNonFinite;
            result.errorMessage = NodePrefix(input) +
                "Node matrixにNaNまたはInfがあります。";
            return result;
        }
        if (std::fabs(result.localMatrix.m[0][3]) > kAffineTolerance ||
            std::fabs(result.localMatrix.m[1][3]) > kAffineTolerance ||
            std::fabs(result.localMatrix.m[2][3]) > kAffineTolerance ||
            std::fabs(result.localMatrix.m[3][3] - 1.0f) > kAffineTolerance) {
            result.error = GltfNodeTransformError::MatrixPerspective;
            result.errorMessage = NodePrefix(input) +
                "Node matrixにPerspective成分が含まれています。";
            return result;
        }

        const float rowLength[3] = {
            Length3(result.localMatrix.m[0][0], result.localMatrix.m[0][1], result.localMatrix.m[0][2]),
            Length3(result.localMatrix.m[1][0], result.localMatrix.m[1][1], result.localMatrix.m[1][2]),
            Length3(result.localMatrix.m[2][0], result.localMatrix.m[2][1], result.localMatrix.m[2][2]),
        };
        if (!IsFinite(rowLength[0]) || !IsFinite(rowLength[1]) ||
            !IsFinite(rowLength[2]) || rowLength[0] <= kScaleTolerance ||
            rowLength[1] <= kScaleTolerance || rowLength[2] <= kScaleTolerance) {
            result.error = GltfNodeTransformError::MatrixNearZeroScale;
            result.errorMessage = NodePrefix(input) +
                "Node matrixのScaleが0に近いため分解できません。";
            return result;
        }

        const float determinant = Determinant3x3(result.localMatrix);
        const float scaleProduct =
            rowLength[0] * rowLength[1] * rowLength[2];
        const float normalizedDeterminant = determinant / scaleProduct;
        if (!IsFinite(determinant) || !IsFinite(scaleProduct) ||
            !IsFinite(normalizedDeterminant) ||
            std::fabs(normalizedDeterminant) <= kScaleTolerance) {
            result.error = GltfNodeTransformError::MatrixInvalidDeterminant;
            result.errorMessage = NodePrefix(input) +
                "Node matrixの行列式が不正です。";
            return result;
        }

        const auto NormalizedDot = [&](int lhsRow, int rhsRow) {
            return
                ((result.localMatrix.m[lhsRow][0] * result.localMatrix.m[rhsRow][0]) +
                 (result.localMatrix.m[lhsRow][1] * result.localMatrix.m[rhsRow][1]) +
                 (result.localMatrix.m[lhsRow][2] * result.localMatrix.m[rhsRow][2])) /
                (rowLength[lhsRow] * rowLength[rhsRow]);
        };
        if (std::fabs(NormalizedDot(0, 1)) > kShearTolerance ||
            std::fabs(NormalizedDot(0, 2)) > kShearTolerance ||
            std::fabs(NormalizedDot(1, 2)) > kShearTolerance) {
            result.error = GltfNodeTransformError::MatrixShear;
            result.errorMessage = NodePrefix(input) +
                "Node matrixにTRSでは表現できないShearを検出しました。";
            return result;
        }

        DirectX::XMFLOAT4X4 source{};
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                source.m[row][column] = result.localMatrix.m[row][column];
            }
        }
        DirectX::XMVECTOR scaleVector{};
        DirectX::XMVECTOR rotationVector{};
        DirectX::XMVECTOR translationVector{};
        if (!DirectX::XMMatrixDecompose(
            &scaleVector,
            &rotationVector,
            &translationVector,
            DirectX::XMLoadFloat4x4(&source))) {
            result.error = GltfNodeTransformError::MatrixDecompositionFailed;
            result.errorMessage = NodePrefix(input) +
                "Node matrixを安全にTRSへ分解できません。";
            return result;
        }

        DirectX::XMFLOAT3 scale{};
        DirectX::XMFLOAT3 translation{};
        DirectX::XMFLOAT4 rotation{};
        DirectX::XMStoreFloat3(&scale, scaleVector);
        DirectX::XMStoreFloat3(&translation, translationVector);
        rotationVector = DirectX::XMQuaternionNormalize(rotationVector);
        DirectX::XMStoreFloat4(&rotation, rotationVector);
        result.scale = { scale.x, scale.y, scale.z };
        result.translation = { translation.x, translation.y, translation.z };
        result.rotation = { rotation.x, rotation.y, rotation.z, rotation.w };
        if (!IsFinite(result.scale) || !IsFinite(result.translation) ||
            std::fabs(result.scale.x) <= kScaleTolerance ||
            std::fabs(result.scale.y) <= kScaleTolerance ||
            std::fabs(result.scale.z) <= kScaleTolerance) {
            result.error = GltfNodeTransformError::MatrixNearZeroScale;
            result.errorMessage = NodePrefix(input) +
                "Node matrixのScaleが0に近いため分解できません。";
            return result;
        }
        const float quaternionLength = std::sqrt(
            (result.rotation.x * result.rotation.x) +
            (result.rotation.y * result.rotation.y) +
            (result.rotation.z * result.rotation.z) +
            (result.rotation.w * result.rotation.w));
        if (!IsFinite(result.rotation) || quaternionLength <= kScaleTolerance) {
            result.error = GltfNodeTransformError::MatrixInvalidQuaternion;
            result.errorMessage = NodePrefix(input) +
                "Node matrixから得たQuaternionが不正です。";
            return result;
        }

        result.eulerRotation = ConvertQuaternionToEulerXYZ(result.rotation);
        const Matrix4x4 quaternionReconstructedMatrix = MakeLocalMatrix(
            result.scale,
            result.rotation,
            result.translation);
        result.reconstructedMatrix = MatrixMath::MakeAffine(
            result.scale,
            result.eulerRotation,
            result.translation);
        result.reconstructionError = (std::max)(
            MaximumElementDifference(
                result.localMatrix,
                quaternionReconstructedMatrix),
            MaximumElementDifference(
                result.localMatrix,
                result.reconstructedMatrix));
        if (!IsFinite(result.reconstructionError) ||
            result.reconstructionError > kReconstructionTolerance) {
            result.error = GltfNodeTransformError::MatrixReconstructionError;
            result.errorMessage = NodePrefix(input) +
                "Node matrixの再構築誤差が許容値を超えています。";
            return result;
        }

        result.valid = true;
        return result;
    }
}

GltfNodeTransformParseResult ParseGltfNodeLocalTransform(
    const GltfNodeTransformInput& input) {
    const bool hasTrs = input.hasTranslation || input.hasRotation || input.hasScale;
    if (input.hasMatrix && hasTrs) {
        return MakeError(
            input,
            GltfNodeTransformError::MatrixAndTrsConflict,
            "NodeにmatrixとTRSが同時に存在します。",
            true,
            true);
    }
    if (input.hasMatrix) {
        return ParseMatrixTransform(input);
    }

    GltfNodeTransformParseResult result{};
    if (!hasTrs) {
        result.usedIdentity = true;
        result.localMatrix = MatrixMath::MakeIdentity4x4();
        result.reconstructedMatrix = result.localMatrix;
        result.valid = true;
        return result;
    }

    result.usedTrs = true;
    result.translation = input.translation;
    result.rotation = {
        input.rotation[0], input.rotation[1],
        input.rotation[2], input.rotation[3] };
    result.scale = input.scale;
    if (!IsFinite(result.translation) || !IsFinite(result.rotation) ||
        !IsFinite(result.scale)) {
        result.error = GltfNodeTransformError::TrsNonFinite;
        result.errorMessage = NodePrefix(input) +
            "TRSにNaNまたはInfがあります。";
        return result;
    }
    result.localMatrix = MakeLocalMatrix(
        result.scale,
        result.rotation,
        result.translation);
    result.reconstructedMatrix = result.localMatrix;
    result.eulerRotation = ConvertQuaternionToEulerXYZ(result.rotation);
    result.valid = true;
    return result;
}

void AccumulateGltfNodeTransformDiagnostics(
    const GltfNodeTransformInput& input,
    const GltfNodeTransformParseResult& result,
    GltfNodeMatrixDiagnostics& diagnostics) {
    ++diagnostics.totalNodeCount;
    diagnostics.matrixNodeCount += input.hasMatrix ? 1u : 0u;
    const bool hasTrs = input.hasTranslation || input.hasRotation || input.hasScale;
    diagnostics.trsNodeCount += hasTrs ? 1u : 0u;
    diagnostics.identityNodeCount += (!input.hasMatrix && !hasTrs) ? 1u : 0u;
    diagnostics.matrixTrsConflictCount +=
        result.error == GltfNodeTransformError::MatrixAndTrsConflict ? 1u : 0u;
    if (result.usedMatrix && result.valid) {
        ++diagnostics.matrixDecompositionSuccessCount;
    } else if (result.usedMatrix) {
        ++diagnostics.matrixDecompositionFailureCount;
    }

    diagnostics.nonFiniteMatrixCount +=
        result.error == GltfNodeTransformError::MatrixNonFinite ? 1u : 0u;
    diagnostics.shearDetectedCount +=
        result.error == GltfNodeTransformError::MatrixShear ? 1u : 0u;
    diagnostics.nearZeroScaleCount +=
        result.error == GltfNodeTransformError::MatrixNearZeroScale ? 1u : 0u;
    diagnostics.invalidAffineMatrixCount +=
        (result.error == GltfNodeTransformError::MatrixPerspective ||
         result.error == GltfNodeTransformError::MatrixInvalidDeterminant)
        ? 1u
        : 0u;
    diagnostics.reconstructionErrorExceededCount +=
        result.error == GltfNodeTransformError::MatrixReconstructionError ? 1u : 0u;
    const bool hasReconstructionResult =
        result.valid ||
        result.error == GltfNodeTransformError::MatrixReconstructionError;
    if (result.usedMatrix &&
        hasReconstructionResult &&
        (diagnostics.maxErrorNodeName.empty() ||
         result.reconstructionError > diagnostics.maxReconstructionError)) {
        diagnostics.maxReconstructionError = result.reconstructionError;
        diagnostics.maxErrorNodeName = input.nodeName;
    }

    if (!result.valid && diagnostics.errorMessage.empty()) {
        diagnostics.failureNodeIndex = input.nodeIndex;
        diagnostics.failureJointIndex = input.jointIndex;
        diagnostics.failureNodeName = input.nodeName;
        diagnostics.errorMessage = result.errorMessage;
        diagnostics.failureOriginalMatrix = result.localMatrix;
        diagnostics.failureReconstructedMatrix = result.reconstructedMatrix;
        diagnostics.failureTranslation = result.translation;
        diagnostics.failureRotation = result.rotation;
        diagnostics.failureScale = result.scale;
    }
}
