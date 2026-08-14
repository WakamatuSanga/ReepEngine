#include "SkinningEditorKrakenBoneColliderPreview.h"

#include "Engine/Animation/Skeleton.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace {
    constexpr float kMinimumLength = 0.00001f;
    constexpr std::array<std::array<float, 2>, 4> kNormalizedSegments = {{
        { 0.15f, 0.35f },
        { 0.35f, 0.55f },
        { 0.55f, 0.75f },
        { 0.75f, 0.95f },
    }};
    constexpr std::array<float, 4> kRecommendedRadii = {
        0.45f, 0.35f, 0.28f, 0.22f,
    };

    bool IsFinite(const Vector3& value) {
        return std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
    }

    Vector3 TransformPosition(
        const Vector3& value,
        const Matrix4x4& matrix) {
        return {
            value.x * matrix.m[0][0] +
                value.y * matrix.m[1][0] +
                value.z * matrix.m[2][0] + matrix.m[3][0],
            value.x * matrix.m[0][1] +
                value.y * matrix.m[1][1] +
                value.z * matrix.m[2][1] + matrix.m[3][1],
            value.x * matrix.m[0][2] +
                value.y * matrix.m[1][2] +
                value.z * matrix.m[2][2] + matrix.m[3][2],
        };
    }

    Vector3 Subtract(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    }

    float Length(const Vector3& value) {
        return std::sqrt(
            value.x * value.x +
            value.y * value.y +
            value.z * value.z);
    }

    float Distance(const Vector3& lhs, const Vector3& rhs) {
        return Length(Subtract(lhs, rhs));
    }

    float ExtractMaximumScale(const Matrix4x4& matrix) {
        const float scaleX = std::sqrt(
            matrix.m[0][0] * matrix.m[0][0] +
            matrix.m[0][1] * matrix.m[0][1] +
            matrix.m[0][2] * matrix.m[0][2]);
        const float scaleY = std::sqrt(
            matrix.m[1][0] * matrix.m[1][0] +
            matrix.m[1][1] * matrix.m[1][1] +
            matrix.m[1][2] * matrix.m[1][2]);
        const float scaleZ = std::sqrt(
            matrix.m[2][0] * matrix.m[2][0] +
            matrix.m[2][1] * matrix.m[2][1] +
            matrix.m[2][2] * matrix.m[2][2]);
        return (std::max)({ scaleX, scaleY, scaleZ });
    }

    std::size_t FindNearestChainBone(
        float normalizedPosition,
        std::size_t chainBoneCount) {
        if (chainBoneCount <= 1) {
            return 0;
        }
        const float scaled = std::clamp(normalizedPosition, 0.0f, 1.0f) *
            static_cast<float>(chainBoneCount - 1);
        return static_cast<std::size_t>(std::lround(scaled));
    }
}

void SkinningEditorKrakenBoneColliderPreview::Reset() {
    Clear();
    settings_ = {};
}

void SkinningEditorKrakenBoneColliderPreview::Clear() {
    capsules_.clear();
    tipSphere_ = {};
    diagnostics_ = {};
    chainJoints_.clear();
    bindTipSkeletonPosition_ = {};
    skeletonRootJointIndex_ = -1;
    detectedChainCount_ = 0;
    selectedChainIndex_ = 0;
    selectedColliderIndex_ = 0;
    hasBindTipPosition_ = false;
}

