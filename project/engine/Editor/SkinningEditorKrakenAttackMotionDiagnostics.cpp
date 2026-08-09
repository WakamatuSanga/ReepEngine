#include "SkinningEditorKrakenMotionPreview.h"

#include "SkinningEditorKrakenAttackMotion.h"
#include "Engine/Animation/Skeleton.h"

#include <cmath>

namespace {
    Vector3 TransformPosition(
        const Vector3& value,
        const Matrix4x4& matrix) {
        return {
            value.x * matrix.m[0][0] +
                value.y * matrix.m[1][0] +
                value.z * matrix.m[2][0] +
                matrix.m[3][0],
            value.x * matrix.m[0][1] +
                value.y * matrix.m[1][1] +
                value.z * matrix.m[2][1] +
                matrix.m[3][1],
            value.x * matrix.m[0][2] +
                value.y * matrix.m[1][2] +
                value.z * matrix.m[2][2] +
                matrix.m[3][2],
        };
    }

    bool IsFiniteVector(const Vector3& value) {
        return std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
    }

    float CalculateDistance(
        const Vector3& lhs,
        const Vector3& rhs) {
        const float deltaX = lhs.x - rhs.x;
        const float deltaY = lhs.y - rhs.y;
        const float deltaZ = lhs.z - rhs.z;
        return std::sqrt(
            deltaX * deltaX +
            deltaY * deltaY +
            deltaZ * deltaZ);
    }
}

void SkinningEditorKrakenMotionPreview::CaptureBindTipPositions() {
    bindChainTipSkeletonPositions_.clear();
    attackTipDiagnostics_ = {};

    if (!skeleton_ || chains_.empty()) {
        return;
    }

    bindChainTipSkeletonPositions_.reserve(chains_.size());
    for (const Chain& chain : chains_) {
        if (chain.joints.empty()) {
            bindChainTipSkeletonPositions_.clear();
            runtimeError_ =
                "Bind Poseの触手先端Boneを取得できませんでした。";
            return;
        }

        const int tipJointIndex = chain.joints.back();
        if (tipJointIndex < 0 ||
            tipJointIndex >= static_cast<int>(skeleton_->joints.size())) {
            bindChainTipSkeletonPositions_.clear();
            runtimeError_ =
                "Bind Poseの触手先端Joint Indexが範囲外です。";
            return;
        }

        const Vector3 tipSkeletonPosition =
            skeleton_->joints[
                static_cast<std::size_t>(tipJointIndex)].worldTranslate;
        if (!IsFiniteVector(tipSkeletonPosition)) {
            bindChainTipSkeletonPositions_.clear();
            runtimeError_ =
                "Bind Poseの触手先端位置に非有限値を検出しました。";
            return;
        }

        bindChainTipSkeletonPositions_.push_back(tipSkeletonPosition);
    }
}

void SkinningEditorKrakenMotionPreview::RefreshAttackTipDiagnostics() {
    attackTipDiagnostics_ = {};

    if (!skeleton_ ||
        !attackMotion_ ||
        chains_.empty() ||
        bindChainTipSkeletonPositions_.empty()) {
        return;
    }

    if (bindChainTipSkeletonPositions_.size() != chains_.size()) {
        runtimeError_ =
            "触手先端のBind Pose診断情報がChain数と一致しません。";
        return;
    }

    const std::size_t selectedChainIndex =
        attackMotion_->GetSelectedChainIndex();
    if (selectedChainIndex >= chains_.size()) {
        runtimeError_ =
            "攻撃対象Chain Indexが診断可能な範囲外です。";
        return;
    }

    const Chain& selectedChain = chains_[selectedChainIndex];
    if (selectedChain.joints.empty()) {
        runtimeError_ =
            "攻撃対象Chainの先端Boneを取得できませんでした。";
        return;
    }

    const int tipJointIndex = selectedChain.joints.back();
    if (tipJointIndex < 0 ||
        tipJointIndex >= static_cast<int>(skeleton_->joints.size())) {
        runtimeError_ =
            "攻撃対象Chainの先端Joint Indexが範囲外です。";
        return;
    }

    const Vector3 tipSkeletonPosition =
        skeleton_->joints[
            static_cast<std::size_t>(tipJointIndex)].worldTranslate;
    const Vector3 bindTipSkeletonPosition =
        bindChainTipSkeletonPositions_[selectedChainIndex];
    const Vector3 tipWorldPosition =
        TransformPosition(tipSkeletonPosition, previewWorldMatrix_);
    const Vector3 bindTipWorldPosition =
        TransformPosition(bindTipSkeletonPosition, previewWorldMatrix_);
    const float distanceFromBind = CalculateDistance(
        tipWorldPosition,
        bindTipWorldPosition);

    if (!IsFiniteVector(tipSkeletonPosition) ||
        !IsFiniteVector(bindTipSkeletonPosition) ||
        !IsFiniteVector(tipWorldPosition) ||
        !IsFiniteVector(bindTipWorldPosition) ||
        !std::isfinite(distanceFromBind)) {
        runtimeError_ =
            "触手先端位置の診断中に非有限値を検出しました。";
        return;
    }

    attackTipDiagnostics_.valid = true;
    attackTipDiagnostics_.jointIndex = tipJointIndex;
    attackTipDiagnostics_.skeletonPosition = tipSkeletonPosition;
    attackTipDiagnostics_.worldPosition = tipWorldPosition;
    attackTipDiagnostics_.bindSkeletonPosition =
        bindTipSkeletonPosition;
    attackTipDiagnostics_.bindWorldPosition = bindTipWorldPosition;
    attackTipDiagnostics_.distanceFromBind = distanceFromBind;
}
