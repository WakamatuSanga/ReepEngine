#include "SkinningEditorKrakenBoneColliderPreviewCollection.h"

#include "Engine/Animation/Skeleton.h"

#include <algorithm>
#include <limits>

SkinningEditorKrakenBoneColliderPreviewCollection::
~SkinningEditorKrakenBoneColliderPreviewCollection() = default;

void SkinningEditorKrakenBoneColliderPreviewCollection::Reset() {
    Clear();
    settings_ = {};
}

void SkinningEditorKrakenBoneColliderPreviewCollection::Clear() {
    for (const auto& preview : chainPreviews_) {
        if (!preview) {
            continue;
        }
        for (KrakenBoneColliderPreview& collider :
            preview->GetCapsules()) {
            collider.phaseActive = false;
            collider.gameplayRegistered = false;
        }
        KrakenTipSphereColliderPreview& tip = preview->GetTipSphere();
        tip.phaseActive = false;
        tip.gameplayRegistered = false;
    }
    chainPreviews_.clear();
    diagnostics_ = {};
    displayChainIndex_ = 0;
    selectedShapeChainIndex_ = 0;
    selectedLocalIndex_ = 0;
    selectedShapeIsTip_ = false;
    connected_ = false;
}

bool SkinningEditorKrakenBoneColliderPreviewCollection::Rebuild(
    const std::vector<std::vector<int>>& chainJoints,
    int skeletonRootJointIndex,
    std::size_t skeletonJointCount,
    const std::vector<Vector3>& bindTipSkeletonPositions) {
    connected_ = false;
    if (chainJoints.empty() ||
        bindTipSkeletonPositions.size() != chainJoints.size() ||
        chainJoints.size() >
            static_cast<std::size_t>((std::numeric_limits<
                std::uint32_t>::max)())) {
        Clear();
        diagnostics_.lastError =
            "全触手チェーンのCollider再構築情報が不正です。";
        return false;
    }

    std::vector<std::unique_ptr<
        SkinningEditorKrakenBoneColliderPreview>> rebuilt;
    rebuilt.reserve(chainJoints.size());
    const std::uint32_t chainCount =
        static_cast<std::uint32_t>(chainJoints.size());
    for (std::size_t chainIndex = 0;
        chainIndex < chainJoints.size();
        ++chainIndex) {
        auto preview = std::make_unique<
            SkinningEditorKrakenBoneColliderPreview>();
        preview->Reset();
        preview->GetSettings().globalRadiusScale =
            settings_.globalRadiusScale;
        if (!preview->Rebuild(
                chainCount,
                static_cast<std::uint32_t>(chainIndex),
                chainJoints[chainIndex],
                skeletonRootJointIndex,
                skeletonJointCount,
                bindTipSkeletonPositions[chainIndex])) {
            const std::string error =
                preview->GetDiagnostics().lastError;
            Clear();
            diagnostics_.lastError = error.empty()
                ? "触手Colliderの再構築に失敗しました。"
                : error;
            return false;
        }
        rebuilt.push_back(std::move(preview));
    }

    chainPreviews_ = std::move(rebuilt);
    connected_ = true;
    displayChainIndex_ = (std::min)(
        displayChainIndex_, chainPreviews_.size() - 1);
    ClampSelection();
    RefreshAggregatedDiagnostics();
    return true;
}

void SkinningEditorKrakenBoneColliderPreviewCollection::Update(
    const Skeleton& skeleton,
    const Matrix4x4& previewWorldMatrix) {
    for (const auto& preview : chainPreviews_) {
        if (!preview) {
            continue;
        }
        preview->GetSettings().globalRadiusScale =
            settings_.globalRadiusScale;
        preview->Update(skeleton, previewWorldMatrix);
    }
    RefreshAggregatedDiagnostics();
}

bool SkinningEditorKrakenBoneColliderPreviewCollection::MatchesChains(
    const std::vector<std::vector<int>>& chainJoints) const {
    if (!connected_ || chainJoints.size() != chainPreviews_.size()) {
        return false;
    }
    for (std::size_t chainIndex = 0;
        chainIndex < chainJoints.size();
        ++chainIndex) {
        if (!chainPreviews_[chainIndex] ||
            !chainPreviews_[chainIndex]->MatchesChain(
                static_cast<std::uint32_t>(chainIndex),
                chainJoints[chainIndex])) {
            return false;
        }
    }
    return true;
}

void SkinningEditorKrakenBoneColliderPreviewCollection::
SetDisplayChainIndex(std::size_t chainIndex) {
    displayChainIndex_ = chainPreviews_.empty()
        ? 0
        : (std::min)(chainIndex, chainPreviews_.size() - 1);
    RefreshAggregatedDiagnostics();
}

