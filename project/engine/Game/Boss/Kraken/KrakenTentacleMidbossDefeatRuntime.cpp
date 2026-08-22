#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"

#include "Engine/Animation/Skeleton.h"
#include "Engine/Game/Player/PlayerBulletManager.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
bool IsFinite(const Vector3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

Vector3 InvalidPosition() {
    const float invalid = std::numeric_limits<float>::quiet_NaN();
    return { invalid, invalid, invalid };
}

Vector3 TransformPosition(const Vector3& value, const Matrix4x4& matrix) {
    return {
        value.x * matrix.m[0][0] + value.y * matrix.m[1][0] +
            value.z * matrix.m[2][0] + matrix.m[3][0],
        value.x * matrix.m[0][1] + value.y * matrix.m[1][1] +
            value.z * matrix.m[2][1] + matrix.m[3][1],
        value.x * matrix.m[0][2] + value.y * matrix.m[1][2] +
            value.z * matrix.m[2][2] + matrix.m[3][2],
    };
}

bool IsFinitePose(const KrakenTentacleMidbossBindLocalPose& pose) {
    return IsFinite(pose.translate) && IsFinite(pose.rotate) &&
        IsFinite(pose.scale);
}
}

void KrakenTentacleMidbossController::Impl::SetEffectContext(
    CombatEffectController* combatEffectControllerValue,
    EnemyDefeatEffectController* defeatEffectControllerValue,
    ImpactDistortionController* impactDistortionControllerValue) {
    effectController.SetContext(
        combatEffectControllerValue,
        defeatEffectControllerValue,
        impactDistortionControllerValue);
}

bool KrakenTentacleMidbossController::Impl::IsDefeatState() const {
    return state == KrakenTentacleMidbossState::Defeated ||
        state == KrakenTentacleMidbossState::Retreating ||
        state == KrakenTentacleMidbossState::RetreatCompleted;
}

bool KrakenTentacleMidbossController::Impl::CaptureDefeatFrozenPose() {
    defeatFrozenPose.clear();
    defeatFrozenPoseValid = false;
    bool capturedCurrentPose = skeleton &&
        skeleton->joints.size() == bindPose.size();
    if (capturedCurrentPose) {
        defeatFrozenPose.reserve(skeleton->joints.size());
        for (const Joint& joint : skeleton->joints) {
            KrakenTentacleMidbossBindLocalPose pose{
                joint.localTranslate, joint.localRotate, joint.localScale };
            if (!IsFinitePose(pose)) {
                capturedCurrentPose = false;
                break;
            }
            defeatFrozenPose.push_back(pose);
        }
    }
    if (capturedCurrentPose &&
        defeatFrozenPose.size() == skeleton->joints.size()) {
        defeatFrozenPoseValid = true;
        ++defeatDiagnostics.frozenPoseCaptureSuccessCount;
        return true;
    }

    defeatFrozenPose.clear();
    ++defeatDiagnostics.frozenPoseCaptureFailureCount;
    const bool bindPoseValid = skeleton &&
        bindPose.size() == skeleton->joints.size() &&
        std::all_of(bindPose.begin(), bindPose.end(), IsFinitePose);
    if (!bindPoseValid) {
        defeatDiagnostics.lastError =
            "撃破時の固定姿勢と初期姿勢の両方を保存できませんでした。";
        return false;
    }
    defeatFrozenPose = bindPose;
    defeatFrozenPoseValid = true;
    defeatDiagnostics.lastWarning =
        "撃破時の固定姿勢を保存できなかったため初期姿勢を使用します。";
    return true;
}

bool KrakenTentacleMidbossController::Impl::ApplyDefeatFrozenPose() {
    if (!skeleton || !defeatFrozenPoseValid ||
        defeatFrozenPose.size() != skeleton->joints.size()) {
        return false;
    }
    for (std::size_t index = 0; index < defeatFrozenPose.size(); ++index) {
        const KrakenTentacleMidbossBindLocalPose& pose =
            defeatFrozenPose[index];
        if (!IsFinitePose(pose)) {
            return false;
        }
        Joint& joint = skeleton->joints[index];
        joint.localTranslate = pose.translate;
        joint.localRotate = pose.rotate;
        joint.localScale = pose.scale;
    }
    return true;
}

