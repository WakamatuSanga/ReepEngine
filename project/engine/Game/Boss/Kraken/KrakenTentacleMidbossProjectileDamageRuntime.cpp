#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"

#include "Engine/Game/Player/PlayerBulletManager.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr std::size_t kInvalidEventSafetyLimit = 64;

bool IsFinite(const Vector3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

KrakenProjectileSnapshotType ToPublicProjectileType(
    KrakenTentacleCollisionProjectileType type) {
    switch (type) {
    case KrakenTentacleCollisionProjectileType::NormalShot:
        return KrakenProjectileSnapshotType::NormalShot;
    case KrakenTentacleCollisionProjectileType::LockedWingShot:
        return KrakenProjectileSnapshotType::LockedWingShot;
    case KrakenTentacleCollisionProjectileType::None:
    default:
        return KrakenProjectileSnapshotType::Unknown;
    }
}

bool ContainsRuntimeId(
    const std::vector<std::uint64_t>& runtimeIds,
    std::uint64_t runtimeId) {
    return std::find(runtimeIds.begin(), runtimeIds.end(), runtimeId) !=
        runtimeIds.end();
}

bool IsProjectileCollisionEvent(
    const KrakenTentacleCollisionEventSnapshot& event) {
    return event.pair.key.targetKind ==
            KrakenTentacleCollisionTargetKind::PlayerBullet &&
        (event.pair.key.role == KrakenColliderPreviewRole::Damage ||
            event.pair.key.role == KrakenColliderPreviewRole::WeakPoint);
}
}

void KrakenTentacleMidbossController::Impl::SetProjectileDamageContext(
    PlayerBulletManager* playerBulletManagerValue) {
    if (projectileDamageBulletManager != playerBulletManagerValue) {
        consumedProjectileIds.clear();
        aggregatedProjectileEventsThisFrame.clear();
        projectileDamageDiagnostics.lastHit = {};
        lastProcessedProjectileDamageFrame = ~std::uint64_t{ 0 };
    }
    projectileDamageBulletManager = playerBulletManagerValue;
}

std::vector<KrakenProjectileEnterEvent>
KrakenTentacleMidbossController::Impl::
GetProjectileEnterEventsThisFrame() const {
    std::vector<KrakenProjectileEnterEvent> events;
    if (diagnostics.lastCollisionQueryFrameIndex == ~std::uint64_t{ 0 }) {
        return events;
    }
    for (const KrakenTentacleCollisionEventSnapshot& collisionEvent :
        collisionFrameEvents) {
        if (collisionEvent.transition !=
                KrakenTentacleCollisionTransition::Enter ||
            !IsProjectileCollisionEvent(collisionEvent)) {
            continue;
        }
        const KrakenTentacleCollisionPairSnapshot& pair = collisionEvent.pair;
        KrakenProjectileEnterEvent event{};
        event.frameNumber = diagnostics.lastCollisionQueryFrameIndex;
        event.krakenColliderId =
            static_cast<std::uint64_t>(pair.key.chainIndex) * 5 +
            pair.key.colliderIndex + 1;
        event.chainIndex = pair.key.chainIndex;
        event.projectileRuntimeId = pair.key.targetRuntimeId;
        event.projectileType = ToPublicProjectileType(pair.projectileType);
        event.role = pair.key.role == KrakenColliderPreviewRole::WeakPoint
            ? KrakenProjectileHitRole::WeakPoint
            : KrakenProjectileHitRole::Body;
        event.projectileDamage = pair.projectileDamage;
        event.penetrationDepth =
            (std::max)(pair.radiusSum - pair.centerDistance, 0.0f);
        event.closestPoint = pair.colliderClosestPosition;
        event.valid = event.krakenColliderId != 0 &&
            event.projectileRuntimeId != 0 &&
            IsFinite(event.closestPoint) &&
            std::isfinite(event.penetrationDepth);
        events.push_back(event);
    }
    return events;
}

void KrakenTentacleMidbossController::Impl::ResetProjectileDamageState(
    bool resetSettings) {
    if (resetSettings) {
        health.Reset();
        projectileDamageEnabled = false;
    }
    consumedProjectileIds.clear();
    aggregatedProjectileEventsThisFrame.clear();
    projectileDamageDiagnostics = {};
    lastProcessedProjectileDamageFrame = ~std::uint64_t{ 0 };
    projectileKillInProgress = false;
}

void KrakenTentacleMidbossController::Impl::HealProjectileDamageHealth() {
    if (health.IsDefeatPending() || defeatStarted || defeatCompleted ||
        IsDefeatState()) {
        RecoverFromDefeat();
        return;
    }
    if (!health.HealFull()) {
        ++projectileDamageDiagnostics.nonFiniteHpCount;
        projectileDamageDiagnostics.lastError =
            "中ボスHPを全回復できませんでした。";
        return;
    }
    consumedProjectileIds.clear();
    aggregatedProjectileEventsThisFrame.clear();
    projectileDamageDiagnostics.lastHit = {};
    projectileDamageDiagnostics.lastError.clear();
    projectileDamageDiagnostics.lastWarning =
        "中ボスHPを全回復し、投射物命中履歴を消去しました。";
}