void SkinningEditorKrakenBoneColliderPreviewCollection::
ResetRecommendedRadii() {
    settings_.globalRadiusScale = 1.0f;
    for (const auto& preview : chainPreviews_) {
        if (preview) {
            preview->ResetRecommendedRadii();
        }
    }
}

void SkinningEditorKrakenBoneColliderPreviewCollection::SetAllEnabled(
    bool enabled) {
    for (const auto& preview : chainPreviews_) {
        if (preview) {
            preview->SetAllEnabled(enabled);
        }
    }
}

void SkinningEditorKrakenBoneColliderPreviewCollection::RunDiagnostics(
    const Skeleton& skeleton) {
    for (const auto& preview : chainPreviews_) {
        if (preview) {
            preview->RunDiagnostics(skeleton);
        }
    }
    RefreshAggregatedDiagnostics();
}

void SkinningEditorKrakenBoneColliderPreviewCollection::
ResetDiagnostics() {
    for (const auto& preview : chainPreviews_) {
        if (preview) {
            preview->ResetDiagnostics();
        }
    }
    diagnostics_ = {};
    RefreshAggregatedDiagnostics();
}

void SkinningEditorKrakenBoneColliderPreviewCollection::RecordDebugDraw(
    std::uint32_t shapeCount) const {
    diagnostics_.lastDebugDrawShapeCount = shapeCount;
    ++diagnostics_.debugDrawCount;
}

void SkinningEditorKrakenBoneColliderPreviewCollection::
SelectFirstForDisplayChain() {
    selectedShapeChainIndex_ = displayChainIndex_;
    selectedLocalIndex_ = 0;
    selectedShapeIsTip_ = false;
    ClampSelection();
}

void SkinningEditorKrakenBoneColliderPreviewCollection::SelectNext() {
    struct Choice {
        std::size_t chainIndex = 0;
        std::size_t localIndex = 0;
        bool tip = false;
    };
    std::vector<Choice> choices;
    for (std::size_t chainIndex = 0;
        chainIndex < chainPreviews_.size();
        ++chainIndex) {
        if (!settings_.showAllChains &&
            chainIndex != displayChainIndex_) {
            continue;
        }
        const auto& preview = chainPreviews_[chainIndex];
        if (!preview) {
            continue;
        }
        for (std::size_t localIndex = 0;
            localIndex < preview->GetCapsules().size();
            ++localIndex) {
            choices.push_back({ chainIndex, localIndex, false });
        }
        if (preview->GetTipSphere().tipJointIndex >= 0) {
            choices.push_back({ chainIndex, 0, true });
        }
    }
    if (choices.empty()) {
        return;
    }
    const auto current = std::find_if(
        choices.begin(),
        choices.end(),
        [&](const Choice& choice) {
            return IsSelected(
                choice.chainIndex,
                choice.localIndex,
                choice.tip);
        });
    const std::size_t nextIndex = current == choices.end()
        ? 0
        : (static_cast<std::size_t>(
            std::distance(choices.begin(), current)) + 1) % choices.size();
    Select(
        choices[nextIndex].chainIndex,
        choices[nextIndex].localIndex,
        choices[nextIndex].tip);
}

void SkinningEditorKrakenBoneColliderPreviewCollection::Select(
    std::size_t chainIndex,
    std::size_t localIndex,
    bool tip) {
    selectedShapeChainIndex_ = chainIndex;
    selectedLocalIndex_ = localIndex;
    selectedShapeIsTip_ = tip;
    ClampSelection();
}

bool SkinningEditorKrakenBoneColliderPreviewCollection::IsSelected(
    std::size_t chainIndex,
    std::size_t localIndex,
    bool tip) const {
    return selectedShapeChainIndex_ == chainIndex &&
        selectedLocalIndex_ == localIndex &&
        selectedShapeIsTip_ == tip;
}

void SkinningEditorKrakenBoneColliderPreviewCollection::
ResynchronizeSelection() {
    SelectFirstForDisplayChain();
}

KrakenBoneColliderPreview*
SkinningEditorKrakenBoneColliderPreviewCollection::GetSelectedCapsule() {
    return const_cast<KrakenBoneColliderPreview*>(
        static_cast<const
            SkinningEditorKrakenBoneColliderPreviewCollection*>(this)
            ->GetSelectedCapsule());
}

const KrakenBoneColliderPreview*
SkinningEditorKrakenBoneColliderPreviewCollection::
GetSelectedCapsule() const {
    if (selectedShapeIsTip_ ||
        selectedShapeChainIndex_ >= chainPreviews_.size() ||
        !chainPreviews_[selectedShapeChainIndex_]) {
        return nullptr;
    }
    const auto& capsules =
        chainPreviews_[selectedShapeChainIndex_]->GetCapsules();
    return selectedLocalIndex_ < capsules.size()
        ? &capsules[selectedLocalIndex_]
        : nullptr;
}