bool SkinningEditorKrakenBoneColliderPreview::Rebuild(
    std::uint32_t detectedChainCount,
    std::uint32_t chainIndex,
    const std::vector<int>& chainJoints,
    int skeletonRootJointIndex,
    std::size_t skeletonJointCount,
    const Vector3& bindTipSkeletonPosition) {
    capsules_.clear();
    tipSphere_ = {};
    chainJoints_.clear();
    selectedColliderIndex_ = 0;
    detectedChainCount_ = detectedChainCount;
    selectedChainIndex_ = chainIndex;
    skeletonRootJointIndex_ = skeletonRootJointIndex;
    hasBindTipPosition_ = false;

    if (detectedChainCount == 0 || chainIndex >= detectedChainCount) {
        SetRebuildError("選択中の触手チェーンが範囲外です。");
        return false;
    }
    if (chainJoints.empty()) {
        SetRebuildError("触手チェーンのボーン数が不足しています。");
        return false;
    }
    if (!IsFinite(bindTipSkeletonPosition)) {
        SetRebuildError("先端のバインド位置が有限値ではありません。");
        return false;
    }
    for (int jointIndex : chainJoints) {
        if (jointIndex < 0 ||
            static_cast<std::size_t>(jointIndex) >= skeletonJointCount ||
            jointIndex == skeletonRootJointIndex) {
            SetRebuildError(
                "触手チェーンに無効なジョイント番号があります。");
            return false;
        }
    }

    chainJoints_ = chainJoints;
    bindTipSkeletonPosition_ = bindTipSkeletonPosition;
    hasBindTipPosition_ = true;
    std::vector<std::pair<int, int>> usedJointPairs;
    for (std::size_t segmentIndex = 0;
        segmentIndex < kNormalizedSegments.size();
        ++segmentIndex) {
        std::size_t startBone = FindNearestChainBone(
            kNormalizedSegments[segmentIndex][0],
            chainJoints.size());
        std::size_t endBone = FindNearestChainBone(
            kNormalizedSegments[segmentIndex][1],
            chainJoints.size());
        if (endBone <= startBone) {
            endBone = (std::min)(startBone + 1, chainJoints.size() - 1);
        }
        if (startBone >= chainJoints.size() || endBone <= startBone) {
            continue;
        }

        const std::pair<int, int> jointPair = {
            chainJoints[startBone], chainJoints[endBone] };
        if (std::find(
                usedJointPairs.begin(),
                usedJointPairs.end(),
                jointPair) != usedJointPairs.end()) {
            continue;
        }
        usedJointPairs.push_back(jointPair);

        KrakenBoneColliderPreview collider{};
        collider.colliderIndex =
            static_cast<std::uint32_t>(capsules_.size());
        collider.chainIndex = chainIndex;
        collider.role = segmentIndex + 1 == kNormalizedSegments.size()
            ? KrakenColliderPreviewRole::Attack
            : KrakenColliderPreviewRole::Damage;
        collider.startJointIndex = jointPair.first;
        collider.endJointIndex = jointPair.second;
        collider.normalizedStart = static_cast<float>(startBone) /
            static_cast<float>(chainJoints.size() - 1);
        collider.normalizedEnd = static_cast<float>(endBone) /
            static_cast<float>(chainJoints.size() - 1);
        collider.recommendedLocalRadius = kRecommendedRadii[segmentIndex];
        collider.localRadius = kRecommendedRadii[segmentIndex];
        capsules_.push_back(collider);
    }
    if (!capsules_.empty()) {
        capsules_.back().role = KrakenColliderPreviewRole::Attack;
    }
    tipSphere_.chainIndex = chainIndex;
    tipSphere_.role = KrakenColliderPreviewRole::WeakPoint;
    tipSphere_.tipJointIndex = chainJoints.back();
    tipSphere_.localRadius = 0.30f;
    diagnostics_.lastError.clear();
    diagnostics_.lastWarning.clear();
    if (capsules_.empty()) {
        diagnostics_.lastWarning = "安全に配置できるカプセルがないため、先端スフィアだけを表示します。";
    }
    diagnostics_.chainCount = detectedChainCount;
    diagnostics_.selectedChainIndex = chainIndex;
    diagnostics_.chainBoneCount =
        static_cast<std::uint32_t>(chainJoints.size());
    diagnostics_.capsuleCount =
        static_cast<std::uint32_t>(capsules_.size());
    diagnostics_.tipSphereCount = 1;
    return true;
}

void SkinningEditorKrakenBoneColliderPreview::Update(
    const Skeleton& skeleton,
    const Matrix4x4& previewWorldMatrix) {
    ++diagnostics_.currentPoseUpdateCount;
    const float previewScale = ExtractMaximumScale(previewWorldMatrix);
    const float globalRadiusScale = settings_.globalRadiusScale;
    for (KrakenBoneColliderPreview& collider : capsules_) {
        collider.valid = false;
        collider.worldStart = {};
        collider.worldEnd = {};
        collider.worldCenter = {};
        collider.worldDirection = { 0.0f, 1.0f, 0.0f };
        collider.worldLength = 0.0f;
        collider.worldRadius = collider.localRadius *
            collider.radiusScale * globalRadiusScale * previewScale;
        if (collider.startJointIndex < 0 ||
            collider.endJointIndex < 0 ||
            static_cast<std::size_t>(collider.startJointIndex) >=
                skeleton.joints.size() ||
            static_cast<std::size_t>(collider.endJointIndex) >=
                skeleton.joints.size()) {
            continue;
        }

        collider.worldStart = TransformPosition(
            skeleton.joints[static_cast<std::size_t>(
                collider.startJointIndex)].worldTranslate,
            previewWorldMatrix);
        collider.worldEnd = TransformPosition(
            skeleton.joints[static_cast<std::size_t>(
                collider.endJointIndex)].worldTranslate,
            previewWorldMatrix);
        collider.worldCenter = {
            (collider.worldStart.x + collider.worldEnd.x) * 0.5f,
            (collider.worldStart.y + collider.worldEnd.y) * 0.5f,
            (collider.worldStart.z + collider.worldEnd.z) * 0.5f,
        };
        const Vector3 difference = Subtract(
            collider.worldEnd,
            collider.worldStart);
        collider.worldLength = Length(difference);
        if (std::isfinite(collider.worldLength) &&
            collider.worldLength > kMinimumLength) {
            const float inverseLength = 1.0f / collider.worldLength;
            collider.worldDirection = {
                difference.x * inverseLength,
                difference.y * inverseLength,
                difference.z * inverseLength,
            };
        }
    }

    ++diagnostics_.tipSphereUpdateCount;
    tipSphere_.valid = false;
    tipSphere_.worldPosition = {};
    tipSphere_.bindWorldPosition = {};
    tipSphere_.distanceFromBind = 0.0f;
    tipSphere_.worldRadius = tipSphere_.localRadius *
        tipSphere_.radiusScale * globalRadiusScale * previewScale;
    if (tipSphere_.tipJointIndex >= 0 &&
        static_cast<std::size_t>(tipSphere_.tipJointIndex) <
            skeleton.joints.size() &&
        hasBindTipPosition_) {
        tipSphere_.worldPosition = TransformPosition(
            skeleton.joints[static_cast<std::size_t>(
                tipSphere_.tipJointIndex)].worldTranslate,
            previewWorldMatrix);
        tipSphere_.bindWorldPosition = TransformPosition(
            bindTipSkeletonPosition_,
            previewWorldMatrix);
        tipSphere_.distanceFromBind = Distance(
            tipSphere_.worldPosition,
            tipSphere_.bindWorldPosition);
    }
    RefreshDiagnostics(skeleton);
}

