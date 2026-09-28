#pragma once

#include "Matrix4x4.h"
#include <array>
#include <cstddef>
#include <string>

struct GltfNodeMatrixDiagnostics;

enum class GltfNodeTransformError {
    None,
    MatrixAndTrsConflict,
    MatrixNotArray,
    MatrixElementCount,
    MatrixNonNumeric,
    MatrixNonFinite,
    MatrixPerspective,
    MatrixNearZeroScale,
    MatrixInvalidDeterminant,
    MatrixShear,
    MatrixDecompositionFailed,
    MatrixInvalidQuaternion,
    MatrixReconstructionError,
    TrsNonFinite,
};

struct GltfNodeTransformInput {
    std::string nodeName;
    int nodeIndex = -1;
    int jointIndex = -1;
    bool hasMatrix = false;
    bool matrixIsArray = true;
    std::size_t matrixElementCount = 0;
    std::size_t matrixNonNumericElementCount = 0;
    std::array<float, 16> matrixValues{};
    bool hasTranslation = false;
    bool hasRotation = false;
    bool hasScale = false;
    Vector3 translation{ 0.0f, 0.0f, 0.0f };
    std::array<float, 4> rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    Vector3 scale{ 1.0f, 1.0f, 1.0f };
};

struct GltfNodeTransformParseResult {
    Vector3 translation{ 0.0f, 0.0f, 0.0f };
    Quaternion rotation{};
    Vector3 eulerRotation{ 0.0f, 0.0f, 0.0f };
    Vector3 scale{ 1.0f, 1.0f, 1.0f };
    Matrix4x4 localMatrix{};
    Matrix4x4 reconstructedMatrix{};
    GltfNodeTransformError error = GltfNodeTransformError::None;
    bool usedMatrix = false;
    bool usedTrs = false;
    bool usedIdentity = false;
    bool valid = false;
    float reconstructionError = 0.0f;
    std::string errorMessage;
};

GltfNodeTransformParseResult ParseGltfNodeLocalTransform(
    const GltfNodeTransformInput& input);

void AccumulateGltfNodeTransformDiagnostics(
    const GltfNodeTransformInput& input,
    const GltfNodeTransformParseResult& result,
    GltfNodeMatrixDiagnostics& diagnostics);
