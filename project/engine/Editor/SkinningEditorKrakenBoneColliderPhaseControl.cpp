#include "SkinningEditorKrakenBoneColliderPhaseControl.h"

#include "SkinningEditorKrakenAttackMotion.h"
#include "SkinningEditorKrakenBoneColliderPreviewCollection.h"

#include <cmath>
#include <limits>

namespace {
constexpr std::uint64_t kNoTransitionFrame =
    std::numeric_limits<std::uint64_t>::max();

bool IsKnownMotionMode(KrakenColliderPhaseMotionMode mode) {
    switch (mode) {
    case KrakenColliderPhaseMotionMode::Manual:
    case KrakenColliderPhaseMotionMode::IdleSway:
    case KrakenColliderPhaseMotionMode::AttackSlamPreview:
        return true;
    default:
        return false;
    }
}

KrakenTentacleColliderAttackPhase ToColliderAttackPhase(
    KrakenTentacleAttackPreviewPhase phase) {
    switch (phase) {
    case KrakenTentacleAttackPreviewPhase::Windup:
        return KrakenTentacleColliderAttackPhase::Windup;
    case KrakenTentacleAttackPreviewPhase::WindupHold:
        return KrakenTentacleColliderAttackPhase::WindupHold;
    case KrakenTentacleAttackPreviewPhase::Slam:
        return KrakenTentacleColliderAttackPhase::Slam;
    case KrakenTentacleAttackPreviewPhase::ImpactHold:
        return KrakenTentacleColliderAttackPhase::ImpactHold;
    case KrakenTentacleAttackPreviewPhase::Recovery:
        return KrakenTentacleColliderAttackPhase::Recovery;
    case KrakenTentacleAttackPreviewPhase::Completed:
        return KrakenTentacleColliderAttackPhase::Completed;
    default:
        return KrakenTentacleColliderAttackPhase::Invalid;
    }
}

bool IsRoleLayerVisible(
    KrakenColliderPreviewRole role,
    const KrakenBoneColliderPreviewSettings& settings) {
    switch (role) {
    case KrakenColliderPreviewRole::Attack:
        return settings.showAttackColliders;
    case KrakenColliderPreviewRole::Damage:
        return settings.showDamageColliders;
    case KrakenColliderPreviewRole::WeakPoint:
        return settings.showWeakPointColliders;
    default:
        return false;
    }
}

bool ResolvePreviewVisibility(
    const SkinningEditorKrakenBoneColliderPreviewCollection& collection,
    KrakenColliderPreviewRole role,
    std::size_t chainIndex,
    std::size_t localIndex,
    bool tipSphere) {
    const KrakenBoneColliderPreviewSettings& settings =
        collection.GetSettings();
    if (!settings.showPreview ||
        (tipSphere ? !settings.showTipSphere : !settings.showCapsules) ||
        !IsRoleLayerVisible(role, settings)) {
        return false;
    }
    if (!settings.showAllChains &&
        chainIndex != collection.GetDisplayChainIndex()) {
        return false;
    }
    if (settings.showOnlySelected &&
        !collection.IsSelected(chainIndex, localIndex, tipSphere)) {
        return false;
    }
    return true;
}

void ResetCurrentCounts(KrakenBoneColliderPhaseDiagnostics& diagnostics) {
    diagnostics.currentColliderCount = 0;
    diagnostics.currentActiveCount = 0;
    diagnostics.currentInactiveCount = 0;
    diagnostics.currentPreviewVisibleCount = 0;
    diagnostics.currentAttackColliderCount = 0;
    diagnostics.currentAttackActiveCount = 0;
    diagnostics.currentAttackInactiveCount = 0;
    diagnostics.currentDamageColliderCount = 0;
    diagnostics.currentDamageActiveCount = 0;
    diagnostics.currentDamageInactiveCount = 0;
    diagnostics.currentWeakPointColliderCount = 0;
    diagnostics.currentWeakPointActiveCount = 0;
    diagnostics.currentWeakPointInactiveCount = 0;
    diagnostics.currentGameplayRegisteredCount = 0;
    diagnostics.nullChainPreviewCount = 0;
}

void RecordRoleEvaluation(
    KrakenBoneColliderPhaseDiagnostics& diagnostics,
    KrakenColliderPreviewRole role,
    bool active) {
    ++diagnostics.phaseEvaluationCount;
    ++diagnostics.currentColliderCount;
    active ? ++diagnostics.currentActiveCount :
        ++diagnostics.currentInactiveCount;

    switch (role) {
    case KrakenColliderPreviewRole::Attack:
        ++diagnostics.attackEvaluationCount;
        ++diagnostics.currentAttackColliderCount;
        active ? ++diagnostics.currentAttackActiveCount :
            ++diagnostics.currentAttackInactiveCount;
        break;
    case KrakenColliderPreviewRole::Damage:
        ++diagnostics.damageEvaluationCount;
        ++diagnostics.currentDamageColliderCount;
        active ? ++diagnostics.currentDamageActiveCount :
            ++diagnostics.currentDamageInactiveCount;
        break;
    case KrakenColliderPreviewRole::WeakPoint:
        ++diagnostics.weakPointEvaluationCount;
        ++diagnostics.currentWeakPointColliderCount;
        active ? ++diagnostics.currentWeakPointActiveCount :
            ++diagnostics.currentWeakPointInactiveCount;
        break;
    default:
        break;
    }
}

template <typename Collider>
void ApplyPhaseEvaluation(
    Collider& collider,
    const KrakenTentacleColliderPhaseEvaluation& result,
    const KrakenColliderPhaseMotionSnapshot& snapshot,
    std::uint64_t frameIndex,
    KrakenBoneColliderPhaseDiagnostics& diagnostics) {
    if (collider.gameplayRegistered) {
        ++diagnostics.gameplayRegistrationDetectionCount;
    }
    collider.gameplayRegistered = false;

    if (collider.phaseActive != result.active) {
        if (collider.lastPhaseTransitionFrame != kNoTransitionFrame &&
            collider.lastPhaseTransitionFrame == frameIndex) {
            ++diagnostics.sameFrameDoubleTransitionCount;
        }
        collider.lastPhaseTransitionFrame = frameIndex;
        diagnostics.lastTransitionFrame = frameIndex;
        diagnostics.lastTransitionTime =
            std::isfinite(snapshot.motionElapsedTime) ?
            snapshot.motionElapsedTime : 0.0f;
        diagnostics.hasLastTransition = true;
        if (result.active) {
            ++diagnostics.activationTransitionCount;
            diagnostics.lastActivatedPhase = snapshot.phase;
            diagnostics.hasLastActivation = true;
        } else {
            ++diagnostics.deactivationTransitionCount;
            diagnostics.lastDeactivatedPhase = snapshot.phase;
            diagnostics.hasLastDeactivation = true;
        }
    }

    collider.phaseActive = result.active;
    collider.phaseReason = result.reason;
}

template <typename Collider>
void ClearColliderState(Collider& collider) {
    collider.previewVisible = false;
    collider.phaseActive = false;
    collider.gameplayRegistered = false;
    collider.phaseReason = KrakenColliderPhaseReason::MotionStateInvalid;
    collider.lastPhaseTransitionFrame = kNoTransitionFrame;
}

} // namespace

