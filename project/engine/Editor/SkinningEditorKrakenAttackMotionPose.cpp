#include "SkinningEditorKrakenAttackMotion.h"
#include "SkinningEditorKrakenMotionPreview.h"

#include "Engine/Animation/AnimationClip.h"
#include "Engine/Animation/Skeleton.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
    constexpr float kQuaternionEpsilon = 0.000001f;
    constexpr float kDegreesToRadians =
        std::numbers::pi_v<float> / 180.0f;

    bool IsFinite(const Vector3& value) {
        return std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
    }

    bool IsFinite(const Quaternion& value) {
        return std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z) &&
            std::isfinite(value.w);
    }

    Quaternion Normalize(const Quaternion& value) {
        if (!IsFinite(value)) {
            return {};
        }
        const float lengthSquared =
            (value.x * value.x) +
            (value.y * value.y) +
            (value.z * value.z) +
            (value.w * value.w);
        if (!std::isfinite(lengthSquared) ||
            lengthSquared <= kQuaternionEpsilon * kQuaternionEpsilon) {
            return {};
        }
        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        return {
            value.x * inverseLength,
            value.y * inverseLength,
            value.z * inverseLength,
            value.w * inverseLength,
        };
    }

    Quaternion Multiply(const Quaternion& lhs, const Quaternion& rhs) {
        return Normalize({
            (lhs.w * rhs.x) + (lhs.x * rhs.w) +
                (lhs.y * rhs.z) - (lhs.z * rhs.y),
            (lhs.w * rhs.y) - (lhs.x * rhs.z) +
                (lhs.y * rhs.w) + (lhs.z * rhs.x),
            (lhs.w * rhs.z) + (lhs.x * rhs.y) -
                (lhs.y * rhs.x) + (lhs.z * rhs.w),
            (lhs.w * rhs.w) - (lhs.x * rhs.x) -
                (lhs.y * rhs.y) - (lhs.z * rhs.z),
        });
    }

    Quaternion EulerXyzToQuaternion(const Vector3& eulerRadians) {
        const float halfX = eulerRadians.x * 0.5f;
        const float halfY = eulerRadians.y * 0.5f;
        const float halfZ = eulerRadians.z * 0.5f;
        const Quaternion rotateX{
            std::sin(halfX), 0.0f, 0.0f, std::cos(halfX) };
        const Quaternion rotateY{
            0.0f, std::sin(halfY), 0.0f, std::cos(halfY) };
        const Quaternion rotateZ{
            0.0f, 0.0f, std::sin(halfZ), std::cos(halfZ) };
        return Multiply(Multiply(rotateZ, rotateY), rotateX);
    }

    Quaternion MakeAxisRotation(
        KrakenTentacleAttackLocalAxis axis,
        float radians) {
        const float halfAngle = radians * 0.5f;
        const float sine = std::sin(halfAngle);
        const float cosine = std::cos(halfAngle);
        switch (axis) {
        case KrakenTentacleAttackLocalAxis::X:
            return Normalize({ sine, 0.0f, 0.0f, cosine });
        case KrakenTentacleAttackLocalAxis::Y:
            return Normalize({ 0.0f, sine, 0.0f, cosine });
        case KrakenTentacleAttackLocalAxis::Z:
            return Normalize({ 0.0f, 0.0f, sine, cosine });
        default:
            return {};
        }
    }

    float NormalizeSign(float value) {
        if (!std::isfinite(value) || value == 0.0f) {
            return 1.0f;
        }
        return value < 0.0f ? -1.0f : 1.0f;
    }

    bool Fail(
        KrakenTentacleAttackPoseResult& result,
        const std::string& errorMessage) {
        result.joints.clear();
        result.fixedBoneCount = 0;
        result.movableBoneCount = 0;
        result.normalizedWeightSum = 0.0f;
        result.valid = false;
        result.errorMessage = errorMessage;
        return false;
    }
}

