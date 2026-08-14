#include "SkinningEditorKrakenMotionPreview.h"

#include "SkinningEditorKrakenAttackMotion.h"
#include "SkinningEditorKrakenBoneColliderPhaseControl.h"
#include "SkinningEditorKrakenBoneColliderPreviewCollection.h"
#include "Engine/Animation/Skeleton.h"
#include "Engine/Core/FrameTimer.h"

#include <algorithm>
#include <cmath>
#include <memory>

namespace {
    KrakenColliderPhaseMotionMode ToColliderMotionMode(
        SkinningEditorKrakenMotionPreview::Mode mode) {
        switch (mode) {
        case SkinningEditorKrakenMotionPreview::Mode::Manual:
            return KrakenColliderPhaseMotionMode::Manual;
        case SkinningEditorKrakenMotionPreview::Mode::IdleSway:
            return KrakenColliderPhaseMotionMode::IdleSway;
        case SkinningEditorKrakenMotionPreview::Mode::AttackSlamPreview:
            return KrakenColliderPhaseMotionMode::AttackSlamPreview;
        default:
            return static_cast<KrakenColliderPhaseMotionMode>(0xff);
        }
    }

    float NormalizePhaseTime(float elapsedTime, float duration) {
        if (!std::isfinite(elapsedTime) ||
            !std::isfinite(duration) ||
            duration <= 0.0f) {
            return 0.0f;
        }
        return std::clamp(elapsedTime / duration, 0.0f, 1.0f);
    }
}

SkinningEditorKrakenMotionPreview::~SkinningEditorKrakenMotionPreview() =
    default;

void SkinningEditorKrakenMotionPreview::InitializeBoneColliderPreview() {
    boneColliderPreview_ = std::make_unique<
        SkinningEditorKrakenBoneColliderPreviewCollection>();
    boneColliderPreview_->Reset();
    boneColliderPhaseControl_ = std::make_unique<
        SkinningEditorKrakenBoneColliderPhaseControl>();
    boneColliderPhaseControl_->Reset(*boneColliderPreview_);
}

void SkinningEditorKrakenMotionPreview::ClearBoneColliderPreview() {
    if (boneColliderPhaseControl_ && boneColliderPreview_) {
        boneColliderPhaseControl_->Finalize(*boneColliderPreview_);
    }
    if (boneColliderPreview_) {
        boneColliderPreview_->Clear();
    }
    boneColliderPhaseControl_.reset();
    boneColliderPreview_.reset();
}

std::size_t
SkinningEditorKrakenMotionPreview::ResolveBoneColliderChainIndex() const {
    if (chains_.empty()) {
        return 0;
    }
    std::size_t chainIndex = static_cast<std::size_t>((std::max)(
        selectedChainIndex_, 0));
    if (mode_ == Mode::AttackSlamPreview && attackMotion_) {
        chainIndex = attackMotion_->GetSelectedChainIndex();
    }
    return (std::min)(chainIndex, chains_.size() - 1);
}

void SkinningEditorKrakenMotionPreview::RefreshBoneColliderPreview(
    bool forceRebuild) {
    if (!boneColliderPreview_ || !boneColliderPhaseControl_) {
        return;
    }
    if (!skeleton_ ||
        chains_.empty() ||
        bindChainTipSkeletonPositions_.size() != chains_.size()) {
        RefreshBoneColliderPhaseControl();
        return;
    }

    std::vector<std::vector<int>> chainJoints;
    chainJoints.reserve(chains_.size());
    for (const Chain& chain : chains_) {
        chainJoints.push_back(chain.joints);
    }
    if (forceRebuild ||
        !boneColliderPreview_->MatchesChains(chainJoints)) {
        boneColliderPhaseControl_->Reset(*boneColliderPreview_);
        if (!boneColliderPreview_->Rebuild(
                chainJoints,
                skeleton_->root,
                skeleton_->joints.size(),
                bindChainTipSkeletonPositions_)) {
            RefreshBoneColliderPhaseControl();
            return;
        }
    }

    boneColliderPreview_->SetDisplayChainIndex(
        ResolveBoneColliderChainIndex());
    boneColliderPreview_->Update(*skeleton_, previewWorldMatrix_);
    RefreshBoneColliderPhaseControl();
}

void SkinningEditorKrakenMotionPreview::
RefreshBoneColliderPhaseControl() {
    if (!boneColliderPreview_ || !boneColliderPhaseControl_) {
        return;
    }

    boneColliderPreview_->SetDisplayChainIndex(
        ResolveBoneColliderChainIndex());
    KrakenColliderPhaseMotionSnapshot snapshot{};
    snapshot.motionMode = ToColliderMotionMode(mode_);
    snapshot.connected = skeleton_ &&
        targetCompatible_ &&
        boneColliderPreview_->IsConnected();
    snapshot.safetyRecovery = recovering_;
    if (attackMotion_) {
        snapshot.phase = attackMotion_->GetPhase();
        snapshot.selectedChainIndex =
            attackMotion_->GetSelectedChainIndex();
        snapshot.motionElapsedTime = attackMotion_->GetElapsedTime();
        snapshot.phaseElapsedTime =
            attackMotion_->GetPhaseElapsedTime();
        snapshot.phaseDuration =
            attackMotion_->GetPhaseDuration(snapshot.phase);
        snapshot.phaseNormalizedTime = NormalizePhaseTime(
            snapshot.phaseElapsedTime,
            snapshot.phaseDuration);
        snapshot.slamDuration =
            attackMotion_->GetSettings().slamDuration;
        snapshot.playing = attackMotion_->IsPlaying();
        snapshot.paused = attackMotion_->IsPaused();
        snapshot.loop = attackMotion_->IsLoopEnabled();
        snapshot.waitingForLoop = attackMotion_->IsWaitingForLoop();
        snapshot.valid = snapshot.connected && hierarchyValid_ &&
            std::isfinite(snapshot.motionElapsedTime) &&
            snapshot.motionElapsedTime >= 0.0f &&
            std::isfinite(snapshot.phaseElapsedTime) &&
            snapshot.phaseElapsedTime >= 0.0f &&
            std::isfinite(snapshot.phaseDuration) &&
            snapshot.phaseDuration >= 0.0f &&
            std::isfinite(snapshot.phaseNormalizedTime);
    }
    boneColliderPhaseControl_->Evaluate(
        *boneColliderPreview_,
        snapshot,
        FrameTimer::GetInstance().GetFrameIndex());
}

bool SkinningEditorKrakenMotionPreview::
ShouldDrawBoneColliderDebugOverlay() const {
    return IsTarget(skeleton_) &&
        boneColliderPreview_ &&
        boneColliderPreview_->GetSettings().showPreview;
}
