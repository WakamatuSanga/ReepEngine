#include "SkinningEditorKrakenBoneColliderPreview.h"

#include "Engine/Animation/Skeleton.h"

#include <cmath>

namespace {
    constexpr float kMinimumColliderLength = 0.00001f;

    bool IsFinite(const Vector3& value) {
        return std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
    }

    bool IsJointIndexValid(
        int jointIndex,
        const Skeleton& skeleton,
        int skeletonRootJointIndex) {
        return jointIndex >= 0 &&
            static_cast<std::size_t>(jointIndex) < skeleton.joints.size() &&
            jointIndex != skeletonRootJointIndex;
    }
}

void SkinningEditorKrakenBoneColliderPreview::RefreshDiagnostics(
    const Skeleton& skeleton) {
    diagnostics_.chainCount = detectedChainCount_;
    diagnostics_.selectedChainIndex = selectedChainIndex_;
    diagnostics_.chainBoneCount =
        static_cast<std::uint32_t>(chainJoints_.size());
    diagnostics_.capsuleCount =
        static_cast<std::uint32_t>(capsules_.size());
    diagnostics_.tipSphereCount = tipSphere_.tipJointIndex >= 0 ? 1u : 0u;
    diagnostics_.enabledColliderCount = 0;
    diagnostics_.disabledColliderCount = 0;
    diagnostics_.invalidJointIndexCount = 0;
    diagnostics_.zeroLengthCapsuleCount = 0;
    diagnostics_.nonFinitePositionCount = 0;
    diagnostics_.nonFiniteRadiusCount = 0;
    diagnostics_.nonPositiveRadiusCount = 0;

    for (KrakenBoneColliderPreview& collider : capsules_) {
        collider.enabled
            ? ++diagnostics_.enabledColliderCount
            : ++diagnostics_.disabledColliderCount;
        const bool startValid = IsJointIndexValid(
            collider.startJointIndex,
            skeleton,
            skeletonRootJointIndex_);
        const bool endValid = IsJointIndexValid(
            collider.endJointIndex,
            skeleton,
            skeletonRootJointIndex_);
        if (!startValid) {
            ++diagnostics_.invalidJointIndexCount;
        }
        if (!endValid) {
            ++diagnostics_.invalidJointIndexCount;
        }
        const bool positionsFinite =
            IsFinite(collider.worldStart) &&
            IsFinite(collider.worldEnd) &&
            IsFinite(collider.worldCenter) &&
            IsFinite(collider.worldDirection) &&
            std::isfinite(collider.worldLength);
        if (!positionsFinite) {
            ++diagnostics_.nonFinitePositionCount;
        }
        const bool radiusFinite =
            std::isfinite(collider.localRadius) &&
            std::isfinite(collider.radiusScale) &&
            std::isfinite(collider.worldRadius);
        const bool radiusPositive =
            collider.localRadius > 0.0f &&
            collider.radiusScale > 0.0f &&
            collider.worldRadius > 0.0f;
        if (!radiusFinite) {
            ++diagnostics_.nonFiniteRadiusCount;
        }
        if (radiusFinite && !radiusPositive) {
            ++diagnostics_.nonPositiveRadiusCount;
        }
        const bool lengthFinite =
            std::isfinite(collider.worldLength);
        const bool zeroLength = startValid && endValid &&
            lengthFinite &&
            (collider.startJointIndex == collider.endJointIndex ||
                collider.worldLength <= kMinimumColliderLength);
        const bool nonZeroLength =
            collider.startJointIndex != collider.endJointIndex &&
            lengthFinite &&
            collider.worldLength > kMinimumColliderLength;
        if (zeroLength) {
            ++diagnostics_.zeroLengthCapsuleCount;
        }
        collider.valid = startValid && endValid &&
            positionsFinite && radiusFinite && radiusPositive &&
            nonZeroLength;
    }

    if (tipSphere_.tipJointIndex >= 0) {
        tipSphere_.enabled
            ? ++diagnostics_.enabledColliderCount
            : ++diagnostics_.disabledColliderCount;
        const bool tipIndexValid = IsJointIndexValid(
            tipSphere_.tipJointIndex,
            skeleton,
            skeletonRootJointIndex_);
        if (!tipIndexValid) {
            ++diagnostics_.invalidJointIndexCount;
        }
        const bool tipPositionFinite =
            IsFinite(tipSphere_.worldPosition) &&
            IsFinite(tipSphere_.bindWorldPosition) &&
            std::isfinite(tipSphere_.distanceFromBind);
        if (!tipPositionFinite) {
            ++diagnostics_.nonFinitePositionCount;
        }
        const bool tipRadiusFinite =
            std::isfinite(tipSphere_.localRadius) &&
            std::isfinite(tipSphere_.radiusScale) &&
            std::isfinite(tipSphere_.worldRadius);
        const bool tipRadiusPositive =
            tipSphere_.localRadius > 0.0f &&
            tipSphere_.radiusScale > 0.0f &&
            tipSphere_.worldRadius > 0.0f;
        if (!tipRadiusFinite) {
            ++diagnostics_.nonFiniteRadiusCount;
        }
        if (tipRadiusFinite && !tipRadiusPositive) {
            ++diagnostics_.nonPositiveRadiusCount;
        }
        tipSphere_.valid = tipIndexValid &&
            tipPositionFinite && tipRadiusFinite && tipRadiusPositive;
    } else {
        tipSphere_.valid = false;
    }

    diagnostics_.currentPoseFollowing =
        !chainJoints_.empty() &&
        diagnostics_.invalidJointIndexCount == 0 &&
        diagnostics_.zeroLengthCapsuleCount == 0 &&
        diagnostics_.nonFinitePositionCount == 0 &&
        diagnostics_.nonFiniteRadiusCount == 0 &&
        diagnostics_.nonPositiveRadiusCount == 0 &&
        diagnostics_.tipSphereCount == 1;
    if (diagnostics_.invalidJointIndexCount > 0) {
        diagnostics_.lastError =
            "無効なジョイント番号を検出しました。";
    } else if (diagnostics_.nonFinitePositionCount > 0) {
        diagnostics_.lastError =
            "非有限のコライダー位置を検出しました。";
    } else if (diagnostics_.nonFiniteRadiusCount > 0) {
        diagnostics_.lastError =
            "非有限のコライダー半径を検出しました。";
    } else if (diagnostics_.nonPositiveRadiusCount > 0) {
        diagnostics_.lastError =
            "0以下のコライダー半径を検出しました。";
    } else if (diagnostics_.zeroLengthCapsuleCount > 0) {
        diagnostics_.lastWarning =
            "ゼロ長のカプセルを検出したため描画しません。";
    } else if (diagnostics_.capsuleCount == 0 &&
        diagnostics_.tipSphereCount == 1) {
        diagnostics_.lastWarning =
            "安全に配置できるカプセルがないため、先端スフィアだけを表示します。";
    } else if (diagnostics_.disabledColliderCount > 0) {
        diagnostics_.lastWarning =
            "無効設定のコライダーは灰色で表示します。";
    }
}