void SkinningEditorKrakenBoneColliderPhaseControl::Reset() {
    settings_ = {};
    diagnostics_ = {};
}

void SkinningEditorKrakenBoneColliderPhaseControl::Reset(
    SkinningEditorKrakenBoneColliderPreviewCollection& collection) {
    Reset();
    ClearColliderStates(collection);
}

void SkinningEditorKrakenBoneColliderPhaseControl::Finalize(
    SkinningEditorKrakenBoneColliderPreviewCollection& collection) {
    ClearColliderStates(collection);
    diagnostics_ = {};
}

void SkinningEditorKrakenBoneColliderPhaseControl::Evaluate(
    SkinningEditorKrakenBoneColliderPreviewCollection& collection,
    const KrakenColliderPhaseMotionSnapshot& snapshot,
    std::uint64_t frameIndex) {
    ++diagnostics_.evaluationPassCount;
    ResetCurrentCounts(diagnostics_);
    diagnostics_.snapshot = snapshot;
    diagnostics_.lastWarning.clear();

    const bool invalidActiveStartRatio =
        !std::isfinite(settings_.attackActiveStartRatio);
    settings_ = SanitizeKrakenTentacleColliderPhaseSettings(settings_);
    if (invalidActiveStartRatio) {
        diagnostics_.lastWarning =
            "振り下ろし有効開始進行率が不正なため推奨値へ戻しました。";
    }

    KrakenTentacleColliderPhaseState phaseState{};
    phaseState.phase = ToColliderAttackPhase(snapshot.phase);
    phaseState.chainCount = collection.GetChainCount();
    phaseState.selectedChainIndex = snapshot.selectedChainIndex;
    phaseState.motionElapsedTime = snapshot.motionElapsedTime;
    phaseState.phaseElapsedTime = snapshot.phaseElapsedTime;
    phaseState.phaseDuration = snapshot.phaseDuration;
    phaseState.phaseNormalizedTime = snapshot.phaseNormalizedTime;
    phaseState.slamDuration = snapshot.slamDuration;
    phaseState.connected = snapshot.connected;
    phaseState.safetyRecovery = snapshot.safetyRecovery;
    phaseState.motionStateValid =
        snapshot.valid && IsKnownMotionMode(snapshot.motionMode);
    phaseState.attackMotionActive = snapshot.motionMode ==
        KrakenColliderPhaseMotionMode::AttackSlamPreview;
    phaseState.waitingForLoop = snapshot.waitingForLoop;
    const KrakenTentacleColliderPhaseContext context =
        BuildKrakenTentacleColliderPhaseContext(
            phaseState,
            settings_);
    diagnostics_.slamProgress = context.slamProgress;

    if (!snapshot.connected) {
        ++diagnostics_.disconnectedSnapshotCount;
        diagnostics_.lastWarning =
            "スケルトン未接続のため全コライダーを無効予定にしました。";
    } else if (snapshot.safetyRecovery) {
        ++diagnostics_.safetyRecoveryCount;
        diagnostics_.lastWarning =
            "スキニング安全復帰中のため攻撃コライダーを無効予定にしました。";
    } else if (!phaseState.motionStateValid ||
        !context.snapshotScalarsFinite) {
        ++diagnostics_.invalidMotionSnapshotCount;
        diagnostics_.lastWarning =
            "動作状態が不正なため攻撃コライダーを無効予定にしました。";
    } else if (phaseState.attackMotionActive &&
        !context.selectedChainValid) {
        ++diagnostics_.outOfRangeChainCount;
        diagnostics_.lastWarning =
            "攻撃対象チェーンが範囲外のため全攻撃コライダーを無効予定にしました。";
    } else if (phaseState.attackMotionActive &&
        !context.phaseKnown) {
        ++diagnostics_.invalidPhaseCount;
        diagnostics_.lastWarning =
            "攻撃フェーズが不正なため全攻撃コライダーを無効予定にしました。";
    } else if (phaseState.attackMotionActive &&
        !context.slamDurationValid) {
        ++diagnostics_.invalidSlamDurationCount;
        diagnostics_.lastWarning =
            "振り下ろし時間が不正または極小のため攻撃コライダーを無効予定にしました。";
    }

    auto evaluateCollider = [&] (
        auto& collider,
        std::size_t localIndex,
        bool tipSphere) {
        collider.previewVisible = ResolvePreviewVisibility(
            collection,
            collider.role,
            collider.chainIndex,
            localIndex,
            tipSphere);
        const KrakenTentacleColliderPhaseEvaluation result =
            EvaluateKrakenTentacleColliderPhase(
            collider.role,
            collider.enabled,
            collider.valid,
            collider.chainIndex,
            context);
        ApplyPhaseEvaluation(
            collider,
            result,
            snapshot,
            frameIndex,
            diagnostics_);
        RecordRoleEvaluation(diagnostics_, collider.role, result.active);
        if (collider.previewVisible) {
            ++diagnostics_.currentPreviewVisibleCount;
        }
    };

    for (auto& chainPreview : collection.GetChainPreviews()) {
        if (!chainPreview) {
            ++diagnostics_.nullChainPreviewCount;
            continue;
        }
        auto& capsules = chainPreview->GetCapsules();
        for (std::size_t index = 0; index < capsules.size(); ++index) {
            evaluateCollider(capsules[index], index, false);
        }
        evaluateCollider(chainPreview->GetTipSphere(), 0, true);
    }

    // This Step never registers preview colliders with Gameplay collision.
    diagnostics_.currentGameplayRegisteredCount = 0;
}

