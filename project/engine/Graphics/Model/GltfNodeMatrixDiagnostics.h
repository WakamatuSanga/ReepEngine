#pragma once

#include "Matrix4x4.h"
#include <cstddef>
#include <string>

struct GltfNodeMatrixDiagnostics {
    std::string sourcePath;
    std::size_t totalNodeCount = 0;
    std::size_t matrixNodeCount = 0;
    std::size_t trsNodeCount = 0;
    std::size_t identityNodeCount = 0;
    std::size_t matrixDecompositionSuccessCount = 0;
    std::size_t matrixDecompositionFailureCount = 0;
    std::size_t matrixTrsConflictCount = 0;
    std::size_t nonFiniteMatrixCount = 0;
    std::size_t shearDetectedCount = 0;
    std::size_t nearZeroScaleCount = 0;
    std::size_t invalidAffineMatrixCount = 0;
    std::size_t reconstructionErrorExceededCount = 0;
    float maxReconstructionError = 0.0f;
    std::string maxErrorNodeName;
    int failureNodeIndex = -1;
    int failureJointIndex = -1;
    std::string failureNodeName;
    std::string errorMessage;
    Matrix4x4 failureOriginalMatrix{};
    Matrix4x4 failureReconstructedMatrix{};
    Vector3 failureTranslation{};
    Quaternion failureRotation{};
    Vector3 failureScale{ 1.0f, 1.0f, 1.0f };
    bool loadSucceeded = false;
};
