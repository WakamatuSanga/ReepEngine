#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossEffectController.h"

#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossController.h"
#include "Engine/Game/Effect/CombatEffectController.h"
#include "Engine/Game/Effect/ImpactDistortionController.h"
#include "Engine/Game/Enemy/EnemyDefeatEffectController.h"

#include <algorithm>

void KrakenTentacleMidbossEffectController::SetContext(
    CombatEffectController* combatEffectController,
    EnemyDefeatEffectController* defeatEffectController,
    ImpactDistortionController* impactDistortionController) {
    combatEffectController_ = combatEffectController;
    defeatEffectController_ = defeatEffectController;
    impactDistortionController_ = impactDistortionController;
}

bool KrakenTentacleMidbossEffectController::PlayHitEffect(
    const KrakenProjectileEnterEvent& event,
    const KrakenTentacleEffectPositionCandidates& candidates) {
    return PlayHitEffectInternal(event, candidates, true);
}

bool KrakenTentacleMidbossEffectController::PlayHitEffectInternal(
    const KrakenProjectileEnterEvent& event,
    const KrakenTentacleEffectPositionCandidates& candidates,
    bool useDeduplication) {
    const bool weakPoint = event.role == KrakenProjectileHitRole::WeakPoint;
    const HitKey key{ event.projectileRuntimeId, event.frameNumber, event.role };
    if (useDeduplication && std::find_if(
            spawnedHitKeys_.begin(), spawnedHitKeys_.end(),
            [&key](const HitKey& existing) {
                return existing.projectileRuntimeId ==
                        key.projectileRuntimeId &&
                    existing.eventFrame == key.eventFrame &&
                    existing.role == key.role;
            }) !=
            spawnedHitKeys_.end()) {
        ++diagnostics_.duplicateHitSuppressionCount;
        return false;
    }
    if (useDeduplication) {
        spawnedHitKeys_.push_back(key);
    }

    KrakenTentacleLastHitEffectDiagnostics last{};
    last.event = event;
    last.runtimeScale = weakPoint
        ? std::clamp(settings_.weakPointHitScale, 0.25f, 4.0f)
        : std::clamp(settings_.bodyHitScale, 0.25f, 4.0f);
    const KrakenTentacleResolvedEffectPosition resolved =
        ResolveKrakenTentacleEffectPosition(candidates);
    diagnostics_.nonFinitePositionRejectionCount +=
        resolved.rejectedCandidateCount;
    diagnostics_.positionFallbackCount += resolved.usedFallback ? 1 : 0;
    last.worldPosition = resolved.worldPosition;
    last.positionSource = resolved.source;
    last.valid = resolved.valid;

    const bool enabled = settings_.hitEffectEnabled &&
        (!weakPoint || settings_.weakPointHitEffectEnabled);
    if (!enabled) {
        diagnostics_.lastHit = last;
        return false;
    }
    if (!resolved.valid) {
        last.spawnFailed = true;
        ++diagnostics_.spawnFailureCount;
        diagnostics_.lastError =
            "命中エフェクト位置を有限値で取得できませんでした。";
        diagnostics_.lastHit = last;
        return false;
    }
    if (!combatEffectController_) {
        last.spawnFailed = true;
        ++diagnostics_.effectManagerMissingCount;
        ++diagnostics_.spawnFailureCount;
        diagnostics_.lastError =
            "共有命中エフェクトが接続されていません。";
        diagnostics_.lastHit = last;
        return false;
    }
    if (!combatEffectController_->IsEnabled()) {
        last.spawnFailed = true;
        ++diagnostics_.spawnFailureCount;
        diagnostics_.lastWarning =
            "共有命中エフェクトが無効なため生成を抑制しました。";
        diagnostics_.lastHit = last;
        return false;
    }

    if (!combatEffectController_->TryPlayPlayerBulletHitEnemy(
            resolved.worldPosition, false, last.runtimeScale)) {
        last.spawnFailed = true;
        ++diagnostics_.spawnFailureCount;
        diagnostics_.lastWarning =
            "共有命中エフェクトが生成を受け付けませんでした。";
        diagnostics_.lastHit = last;
        return false;
    }
    last.spawnSucceeded = true;
    diagnostics_.lastError.clear();
    if (weakPoint) {
        ++diagnostics_.weakPointHitSpawnCount;
    } else {
        ++diagnostics_.bodyHitSpawnCount;
    }
    diagnostics_.lastHit = last;
    return true;
}