KrakenTentacleEffectPositionCandidates
KrakenTentacleMidbossController::Impl::BuildHitEffectPositionCandidates(
    const KrakenProjectileEnterEvent& event) const {
    KrakenTentacleEffectPositionCandidates candidates{};
    candidates.count = 5;
    candidates.values[0] = { event.closestPoint,
        KrakenTentacleEffectPositionSource::CollisionClosestPoint };

    Vector3 colliderCenter = InvalidPosition();
    for (const KrakenTentacleMidbossCapsuleSnapshot& snapshot :
        capsuleSnapshots) {
        if (snapshot.colliderId == event.krakenColliderId && snapshot.valid) {
            colliderCenter = snapshot.worldCenter;
            break;
        }
    }
    if (!IsFinite(colliderCenter)) {
        for (const KrakenTentacleMidbossTipSnapshot& snapshot : tipSnapshots) {
            if (snapshot.colliderId == event.krakenColliderId &&
                snapshot.valid) {
                colliderCenter = snapshot.worldPosition;
                break;
            }
        }
    }
    candidates.values[1] = { colliderCenter,
        KrakenTentacleEffectPositionSource::ColliderWorldCenter };

    Vector3 projectileCenter = InvalidPosition();
    if (projectileDamageBulletManager) {
        const auto snapshots =
            projectileDamageBulletManager->GetActiveCollisionSnapshots();
        const auto found = std::find_if(
            snapshots.begin(), snapshots.end(),
            [&event](const auto& snapshot) {
                return snapshot.runtimeId == event.projectileRuntimeId &&
                    snapshot.active && !snapshot.killed;
            });
        if (found != snapshots.end()) {
            projectileCenter = found->worldPosition;
        }
    }
    candidates.values[2] = { projectileCenter,
        KrakenTentacleEffectPositionSource::ProjectileWorldCenter };

    const Vector3 boundsCenter = diagnostics.skinnedBounds.valid
        ? TransformPosition(diagnostics.skinnedBounds.center, worldMatrix)
        : InvalidPosition();
    candidates.values[3] = { boundsCenter,
        KrakenTentacleEffectPositionSource::SkinnedBoundsWorldCenter };
    candidates.values[4] = { worldPosition,
        KrakenTentacleEffectPositionSource::MidbossWorldPosition };
    return candidates;
}

KrakenTentacleEffectPositionCandidates
KrakenTentacleMidbossController::Impl::
BuildDefeatEffectPositionCandidates() const {
    KrakenTentacleEffectPositionCandidates candidates{};
    candidates.count = 4;
    const Vector3 boundsCenter = diagnostics.skinnedBounds.valid
        ? TransformPosition(diagnostics.skinnedBounds.center, worldMatrix)
        : InvalidPosition();
    candidates.values[0] = { boundsCenter,
        KrakenTentacleEffectPositionSource::SkinnedBoundsWorldCenter };

    Vector3 tipAverage{};
    std::size_t validTipCount = 0;
    for (const KrakenTentacleMidbossTipSnapshot& tip : tipSnapshots) {
        if (!tip.valid || !IsFinite(tip.worldPosition)) {
            continue;
        }
        tipAverage.x += tip.worldPosition.x;
        tipAverage.y += tip.worldPosition.y;
        tipAverage.z += tip.worldPosition.z;
        ++validTipCount;
    }
    if (validTipCount == chains.size() && validTipCount > 0) {
        const float inverseCount = 1.0f / static_cast<float>(validTipCount);
        tipAverage.x *= inverseCount;
        tipAverage.y *= inverseCount;
        tipAverage.z *= inverseCount;
    } else {
        tipAverage = InvalidPosition();
    }
    candidates.values[1] = { tipAverage,
        KrakenTentacleEffectPositionSource::TentacleTipAverage };

    Vector3 rootWorldPosition = InvalidPosition();
    const auto root = std::find_if(
        boneSnapshots.begin(), boneSnapshots.end(),
        [](const KrakenTentacleMidbossBoneSnapshot& snapshot) {
            return snapshot.isRoot && snapshot.valid;
        });
    if (root != boneSnapshots.end()) {
        rootWorldPosition = root->worldPosition;
    }
    candidates.values[2] = { rootWorldPosition,
        KrakenTentacleEffectPositionSource::SkeletonRootWorldPosition };
    candidates.values[3] = { worldPosition,
        KrakenTentacleEffectPositionSource::MidbossWorldPosition };
    return candidates;
}

