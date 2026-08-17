#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"

#include "Engine/Animation/Skeleton.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr std::size_t kExpectedCapsulesPerChain = 4;
    constexpr std::uint64_t kColliderSlotsPerChain = 5;

    bool IsFinite(const Vector3& value) {
        return std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
    }

    Vector3 TransformPosition(
        const Vector3& value,
        const Matrix4x4& matrix) {
        return {
            value.x * matrix.m[0][0] +
                value.y * matrix.m[1][0] +
                value.z * matrix.m[2][0] + matrix.m[3][0],
            value.x * matrix.m[0][1] +
                value.y * matrix.m[1][1] +
                value.z * matrix.m[2][1] + matrix.m[3][1],
            value.x * matrix.m[0][2] +
                value.y * matrix.m[1][2] +
                value.z * matrix.m[2][2] + matrix.m[3][2],
        };
    }

    std::size_t FindChainIndex(
        int jointIndex,
        const std::vector<KrakenTentacleChain>& chains) {
        for (std::size_t chainIndex = 0;
            chainIndex < chains.size(); ++chainIndex) {
            const std::vector<int>& joints = chains[chainIndex].joints;
            if (std::find(joints.begin(), joints.end(), jointIndex) !=
                joints.end()) {
                return chainIndex;
            }
        }
        return static_cast<std::size_t>(-1);
    }
}

bool KrakenTentacleMidbossController::Impl::RebuildColliderDefinitions() {
    colliderDefinitions.clear();
    if (!skeleton || chains.empty()) {
        lastError = "Collider定義に必要なSkeletonまたはChainがありません。";
        return false;
    }
    colliderDefinitions.reserve(chains.size());
    for (std::size_t chainIndex = 0;
        chainIndex < chains.size(); ++chainIndex) {
        const KrakenTentacleChain& chain = chains[chainIndex];
        if (chain.joints.empty()) {
            lastError = "Collider定義の対象Chainが空です。";
            return false;
        }
        const int tipJointIndex = chain.joints.back();
        if (tipJointIndex < 0 ||
            static_cast<std::size_t>(tipJointIndex) >=
                skeleton->joints.size()) {
            lastError = "Collider定義のTip Jointが範囲外です。";
            return false;
        }
        KrakenTentacleColliderDefinitionResult result =
            BuildKrakenTentacleColliderDefinitions(
                static_cast<std::uint32_t>(chains.size()),
                static_cast<std::uint32_t>(chainIndex),
                chain.joints,
                skeleton->root,
                skeleton->joints.size(),
                skeleton->joints[
                    static_cast<std::size_t>(tipJointIndex)].worldTranslate);
        if (!result.valid ||
            result.capsules.size() != kExpectedCapsulesPerChain) {
            lastError = result.error ==
                    KrakenTentacleColliderDefinitionError::None
                ? "1 Chainあたり4個のCapsuleを生成できませんでした。"
                : GetKrakenTentacleColliderDefinitionErrorJapaneseLabel(
                    result.error);
            colliderDefinitions.clear();
            return false;
        }
        colliderDefinitions.push_back(std::move(result));
    }
    return true;
}

void KrakenTentacleMidbossController::Impl::RefreshBoneSnapshots() {
    boneSnapshots.clear();
    if (!skeleton) {
        return;
    }
    boneSnapshots.reserve(skeleton->joints.size());
    for (const Joint& joint : skeleton->joints) {
        KrakenTentacleMidbossBoneSnapshot snapshot{};
        snapshot.jointIndex = joint.index;
        snapshot.parentIndex = joint.parentIndex;
        snapshot.chainIndex = FindChainIndex(joint.index, chains);
        snapshot.worldPosition = TransformPosition(
            joint.worldTranslate, worldMatrix);
        snapshot.isRoot = joint.index == skeleton->root;
        snapshot.valid = IsFinite(snapshot.worldPosition) &&
            joint.index >= 0 &&
            static_cast<std::size_t>(joint.index) <
                skeleton->joints.size();
        boneSnapshots.push_back(snapshot);
    }
}

