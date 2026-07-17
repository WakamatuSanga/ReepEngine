#include "AimCorridorTargetingController.h"

#include "AimCorridorTargetMarkerRenderer.h"
#include "Engine/Game/UI/AimCorridorVisualController.h"

#include <algorithm>

void AimCorridorTargetingController::SwitchCandidate(
    const ProjectedTarget& target,
    const char* reason) {
    candidateTargetId_ = target.runtimeId;
    lockedTargetId_.clear();
    currentTarget_ = target;
    currentTargetValid_ = true;
    lockState_ = AimLockState::Candidate;
    currentCandidateScore_ = target.score;
    lockElapsed_ = 0.0f;
    lockProgress_ = 0.0f;
    targetHoldElapsed_ = 0.0f;
    breakGraceElapsed_ = 0.0f;
    lastSwitchReason_ = reason ? reason : "候補を変更";
}

void AimCorridorTargetingController::ClearTarget(
    bool countLockBreak,
    const char* reason) {
    if (countLockBreak && lockState_ == AimLockState::Locked) {
        ++lockBreakCount_;
    }
    candidateTargetId_.clear();
    lockedTargetId_.clear();
    currentTarget_ = {};
    currentTargetValid_ = false;
    lockedTargetWorldPosition_ = {};
    lockedTargetAimPosition_ = {};
    lockState_ = AimLockState::None;
    currentCandidateScore_ = 0.0f;
    lockElapsed_ = 0.0f;
    lockProgress_ = 0.0f;
    targetHoldElapsed_ = 0.0f;
    breakGraceElapsed_ = 0.0f;
    lastSwitchReason_ = reason ? reason : "対象を解除";
}

void AimCorridorTargetingController::UpdateSelection(float scaledDeltaTime) {
    const ProjectedTarget* current = FindProjectedTarget(candidateTargetId_);
    if (HasCandidate() && !current) {
        ClearTarget(true, "対象が死亡・削除・カメラ背後");
    }

    current = FindProjectedTarget(candidateTargetId_);
    if (current) {
        currentTarget_ = *current;
        currentTargetValid_ = true;
        currentCandidateScore_ = current->score;
        if (lockState_ == AimLockState::Locked) {
            lockedTargetWorldPosition_ = current->worldPosition;
            lockedTargetAimPosition_ = current->worldPosition;
        }
        if (!current->overlapsSoftRect) {
            breakGraceElapsed_ += scaledDeltaTime;
            if (breakGraceElapsed_ > lockBreakGraceTime_) {
                ClearTarget(true, "補助捕捉範囲外の猶予を超過");
            } else {
                lastSwitchReason_ = "補助捕捉範囲外・解除猶予中";
                return;
            }
        } else {
            breakGraceElapsed_ = 0.0f;
        }
    }

    current = FindProjectedTarget(candidateTargetId_);
    if (!current) {
        if (const ProjectedTarget* best = FindBestCandidate()) {
            SwitchCandidate(*best, "最良候補を選択");
        }
        return;
    }

    if (lockState_ == AimLockState::Locked) {
        lockedTargetId_ = current->runtimeId;
        lockedTargetWorldPosition_ = current->worldPosition;
        lockedTargetAimPosition_ = current->worldPosition;
        lockProgress_ = 1.0f;
        return;
    }

    targetHoldElapsed_ += scaledDeltaTime;
    const ProjectedTarget* best = FindBestCandidate();
    if (best && best->runtimeId != current->runtimeId
        && targetHoldElapsed_ >= targetHoldTime_
        && best->score + targetSwitchMargin_ < current->score) {
        SwitchCandidate(*best, "切替余裕値を満たす候補へ変更");
        return;
    }

    if (lockState_ == AimLockState::Candidate) {
        lockState_ = AimLockState::Acquiring;
    }
    if (lockState_ == AimLockState::Acquiring) {
        lockElapsed_ += scaledDeltaTime;
        lockProgress_ = std::clamp(lockElapsed_ / lockAcquireTime_, 0.0f, 1.0f);
        if (lockElapsed_ >= lockAcquireTime_) {
            lockState_ = AimLockState::Locked;
            lockedTargetId_ = current->runtimeId;
            lockedTargetWorldPosition_ = current->worldPosition;
            lockedTargetAimPosition_ = current->worldPosition;
            lockProgress_ = 1.0f;
            ++lockCompletedCount_;
            lastSwitchReason_ = "ロック取得完了";
        }
    }
}

void AimCorridorTargetingController::PublishVisualState(float unscaledDeltaTime) {
    AimCorridorVisualController::AimReticleVisualState visualState =
        AimCorridorVisualController::AimReticleVisualState::Normal;
    float visualProgress = lockProgress_;
    AimLockState publishedLockState = lockState_;
    if (debugForcedState_ >= 0) {
        publishedLockState = static_cast<AimLockState>(std::clamp(debugForcedState_, 0, 3));
        if (publishedLockState == AimLockState::Acquiring) {
            visualProgress = 0.5f;
        } else if (publishedLockState == AimLockState::Locked) {
            visualProgress = 1.0f;
        }
    }
    switch (publishedLockState) {
    case AimLockState::Candidate:
        visualState = AimCorridorVisualController::AimReticleVisualState::CandidatePreview;
        break;
    case AimLockState::Acquiring:
        visualState = AimCorridorVisualController::AimReticleVisualState::AcquiringPreview;
        break;
    case AimLockState::Locked:
        visualState = AimCorridorVisualController::AimReticleVisualState::LockedPreview;
        break;
    case AimLockState::None:
    default:
        break;
    }
    if (visualController_) {
        visualController_->SetTargetingVisualState(visualState, visualProgress);
    }

    const bool markerVisible = gameModeActive_ && playerAlive_ && currentTargetValid_
        && publishedLockState == AimLockState::Locked;
    if (markerRenderer_) {
        markerRenderer_->Update(
            unscaledDeltaTime,
            markerVisible,
            currentTarget_.worldPosition,
            currentTarget_.screenRadius,
            currentTarget_.cameraDepth);
    }
}
