#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"

#include "Engine/Animation/Skeleton.h"
#include "Engine/Graphics/Model/GltfSkinnedModel.h"

#include <cmath>

namespace {
    constexpr std::size_t kExpectedJointCount = 41;
    constexpr std::size_t kExpectedChainCount = 4;
    constexpr std::size_t kExpectedBonesPerChain = 10;
    constexpr const char* kExpectedRootName =
        "Kraken_Tentacle_Rig_Root";

    bool IsFinite(const Vector3& value) {
        return std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
    }

    bool IsFinite(const Matrix4x4& matrix) {
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                if (!std::isfinite(matrix.m[row][column])) {
                    return false;
                }
            }
        }
        return true;
    }
}

bool KrakenTentacleMidbossController::Impl::CaptureBindPoseAndChains() {
    bindPose.clear();
    bindLocalEulerRadians.clear();
    chains.clear();
    colliderDefinitions.clear();
    if (!skeleton || skeleton->GetJoints().size() != kExpectedJointCount ||
        skeleton->GetRootIndex() != 0) {
        lastError = "SkeletonのJoint数またはRoot番号が期待値と一致しません。";
        return false;
    }
    rootName = skeleton->GetJoints().front().name;
    if (rootName != kExpectedRootName) {
        lastError = "SkeletonのRoot名が期待値と一致しません。";
        return false;
    }

    bindPose.reserve(skeleton->GetJoints().size());
    bindLocalEulerRadians.reserve(skeleton->GetJoints().size());
    for (const Joint& joint : skeleton->GetJoints()) {
        if (!IsFinite(joint.localTranslate) ||
            !IsFinite(joint.localRotate) ||
            !IsFinite(joint.localScale)) {
            lastError = "Bind Poseに非有限値のJoint Transformがあります。";
            bindPose.clear();
            bindLocalEulerRadians.clear();
            return false;
        }
        bindPose.push_back({
            joint.localTranslate,
            joint.localRotate,
            joint.localScale,
        });
        bindLocalEulerRadians.push_back(joint.localRotate);
    }

    std::string chainError;
    if (!DetectKrakenTentacleChains(*skeleton, chains, chainError)) {
        lastError = chainError.empty()
            ? "触手Chainの検出に失敗しました。"
            : chainError;
        return false;
    }
    if (chains.size() != kExpectedChainCount) {
        lastError = "触手Chain数が期待値の4本と一致しません。";
        return false;
    }
    for (const KrakenTentacleChain& chain : chains) {
        if (chain.joints.size() != kExpectedBonesPerChain) {
            lastError = "触手ChainのBone数が期待値の10本と一致しません。";
            return false;
        }
    }
    return RebuildColliderDefinitions();
}

bool KrakenTentacleMidbossController::Impl::RestoreBindPose() {
    if (!skeleton || bindPose.size() != skeleton->GetJoints().size()) {
        return false;
    }
    for (std::size_t jointIndex = 0;
        jointIndex < skeleton->GetJoints().size(); ++jointIndex) {
        const KrakenTentacleMidbossBindLocalPose& bind =
            bindPose[jointIndex];
        skeleton->SetJointLocalTransform(jointIndex, {bind.scale, bind.rotate, bind.translate});
    }
    return true;
}