void KrakenTentacleMidbossController::Impl::UpdateProjectileDamage() {
    if (health.IsDefeatPending() && attackDamageEnabled) {
        attackDamageEnabled = false;
        InvalidateAttackDamageSequence();
    }
    const std::uint64_t frameNumber =
        diagnostics.lastCollisionQueryFrameIndex;
    projectileDamageDiagnostics.frameAggregation = {};
    aggregatedProjectileEventsThisFrame.clear();
    if (frameNumber == ~std::uint64_t{ 0 } ||
        lastProcessedProjectileDamageFrame == frameNumber) {
        return;
    }
    lastProcessedProjectileDamageFrame = frameNumber;

    for (const KrakenTentacleCollisionEventSnapshot& event :
        collisionFrameEvents) {
        if (!IsProjectileCollisionEvent(event)) {
            continue;
        }
        if (event.transition == KrakenTentacleCollisionTransition::Stay) {
            ++projectileDamageDiagnostics.stayDamageSuppressionCount;
        } else if (
            event.transition == KrakenTentacleCollisionTransition::Exit) {
            ++projectileDamageDiagnostics.exitDamageSuppressionCount;
        }
    }

    aggregatedProjectileEventsThisFrame =
        AggregateKrakenProjectileEnterEvents(
            GetProjectileEnterEventsThisFrame(),
            frameNumber,
            projectileDamageDiagnostics.frameAggregation);
    const KrakenProjectileAggregationDiagnostics& aggregation =
        projectileDamageDiagnostics.frameAggregation;
    projectileDamageDiagnostics.runtimeIdZeroCount +=
        aggregation.runtimeIdZeroCount;
    projectileDamageDiagnostics.invalidRoleCount +=
        aggregation.invalidRoleCount;
    projectileDamageDiagnostics.invalidEventRejectionCount +=
        aggregation.invalidEventCount + aggregation.oldFrameRejectionCount;
    projectileDamageDiagnostics.duplicateRuntimeIdCount +=
        diagnostics.stableRuntimeIdDuplicateCount;

    std::size_t invalidDamageEventCount = 0;
    for (const KrakenProjectileEnterEvent& event :
        aggregatedProjectileEventsThisFrame) {
        if (!std::isfinite(event.projectileDamage)) {
            ++projectileDamageDiagnostics.nonFiniteDamageCount;
            ++invalidDamageEventCount;
        } else if (event.projectileDamage <= 0.0f) {
            ++projectileDamageDiagnostics.invalidDamageCount;
            ++invalidDamageEventCount;
        }
    }
    const std::size_t structuralInvalidCount =
        aggregation.invalidEventCount + aggregation.oldFrameRejectionCount +
        aggregation.runtimeIdZeroCount + aggregation.invalidRoleCount;
    if (structuralInvalidCount + invalidDamageEventCount >=
        kInvalidEventSafetyLimit) {
        projectileDamageEnabled = false;
        projectileDamageDiagnostics.lastError =
            "不正な投射物イベントが大量に検出されたため、ダメージを無効化しました。";
        return;
    }
    if (projectileDamageEnabled && IsVisible() &&
        !projectileDamageBulletManager) {
        ++projectileDamageDiagnostics.playerBulletManagerMissingCount;
        projectileDamageDiagnostics.lastError =
            "プレイヤー弾管理が未接続のためダメージを拒否しました。";
        return;
    }
    if (aggregatedProjectileEventsThisFrame.empty()) {
        return;
    }
    if (!initialized || projectileDamageFinalizing ||
        projectileKillInProgress) {
        projectileDamageDiagnostics.invalidEventRejectionCount +=
            aggregatedProjectileEventsThisFrame.size();
        return;
    }
    if (!projectileDamageEnabled) {
        projectileDamageDiagnostics.projectileDamageDisabledRejectionCount +=
            aggregatedProjectileEventsThisFrame.size();
        return;
    }
    if (!IsVisible()) {
        projectileDamageDiagnostics.hiddenRejectionCount +=
            aggregatedProjectileEventsThisFrame.size();
        return;
    }
    if (!health.IsValid() || !std::isfinite(health.GetMaxHp()) ||
        !std::isfinite(health.GetCurrentHp()) || health.GetMaxHp() <= 0.0f) {
        ++projectileDamageDiagnostics.nonFiniteHpCount;
        projectileDamageDiagnostics.lastError =
            "中ボスHPが不正なためダメージを拒否しました。";
        return;
    }
    if (health.IsDefeatPending() || health.GetCurrentHp() <= 0.0f) {
        projectileDamageDiagnostics.defeatPendingRejectionCount +=
            aggregatedProjectileEventsThisFrame.size();
        defeatDiagnostics.additionalDamageRejectionCount +=
            aggregatedProjectileEventsThisFrame.size();
        effectController.RecordHpZeroHitEffectSuppression(
            aggregatedProjectileEventsThisFrame.size());
        return;
    }
    for (std::size_t index = 0;
        index < aggregatedProjectileEventsThisFrame.size(); ++index) {
        const KrakenProjectileEnterEvent& event =
            aggregatedProjectileEventsThisFrame[index];
        if (health.IsDefeatPending() || health.GetCurrentHp() <= 0.0f) {
            projectileDamageDiagnostics.defeatPendingRejectionCount +=
                aggregatedProjectileEventsThisFrame.size() - index;
            defeatDiagnostics.additionalDamageRejectionCount +=
                aggregatedProjectileEventsThisFrame.size() - index;
            effectController.RecordHpZeroHitEffectSuppression(
                aggregatedProjectileEventsThisFrame.size() - index);
            break;
        }
        if (ContainsRuntimeId(
                consumedProjectileIds, event.projectileRuntimeId)) {
            ++projectileDamageDiagnostics
                .consumedProjectileSuppressionCount;
            continue;
        }
        if (!event.valid || event.projectileRuntimeId == 0 ||
            !std::isfinite(event.projectileDamage) ||
            event.projectileDamage <= 0.0f) {
            ++projectileDamageDiagnostics.invalidEventRejectionCount;
            continue;
        }

        const bool weakPoint =
            event.role == KrakenProjectileHitRole::WeakPoint;
        const float multiplier = weakPoint
            ? health.GetWeakPointMultiplier() : 1.0f;
        if (!std::isfinite(multiplier)) {
            ++projectileDamageDiagnostics.nonFiniteMultiplierCount;
            continue;
        }
        const float finalDamage = event.projectileDamage * multiplier;
        if (!std::isfinite(finalDamage) || finalDamage <= 0.0f) {
            ++projectileDamageDiagnostics.nonFiniteDamageCount;
            continue;
        }

        KrakenTentacleHealthDamageApplication application{};
        if (!health.TryApplyDamage(finalDamage, application)) {
            ++projectileDamageDiagnostics.invalidDamageCount;
            continue;
        }
        consumedProjectileIds.push_back(event.projectileRuntimeId);
        projectileDamageDiagnostics.totalDamage += application.appliedDamage;
        if (weakPoint) {
            ++projectileDamageDiagnostics.weakPointDamageAppliedCount;
        } else {
            ++projectileDamageDiagnostics.bodyDamageAppliedCount;
        }

        effectController.PlayHitEffect(
            event, BuildHitEffectPositionCandidates(event));

        projectileKillInProgress = true;
        const bool killSucceeded =
            projectileDamageBulletManager->TryKillProjectileByRuntimeId(
                event.projectileRuntimeId,
                weakPoint
                    ? PlayerBulletManager::PlayerProjectileKillReason::
                        KrakenWeakPointHit
                    : PlayerBulletManager::PlayerProjectileKillReason::
                        KrakenBodyHit);
        projectileKillInProgress = false;
        if (killSucceeded) {
            ++projectileDamageDiagnostics.bulletKillSuccessCount;
        } else {
            ++projectileDamageDiagnostics.bulletKillFailureCount;
            ++projectileDamageDiagnostics.killTargetNotFoundCount;
        }

        KrakenProjectileLastHitDiagnostics& lastHit =
            projectileDamageDiagnostics.lastHit;
        lastHit.event = event;
        lastHit.weakPointMultiplier = multiplier;
        lastHit.finalDamage = finalDamage;
        lastHit.hpBefore = application.hpBefore;
        lastHit.hpAfter = application.hpAfter;
        lastHit.bulletKillSucceeded = killSucceeded;
        lastHit.bulletKillFailed = !killSucceeded;
        lastHit.valid = true;

        if (application.result ==
            KrakenTentacleHealthDamageResult::DefeatStarted) {
            ++projectileDamageDiagnostics.hpZeroReachedCount;
            ++projectileDamageDiagnostics.defeatPendingStartCount;
            BeginDefeat();
        }
    }
}

