#include "SkinningEditorKrakenBoneColliderPreview.h"

#include "Engine/Animation/Skeleton.h"

#include <algorithm>

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

    const KrakenTentacleColliderDefinitionResult definitions =
        BuildKrakenTentacleColliderDefinitions(
            detectedChainCount,
            chainIndex,
            chainJoints,
            skeletonRootJointIndex,
            skeletonJointCount,
            bindTipSkeletonPosition);
    if (!definitions.valid) {
        SetRebuildError(
            GetKrakenTentacleColliderDefinitionErrorJapaneseLabel(
                definitions.error));
        return false;
    }

    chainJoints_ = chainJoints;
    bindTipSkeletonPosition_ = bindTipSkeletonPosition;
    hasBindTipPosition_ = true;
    capsules_.reserve(definitions.capsules.size());
    for (const KrakenTentacleCapsuleColliderDefinition& definition :
        definitions.capsules) {
        KrakenBoneColliderPreview collider{};
        collider.colliderIndex = definition.colliderIndex;
        collider.chainIndex = definition.chainIndex;
        collider.role = definition.role;
        collider.startJointIndex = definition.startJointIndex;
        collider.endJointIndex = definition.endJointIndex;
        collider.normalizedStart = definition.normalizedStart;
        collider.normalizedEnd = definition.normalizedEnd;
        collider.recommendedLocalRadius =
            definition.recommendedLocalRadius;
        collider.localRadius = definition.recommendedLocalRadius;
        capsules_.push_back(collider);
    }
    tipSphere_.chainIndex = definitions.tipSphere.chainIndex;
    tipSphere_.role = definitions.tipSphere.role;
    tipSphere_.tipJointIndex = definitions.tipSphere.tipJointIndex;
    tipSphere_.localRadius =
        definitions.tipSphere.recommendedLocalRadius;
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
    const float globalRadiusScale = settings_.globalRadiusScale;
    for (KrakenBoneColliderPreview& collider : capsules_) {
        const KrakenTentacleCapsuleColliderEvaluation evaluation =
            EvaluateKrakenTentacleCapsuleCollider(
                skeleton,
                previewWorldMatrix,
                skeletonRootJointIndex_,
                collider.startJointIndex,
                collider.endJointIndex,
                collider.localRadius,
                collider.radiusScale,
                globalRadiusScale);
        collider.worldStart = evaluation.worldStart;
        collider.worldEnd = evaluation.worldEnd;
        collider.worldCenter = evaluation.worldCenter;
        collider.worldDirection = evaluation.worldDirection;
        collider.worldLength = evaluation.worldLength;
        collider.worldRadius = evaluation.worldRadius;
        collider.valid = evaluation.valid;
    }

    ++diagnostics_.tipSphereUpdateCount;
    const KrakenTentacleTipSphereColliderEvaluation tipEvaluation =
        EvaluateKrakenTentacleTipSphereCollider(
            skeleton,
            previewWorldMatrix,
            skeletonRootJointIndex_,
            tipSphere_.tipJointIndex,
            bindTipSkeletonPosition_,
            hasBindTipPosition_,
            tipSphere_.localRadius,
            tipSphere_.radiusScale,
            globalRadiusScale);
    tipSphere_.worldPosition = tipEvaluation.worldPosition;
    tipSphere_.bindWorldPosition = tipEvaluation.bindWorldPosition;
    tipSphere_.distanceFromBind = tipEvaluation.distanceFromBind;
    tipSphere_.worldRadius = tipEvaluation.worldRadius;
    tipSphere_.valid = tipEvaluation.valid;
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