void SkinningEditorKrakenBoneColliderPhaseControl::DeactivateAll(
    SkinningEditorKrakenBoneColliderPreviewCollection& collection,
    KrakenColliderPhaseReason reason,
    std::uint64_t frameIndex) {
    ResetCurrentCounts(diagnostics_);
    const KrakenColliderPhaseMotionSnapshot& snapshot = diagnostics_.snapshot;
    auto deactivate = [&] (auto& collider) {
        ApplyPhaseEvaluation(
            collider,
            { false, reason },
            snapshot,
            frameIndex,
            diagnostics_);
        RecordRoleEvaluation(diagnostics_, collider.role, false);
        if (collider.previewVisible) {
            ++diagnostics_.currentPreviewVisibleCount;
        }
    };

    for (auto& chainPreview : collection.GetChainPreviews()) {
        if (!chainPreview) {
            ++diagnostics_.nullChainPreviewCount;
            continue;
        }
        for (auto& capsule : chainPreview->GetCapsules()) {
            deactivate(capsule);
        }
        deactivate(chainPreview->GetTipSphere());
    }
    diagnostics_.currentGameplayRegisteredCount = 0;
}

void SkinningEditorKrakenBoneColliderPhaseControl::ResetRecommendedSettings() {
    settings_ = {};
}

