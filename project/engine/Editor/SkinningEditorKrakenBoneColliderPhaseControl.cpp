#include "SkinningEditorKrakenBoneColliderPhaseControl.h"

#include "SkinningEditorKrakenAttackMotion.h"
#include "SkinningEditorKrakenBoneColliderPreviewCollection.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr float kDefaultAttackActiveStartRatio = 0.65f;
constexpr float kMinimumValidSlamDuration = 0.001f;
constexpr std::uint64_t kNoTransitionFrame =
    std::numeric_limits<std::uint64_t>::max();

struct PhaseEvaluationResult {
    bool active = false;
    KrakenColliderPhaseReason reason =
        KrakenColliderPhaseReason::MotionStateInvalid;
};

struct PhaseEvaluationContext {
    const KrakenColliderPhaseMotionSnapshot& snapshot;
    std::size_t chainCount = 0;
    float attackActiveStartRatio = kDefaultAttackActiveStartRatio;
    float slamProgress = 0.0f;
    bool impactHoldActive = true;
    bool snapshotScalarsFinite = false;
    bool motionModeKnown = false;
    bool phaseKnown = false;
    bool selectedChainValid = false;
    bool slamDurationValid = false;
};

float Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

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

bool IsKnownAttackPhase(KrakenTentacleAttackPreviewPhase phase) {
    switch (phase) {
    case KrakenTentacleAttackPreviewPhase::Windup:
    case KrakenTentacleAttackPreviewPhase::WindupHold:
    case KrakenTentacleAttackPreviewPhase::Slam:
    case KrakenTentacleAttackPreviewPhase::ImpactHold:
    case KrakenTentacleAttackPreviewPhase::Recovery:
    case KrakenTentacleAttackPreviewPhase::Completed:
        return true;
    default:
        return false;
    }
}

bool AreSnapshotScalarsFinite(
    const KrakenColliderPhaseMotionSnapshot& snapshot) {
    return std::isfinite(snapshot.motionElapsedTime) &&
        std::isfinite(snapshot.phaseElapsedTime) &&
        std::isfinite(snapshot.phaseDuration) &&
        std::isfinite(snapshot.phaseNormalizedTime);
}