bool KrakenTentacleMidbossController::Impl::BeginDefeat() {
    if (defeatStarted) {
        ++defeatDiagnostics.duplicateBeginSuppressionCount;
        return false;
    }
    if (!initialized || state == KrakenTentacleMidbossState::Hidden ||
        !health.IsDefeatPending() || defeatFinalizing || safetyStopped) {
        defeatDiagnostics.lastWarning =
            "撃破開始条件を満たしていないため開始を拒否しました。";
        return false;
    }
    if (!health.IsValid() || !std::isfinite(health.GetCurrentHp()) ||
        !IsFinite(worldPosition) || !skeleton || !model) {
        defeatDiagnostics.lastError =
            "撃破開始に必要なHP、位置、モデルまたはスケルトンが不正です。";
        EnterHidden(defeatDiagnostics.lastError, true);
        return false;
    }
    if (!IsKrakenTentacleDefeatSettingsValid(defeatSettings)) {
        if (!std::isfinite(defeatSettings.holdTime) ||
            !std::isfinite(defeatSettings.retreatDuration)) {
            ++defeatDiagnostics.nonFiniteTimeCount;
        }
        if (!std::isfinite(defeatSettings.retreatDistance) ||
            defeatSettings.retreatDistance < 0.0f) {
            ++defeatDiagnostics.invalidDistanceCount;
        }
        defeatDiagnostics.lastError = "撃破・落下設定が不正です。";
        EnterHidden(defeatDiagnostics.lastError, true);
        return false;
    }

    defeatStartWorldPosition = worldPosition;
    defeatStartWorldPositionValid = true;
    if (!CaptureDefeatFrozenPose()) {
        EnterHidden(defeatDiagnostics.lastError, true);
        return false;
    }

    defeatStarted = true;
    defeatCompleted = false;
    defeatBeganThisUpdate = true;
    ++defeatSequenceId;
    if (defeatSequenceId == 0) {
        defeatSequenceId = 1;
    }
    ++defeatDiagnostics.beginCount;
    attackDamageEnabled = false;
    projectileDamageEnabled = false;
    InvalidateAttackDamageSequence();
    ResetCollisionQueryState(false);

    const KrakenTentacleMidbossState stateAtSpawn = state;
    effectController.PlayDefeatEffect(
        defeatSequenceId,
        stateAtSpawn,
        BuildDefeatEffectPositionCandidates());
    EnterState(KrakenTentacleMidbossState::Defeated);
    RefreshColliderSnapshots();
    RefreshCollisionRegistrationState();
    defeatDiagnostics.lastError.clear();
    return true;
}

bool KrakenTentacleMidbossController::Impl::UpdateDefeatMotion(
    float deltaTime) {
    if (!IsDefeatState()) {
        return false;
    }
    if (state == KrakenTentacleMidbossState::RetreatCompleted) {
        diagnostics.cpuSkinningUpdateCount = 0;
        diagnostics.computeDispatchCount = 0;
        diagnostics.drawCallCount = 0;
        return true;
    }
    if (defeatBeganThisUpdate) {
        worldPosition = defeatStartWorldPosition;
        return true;
    }
    if (!std::isfinite(deltaTime) || deltaTime < 0.0f ||
        !defeatStartWorldPositionValid ||
        !IsKrakenTentacleDefeatSettingsValid(defeatSettings)) {
        ++defeatDiagnostics.nonFiniteTimeCount;
        defeatDiagnostics.lastError =
            "撃破落下の時間または開始位置が不正です。";
        EnterHidden(defeatDiagnostics.lastError, true);
        return true;
    }

    const KrakenTentacleDefeatMotionPhase phase =
        state == KrakenTentacleMidbossState::Defeated
        ? KrakenTentacleDefeatMotionPhase::Defeated
        : KrakenTentacleDefeatMotionPhase::Retreating;
    const KrakenTentacleDefeatAdvanceResult advance =
        AdvanceKrakenTentacleDefeatMotion(
            phase,
            stateElapsedTime,
            deltaTime,
            defeatStartWorldPosition,
            defeatSettings);
    if (!advance.valid) {
        ++defeatDiagnostics.nonFinitePositionCount;
        defeatDiagnostics.lastError = "撃破落下位置の評価に失敗しました。";
        EnterHidden(defeatDiagnostics.lastError, true);
        return true;
    }
    worldPosition = advance.motion.worldPosition;
    stateElapsedTime = advance.stateElapsedTime;
    defeatDiagnostics.retreatProgress = advance.motion.normalizedTime;
    defeatDiagnostics.easedRetreatProgress = advance.motion.easedTime;
    if (advance.beganRetreat) {
        state = KrakenTentacleMidbossState::Retreating;
        ++defeatDiagnostics.retreatBeginCount;
    }
    if (!advance.completedNow) {
        return true;
    }

    state = KrakenTentacleMidbossState::RetreatCompleted;
    stateElapsedTime = 0.0f;
    defeatCompleted = true;
    ++defeatDiagnostics.retreatCompleteCount;
    attackDamageEnabled = false;
    projectileDamageEnabled = false;
    ResetCollisionQueryState(false);
    diagnostics.cpuSkinningUpdateCount = 0;
    diagnostics.computeDispatchCount = 0;
    diagnostics.drawCallCount = 0;
    diagnostics.materialBindingCount = 0;
    return true;
}

