#include "Skeleton.h"
#include "AnimationClip.h"
#include <cmath>
#include <utility>

namespace {
    Quaternion NormalizeQuaternion(const Quaternion& quaternion) {
        float length = std::sqrt(
            (quaternion.x * quaternion.x) +
            (quaternion.y * quaternion.y) +
            (quaternion.z * quaternion.z) +
            (quaternion.w * quaternion.w));
        if (length <= 0.000001f) {
            return {};
        }

        float invLength = 1.0f / length;
        return {
            quaternion.x * invLength,
            quaternion.y * invLength,
            quaternion.z * invLength,
            quaternion.w * invLength
        };
    }

    Quaternion MultiplyQuaternion(const Quaternion& lhs, const Quaternion& rhs) {
        return NormalizeQuaternion({
            (lhs.w * rhs.x) + (lhs.x * rhs.w) + (lhs.y * rhs.z) - (lhs.z * rhs.y),
            (lhs.w * rhs.y) - (lhs.x * rhs.z) + (lhs.y * rhs.w) + (lhs.z * rhs.x),
            (lhs.w * rhs.z) + (lhs.x * rhs.y) - (lhs.y * rhs.x) + (lhs.z * rhs.w),
            (lhs.w * rhs.w) - (lhs.x * rhs.x) - (lhs.y * rhs.y) - (lhs.z * rhs.z)
            });
    }

    Quaternion ConvertEulerXYZToQuaternion(const Vector3& euler) {
        float halfX = euler.x * 0.5f;
        float halfY = euler.y * 0.5f;
        float halfZ = euler.z * 0.5f;
        Quaternion rotateX{ std::sin(halfX), 0.0f, 0.0f, std::cos(halfX) };
        Quaternion rotateY{ 0.0f, std::sin(halfY), 0.0f, std::cos(halfY) };
        Quaternion rotateZ{ 0.0f, 0.0f, std::sin(halfZ), std::cos(halfZ) };
        return MultiplyQuaternion(MultiplyQuaternion(rotateX, rotateY), rotateZ);
    }

    Matrix4x4 MakeQuaternionRotationMatrix(const Quaternion& quaternion) {
        Quaternion q = NormalizeQuaternion(quaternion);
        Matrix4x4 result = MatrixMath::MakeIdentity4x4();

        float xx = q.x * q.x;
        float yy = q.y * q.y;
        float zz = q.z * q.z;
        float xy = q.x * q.y;
        float xz = q.x * q.z;
        float yz = q.y * q.z;
        float wx = q.w * q.x;
        float wy = q.w * q.y;
        float wz = q.w * q.z;

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

    Matrix4x4 MakeAffineMatrix(const QuaternionTransform& transform) {
        Matrix4x4 scaleMatrix = MatrixMath::MakeScale(transform.scale);
        Matrix4x4 rotationMatrix = MakeQuaternionRotationMatrix(transform.rotate);
        Matrix4x4 translateMatrix = MatrixMath::MakeTranslate(transform.translate);
        return MatrixMath::Multipty(MatrixMath::Multipty(scaleMatrix, rotationMatrix), translateMatrix);
    }

    bool IsZeroMatrix(const Matrix4x4& matrix) {
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                if (std::fabs(matrix.m[row][column]) > 0.000001f) {
                    return false;
                }
            }
        }
        return true;
    }

    void CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints) {
        const int32_t index = static_cast<int32_t>(joints.size());
        Joint joint{};
        joint.name = node.name;
        joint.transform = node.transform;
        joint.localMatrix = IsZeroMatrix(node.localMatrix) ? MakeAffineMatrix(node.transform) : node.localMatrix;
        joint.skeletonSpaceMatrix = MatrixMath::MakeIdentity4x4();
        joint.parentIndex = parent.value_or(-1);
        joint.localTranslate = node.transform.translate;
        joint.localRotate = ConvertQuaternionToEulerXYZ(node.transform.rotate);
        joint.localScale = node.transform.scale;
        joint.sourceNodeTranslation = node.transform.translate;
        joint.sourceNodeScale = node.transform.scale;
        joints.push_back(std::move(joint));

        for (const Node& child : node.children) {
            CreateJoint(child, index, joints);
        }
    }
}