bool KrakenTentacleMidbossController::Impl::ApplyCurrentPose() {
    if (!skeleton) {
        lastError = "スケルトンがないため現在姿勢を生成できませんでした。";
        return false;
    }
    if (state == KrakenTentacleMidbossState::Defeated ||
        state == KrakenTentacleMidbossState::Retreating) {
        if (!ApplyDefeatFrozenPose()) {
            lastError = "撃破時の固定姿勢を現在姿勢へ適用できませんでした。";
            return false;
        }
        return true;
    }
    if (!RestoreBindPose()) {
        lastError = "Bind PoseをCurrent Poseへ復元できませんでした。";
        return false;
    }
    if (!ApplyKrakenTentaclePlacementToRestoredPose(*skeleton, chains)) {
        lastError = "触手の個別配置をCurrent Poseへ適用できませんでした。";
        return false;
    }
    if (state == KrakenTentacleMidbossState::Idle) {
        if (!idleSwayEnabled) {
            return true;
        }
        KrakenTentacleIdlePoseResult result{};
        if (!BuildKrakenTentacleIdlePose(
                idleSettings,
                idleTime,
                chains,
                true,
                selectedAttackChainIndex,
                skeleton->GetJoints().size(),
                skeleton->GetRootIndex(),
                result) ||
            !result.valid) {
            lastError = result.errorMessage.empty()
                ? "Idle Sway Poseの生成に失敗しました。"
                : result.errorMessage;
            return false;
        }
        for (const KrakenTentacleIdleJointPose& pose : result.joints) {
            if (!pose.finite || pose.jointIndex < 0 ||
                static_cast<std::size_t>(pose.jointIndex) >=
                    skeleton->GetJoints().size() ||
                pose.jointIndex == skeleton->GetRootIndex()) {
                lastError = "Idle Sway Poseに無効なJointがあります。";
                return false;
            }
            auto rotation = skeleton->GetJoints()[pose.jointIndex].localRotate;
            rotation.x += pose.localEulerOffsetRadians.x;
            rotation.y += pose.localEulerOffsetRadians.y;
            rotation.z += pose.localEulerOffsetRadians.z;
            skeleton->SetJointLocalRotation(pose.jointIndex, rotation);
        }
        return true;
    }
    if (!IsAttackState()) {
        return state == KrakenTentacleMidbossState::Hidden;
    }
    if (selectedAttackChainIndex >= chains.size()) {
        lastError = "攻撃Poseの対象Chainが範囲外です。";
        return false;
    }
    if (IsSweepState()) {
        return ApplySweepPose();
    }

    attackSettings = SanitizeKrakenTentacleAttackSettings(attackSettings);
    const KrakenTentacleAttackPoseTotals totals =
        EvaluateKrakenTentacleAttackPoseTotals(
            attackSettings,
            GetAttackPhase(),
            stateElapsedTime);
    if (!wholeSlamDiagnostics.attackTargetSnapshotValid ||
        !totals.finite ||
        !ApplyWholeSlamPoseToSkeleton(
            *skeleton,
            selectedAttackChainIndex,
            GetAttackPhase(), stateElapsedTime, idleTime,
            totals,
            wholeSlamDiagnostics.attackTargetWorldPosition,
            &wholeSlamDiagnostics.pose)) {
        lastError = "触手全体叩きつけPoseの生成に失敗しました。";
        return false;
    }
    return true;
}

bool KrakenTentacleMidbossController::Impl::ValidateCurrentPose() const {
    if (!skeleton || skeleton->GetRootIndex() < 0 ||
        static_cast<std::size_t>(skeleton->GetRootIndex()) >=
            skeleton->GetJoints().size()) {
        return false;
    }
    for (const Joint& joint : skeleton->GetJoints()) {
        if (!IsFinite(joint.localTranslate) ||
            !IsFinite(joint.localRotate) ||
            !IsFinite(joint.localScale) ||
            !IsFinite(joint.localMatrix) ||
            !IsFinite(joint.skeletonSpaceMatrix) ||
            !IsFinite(joint.worldMatrix) ||
            !IsFinite(joint.worldTranslate)) {
            return false;
        }
    }
    return true;
}

bool KrakenTentacleMidbossController::Impl::UpdateCurrentPoseAndSkinning() {
    if (!skeleton || !model || !ApplyCurrentPose()) {
        EnterHidden(
            lastError.empty()
                ? "Current Poseの生成に失敗しました。"
                : lastError,
            true);
        return false;
    }
    skeleton->UpdateWorldTransforms();
    if (!ValidateCurrentPose()) {
        EnterHidden(
            "Current PoseのJoint Transformに非有限値があります。",
            true);
        return false;
    }

    UpdateObjectTransform();
    model->UpdateSkinning();
    diagnostics.cpuSkinningUpdateCount = 1;
    RefreshSkinningDiagnostics();
    if (diagnostics.paletteCount != kExpectedJointCount ||
        diagnostics.nonFinitePaletteCount != 0 ||
        diagnostics.nonFiniteSkinnedVertexCount != 0 ||
        diagnostics.weightlessVertexCount != 0 ||
        diagnostics.invalidJointInfluenceCount != 0 ||
        diagnostics.boundsAbnormal) {
        EnterHidden(
            "Skinning診断で異常を検出したためBind Poseへ安全復帰しました。",
            true);
        return false;
    }

    RefreshColliderSnapshots();
    if (diagnostics.nonFiniteColliderCount != 0 ||
        diagnostics.invalidColliderJointCount != 0) {
        EnterHidden(
            "Collider Snapshotに無効な値を検出したため表示を停止しました。",
            true);
        return false;
    }
    RefreshBoneSnapshots();
    RefreshPlacementDiagnostics();
    RefreshAttackReachDiagnostics();
    return true;
}