void KrakenTentacleMidbossController::Impl::AbortDefeatForHide() {
    if (!defeatStarted && !defeatCompleted && defeatFrozenPose.empty()) {
        return;
    }
    defeatStarted = false;
    defeatCompleted = false;
    defeatBeganThisUpdate = false;
    defeatFrozenPoseValid = false;
    defeatFrozenPose.clear();
    attackDamageEnabled = false;
    projectileDamageEnabled = false;
    ResetCollisionQueryState(false);
}

void KrakenTentacleMidbossController::Impl::ResetDefeatState(
    bool resetSettings,
    bool restoreWorldPosition) {
    if (restoreWorldPosition && defeatStartWorldPositionValid &&
        IsFinite(defeatStartWorldPosition)) {
        worldPosition = defeatStartWorldPosition;
    }
    defeatStarted = false;
    defeatCompleted = false;
    defeatFrozenPoseValid = false;
    defeatStartWorldPositionValid = false;
    defeatBeganThisUpdate = false;
    defeatFrozenPose.clear();
    defeatStartWorldPosition = {};
    defeatSequenceId = 0;
    defeatDiagnostics = {};
    effectController.Reset(resetSettings);
    if (resetSettings) {
        defeatSettings = {};
    }
}

void KrakenTentacleMidbossController::Impl::RecoverFromDefeat() {
    if (!health.HealFull()) {
        defeatDiagnostics.lastError = "中ボスHPを全回復できませんでした。";
        return;
    }
    const Vector3 returnPosition = defeatStartWorldPositionValid
        ? defeatStartWorldPosition : worldPosition;
    ResetDefeatState(false, false);
    worldPosition = returnPosition;
    consumedProjectileIds.clear();
    aggregatedProjectileEventsThisFrame.clear();
    projectileDamageDiagnostics.lastHit = {};
    projectileDamageDiagnostics.lastError.clear();
    projectileDamageDiagnostics.lastWarning =
        "撃破状態を解除して全回復しました。";
    state = KrakenTentacleMidbossState::Hidden;
    stateElapsedTime = 0.0f;
    RestoreBindPose();
    if (skeleton) {
        UpdateSkeletonWorldTransforms(*skeleton);
    }
    UpdateObjectTransform();
    RefreshColliderSnapshots();
    RefreshBoneSnapshots();
    ResetCollisionQueryState(false);
}

void KrakenTentacleMidbossController::Impl::ForceDefeatForDebug() {
    if (state == KrakenTentacleMidbossState::Hidden) {
        defeatDiagnostics.lastWarning =
            "非表示中は撃破落下を強制開始できません。";
        return;
    }
    if (!health.ForceDefeatForDebug()) {
        defeatDiagnostics.lastError = "デバッグ用HP 0設定に失敗しました。";
        return;
    }
    BeginDefeat();
}

void KrakenTentacleMidbossController::Impl::RestoreDefeatStartPosition() {
    if (!defeatStartWorldPositionValid ||
        !IsFinite(defeatStartWorldPosition)) {
        defeatDiagnostics.lastWarning = "撃破開始位置は保存されていません。";
        return;
    }
    worldPosition = defeatStartWorldPosition;
}

void KrakenTentacleMidbossController::Impl::ProcessDefeatEffectTest(
    bool weakPoint,
    bool defeatEffect) {
    const KrakenTentacleEffectPositionCandidates candidates =
        BuildDefeatEffectPositionCandidates();
    if (defeatEffect) {
        effectController.PlayTestDefeatEffect(state, candidates);
    } else {
        effectController.PlayTestHitEffect(weakPoint, candidates);
    }
}
