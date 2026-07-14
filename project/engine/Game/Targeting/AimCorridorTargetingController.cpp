#include "AimCorridorTargetingController.h"

#include "AimCorridorTargetMarkerRenderer.h"
#include "Engine/Game/UI/AimCorridorVisualController.h"

#include <algorithm>
#include <cmath>

AimCorridorTargetingController::AimCorridorTargetingController() = default;
AimCorridorTargetingController::~AimCorridorTargetingController() = default;

bool AimCorridorTargetingController::Initialize(
    DirectXCommon* dxCommon,
    EnemyManager* enemyManager,
    Camera* camera,
    AimCorridorVisualController* visualController) {
    Finalize();
    dxCommon_ = dxCommon;
    enemyManager_ = enemyManager;
    camera_ = camera;
    visualController_ = visualController;
    initialized_ = dxCommon_ && enemyManager_ && camera_ && visualController_;
    if (!initialized_) {
        return false;
    }
    markerRenderer_ = std::make_unique<AimCorridorTargetMarkerRenderer>();
    markerRenderer_->Initialize(dxCommon_, camera_);
    Reset();
    return true;
}

void AimCorridorTargetingController::Finalize() {
    Reset();
    if (markerRenderer_) {
        markerRenderer_->Finalize();
    }
    markerRenderer_.reset();
    projectedTargets_.clear();
    visualController_ = nullptr;
    camera_ = nullptr;
    enemyManager_ = nullptr;
    dxCommon_ = nullptr;
    initialized_ = false;
}

void AimCorridorTargetingController::Reset() {
    candidateTargetId_.clear();
    lockedTargetId_.clear();
    bestCandidateId_.clear();
    currentTarget_ = {};
    currentTargetValid_ = false;
    lockedTargetWorldPosition_ = {};
    lockedTargetAimPosition_ = {};
    lockState_ = AimLockState::None;
    currentCandidateScore_ = 0.0f;
    bestCandidateScore_ = 0.0f;
    lockElapsed_ = 0.0f;
    lockProgress_ = 0.0f;
    targetHoldElapsed_ = 0.0f;
    breakGraceElapsed_ = 0.0f;
    candidateCount_ = 0;
    lockCompletedCount_ = 0;
    lockBreakCount_ = 0;
    visibleRect_ = {};
    softRect_ = {};
    projectedTargets_.clear();
    lastSwitchReason_ = "状態をリセット";
    debugForcedState_ = -1;
    if (markerRenderer_) {
        markerRenderer_->Reset();
    }
    if (visualController_) {
        visualController_->SetTargetingVisualState(
            AimCorridorVisualController::AimReticleVisualState::Normal, 0.0f);
    }
}

void AimCorridorTargetingController::SetGameModeActive(bool active) {
    if (gameModeActive_ == active) {
        return;
    }
    Reset();
    gameModeActive_ = active;
}

void AimCorridorTargetingController::SetPlayerAlive(bool alive) {
    if (playerAlive_ == alive) {
        return;
    }
    Reset();
    playerAlive_ = alive;
}

void AimCorridorTargetingController::Update(float scaledDeltaTime, float unscaledDeltaTime) {
    const float safeScaledDeltaTime = std::clamp(
        std::isfinite(scaledDeltaTime) ? scaledDeltaTime : 0.0f, 0.0f, 0.1f);
    const float safeUnscaledDeltaTime = std::clamp(
        std::isfinite(unscaledDeltaTime) ? unscaledDeltaTime : 0.0f, 0.0f, 0.1f);
    ClampParameters();

    const bool canTarget = initialized_ && enabled_ && gameModeActive_ && playerAlive_
        && enemyManager_ && camera_ && visualController_
        && visualController_->IsVisible() && visualController_->IsGameModeActive();
    if (!canTarget) {
        if (HasCandidate() || HasLockedTarget()) {
            ClearTarget(false, "照準機能が無効");
        }
        projectedTargets_.clear();
        visibleRect_ = {};
        softRect_ = {};
        candidateCount_ = 0;
        currentTargetValid_ = false;
        PublishVisualState(safeUnscaledDeltaTime);
        return;
    }

    ProjectTargets();
    if (!visibleRect_.valid) {
        ClearTarget(false, "メイン照準矩形が無効");
    } else {
        UpdateSelection(safeScaledDeltaTime);
    }
    PublishVisualState(safeUnscaledDeltaTime);
}

void AimCorridorTargetingController::Draw() {
    if (markerRenderer_) {
        markerRenderer_->Draw();
    }
}

void AimCorridorTargetingController::ClampParameters() {
    softAssistScale_ = std::clamp(softAssistScale_, 1.0f, 2.5f);
    fallbackWorldRadius_ = std::clamp(fallbackWorldRadius_, 0.01f, 100.0f);
    minimumScreenRadius_ = std::clamp(minimumScreenRadius_, 0.001f, 0.1f);
    maximumScreenRadius_ = std::clamp(maximumScreenRadius_, minimumScreenRadius_, 0.5f);
    minimumTargetDepth_ = std::clamp(minimumTargetDepth_, 0.01f, 1000.0f);
    maximumTargetDepth_ = std::clamp(maximumTargetDepth_, minimumTargetDepth_ + 0.01f, 5000.0f);
    depthScoreWeight_ = std::clamp(depthScoreWeight_, 0.0f, 1.0f);
    visibleRectBonus_ = std::clamp(visibleRectBonus_, 0.0f, 1.0f);
    targetHoldTime_ = std::clamp(targetHoldTime_, 0.0f, 2.0f);
    targetSwitchMargin_ = std::clamp(targetSwitchMargin_, 0.0f, 1.0f);
    lockAcquireTime_ = std::clamp(lockAcquireTime_, 0.2f, 3.0f);
    lockBreakGraceTime_ = std::clamp(lockBreakGraceTime_, 0.0f, 2.0f);
    maximumCandidateCount_ = std::clamp(maximumCandidateCount_, 1, 32);
}