PhaseEvaluationResult EvaluatePhaseState(
    KrakenColliderPreviewRole role,
    bool enabled,
    bool valid,
    std::size_t colliderChainIndex,
    const PhaseEvaluationContext& context) {
    if (!enabled) {
        return { false, KrakenColliderPhaseReason::ColliderDisabled };
    }
    if (!valid) {
        return { false, KrakenColliderPhaseReason::ColliderInvalid };
    }
    if (!context.snapshot.connected) {
        return { false, KrakenColliderPhaseReason::PreviewDisconnected };
    }
    if (role == KrakenColliderPreviewRole::Damage) {
        return { true, KrakenColliderPhaseReason::DamageAlwaysActive };
    }
    if (role == KrakenColliderPreviewRole::WeakPoint) {
        return { true, KrakenColliderPhaseReason::WeakPointAlwaysActive };
    }
    if (role != KrakenColliderPreviewRole::Attack) {
        return { false, KrakenColliderPhaseReason::UnknownRole };
    }
    if (context.snapshot.safetyRecovery) {
        return { false, KrakenColliderPhaseReason::SafetyRecovery };
    }

    if (!context.snapshot.valid || !context.snapshotScalarsFinite ||
        !context.motionModeKnown) {
        return { false, KrakenColliderPhaseReason::MotionStateInvalid };
    }
    if (context.snapshot.motionMode !=
        KrakenColliderPhaseMotionMode::AttackSlamPreview) {
        return { false, KrakenColliderPhaseReason::NotAttackMotionMode };
    }
    if (context.snapshot.waitingForLoop) {
        return { false, KrakenColliderPhaseReason::LoopWaitInactive };
    }
    if (!context.selectedChainValid) {
        return { false, KrakenColliderPhaseReason::AttackChainOutOfRange };
    }
    if (colliderChainIndex != context.snapshot.selectedChainIndex) {
        return { false, KrakenColliderPhaseReason::DifferentAttackChain };
    }
    if (!context.slamDurationValid) {
        return { false, KrakenColliderPhaseReason::InvalidSlamDuration };
    }
    if (!context.phaseKnown) {
        return { false, KrakenColliderPhaseReason::UnknownPhase };
    }

    switch (context.snapshot.phase) {
    case KrakenTentacleAttackPreviewPhase::Windup:
        return { false, KrakenColliderPhaseReason::WindupInactive };
    case KrakenTentacleAttackPreviewPhase::WindupHold:
        return { false, KrakenColliderPhaseReason::WindupHoldInactive };
    case KrakenTentacleAttackPreviewPhase::Slam:
        if (context.slamProgress >= context.attackActiveStartRatio) {
            return { true, KrakenColliderPhaseReason::AttackSlamLateActive };
        }
        return { false, KrakenColliderPhaseReason::SlamBeforeThreshold };
    case KrakenTentacleAttackPreviewPhase::ImpactHold:
        if (context.impactHoldActive) {
            return {
                true,
                KrakenColliderPhaseReason::AttackImpactHoldActive,
            };
        }
        return { false, KrakenColliderPhaseReason::ImpactHoldDisabled };
    case KrakenTentacleAttackPreviewPhase::Recovery:
        return { false, KrakenColliderPhaseReason::RecoveryInactive };
    case KrakenTentacleAttackPreviewPhase::Completed:
        return { false, KrakenColliderPhaseReason::CompletedInactive };
    default:
        return { false, KrakenColliderPhaseReason::UnknownPhase };
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
    const PhaseEvaluationResult& result,
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

    if (!std::isfinite(settings_.attackActiveStartRatio)) {
        settings_.attackActiveStartRatio =
            kDefaultAttackActiveStartRatio;
        diagnostics_.lastWarning =
            "振り下ろし有効開始進行率が不正なため推奨値へ戻しました。";
    }
    settings_.attackActiveStartRatio =
        Clamp01(settings_.attackActiveStartRatio);

    PhaseEvaluationContext context{
        snapshot,
        collection.GetChainCount(),
        settings_.attackActiveStartRatio,
        0.0f,
        settings_.impactHoldActive,
        AreSnapshotScalarsFinite(snapshot),
        IsKnownMotionMode(snapshot.motionMode),
        IsKnownAttackPhase(snapshot.phase),
        snapshot.selectedChainIndex < collection.GetChainCount(),
        std::isfinite(snapshot.slamDuration) &&
            snapshot.slamDuration > kMinimumValidSlamDuration,
    };
    if (context.slamDurationValid &&
        std::isfinite(snapshot.phaseElapsedTime)) {
        context.slamProgress = Clamp01(
            snapshot.phaseElapsedTime /
            (std::max)(snapshot.slamDuration, kMinimumValidSlamDuration));
    }
    diagnostics_.slamProgress = context.slamProgress;

    if (!snapshot.connected) {
        ++diagnostics_.disconnectedSnapshotCount;
        diagnostics_.lastWarning =
            "スケルトン未接続のため全コライダーを無効予定にしました。";
    } else if (snapshot.safetyRecovery) {
        ++diagnostics_.safetyRecoveryCount;
        diagnostics_.lastWarning =
            "スキニング安全復帰中のため攻撃コライダーを無効予定にしました。";
    } else if (!snapshot.valid || !context.snapshotScalarsFinite ||
        !context.motionModeKnown) {
        ++diagnostics_.invalidMotionSnapshotCount;
        diagnostics_.lastWarning =
            "動作状態が不正なため攻撃コライダーを無効予定にしました。";
    } else if (snapshot.motionMode ==
            KrakenColliderPhaseMotionMode::AttackSlamPreview &&
        !context.selectedChainValid) {
        ++diagnostics_.outOfRangeChainCount;
        diagnostics_.lastWarning =
            "攻撃対象チェーンが範囲外のため全攻撃コライダーを無効予定にしました。";
    } else if (snapshot.motionMode ==
            KrakenColliderPhaseMotionMode::AttackSlamPreview &&
        !context.phaseKnown) {
        ++diagnostics_.invalidPhaseCount;
        diagnostics_.lastWarning =
            "攻撃フェーズが不正なため全攻撃コライダーを無効予定にしました。";
    } else if (snapshot.motionMode ==
            KrakenColliderPhaseMotionMode::AttackSlamPreview &&
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
        const PhaseEvaluationResult result = EvaluatePhaseState(
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
    settings_ = settings;
    if (!std::isfinite(settings_.attackActiveStartRatio)) {
        settings_.attackActiveStartRatio =
            kDefaultAttackActiveStartRatio;
    }
    settings_.attackActiveStartRatio =
        Clamp01(settings_.attackActiveStartRatio);
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
