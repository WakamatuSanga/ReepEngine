#pragma once
#include "Matrix4x4.h"
#include <string>
#include <vector>

struct Joint {
    std::string name;
    int parentIndex = -1;
    std::vector<int> children;
    Vector3 localTranslate{ 0.0f, 0.0f, 0.0f };
    Vector3 localRotate{ 0.0f, 0.0f, 0.0f };
    Vector3 localScale{ 1.0f, 1.0f, 1.0f };
    Matrix4x4 worldMatrix{};
    Vector3 worldTranslate{ 0.0f, 0.0f, 0.0f };
};

struct Skeleton {
    std::string name;
    std::vector<Joint> joints;
};

void UpdateSkeletonWorldTransforms(Skeleton& skeleton);