Skeleton CreateSkeleton(const Node& rootNode) {
    std::vector<Joint> joints;
    CreateJoint(rootNode, std::nullopt, joints);
    return Skeleton(rootNode.name, std::move(joints));
}

Skeleton::Skeleton(std::string name, std::vector<Joint> joints)
    : name_(std::move(name)), joints_(std::move(joints)) {
    for (Joint& joint : joints_) {
        joint.children.clear();
    }
    for (size_t index = 0; index < joints_.size(); ++index) {
        Joint& joint = joints_[index];
        joint.index = static_cast<int32_t>(index);
        joint.parent = joint.parentIndex >= 0 ? std::optional<int32_t>{joint.parentIndex} : std::nullopt;
        if (joint.parentIndex < 0 && root_ < 0) {
            root_ = joint.index;
        }
        if (joint.parentIndex >= 0 && static_cast<size_t>(joint.parentIndex) < joints_.size()) {
            joints_[static_cast<size_t>(joint.parentIndex)].children.push_back(joint.index);
        }
        if (!joint.name.empty()) {
            jointMap_[joint.name] = joint.index;
        }
    }
    if (root_ < 0 && !joints_.empty()) {
        root_ = 0;
    }
    UpdateWorldTransforms();
}

int32_t Skeleton::FindJointIndex(const std::string& name) const {
    const auto match = jointMap_.find(name);
    return match == jointMap_.end() ? -1 : match->second;
}

void Skeleton::SetJointLocalTransform(size_t index, const Transform& transform) {
    if (index >= joints_.size()) return;
    Joint& joint = joints_[index];
    joint.localScale = transform.scale;
    joint.localRotate = transform.rotate;
    joint.localTranslate = transform.translate;
}

void Skeleton::SetJointLocalRotation(size_t index, const Vector3& rotation) {
    if (index < joints_.size()) joints_[index].localRotate = rotation;
}

void Skeleton::SetJointLocalTranslation(size_t index, const Vector3& translation) {
    if (index < joints_.size()) joints_[index].localTranslate = translation;
}

void Skeleton::SetJointSkeletonSpaceMatrix(size_t index, const Matrix4x4& matrix) {
    if (index >= joints_.size()) return;
    Joint& joint = joints_[index];
    joint.skeletonSpaceMatrix = matrix;
    joint.worldMatrix = joint.skeletonSpaceMatrix;
    joint.worldTranslate = { joint.worldMatrix.m[3][0], joint.worldMatrix.m[3][1], joint.worldMatrix.m[3][2] };
}

void Skeleton::ApplyJointWorldAdjustment(size_t index, const Matrix4x4& adjustment) {
    if (index < joints_.size()) {
        SetJointSkeletonSpaceMatrix(index, MatrixMath::Multipty(joints_[index].skeletonSpaceMatrix, adjustment));
    }
}

void Skeleton::UpdateWorldTransforms() {
    for (Joint& joint : joints_) {
        joint.transform.translate = joint.localTranslate;
        joint.transform.rotate = ConvertEulerXYZToQuaternion(joint.localRotate);
        joint.transform.scale = joint.localScale;
        joint.localMatrix = MatrixMath::MakeAffine(
            joint.localScale,
            joint.localRotate,
            joint.localTranslate);

        if (joint.parentIndex < 0 || joint.parentIndex >= static_cast<int>(joints_.size())) {
            joint.skeletonSpaceMatrix = joint.localMatrix;
        } else {
            joint.skeletonSpaceMatrix = MatrixMath::Multipty(
                joint.localMatrix,
                joints_[static_cast<size_t>(joint.parentIndex)].skeletonSpaceMatrix);
        }

        joint.worldMatrix = joint.skeletonSpaceMatrix;
        joint.worldTranslate = {
            joint.worldMatrix.m[3][0],
            joint.worldMatrix.m[3][1],
            joint.worldMatrix.m[3][2]
        };
    }
}

void ApplyAnimation(Skeleton& skeleton, const AnimationClip& animation, float animationTime) {
    ApplyAnimationClipAtTime(animation, skeleton, animationTime);
}