KrakenTipSphereColliderPreview*
SkinningEditorKrakenBoneColliderPreviewCollection::
GetSelectedTipSphere() {
    return const_cast<KrakenTipSphereColliderPreview*>(
        static_cast<const
            SkinningEditorKrakenBoneColliderPreviewCollection*>(this)
            ->GetSelectedTipSphere());
}

const KrakenTipSphereColliderPreview*
SkinningEditorKrakenBoneColliderPreviewCollection::
GetSelectedTipSphere() const {
    if (!selectedShapeIsTip_ ||
        selectedShapeChainIndex_ >= chainPreviews_.size() ||
        !chainPreviews_[selectedShapeChainIndex_]) {
        return nullptr;
    }
    const KrakenTipSphereColliderPreview& tip =
        chainPreviews_[selectedShapeChainIndex_]->GetTipSphere();
    return tip.tipJointIndex >= 0 ? &tip : nullptr;
}

void SkinningEditorKrakenBoneColliderPreviewCollection::ClampSelection() {
    if (chainPreviews_.empty()) {
        selectedShapeChainIndex_ = 0;
        selectedLocalIndex_ = 0;
        selectedShapeIsTip_ = false;
        return;
    }
    selectedShapeChainIndex_ = (std::min)(
        selectedShapeChainIndex_, chainPreviews_.size() - 1);
    const auto& preview = chainPreviews_[selectedShapeChainIndex_];
    if (!preview) {
        selectedLocalIndex_ = 0;
        selectedShapeIsTip_ = false;
        return;
    }
    if (selectedShapeIsTip_ &&
        preview->GetTipSphere().tipJointIndex >= 0) {
        selectedLocalIndex_ = 0;
        return;
    }
    selectedShapeIsTip_ = false;
    if (!preview->GetCapsules().empty()) {
        selectedLocalIndex_ = (std::min)(
            selectedLocalIndex_, preview->GetCapsules().size() - 1);
    } else {
        selectedLocalIndex_ = 0;
        selectedShapeIsTip_ =
            preview->GetTipSphere().tipJointIndex >= 0;
    }
}

void SkinningEditorKrakenBoneColliderPreviewCollection::
RefreshAggregatedDiagnostics() {
    const std::uint64_t debugDrawCount = diagnostics_.debugDrawCount;
    const std::uint32_t lastDrawCount =
        diagnostics_.lastDebugDrawShapeCount;
    diagnostics_ = {};
    diagnostics_.debugDrawCount = debugDrawCount;
    diagnostics_.lastDebugDrawShapeCount = lastDrawCount;
    diagnostics_.chainCount =
        static_cast<std::uint32_t>(chainPreviews_.size());
    diagnostics_.selectedChainIndex =
        static_cast<std::uint32_t>(displayChainIndex_);
    diagnostics_.currentPoseFollowing = connected_ &&
        !chainPreviews_.empty();
    for (std::size_t chainIndex = 0;
        chainIndex < chainPreviews_.size();
        ++chainIndex) {
        const auto& preview = chainPreviews_[chainIndex];
        if (!preview) {
            diagnostics_.currentPoseFollowing = false;
            continue;
        }
        const KrakenBoneColliderPreviewDiagnostics& child =
            preview->GetDiagnostics();
        if (chainIndex == displayChainIndex_) {
            diagnostics_.chainBoneCount = child.chainBoneCount;
        }
        diagnostics_.capsuleCount += child.capsuleCount;
        diagnostics_.tipSphereCount += child.tipSphereCount;
        diagnostics_.enabledColliderCount += child.enabledColliderCount;
        diagnostics_.disabledColliderCount += child.disabledColliderCount;
        diagnostics_.invalidJointIndexCount += child.invalidJointIndexCount;
        diagnostics_.zeroLengthCapsuleCount += child.zeroLengthCapsuleCount;
        diagnostics_.nonFinitePositionCount += child.nonFinitePositionCount;
        diagnostics_.nonFiniteRadiusCount += child.nonFiniteRadiusCount;
        diagnostics_.nonPositiveRadiusCount += child.nonPositiveRadiusCount;
        diagnostics_.currentPoseUpdateCount = (std::max)(
            diagnostics_.currentPoseUpdateCount,
            child.currentPoseUpdateCount);
        diagnostics_.tipSphereUpdateCount = (std::max)(
            diagnostics_.tipSphereUpdateCount,
            child.tipSphereUpdateCount);
        diagnostics_.currentPoseFollowing =
            diagnostics_.currentPoseFollowing &&
            child.currentPoseFollowing;
        if (diagnostics_.lastError.empty() &&
            !child.lastError.empty()) {
            diagnostics_.lastError = child.lastError;
        }
        if (diagnostics_.lastWarning.empty() &&
            !child.lastWarning.empty()) {
            diagnostics_.lastWarning = child.lastWarning;
        }
    }
}
