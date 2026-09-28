#include "AimCorridorTargetingController.h"

#include "Engine/Game/Targeting/PlayerLockOnTargetProvider.h"
#include "Engine/Game/UI/AimCorridorVisualController.h"
#include "Engine/Graphics/Camera/Camera.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {
    Vector3 Add(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 Subtract(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    }

    Vector3 Scale(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    float Dot(const Vector3& lhs, const Vector3& rhs) {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
    }

    Vector3 NormalizeOr(const Vector3& value, const Vector3& fallback) {
        const float lengthSquared = Dot(value, value);
        if (!std::isfinite(lengthSquared) || lengthSquared <= 0.000001f) {
            return fallback;
        }
        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        return Scale(value, inverseLength);
    }
}

bool AimCorridorTargetingController::ProjectWorldToScreen(
    const Vector3& worldPosition,
    Vector2& screenUv,
    float& clipW) const {
    screenUv = {};
    clipW = 0.0f;
    if (!camera_) {
        return false;
    }
    const Matrix4x4& viewProjection = camera_->GetViewProjectionMatrix();
    const float clipX = worldPosition.x * viewProjection.m[0][0]
        + worldPosition.y * viewProjection.m[1][0]
        + worldPosition.z * viewProjection.m[2][0] + viewProjection.m[3][0];
    const float clipY = worldPosition.x * viewProjection.m[0][1]
        + worldPosition.y * viewProjection.m[1][1]
        + worldPosition.z * viewProjection.m[2][1] + viewProjection.m[3][1];
    clipW = worldPosition.x * viewProjection.m[0][3]
        + worldPosition.y * viewProjection.m[1][3]
        + worldPosition.z * viewProjection.m[2][3] + viewProjection.m[3][3];
    if (!std::isfinite(clipW) || clipW <= 0.0001f) {
        return false;
    }
    const float ndcX = clipX / clipW;
    const float ndcY = clipY / clipW;
    if (!std::isfinite(ndcX) || !std::isfinite(ndcY)) {
        return false;
    }
    screenUv = { ndcX * 0.5f + 0.5f, -ndcY * 0.5f + 0.5f };
    return std::isfinite(screenUv.x) && std::isfinite(screenUv.y);
}

bool AimCorridorTargetingController::RectsOverlap(
    const Vector2& lhsMinimum,
    const Vector2& lhsMaximum,
    const Vector2& rhsMinimum,
    const Vector2& rhsMaximum) {
    return lhsMinimum.x <= rhsMaximum.x && lhsMaximum.x >= rhsMinimum.x
        && lhsMinimum.y <= rhsMaximum.y && lhsMaximum.y >= rhsMinimum.y;
}

void AimCorridorTargetingController::ProjectTargets() {
    projectedTargets_.clear();
    krakenNaturalLockDiagnostics_.clear();
    visibleRect_ = {};
    softRect_ = {};
    candidateCount_ = 0;
    bestCandidateId_.clear();
    bestCandidateScore_ = 0.0f;
    currentTargetValid_ = false;
    if (!targetProvider_ || !camera_ || !visualController_) {
        return;
    }

    const AimReticleScreenRect& reticleRect = visualController_->GetMainReticleScreenRect();
    if (!reticleRect.valid) {
        return;
    }
    visibleRect_ = {
        reticleRect.centerUV,
        reticleRect.halfSizeUV,
        reticleRect.minUV,
        reticleRect.maxUV,
        true,
    };
    softRect_.center = visibleRect_.center;
    softRect_.halfSize = {
        visibleRect_.halfSize.x * softAssistScale_,
        visibleRect_.halfSize.y * softAssistScale_,
    };
    softRect_.minimum = {
        softRect_.center.x - softRect_.halfSize.x,
        softRect_.center.y - softRect_.halfSize.y,
    };
    softRect_.maximum = {
        softRect_.center.x + softRect_.halfSize.x,
        softRect_.center.y + softRect_.halfSize.y,
    };
    softRect_.valid = true;

    const Matrix4x4& cameraWorld = camera_->GetWorldMatrix();
    const Vector3 cameraRight = NormalizeOr(
        { cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2] },
        { 1.0f, 0.0f, 0.0f });
    const Vector3 cameraUp = NormalizeOr(
        { cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2] },
        { 0.0f, 1.0f, 0.0f });
    const Vector3 cameraForward = NormalizeOr(
        { cameraWorld.m[2][0], cameraWorld.m[2][1], cameraWorld.m[2][2] },
        { 0.0f, 0.0f, 1.0f });

    targetProvider_->CollectTargetableTargets(targetSnapshots_);
    projectedTargets_.reserve((std::min)(targetSnapshots_.size(), static_cast<size_t>(maximumCandidateCount_)));
    krakenNaturalLockDiagnostics_.reserve(targetSnapshots_.size());
    float bestScore = (std::numeric_limits<float>::max)();
    for (std::size_t snapshotIndex = 0;
         snapshotIndex < targetSnapshots_.size(); ++snapshotIndex) {
        const PlayerLockOnTargetSnapshot& targetView =
            targetSnapshots_[snapshotIndex];
        KrakenNaturalLockTargetDiagnostic* krakenDiagnostic = nullptr;
        if (targetView.kind == PlayerLockOnTargetKind::KrakenWeakPoint) {
            KrakenNaturalLockTargetDiagnostic diagnostic{};
            diagnostic.targetId = targetView.id;
            diagnostic.chainIndex = targetView.subTargetIndex;
            diagnostic.worldPosition = targetView.worldPosition;
            diagnostic.worldRadius = targetView.worldRadius;
            diagnostic.sourceColliderId = targetView.sourceColliderId;
            diagnostic.cameraPosition = camera_->GetViewTranslate();
            diagnostic.cameraForward = cameraForward;
            diagnostic.providerCandidateIndex = snapshotIndex;
            diagnostic.sourceReceived = true;
            diagnostic.providerAdded = true;
            diagnostic.alive = targetView.alive;
            diagnostic.targetable = targetView.targetable;
            diagnostic.valid = targetView.valid;
            diagnostic.projectionFailureReason = "なし";
            diagnostic.rejectionReason = "未評価";
            krakenNaturalLockDiagnostics_.push_back(std::move(diagnostic));
            krakenDiagnostic = &krakenNaturalLockDiagnostics_.back();
        }
        if (projectedTargets_.size() >= static_cast<size_t>(maximumCandidateCount_)) {
            if (krakenDiagnostic) {
                krakenDiagnostic->projectionFailureReason = "最大候補数を超過";
                krakenDiagnostic->rejectionReason = "Aim候補上限で拒否";
            }
            break;
        }
        if (!targetView.valid || !targetView.alive || !targetView.targetable
            || targetView.id.empty() || !std::isfinite(targetView.worldPosition.x)
            || !std::isfinite(targetView.worldPosition.y)
            || !std::isfinite(targetView.worldPosition.z)) {
            if (krakenDiagnostic) {
                krakenDiagnostic->projectionFailureReason =
                    "Providerスナップショットが無効";
                krakenDiagnostic->rejectionReason = "Aim受信時に拒否";
            }
            continue;
        }
        ProjectedTarget target{};
        target.runtimeId = targetView.id;
        target.worldPosition = targetView.worldPosition;
        target.targetKind = targetView.kind;
        target.subTargetIndex = targetView.subTargetIndex;
        target.markerWorldPosition = target.worldPosition;
        if (krakenDiagnostic) {
            if (!ProjectKrakenTarget(targetView, target, *krakenDiagnostic)) {
                krakenDiagnostic->projectionFailureReason = "触手形状を画面へ投影できません";
                krakenDiagnostic->rejectionReason = "触手形状が画面・深度範囲外";
                continue;
            }
        } else {
            const Vector3 cameraOffset =
                Subtract(target.worldPosition, camera_->GetViewTranslate());
            target.cameraDepth = Dot(cameraOffset, cameraForward);
            if (!std::isfinite(target.cameraDepth) || target.cameraDepth < minimumTargetDepth_
                || target.cameraDepth > maximumTargetDepth_) {
                continue;
            }
            if (!ProjectWorldToScreen(target.worldPosition, target.screenUv, target.clipW)) {
                continue;
            }

            const float worldRadius = std::isfinite(targetView.worldRadius) && targetView.worldRadius > 0.0f
                ? targetView.worldRadius
                : fallbackWorldRadius_;
            Vector2 rightUv{};
            Vector2 upUv{};
            float unusedClipW = 0.0f;
            const bool rightValid = ProjectWorldToScreen(
                Add(target.worldPosition, Scale(cameraRight, worldRadius)), rightUv, unusedClipW);
            const bool upValid = ProjectWorldToScreen(
                Add(target.worldPosition, Scale(cameraUp, worldRadius)), upUv, unusedClipW);
            float radiusX = rightValid ? std::abs(rightUv.x - target.screenUv.x) : minimumScreenRadius_;
            float radiusY = upValid ? std::abs(upUv.y - target.screenUv.y) : minimumScreenRadius_;
            if (!considerEnemyBounds_) {
                radiusX = minimumScreenRadius_;
                radiusY = minimumScreenRadius_;
            }
            target.screenRadius = {
                std::clamp(radiusX, minimumScreenRadius_, maximumScreenRadius_),
                std::clamp(radiusY, minimumScreenRadius_, maximumScreenRadius_),
            };
            target.boundsMinimum = {
                target.screenUv.x - target.screenRadius.x,
                target.screenUv.y - target.screenRadius.y,
            };
            target.boundsMaximum = {
                target.screenUv.x + target.screenRadius.x,
                target.screenUv.y + target.screenRadius.y,
            };
            target.overlapsVisibleRect = RectsOverlap(
                target.boundsMinimum, target.boundsMaximum, visibleRect_.minimum, visibleRect_.maximum);
            target.overlapsSoftRect = RectsOverlap(
                target.boundsMinimum, target.boundsMaximum, softRect_.minimum, softRect_.maximum);
            const float normalizedX = (target.screenUv.x - visibleRect_.center.x)
                / (std::max)(softRect_.halfSize.x, 0.00001f);
            const float normalizedY = (target.screenUv.y - visibleRect_.center.y)
                / (std::max)(softRect_.halfSize.y, 0.00001f);
            const float centerScore = std::sqrt(normalizedX * normalizedX + normalizedY * normalizedY);
            const float normalizedDepth = std::clamp(
                (target.cameraDepth - minimumTargetDepth_) / (maximumTargetDepth_ - minimumTargetDepth_),
                0.0f,
                1.0f);
            target.score = centerScore + normalizedDepth * depthScoreWeight_
                - (target.overlapsVisibleRect ? visibleRectBonus_ : 0.0f);
            target.projectionValid = true;
        }
        if (krakenDiagnostic) {
            const float deltaX =
                target.screenUv.x - softRect_.center.x;
            const float deltaY =
                target.screenUv.y - softRect_.center.y;
            const float allowedX =
                softRect_.halfSize.x + target.screenRadius.x;
            const float allowedY =
                softRect_.halfSize.y + target.screenRadius.y;
            krakenDiagnostic->screenRadius = target.screenRadius;
            krakenDiagnostic->screenDistance =
                std::sqrt(deltaX * deltaX + deltaY * deltaY);
            krakenDiagnostic->lockAllowedDistance =
                std::sqrt(allowedX * allowedX + allowedY * allowedY);
            krakenDiagnostic->corridorInside = target.overlapsSoftRect;
            krakenDiagnostic->candidateScore = target.score;
            krakenDiagnostic->rejectionReason = target.overlapsSoftRect
                ? "より良い候補との比較待ち"
                : "補助捕捉矩形外";
        }
        if (krakenDiagnostic ? target.overlapsVisibleRect : target.overlapsSoftRect) {
            ++candidateCount_;
            if (target.score < bestScore) {
                bestScore = target.score;
                bestCandidateId_ = target.runtimeId;
                bestCandidateScore_ = target.score;
            }
        }
        projectedTargets_.push_back(std::move(target));
    }
    for (KrakenNaturalLockTargetDiagnostic& diagnostic :
         krakenNaturalLockDiagnostics_) {
        diagnostic.candidateSelected =
            !bestCandidateId_.empty() && diagnostic.targetId == bestCandidateId_;
        if (diagnostic.candidateSelected) {
            diagnostic.rejectionReason = "なし";
        } else if (diagnostic.corridorInside) {
            diagnostic.rejectionReason = "既存スコアで別候補を選択";
        }
    }
}

const AimCorridorTargetingController::ProjectedTarget*
AimCorridorTargetingController::FindProjectedTarget(const std::string& runtimeId) const {
    if (runtimeId.empty()) {
        return nullptr;
    }
    for (const ProjectedTarget& target : projectedTargets_) {
        if (target.runtimeId == runtimeId) {
            return &target;
        }
    }
    return nullptr;
}

const AimCorridorTargetingController::ProjectedTarget*
AimCorridorTargetingController::FindBestCandidate() const {
    return FindProjectedTarget(bestCandidateId_);
}
