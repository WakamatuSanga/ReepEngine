#include "Skeleton.h"
#include "AnimationClip.h"
#include <cmath>

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
}

int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints) {
    int32_t jointIndex = static_cast<int32_t>(joints.size());

    Joint joint{};
    joint.name = node.name;
    joint.transform = node.transform;
    joint.localMatrix = IsZeroMatrix(node.localMatrix) ? MakeAffineMatrix(node.transform) : node.localMatrix;
    joint.skeletonSpaceMatrix = MatrixMath::MakeIdentity4x4();
    joint.index = jointIndex;
    joint.parent = parent;
    joint.parentIndex = parent ? *parent : -1;
    joint.localTranslate = node.transform.translate;
    joint.localRotate = ConvertQuaternionToEulerXYZ(node.transform.rotate);
    joint.localScale = node.transform.scale;
    joint.sourceNodeTranslation = node.transform.translate;
    joint.sourceNodeScale = node.transform.scale;
    joints.push_back(joint);

    for (const Node& child : node.children) {
        int32_t childIndex = CreateJoint(child, jointIndex, joints);
        joints[static_cast<size_t>(jointIndex)].children.push_back(childIndex);
    }

    return jointIndex;
}

Skeleton CreateSkeleton(const Node& rootNode) {
    Skeleton skeleton{};
    skeleton.name = rootNode.name;
    skeleton.root = CreateJoint(rootNode, std::nullopt, skeleton.joints);

    for (int32_t jointIndex = 0; jointIndex < static_cast<int32_t>(skeleton.joints.size()); ++jointIndex) {
        Joint& joint = skeleton.joints[static_cast<size_t>(jointIndex)];
        joint.index = jointIndex;
        if (!joint.name.empty()) {
            skeleton.jointMap[joint.name] = jointIndex;
        }
    }

    Update(skeleton);
    return skeleton;
}

void Update(Skeleton& skeleton) {
    skeleton.jointMap.clear();
    if (skeleton.root < 0 && !skeleton.joints.empty()) {
        skeleton.root = 0;
    }

    for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
        Joint& joint = skeleton.joints[jointIndex];
        joint.index = static_cast<int32_t>(jointIndex);
        joint.parent = (joint.parentIndex >= 0) ? std::optional<int32_t>{ joint.parentIndex } : std::nullopt;
        if (joint.parentIndex < 0 && skeleton.root < 0) {
            skeleton.root = joint.index;
        }
        if (!joint.name.empty()) {
            skeleton.jointMap[joint.name] = joint.index;
        }

        joint.transform.translate = joint.localTranslate;
        joint.transform.rotate = ConvertEulerXYZToQuaternion(joint.localRotate);
        joint.transform.scale = joint.localScale;
        joint.localMatrix = MatrixMath::MakeAffine(
            joint.localScale,
            joint.localRotate,
            joint.localTranslate);

        if (joint.parentIndex < 0 || joint.parentIndex >= static_cast<int>(skeleton.joints.size())) {
            joint.skeletonSpaceMatrix = joint.localMatrix;
        } else {
            joint.skeletonSpaceMatrix = MatrixMath::Multipty(
                joint.localMatrix,
                skeleton.joints[static_cast<size_t>(joint.parentIndex)].skeletonSpaceMatrix);
        }

        joint.worldMatrix = joint.skeletonSpaceMatrix;
        joint.worldTranslate = {
            joint.worldMatrix.m[3][0],
            joint.worldMatrix.m[3][1],
            joint.worldMatrix.m[3][2]
        };
    }
}

void UpdateSkeletonWorldTransforms(Skeleton& skeleton) {
    Update(skeleton);
}

void ApplyAnimation(Skeleton& skeleton, const AnimationClip& animation, float animationTime) {
    ApplyAnimationClipAtTime(animation, skeleton, animationTime);
}
