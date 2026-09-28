#pragma once

#include "Matrix4x4.h"
#include <string>
#include <vector>

class GltfSkinnedModel;
struct Skeleton;

struct SkinningEditorSkeletonJointSnapshot {
    std::string name;
    int index = -1;
    int parentIndex = -1;
    Vector3 localTranslation{};
    Vector3 localRotation{};
    Vector3 localScale{ 1.0f, 1.0f, 1.0f };
    Matrix4x4 localMatrix{};
    Matrix4x4 globalMatrix{};
    Vector3 globalPosition{};
};

struct SkinningEditorSkeletonSnapshot {
    std::vector<SkinningEditorSkeletonJointSnapshot> joints;
    std::vector<Matrix4x4> paletteMatrices;
    int rootIndex = -1;
    std::string rootName;
    bool valid = false;
};

struct SkinningEditorSkeletonComparisonResult {
    bool executed = false;
    bool succeeded = false;
    std::size_t originalJointCount = 0;
    std::size_t compatibleJointCount = 0;
    std::size_t mismatchJointCount = 0;
    float maxLocalMatrixDifference = 0.0f;
    float maxGlobalMatrixDifference = 0.0f;
    float maxPositionDifference = 0.0f;
    float maxPaletteMatrixDifference = 0.0f;
    std::string maxErrorJointName;
    std::string errorMessage;
};

SkinningEditorSkeletonSnapshot CaptureSkinningEditorSkeletonSnapshot(
    const Skeleton* skeleton,
    const GltfSkinnedModel* model);

SkinningEditorSkeletonComparisonResult CompareSkinningEditorSkeletons(
    const SkinningEditorSkeletonSnapshot& original,
    const SkinningEditorSkeletonSnapshot& compatible);