bool BuildKrakenTentacleAttackPose(
    const KrakenTentacleAttackPreviewSettings& settings,
    const KrakenTentacleAttackPoseTotals& totals,
    const std::vector<int>& chainJoints,
    const std::vector<Vector3>& bindLocalEulerRadians,
    int skeletonRootJointIndex,
    KrakenTentacleAttackPoseResult& outResult) {
    outResult.joints.clear();
    outResult.fixedBoneCount = 0;
    outResult.movableBoneCount = 0;
    outResult.normalizedWeightSum = 0.0f;
    outResult.valid = false;
    outResult.errorMessage.clear();
    if (!totals.finite ||
        !std::isfinite(totals.primaryDegrees) ||
        !std::isfinite(totals.secondaryDegrees)) {
        return Fail(
            outResult,
            "攻撃Poseの全体角度が有限値ではありません。");
    }
    if (chainJoints.empty()) {
        return Fail(outResult, "攻撃Poseを適用する触手Chainが空です。");
    }
    if (bindLocalEulerRadians.empty()) {
        return Fail(outResult, "Bind Local Rotationを取得できません。");
    }
    if (!std::isfinite(settings.tipBias) || settings.tipBias <= 0.0f) {
        return Fail(outResult, "Tip Biasは0より大きい有限値が必要です。");
    }

    const std::size_t fixedBoneCount = (std::min)(
        static_cast<std::size_t>(settings.fixedLeadingBoneCount),
        chainJoints.size());
    if (fixedBoneCount >= chainJoints.size()) {
        return Fail(
            outResult,
            "固定先頭Bone数がChain Bone数以上のため、動かせるBoneがありません。");
    }
    const std::size_t movableBoneCount =
        chainJoints.size() - fixedBoneCount;

    for (std::size_t chainIndex = 0;
        chainIndex < chainJoints.size();
        ++chainIndex) {
        const int jointIndex = chainJoints[chainIndex];
        for (std::size_t previousIndex = 0;
            previousIndex < chainIndex;
            ++previousIndex) {
            if (chainJoints[previousIndex] == jointIndex) {
                return Fail(
                    outResult,
                    "触手Chainに重複したJoint Indexがあります。");
            }
        }
        if (jointIndex < 0 ||
            static_cast<std::size_t>(jointIndex) >=
                bindLocalEulerRadians.size()) {
            return Fail(
                outResult,
                "触手Chainに範囲外のJoint Indexがあります。");
        }
        if (jointIndex == skeletonRootJointIndex) {
            return Fail(
                outResult,
                "Skeleton Rootへ攻撃回転を適用することはできません。");
        }
        if (!IsFinite(bindLocalEulerRadians[
                static_cast<std::size_t>(jointIndex)])) {
            return Fail(
                outResult,
                "Bind Local Rotationに非有限値があります。");
        }
    }

    float rawWeightSum = 0.0f;
    for (std::size_t movableIndex = 0;
        movableIndex < movableBoneCount;
        ++movableIndex) {
        const float chainT = static_cast<float>(movableIndex + 1) /
            static_cast<float>(movableBoneCount);
        const float rawWeight = std::pow(chainT, settings.tipBias);
        if (!std::isfinite(rawWeight) || rawWeight < 0.0f) {
            return Fail(
                outResult,
                "Chain角度の配分Weightを計算できませんでした。");
        }
        rawWeightSum += rawWeight;
    }
    if (!std::isfinite(rawWeightSum) ||
        rawWeightSum <= kQuaternionEpsilon) {
        return Fail(
            outResult,
            "Chain角度の配分Weight合計が不正です。");
    }

    const bool hasZeroTotals =
        totals.primaryDegrees == 0.0f &&
        totals.secondaryDegrees == 0.0f;
    outResult.joints.reserve(chainJoints.size());
    outResult.fixedBoneCount = fixedBoneCount;
    outResult.movableBoneCount = movableBoneCount;
    const float primarySign = NormalizeSign(settings.primarySign);
    const float secondarySign = NormalizeSign(settings.secondarySign);
    for (std::size_t chainBoneIndex = 0;
        chainBoneIndex < chainJoints.size();
        ++chainBoneIndex) {
        const int jointIndex = chainJoints[chainBoneIndex];
        const Vector3& bindEuler = bindLocalEulerRadians[
            static_cast<std::size_t>(jointIndex)];
        KrakenTentacleAttackJointPose pose{};
        pose.jointIndex = jointIndex;
        pose.chainBoneIndex = chainBoneIndex;
        pose.fixed = chainBoneIndex < fixedBoneCount;

        if (pose.fixed) {
            pose.attackOffset = {};
            pose.absoluteLocalRotation = EulerXyzToQuaternion(bindEuler);
            pose.absoluteLocalEulerRadians = bindEuler;
        } else {
            const std::size_t movableIndex =
                chainBoneIndex - fixedBoneCount;
            const float chainT =
                static_cast<float>(movableIndex + 1) /
                static_cast<float>(movableBoneCount);
            pose.normalizedWeight =
                std::pow(chainT, settings.tipBias) / rawWeightSum;
            pose.primaryDegrees = totals.primaryDegrees *
                primarySign * pose.normalizedWeight;
            pose.secondaryDegrees = totals.secondaryDegrees *
                secondarySign * pose.normalizedWeight;
            if (hasZeroTotals) {
                pose.attackOffset = {};
                pose.absoluteLocalRotation =
                    EulerXyzToQuaternion(bindEuler);
                pose.absoluteLocalEulerRadians = bindEuler;
            } else {
                const Quaternion primaryRotation = MakeAxisRotation(
                    settings.primaryAxis,
                    pose.primaryDegrees * kDegreesToRadians);
                const Quaternion secondaryRotation = MakeAxisRotation(
                    settings.secondaryAxis,
                    pose.secondaryDegrees * kDegreesToRadians);
                pose.attackOffset = Multiply(
                    secondaryRotation,
                    primaryRotation);
                pose.absoluteLocalRotation = Multiply(
                    EulerXyzToQuaternion(bindEuler),
                    pose.attackOffset);
                pose.absoluteLocalEulerRadians =
                    ConvertQuaternionToEulerXYZ(
                        pose.absoluteLocalRotation);
            }
        }
        pose.finite = IsFinite(pose.attackOffset) &&
            IsFinite(pose.absoluteLocalRotation) &&
            IsFinite(pose.absoluteLocalEulerRadians) &&
            std::isfinite(pose.normalizedWeight) &&
            std::isfinite(pose.primaryDegrees) &&
            std::isfinite(pose.secondaryDegrees);
        if (!pose.finite) {
            return Fail(
                outResult,
                "攻撃PoseのQuaternion合成で非有限値を検出しました。");
        }
        outResult.normalizedWeightSum += pose.normalizedWeight;
        outResult.joints.push_back(pose);
    }

    if (!std::isfinite(outResult.normalizedWeightSum) ||
        std::fabs(outResult.normalizedWeightSum - 1.0f) > 0.0001f) {
        return Fail(
            outResult,
            "Chain角度の正規化Weight合計が1になりませんでした。");
    }
    outResult.valid = true;
    return true;
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

    const bool built = BuildKrakenTentacleAttackPose(
        attackMotion_->GetSettings(),
        attackMotion_->EvaluatePoseTotals(),
        chains_[chainIndex].joints,
        bindLocalEulerRadians_,
        skeleton_->root,
        *attackPoseResult_);
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
