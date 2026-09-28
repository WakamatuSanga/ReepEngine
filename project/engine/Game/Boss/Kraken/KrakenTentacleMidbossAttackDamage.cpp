#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"

#include "Engine/Game/Effect/CombatEffectController.h"
#include "Engine/Game/GameState/PlayerDeathSequenceController.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Game/Player/PlayerDamageFeedbackController.h"

#include <algorithm>

namespace {
    constexpr std::uint64_t kColliderSlotsPerChain = 5;

    bool IsAttackPlayerEvent(
        const KrakenTentacleCollisionEventSnapshot& event) {
        return event.pair.key.role == KrakenColliderPreviewRole::Attack &&
            event.pair.key.targetKind ==
                KrakenTentacleCollisionTargetKind::Player;
    }
}

void KrakenTentacleMidbossController::Impl::SetAttackDamageContext(
    PlayerDamageFeedbackController* damageFeedbackControllerValue,
    PlayerDeathSequenceController* deathSequenceControllerValue,
    CombatEffectController* combatEffectControllerValue) {
    if (damageFeedbackController != damageFeedbackControllerValue ||
        deathSequenceController != deathSequenceControllerValue) {
        InvalidateAttackDamageSequence();
    }
    damageFeedbackController = damageFeedbackControllerValue;
    deathSequenceController = deathSequenceControllerValue;
    attackDamageEffectController = combatEffectControllerValue;
}

std::vector<KrakenAttackPlayerEnterEvent>
KrakenTentacleMidbossController::Impl::
GetAttackPlayerEnterEventsThisFrame() const {
    std::vector<KrakenAttackPlayerEnterEvent> events;
    for (const KrakenTentacleCollisionEventSnapshot& collisionEvent :
        collisionFrameEvents) {
        if (collisionEvent.transition !=
                KrakenTentacleCollisionTransition::Enter ||
            !IsAttackPlayerEvent(collisionEvent)) {
            continue;
        }
        KrakenAttackPlayerEnterEvent event{};
        event.frameNumber = diagnostics.collisionQueryFrameCount;
        event.krakenColliderId =
            static_cast<std::uint64_t>(collisionEvent.pair.key.chainIndex) *
                kColliderSlotsPerChain +
            collisionEvent.pair.key.colliderIndex + 1;
        event.chainIndex = collisionEvent.pair.key.chainIndex;
        event.penetrationDepth = (std::max)(
            collisionEvent.pair.radiusSum -
                collisionEvent.pair.centerDistance,
            0.0f);
        event.closestPoint = collisionEvent.pair.colliderClosestPosition;
        event.valid = event.frameNumber != 0 &&
            event.krakenColliderId != 0;
        events.push_back(event);
    }
    std::sort(
        events.begin(), events.end(),
        [](const KrakenAttackPlayerEnterEvent& lhs,
           const KrakenAttackPlayerEnterEvent& rhs) {
            return lhs.krakenColliderId < rhs.krakenColliderId;
        });
    return events;
}

bool KrakenTentacleMidbossController::Impl::
IsAttackDamagePhaseActive() const {
    if (state != KrakenTentacleMidbossState::Slam &&
        state != KrakenTentacleMidbossState::ImpactHold) {
        return false;
    }
    return std::any_of(
        capsuleSnapshots.begin(), capsuleSnapshots.end(),
        [this](const KrakenTentacleMidbossCapsuleSnapshot& collider) {
            return collider.role == KrakenColliderPreviewRole::Attack &&
                collider.chainIndex == selectedAttackChainIndex &&
                collider.phaseActive;
        });
}

void KrakenTentacleMidbossController::Impl::BeginAttackDamageSequence() {
    attackSequenceCounter =
        GetNextKrakenAttackSequenceId(attackSequenceCounter);
    currentAttackSequenceId = attackSequenceCounter;
    hitAttemptConsumedThisAttack = false;
    damageAppliedThisAttack = false;
    damageBlockedThisAttack = false;
    exitObservedAfterHitAttempt = false;
    lastProcessedAttackDamageQueryFrame = 0;
    attackDamageDiagnostics.currentDamageCount = 0;
    attackDamageDiagnostics.lastEnterFrame = 0;
    attackDamageDiagnostics.lastColliderId = 0;
    attackDamageDiagnostics.lastPenetrationDepth = 0.0f;
    attackDamageDiagnostics.lastClosestPoint = {};
    attackDamageDiagnostics.hpBefore = -1;
    attackDamageDiagnostics.hpAfter = -1;
    attackDamageDiagnostics.lastResult = KrakenAttackDamageResult::None;
    ++attackDamageDiagnostics.attackStartCount;
}

