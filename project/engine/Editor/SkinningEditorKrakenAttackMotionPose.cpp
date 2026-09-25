#include "SkinningEditorKrakenAttackMotion.h"
#include "SkinningEditorKrakenMotionPreview.h"

#include "Engine/Animation/Skeleton.h"
#include "Engine/Game/Boss/Kraken/KrakenTentacleAttackWholeSlam.h"

namespace {
Vector3 TransformPosition(const Vector3& value, const Matrix4x4& matrix) {
    return {
        value.x * matrix.m[0][0] + value.y * matrix.m[1][0] +
            value.z * matrix.m[2][0] + matrix.m[3][0],
        value.x * matrix.m[0][1] + value.y * matrix.m[1][1] +
            value.z * matrix.m[2][1] + matrix.m[3][1],
        value.x * matrix.m[0][2] + value.y * matrix.m[1][2] +
            value.z * matrix.m[2][2] + matrix.m[3][2],
    };
}
}

void SkinningEditorKrakenMotionPreview::UpdateAttackMotion(
    float unscaledDeltaTime) {
    if (!attackMotion_) {
        runtimeError_ =
            "\u653B\u6483Motion Controller\u3092\u521D\u671F\u5316\u3067\u304D\u3066\u3044\u307E\u305B\u3093\u3002";
        return;
    }
    if (!attackMotion_->RevalidateSelectedChain(chains_.size())) {
        runtimeError_ = attackMotion_->GetLastError();
        return;
    }

    attackMotion_->Update(unscaledDeltaTime, chains_.size());
    if (!attackMotion_->GetLastError().empty()) {
        runtimeError_ = attackMotion_->GetLastError();
    }
}

void SkinningEditorKrakenMotionPreview::ApplyAttackPose() {
    if (!skeleton_ || !attackMotion_ || !attackPoseResult_) {
        runtimeError_ =
            "\u653B\u6483Pose\u306E\u5FC5\u8981\u306A\u60C5\u5831\u3092\u53D6\u5F97\u3067\u304D\u307E\u305B\u3093\u3002";
        return;
    }
    if (bindLocalEulerRadians_.size() != skeleton_->joints.size()) {
        runtimeError_ =
            "Bind Local Rotation\u6570\u304CJoint\u6570\u3068\u4E00\u81F4\u3057\u307E\u305B\u3093\u3002";
        attackMotion_->Stop();
        return;
    }
    if (!attackMotion_->RevalidateSelectedChain(chains_.size())) {
        runtimeError_ = attackMotion_->GetLastError();
        return;
    }

    const std::size_t chainIndex =
        attackMotion_->GetSelectedChainIndex();
    if (chainIndex >= chains_.size()) {
        runtimeError_ =
            "\u9078\u629E\u3057\u305F\u653B\u6483Chain\u304C\u7BC4\u56F2\u5916\u3067\u3059\u3002";
        attackMotion_->Stop();
        return;
    }

    UpdateSkeletonWorldTransforms(*skeleton_);
    const Vector3 previewTarget = TransformPosition(
        { 0.0f, 10.36f, 5.83f }, previewWorldMatrix_);
    KrakenTentacleWholeSlamPoseDiagnostics diagnostics{};
    const bool built = BuildKrakenTentacleWholeSlamPose(
        attackMotion_->GetSettings(),
        KrakenTentacleWholeSlamTargetSettings{},
        attackMotion_->GetPhase(),
        attackMotion_->EvaluatePoseTotals(),
        chains_[chainIndex].joints,
        bindLocalEulerRadians_,
        *skeleton_,
        previewWorldMatrix_,
        previewTarget,
        *attackPoseResult_,
        diagnostics);
    if (!built || !attackPoseResult_->valid) {
        runtimeError_ = attackPoseResult_->errorMessage.empty()
            ? "\u653B\u6483Pose\u3092\u751F\u6210\u3067\u304D\u307E\u305B\u3093\u3067\u3057\u305F\u3002"
            : attackPoseResult_->errorMessage;
        attackMotion_->Stop();
        return;
    }

    for (const KrakenTentacleAttackJointPose& pose :
        attackPoseResult_->joints) {
        if (pose.jointIndex < 0 ||
            pose.jointIndex >=
                static_cast<int>(skeleton_->joints.size())) {
            runtimeError_ =
                "\u653B\u6483Pose\u306B\u7BC4\u56F2\u5916\u306EJoint Index\u304C\u3042\u308A\u307E\u3059\u3002";
            attackMotion_->Stop();
            return;
        }
        skeleton_->joints[
            static_cast<std::size_t>(pose.jointIndex)].localRotate =
            pose.absoluteLocalEulerRadians;
    }
}
