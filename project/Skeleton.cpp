#include "Skeleton.h"

void UpdateSkeletonWorldTransforms(Skeleton& skeleton) {
    for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
        Joint& joint = skeleton.joints[jointIndex];
        Matrix4x4 localMatrix = MatrixMath::MakeAffine(
            joint.localScale,
            joint.localRotate,
            joint.localTranslate);

        if (joint.parentIndex < 0 || joint.parentIndex >= static_cast<int>(skeleton.joints.size())) {
            joint.worldMatrix = localMatrix;
        } else {
            joint.worldMatrix = MatrixMath::Multipty(
                localMatrix,
                skeleton.joints[static_cast<size_t>(joint.parentIndex)].worldMatrix);
        }

        joint.worldTranslate = {
            joint.worldMatrix.m[3][0],
            joint.worldMatrix.m[3][1],
            joint.worldMatrix.m[3][2]
        };
    }
}