void KrakenTentacleMidbossController::Impl::
InvalidateAttackDamageSequence() {
    currentAttackSequenceId = 0;
    hitAttemptConsumedThisAttack = false;
    damageAppliedThisAttack = false;
    damageBlockedThisAttack = false;
    exitObservedAfterHitAttempt = false;
    lastProcessedAttackDamageQueryFrame = 0;
    attackDamageDiagnostics.currentDamageCount = 0;
    attackDamageDiagnostics.attackPhaseActive = false;
}

void KrakenTentacleMidbossController::Impl::ResetAttackDamageState(
    bool resetSettings,
    bool resetSequenceCounter) {
    if (resetSettings) {
        attackDamageEnabled = false;
        attackDamage = 1;
    }
    if (resetSequenceCounter) {
        attackSequenceCounter = 0;
    }
    currentAttackSequenceId = 0;
    lastProcessedAttackDamageQueryFrame = 0;
    hitAttemptConsumedThisAttack = false;
    damageAppliedThisAttack = false;
    damageBlockedThisAttack = false;
    exitObservedAfterHitAttempt = false;
    attackDamageDiagnostics = {};
}

void KrakenTentacleMidbossController::Impl::UpdateAttackDamage() {
    const std::uint64_t queryFrameNumber =
        diagnostics.collisionQueryFrameCount;
    if (queryFrameNumber == 0) {
        return;
    }
    if (lastProcessedAttackDamageQueryFrame == queryFrameNumber) {
        ++attackDamageDiagnostics.sameFrameSuppressionCount;
        return;
    }
    lastProcessedAttackDamageQueryFrame = queryFrameNumber;

    attackDamageDiagnostics.attackPhaseActive =
        IsAttackDamagePhaseActive();
    attackDamageDiagnostics.playerConnected = collisionPlayer &&
        damageFeedbackController && deathSequenceController;
    const bool deathActive = deathSequenceController &&
        deathSequenceController->IsActiveOrFinished();
    const bool hpDead = damageFeedbackController &&
        damageFeedbackController->IsDead();
    attackDamageDiagnostics.playerAlive = collisionPlayerAlive &&
        !deathActive && !hpDead;
    attackDamageDiagnostics.barrelRollInvincible =
        collisionPlayer && collisionPlayer->IsInvincible();
    attackDamageDiagnostics.damageInvincible =
        damageFeedbackController && damageFeedbackController->IsInvincible();
    attackDamageDiagnostics.respawnInvincible = false;
    attackDamageDiagnostics.damageAcceptable =
        attackDamageDiagnostics.playerConnected &&
        attackDamageDiagnostics.playerAlive &&
        !attackDamageDiagnostics.barrelRollInvincible &&
        !attackDamageDiagnostics.damageInvincible;

    if (attackDamageEnabled && currentAttackSequenceId != 0) {
        for (const KrakenTentacleCollisionEventSnapshot& event :
            collisionFrameEvents) {
            if (!IsAttackPlayerEvent(event)) {
                continue;
            }
            if (event.transition == KrakenTentacleCollisionTransition::Stay) {
                ++attackDamageDiagnostics.stayDamageSuppressionCount;
            } else if (
                event.transition == KrakenTentacleCollisionTransition::Exit) {
                ++attackDamageDiagnostics.exitDamageSuppressionCount;
                exitObservedAfterHitAttempt |= hitAttemptConsumedThisAttack;
            }
        }
    }

    const std::vector<KrakenAttackPlayerEnterEvent> enterEvents =
        GetAttackPlayerEnterEventsThisFrame();
    const KrakenAttackDamageEventSelection selection =
        SelectKrakenAttackDamageEnterEvent(
            enterEvents,
            queryFrameNumber,
            static_cast<std::uint32_t>(selectedAttackChainIndex),
            attackDamageDiagnostics.attackPhaseActive,
            attackDamageEnabled,
            currentAttackSequenceId,
            hitAttemptConsumedThisAttack);
    attackDamageDiagnostics.oldEventRejectionCount +=
        selection.oldFrameCount;
    attackDamageDiagnostics.chainMismatchRejectionCount +=
        selection.chainMismatchCount;
    attackDamageDiagnostics.phaseMismatchRejectionCount +=
        selection.phaseMismatchCount;
    if (selection.enterEventCount == 0) {
        return;
    }
    if (!selection.ready) {
        attackDamageDiagnostics.lastResult = selection.result;
        if (selection.result ==
            KrakenAttackDamageResult::HitAlreadyConsumed) {
            ++attackDamageDiagnostics.sameAttackHitSuppressionCount;
            if (exitObservedAfterHitAttempt) {
                ++attackDamageDiagnostics.reenterDamageSuppressionCount;
            }
        }
        return;
    }

    hitAttemptConsumedThisAttack = true;
    ++attackDamageDiagnostics.damageAttemptCount;
    attackDamageDiagnostics.lastEnterFrame = selection.event.frameNumber;
    attackDamageDiagnostics.lastColliderId =
        selection.event.krakenColliderId;
    attackDamageDiagnostics.lastPenetrationDepth =
        selection.event.penetrationDepth;
    attackDamageDiagnostics.lastClosestPoint = selection.event.closestPoint;
    attackDamageDiagnostics.hpBefore = damageFeedbackController
        ? damageFeedbackController->GetHp() : -1;

    if (!attackDamageDiagnostics.playerConnected) {
        damageBlockedThisAttack = true;
        ++attackDamageDiagnostics.contextUnavailableCount;
        attackDamageDiagnostics.lastResult =
            KrakenAttackDamageResult::ContextUnavailable;
    } else if (!attackDamageDiagnostics.playerAlive) {
        damageBlockedThisAttack = true;
        ++attackDamageDiagnostics.playerDeathRejectionCount;
        attackDamageDiagnostics.lastResult =
            KrakenAttackDamageResult::BlockedByDeath;
    } else if (attackDamageDiagnostics.barrelRollInvincible) {
        damageBlockedThisAttack = true;
        ++attackDamageDiagnostics.invincibilityRejectionCount;
        ++attackDamageDiagnostics.barrelRollRejectionCount;
        attackDamageDiagnostics.lastResult =
            KrakenAttackDamageResult::BlockedByBarrelRoll;
    } else if (attackDamageDiagnostics.damageInvincible) {
        damageBlockedThisAttack = true;
        ++attackDamageDiagnostics.invincibilityRejectionCount;
        ++attackDamageDiagnostics.damageInvincibilityRejectionCount;
        attackDamageDiagnostics.lastResult =
            KrakenAttackDamageResult::BlockedByDamageInvincibility;
    } else {
        const int damageAmount = std::clamp(
            attackDamage, 1, damageFeedbackController->GetMaxHp());
        const bool applied = damageFeedbackController->ApplyDamage(
            selection.event.closestPoint, damageAmount);
        if (!applied) {
            damageBlockedThisAttack = true;
            ++attackDamageDiagnostics.damageApiRejectionCount;
            attackDamageDiagnostics.lastResult =
                KrakenAttackDamageResult::DamageApiRejected;
        } else {
            damageAppliedThisAttack = true;
            ++attackDamageDiagnostics.damageAppliedCount;
            ++attackDamageDiagnostics.currentDamageCount;
            attackDamageDiagnostics.maximumDamageCountPerAttack =
                (std::max)(
                    attackDamageDiagnostics.maximumDamageCountPerAttack,
                    attackDamageDiagnostics.currentDamageCount);
            attackDamageDiagnostics.lastResult =
                KrakenAttackDamageResult::Applied;
            if (damageFeedbackController->IsDead()) {
                if (attackDamageEffectController && collisionPlayer) {
                    attackDamageEffectController->PlayPlayerDeathExplosion(
                        collisionPlayer->GetWorldPosition());
                }
                deathSequenceController->StartDeathSequence();
            }
        }
    }
    attackDamageDiagnostics.hpAfter = damageFeedbackController
        ? damageFeedbackController->GetHp() : -1;
    const bool deathActiveAfter = deathSequenceController &&
        deathSequenceController->IsActiveOrFinished();
    const bool hpDeadAfter = damageFeedbackController &&
        damageFeedbackController->IsDead();
    attackDamageDiagnostics.playerAlive = collisionPlayerAlive &&
        !deathActiveAfter && !hpDeadAfter;
    attackDamageDiagnostics.damageInvincible =
        damageFeedbackController && damageFeedbackController->IsInvincible();
    attackDamageDiagnostics.damageAcceptable =
        attackDamageDiagnostics.playerConnected &&
        attackDamageDiagnostics.playerAlive &&
        !attackDamageDiagnostics.barrelRollInvincible &&
        !attackDamageDiagnostics.damageInvincible;
}

void KrakenTentacleMidbossController::SetAttackDamageContext(
    PlayerDamageFeedbackController* damageFeedbackControllerValue,
    PlayerDeathSequenceController* deathSequenceControllerValue,
    CombatEffectController* combatEffectControllerValue) {
    if (impl_) {
        impl_->SetAttackDamageContext(
            damageFeedbackControllerValue,
            deathSequenceControllerValue,
            combatEffectControllerValue);
    }
}

void KrakenTentacleMidbossController::SetAttackDamageEnabled(bool enabled) {
    if (impl_) {
        impl_->attackDamageEnabled = enabled &&
            !impl_->health.IsDefeatPending() && !impl_->IsDefeatState();
    }
}

bool KrakenTentacleMidbossController::IsAttackDamageEnabled() const {
    return impl_ && impl_->attackDamageEnabled;
}

std::vector<KrakenAttackPlayerEnterEvent>
KrakenTentacleMidbossController::GetAttackPlayerEnterEventsThisFrame() const {
    return impl_
        ? impl_->GetAttackPlayerEnterEventsThisFrame()
        : std::vector<KrakenAttackPlayerEnterEvent>{};
}