void KrakenTentacleMidbossController::Impl::RefreshColliderSnapshots() {
    capsuleSnapshots.clear();
    tipSnapshots.clear();
    diagnostics.colliderCount = 0;
    diagnostics.attackColliderCount = 0;
    diagnostics.damageColliderCount = 0;
    diagnostics.weakPointCount = 0;
    diagnostics.phaseActiveCount = 0;
    diagnostics.gameplayRegisteredCount = 0;
    diagnostics.collisionRegistrationRequestedCount = 0;
    diagnostics.collisionRegistrationFailureCount = 0;
    diagnostics.registeredAttackColliderCount = 0;
    diagnostics.registeredDamageColliderCount = 0;
    diagnostics.registeredWeakPointCount = 0;
    diagnostics.invalidColliderJointCount = 0;
    diagnostics.zeroLengthColliderCount = 0;
    diagnostics.nonFiniteColliderCount = 0;
    if (!skeleton || colliderDefinitions.size() != chains.size()) {
        return;
    }

    const float phaseDuration = GetCurrentStateDuration();
    KrakenTentacleColliderPhaseState phaseState{};
    phaseState.phase = GetColliderAttackPhase();
    phaseState.chainCount = chains.size();
    phaseState.selectedChainIndex = selectedAttackChainIndex;
    phaseState.motionElapsedTime = attackElapsedTime;
    phaseState.phaseElapsedTime = stateElapsedTime;
    phaseState.phaseDuration = phaseDuration;
    phaseState.phaseNormalizedTime = phaseDuration > 0.0f
        ? std::clamp(stateElapsedTime / phaseDuration, 0.0f, 1.0f)
        : 0.0f;
    phaseState.slamDuration = GetKrakenTentacleAttackPhaseDuration(
        attackSettings, KrakenTentacleAttackPreviewPhase::Slam);
    phaseState.connected = IsVisible();
    phaseState.safetyRecovery = safetyStopped;
    phaseState.motionStateValid = IsVisible();
    phaseState.attackMotionActive = IsAttackState();
    phaseState.waitingForLoop = false;
    const KrakenTentacleColliderPhaseContext phaseContext =
        BuildKrakenTentacleColliderPhaseContext(
            phaseState, colliderPhaseSettings);

    for (const KrakenTentacleColliderDefinitionResult& definitions :
        colliderDefinitions) {
        for (const KrakenTentacleCapsuleColliderDefinition& definition :
            definitions.capsules) {
            const KrakenTentacleCapsuleColliderEvaluation evaluation =
                EvaluateKrakenTentacleCapsuleCollider(
                    *skeleton,
                    worldMatrix,
                    skeleton->root,
                    definition.startJointIndex,
                    definition.endJointIndex,
                    definition.recommendedLocalRadius,
                    colliderRadiusScale,
                    colliderGlobalRadiusScale);
            const KrakenTentacleColliderPhaseEvaluation phase =
                EvaluateKrakenTentacleColliderPhase(
                    definition.role,
                    true,
                    evaluation.valid,
                    definition.chainIndex,
                    phaseContext);
            KrakenTentacleMidbossCapsuleSnapshot snapshot{};
            snapshot.colliderId =
                static_cast<std::uint64_t>(definition.chainIndex) *
                    kColliderSlotsPerChain +
                definition.colliderIndex + 1;
            snapshot.chainIndex = definition.chainIndex;
            snapshot.colliderIndex = definition.colliderIndex;
            snapshot.role = definition.role;
            snapshot.startJointIndex = definition.startJointIndex;
            snapshot.endJointIndex = definition.endJointIndex;
            snapshot.worldStart = evaluation.worldStart;
            snapshot.worldEnd = evaluation.worldEnd;
            snapshot.worldCenter = evaluation.worldCenter;
            snapshot.worldRadius = evaluation.worldRadius;
            snapshot.worldLength = evaluation.worldLength;
            snapshot.valid = evaluation.valid;
            snapshot.zeroLength = evaluation.zeroLength;
            snapshot.phaseActive = phase.active;
            snapshot.phaseReason = phase.reason;
            capsuleSnapshots.push_back(snapshot);

            ++diagnostics.colliderCount;
            if (definition.role == KrakenColliderPreviewRole::Attack) {
                ++diagnostics.attackColliderCount;
                if (definition.chainIndex == selectedAttackChainIndex) {
                    diagnostics.lastPhaseReason = phase.reason;
                }
            } else {
                ++diagnostics.damageColliderCount;
            }
            diagnostics.phaseActiveCount += phase.active ? 1 : 0;
            diagnostics.invalidColliderJointCount +=
                (!evaluation.startJointValid || !evaluation.endJointValid)
                ? 1 : 0;
            diagnostics.zeroLengthColliderCount +=
                evaluation.zeroLength ? 1 : 0;
            diagnostics.nonFiniteColliderCount +=
                (!evaluation.positionsFinite || !evaluation.radiusFinite)
                ? 1 : 0;
        }

        const KrakenTentacleTipSphereColliderDefinition& definition =
            definitions.tipSphere;
        const KrakenTentacleTipSphereColliderEvaluation evaluation =
            EvaluateKrakenTentacleTipSphereCollider(
                *skeleton,
                worldMatrix,
                skeleton->root,
                definition.tipJointIndex,
                definition.bindTipSkeletonPosition,
                true,
                definition.recommendedLocalRadius,
                colliderRadiusScale,
                colliderGlobalRadiusScale);
        const KrakenTentacleColliderPhaseEvaluation phase =
            EvaluateKrakenTentacleColliderPhase(
                definition.role,
                true,
                evaluation.valid,
                definition.chainIndex,
                phaseContext);
        KrakenTentacleMidbossTipSnapshot snapshot{};
        snapshot.colliderId =
            static_cast<std::uint64_t>(definition.chainIndex) *
                kColliderSlotsPerChain +
            kColliderSlotsPerChain;
        snapshot.chainIndex = definition.chainIndex;
        snapshot.role = definition.role;
        snapshot.jointIndex = definition.tipJointIndex;
        snapshot.worldPosition = evaluation.worldPosition;
        snapshot.bindWorldPosition = evaluation.bindWorldPosition;
        snapshot.worldRadius = evaluation.worldRadius;
        snapshot.distanceFromBind = evaluation.distanceFromBind;
        snapshot.valid = evaluation.valid;
        snapshot.phaseActive = phase.active;
        snapshot.phaseReason = phase.reason;
        tipSnapshots.push_back(snapshot);

        ++diagnostics.colliderCount;
        ++diagnostics.weakPointCount;
        diagnostics.phaseActiveCount += phase.active ? 1 : 0;
        diagnostics.invalidColliderJointCount +=
            evaluation.jointValid ? 0 : 1;
        diagnostics.nonFiniteColliderCount +=
            (!evaluation.positionsFinite || !evaluation.radiusFinite)
            ? 1 : 0;
    }
}