bool KrakenTentacleMidbossEffectController::PlayDefeatEffect(
    std::uint64_t defeatSequenceId,
    KrakenTentacleMidbossState stateAtSpawn,
    const KrakenTentacleEffectPositionCandidates& candidates) {
    return PlayDefeatEffectInternal(
        defeatSequenceId, stateAtSpawn, candidates, true);
}

bool KrakenTentacleMidbossEffectController::PlayDefeatEffectInternal(
    std::uint64_t defeatSequenceId,
    KrakenTentacleMidbossState stateAtSpawn,
    const KrakenTentacleEffectPositionCandidates& candidates,
    bool useDeduplication) {
    if (useDeduplication && defeatSequenceId != 0 &&
        spawnedDefeatSequenceId_ == defeatSequenceId) {
        ++diagnostics_.duplicateDefeatSuppressionCount;
        return false;
    }
    if (useDeduplication) {
        spawnedDefeatSequenceId_ = defeatSequenceId;
    }

    KrakenTentacleLastDefeatEffectDiagnostics last{};
    last.defeatSequenceId = defeatSequenceId;
    last.stateAtSpawn = static_cast<std::uint8_t>(stateAtSpawn);
    last.runtimeScale = std::clamp(
        settings_.defeatEffectScale, 0.25f, 4.0f);
    const KrakenTentacleResolvedEffectPosition resolved =
        ResolveKrakenTentacleEffectPosition(candidates);
    diagnostics_.nonFinitePositionRejectionCount +=
        resolved.rejectedCandidateCount;
    diagnostics_.positionFallbackCount += resolved.usedFallback ? 1 : 0;
    last.worldPosition = resolved.worldPosition;
    last.positionSource = resolved.source;
    last.valid = resolved.valid;

    if (!settings_.defeatEffectEnabled) {
        diagnostics_.lastDefeat = last;
        return false;
    }
    if (!resolved.valid) {
        last.spawnFailed = true;
        ++diagnostics_.spawnFailureCount;
        diagnostics_.lastError =
            "撃破エフェクト位置を有限値で取得できませんでした。";
        diagnostics_.lastDefeat = last;
        return false;
    }
    if (!defeatEffectController_) {
        last.spawnFailed = true;
        ++diagnostics_.effectManagerMissingCount;
        ++diagnostics_.spawnFailureCount;
        diagnostics_.lastError =
            "共有撃破エフェクトが接続されていません。";
        diagnostics_.lastDefeat = last;
        return false;
    }

    defeatEffectController_->SpawnDefeatEffect(
        resolved.worldPosition, last.runtimeScale);
    if (settings_.useImpactDistortion && impactDistortionController_) {
        impactDistortionController_->TriggerEnemyDefeat(
            resolved.worldPosition);
    }
    last.spawned = true;
    last.spawnSucceeded = true;
    ++diagnostics_.defeatSpawnCount;
    diagnostics_.lastError.clear();
    diagnostics_.lastDefeat = last;
    return true;
}

bool KrakenTentacleMidbossEffectController::PlayTestHitEffect(
    bool weakPoint,
    const KrakenTentacleEffectPositionCandidates& candidates) {
    KrakenProjectileEnterEvent event{};
    event.role = weakPoint
        ? KrakenProjectileHitRole::WeakPoint
        : KrakenProjectileHitRole::Body;
    event.valid = true;
    return PlayHitEffectInternal(event, candidates, false);
}

bool KrakenTentacleMidbossEffectController::PlayTestDefeatEffect(
    KrakenTentacleMidbossState stateAtSpawn,
    const KrakenTentacleEffectPositionCandidates& candidates) {
    return PlayDefeatEffectInternal(0, stateAtSpawn, candidates, false);
}

void KrakenTentacleMidbossEffectController::
RecordHpZeroHitEffectSuppression(std::size_t count) {
    diagnostics_.hpZeroHitEffectSuppressionCount += count;
}

void KrakenTentacleMidbossEffectController::Reset(bool resetSettings) {
    spawnedHitKeys_.clear();
    spawnedDefeatSequenceId_ = 0;
    diagnostics_ = {};
    if (resetSettings) {
        settings_ = {};
    }
}

void KrakenTentacleMidbossEffectController::Finalize() {
    Reset(true);
    combatEffectController_ = nullptr;
    defeatEffectController_ = nullptr;
    impactDistortionController_ = nullptr;
}