void KrakenTentacleMidbossController::SetProjectileDamageContext(
    PlayerBulletManager* playerBulletManager) {
    if (impl_) {
        impl_->SetProjectileDamageContext(playerBulletManager);
    }
}

void KrakenTentacleMidbossController::SetProjectileDamageEnabled(
    bool enabled) {
    if (impl_) {
        impl_->projectileDamageEnabled = enabled &&
            impl_->health.IsValid() && !impl_->health.IsDefeatPending() &&
            !impl_->IsDefeatState();
    }
}

bool KrakenTentacleMidbossController::IsProjectileDamageEnabled() const {
    return impl_ && impl_->projectileDamageEnabled;
}

std::vector<KrakenProjectileEnterEvent>
KrakenTentacleMidbossController::GetProjectileEnterEventsThisFrame() const {
    return impl_ ? impl_->GetProjectileEnterEventsThisFrame()
                 : std::vector<KrakenProjectileEnterEvent>{};
}

float KrakenTentacleMidbossController::GetMaxHp() const {
    return impl_ ? impl_->health.GetMaxHp() : 0.0f;
}

float KrakenTentacleMidbossController::GetCurrentHp() const {
    return impl_ ? impl_->health.GetCurrentHp() : 0.0f;
}

bool KrakenTentacleMidbossController::IsDefeatPending() const {
    return impl_ && impl_->health.IsDefeatPending();
}
