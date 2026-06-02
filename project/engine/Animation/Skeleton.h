#pragma once
#include "Matrix4x4.h"
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct AnimationClip;

using EulerTransform = Transform;

struct QuaternionTransform {
    Vector3 scale{ 1.0f, 1.0f, 1.0f };
    Quaternion rotate{};
    Vector3 translate{ 0.0f, 0.0f, 0.0f };
};

struct Node {
    QuaternionTransform transform{};
    Matrix4x4 localMatrix{};
    std::string name;
    std::vector<Node> children;
};

struct Joint {
    std::string name;
    int parentIndex = -1;
    std::vector<int> children;
    QuaternionTransform transform{};
    Matrix4x4 localMatrix{};
    Matrix4x4 skeletonSpaceMatrix{};
    int32_t index = -1;
    std::optional<int32_t> parent = std::nullopt;
    Vector3 localTranslate{ 0.0f, 0.0f, 0.0f };
    Vector3 localRotate{ 0.0f, 0.0f, 0.0f };
    Vector3 localScale{ 1.0f, 1.0f, 1.0f };
    Vector3 sourceNodeTranslation{ 0.0f, 0.0f, 0.0f };
    Vector3 sourceNodeScale{ 1.0f, 1.0f, 1.0f };
    Matrix4x4 worldMatrix{};
    Vector3 worldTranslate{ 0.0f, 0.0f, 0.0f };
};

struct Skeleton {
    std::string name;
    int32_t root = -1;
    std::map<std::string, int32_t> jointMap;
    std::vector<Joint> joints;
};

int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints);
Skeleton CreateSkeleton(const Node& rootNode);
void Update(Skeleton& skeleton);
void UpdateSkeletonWorldTransforms(Skeleton& skeleton);
void ApplyAnimation(Skeleton& skeleton, const AnimationClip& animation, float animationTime);