void SkinningEditorKrakenBoneColliderPhaseControl::SetSettings(
    const KrakenBoneColliderPhaseControlSettings& settings) {
    settings_ = SanitizeKrakenTentacleColliderPhaseSettings(settings);
}

void SkinningEditorKrakenBoneColliderPhaseControl::ResetDiagnostics() {
    diagnostics_ = {};
}

void SkinningEditorKrakenBoneColliderPhaseControl::ClearColliderStates(
    SkinningEditorKrakenBoneColliderPreviewCollection& collection) {
    for (auto& chainPreview : collection.GetChainPreviews()) {
        if (!chainPreview) {
            continue;
        }
        for (auto& capsule : chainPreview->GetCapsules()) {
            ClearColliderState(capsule);
        }
        ClearColliderState(chainPreview->GetTipSphere());
    }
}

const char* GetKrakenColliderPhaseMotionModeJapaneseLabel(
    KrakenColliderPhaseMotionMode mode) {
    switch (mode) {
    case KrakenColliderPhaseMotionMode::Manual:
        return "手動";
    case KrakenColliderPhaseMotionMode::IdleSway:
        return "待機揺れ";
    case KrakenColliderPhaseMotionMode::AttackSlamPreview:
        return "触手攻撃プレビュー";
    default:
        return "不正な動作モード";
    }
}

const char* GetKrakenColliderPhaseReasonJapaneseLabel(
    KrakenColliderPhaseReason reason) {
    switch (reason) {
    case KrakenColliderPhaseReason::DamageAlwaysActive:
        return "ダメージコライダーは常時有効予定です。";
    case KrakenColliderPhaseReason::WeakPointAlwaysActive:
        return "弱点コライダーは常時有効予定です。";
    case KrakenColliderPhaseReason::AttackSlamLateActive:
        return "振り下ろし進行率が有効開始値へ到達しました。";
    case KrakenColliderPhaseReason::AttackImpactHoldActive:
        return "打撃位置停止中です。";
    case KrakenColliderPhaseReason::ColliderDisabled:
        return "コライダーが無効設定です。";
    case KrakenColliderPhaseReason::ColliderInvalid:
        return "コライダーのボーン情報または形状が不正です。";
    case KrakenColliderPhaseReason::MotionStateInvalid:
        return "動作状態が不正なため無効です。";
    case KrakenColliderPhaseReason::PreviewDisconnected:
        return "スケルトン未接続のため無効です。";
    case KrakenColliderPhaseReason::SafetyRecovery:
        return "スキニング安全復帰中のため無効です。";
    case KrakenColliderPhaseReason::NotAttackMotionMode:
        return "攻撃モーション以外のため無効です。";
    case KrakenColliderPhaseReason::AttackChainOutOfRange:
        return "攻撃対象チェーンが範囲外のため無効です。";
    case KrakenColliderPhaseReason::DifferentAttackChain:
        return "別の触手チェーンが攻撃中です。";
    case KrakenColliderPhaseReason::WindupInactive:
        return "振りかぶり中のため無効です。";
    case KrakenColliderPhaseReason::WindupHoldInactive:
        return "振りかぶり停止中のため無効です。";
    case KrakenColliderPhaseReason::SlamBeforeThreshold:
        return "振り下ろし進行率が有効開始値未満です。";
    case KrakenColliderPhaseReason::InvalidSlamDuration:
        return "振り下ろし時間が不正または極小のため無効です。";
    case KrakenColliderPhaseReason::ImpactHoldDisabled:
        return "打撃位置停止の有効予定設定がオフです。";
    case KrakenColliderPhaseReason::LoopWaitInactive:
        return "ループ待機中のため無効です。";
    case KrakenColliderPhaseReason::RecoveryInactive:
        return "復帰中のため無効です。";
    case KrakenColliderPhaseReason::CompletedInactive:
        return "攻撃完了中のため無効です。";
    case KrakenColliderPhaseReason::UnknownPhase:
        return "不正な攻撃フェーズのため無効です。";
    case KrakenColliderPhaseReason::UnknownRole:
        return "不正なコライダー役割のため無効です。";
    default:
        return "不明なフェーズ判定理由です。";
    }
}
