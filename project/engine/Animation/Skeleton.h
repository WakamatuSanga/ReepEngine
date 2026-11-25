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
    Skeleton(std::string name, std::vector<Joint> joints);

    const std::string& GetName() const { return name_; }
    int32_t GetRootIndex() const { return root_; }
    const std::vector<Joint>& GetJoints() const { return joints_; }
    int32_t FindJointIndex(const std::string& name) const;
    size_t GetNamedJointCount() const { return jointMap_.size(); }

    void SetJointLocalTransform(size_t index, const Transform& transform);
    void SetJointLocalRotation(size_t index, const Vector3& rotation);
    void SetJointLocalTranslation(size_t index, const Vector3& translation);
    void SetJointSkeletonSpaceMatrix(size_t index, const Matrix4x4& matrix);
    void ApplyJointWorldAdjustment(size_t index, const Matrix4x4& adjustment);
    void UpdateWorldTransforms();

private:
    std::string name_;
    int32_t root_ = -1;
    std::map<std::string, int32_t> jointMap_;
    std::vector<Joint> joints_;
};

Skeleton CreateSkeleton(const Node& rootNode);
void ApplyAnimation(Skeleton& skeleton, const AnimationClip& animation, float animationTime);