bool SkinningEditorKrakenBoneColliderPreview::MatchesChain(
    std::uint32_t chainIndex,
    const std::vector<int>& chainJoints) const {
    return selectedChainIndex_ == chainIndex &&
        chainJoints_ == chainJoints;
}

bool SkinningEditorKrakenBoneColliderPreview::SetCapsuleJointPair(
    std::size_t colliderIndex,
    int startJointIndex,
    int endJointIndex) {
    if (colliderIndex >= capsules_.size() ||
        startJointIndex == endJointIndex ||
        startJointIndex == skeletonRootJointIndex_ ||
        endJointIndex == skeletonRootJointIndex_) {
        diagnostics_.lastError =
            "開始と終了には異なる有効なボーンが必要です。";
        return false;
    }
    const auto startIt = std::find(
        chainJoints_.begin(), chainJoints_.end(), startJointIndex);
    const auto endIt = std::find(
        chainJoints_.begin(), chainJoints_.end(), endJointIndex);
    if (startIt == chainJoints_.end() || endIt == chainJoints_.end()) {
        diagnostics_.lastError =
            "選択中のチェーン以外のボーンは指定できません。";
        return false;
    }

    KrakenBoneColliderPreview& collider = capsules_[colliderIndex];
    collider.startJointIndex = startJointIndex;
    collider.endJointIndex = endJointIndex;
    const float denominator = static_cast<float>(chainJoints_.size() - 1);
    collider.normalizedStart = static_cast<float>(
        std::distance(chainJoints_.begin(), startIt)) / denominator;
    collider.normalizedEnd = static_cast<float>(
        std::distance(chainJoints_.begin(), endIt)) / denominator;
    diagnostics_.lastError.clear();
    return true;
}

void SkinningEditorKrakenBoneColliderPreview::ResetRecommendedRadii() {
    for (std::size_t index = 0; index < capsules_.size(); ++index) {
        capsules_[index].localRadius = capsules_[index].recommendedLocalRadius;
        capsules_[index].radiusScale = 1.0f;
    }
    tipSphere_.localRadius = 0.30f;
    tipSphere_.radiusScale = 1.0f;
    settings_.globalRadiusScale = 1.0f;
}

void SkinningEditorKrakenBoneColliderPreview::SetAllEnabled(bool enabled) {
    for (KrakenBoneColliderPreview& collider : capsules_) {
        collider.enabled = enabled;
    }
    tipSphere_.enabled = enabled;
}

void SkinningEditorKrakenBoneColliderPreview::SelectFirst() {
    selectedColliderIndex_ = 0;
}

void SkinningEditorKrakenBoneColliderPreview::SelectNext() {
    const std::size_t selectableCount = GetSelectableColliderCount();
    selectedColliderIndex_ = selectableCount > 0
        ? (selectedColliderIndex_ + 1) % selectableCount
        : 0;
}

void SkinningEditorKrakenBoneColliderPreview::SetSelectedColliderIndex(
    std::size_t colliderIndex) {
    const std::size_t selectableCount = GetSelectableColliderCount();
    selectedColliderIndex_ = selectableCount > 0
        ? (std::min)(colliderIndex, selectableCount - 1)
        : 0;
}

std::size_t
SkinningEditorKrakenBoneColliderPreview::GetSelectableColliderCount() const {
    return capsules_.size() + (tipSphere_.tipJointIndex >= 0 ? 1u : 0u);
}

bool SkinningEditorKrakenBoneColliderPreview::IsTipSphereSelected() const {
    return tipSphere_.tipJointIndex >= 0 &&
        selectedColliderIndex_ == capsules_.size();
}

void SkinningEditorKrakenBoneColliderPreview::SetRebuildError(
    const std::string& message) {
    diagnostics_ = {};
    diagnostics_.chainCount = detectedChainCount_;
    diagnostics_.selectedChainIndex = selectedChainIndex_;
    diagnostics_.lastError = message;
}

const char* GetKrakenColliderPreviewRoleJapaneseLabel(
    KrakenColliderPreviewRole role) {
    switch (role) {
    case KrakenColliderPreviewRole::Attack:
        return "攻撃";
    case KrakenColliderPreviewRole::Damage:
        return "ダメージ";
    case KrakenColliderPreviewRole::WeakPoint:
        return "弱点";
    default:
        return "不明";
    }
}