void SkinningEditorKrakenBoneColliderPreview::RunDiagnostics(
    const Skeleton& skeleton) {
    RefreshDiagnostics(skeleton);
}

void SkinningEditorKrakenBoneColliderPreview::ResetDiagnostics() {
    const std::uint32_t chainCount = detectedChainCount_;
    const std::uint32_t chainIndex = selectedChainIndex_;
    const std::uint32_t chainBoneCount =
        static_cast<std::uint32_t>(chainJoints_.size());
    const std::uint32_t capsuleCount =
        static_cast<std::uint32_t>(capsules_.size());
    const std::uint32_t tipSphereCount =
        tipSphere_.tipJointIndex >= 0 ? 1u : 0u;
    diagnostics_ = {};
    diagnostics_.chainCount = chainCount;
    diagnostics_.selectedChainIndex = chainIndex;
    diagnostics_.chainBoneCount = chainBoneCount;
    diagnostics_.capsuleCount = capsuleCount;
    diagnostics_.tipSphereCount = tipSphereCount;
}

void SkinningEditorKrakenBoneColliderPreview::RecordDebugDraw(
    std::uint32_t shapeCount) const {
    diagnostics_.lastDebugDrawShapeCount = shapeCount;
    ++diagnostics_.debugDrawCount;
}
