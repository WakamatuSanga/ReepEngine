#include "Engine/Game/Boss/Kraken/KrakenTentacleAttackWholeSlam.h"

#include "Engine/Animation/AnimationClip.h"
#include "Engine/Animation/Skeleton.h"
#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"
#include "Engine/Game/Player/Player.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
constexpr float kEpsilon = 0.00001f;
constexpr float kDegreesToRadians =
    std::numbers::pi_v<float> / 180.0f;
constexpr float kRadiansToDegrees =
    180.0f / std::numbers::pi_v<float>;

Vector3 Subtract(const Vector3& lhs, const Vector3& rhs) {
    return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
}

Vector3 Scale(const Vector3& value, float scale) {
    return { value.x * scale, value.y * scale, value.z * scale };
}

float Dot(const Vector3& lhs, const Vector3& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

float Length(const Vector3& value) {
    return std::sqrt(Dot(value, value));
}

bool IsFinite(const Vector3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

bool IsFinite(const Quaternion& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z) && std::isfinite(value.w);
}

bool IsFinite(const Matrix4x4& value) {
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            if (!std::isfinite(value.m[row][column])) {
                return false;
            }
        }
    }
    return true;
}

Vector3 Normalize(const Vector3& value, const Vector3& fallback = {}) {
    const float length = Length(value);
    return std::isfinite(length) && length > kEpsilon
        ? Scale(value, 1.0f / length) : fallback;
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

Vector3 TransformDirection(const Vector3& value, const Matrix4x4& matrix) {
    return {
        value.x * matrix.m[0][0] + value.y * matrix.m[1][0] +
            value.z * matrix.m[2][0],
        value.x * matrix.m[0][1] + value.y * matrix.m[1][1] +
            value.z * matrix.m[2][1],
        value.x * matrix.m[0][2] + value.y * matrix.m[1][2] +
            value.z * matrix.m[2][2],
    };
}

Quaternion Normalize(const Quaternion& value) {
    const float length = std::sqrt(
        value.x * value.x + value.y * value.y +
        value.z * value.z + value.w * value.w);
    if (!std::isfinite(length) || length <= kEpsilon) {
        return {};
    }
    const float inverse = 1.0f / length;
    return { value.x * inverse, value.y * inverse,
        value.z * inverse, value.w * inverse };
}

Quaternion Multiply(const Quaternion& lhs, const Quaternion& rhs) {
    return Normalize({
        lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
        lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
        lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
    });
}

Quaternion EulerXyzToQuaternion(const Vector3& radians) {
    const Quaternion x{ std::sin(radians.x * 0.5f), 0.0f, 0.0f,
        std::cos(radians.x * 0.5f) };
    const Quaternion y{ 0.0f, std::sin(radians.y * 0.5f), 0.0f,
        std::cos(radians.y * 0.5f) };
    const Quaternion z{ 0.0f, 0.0f, std::sin(radians.z * 0.5f),
        std::cos(radians.z * 0.5f) };
    return Multiply(Multiply(x, y), z);
}

Quaternion AxisRotation(const Vector3& axis, float degrees) {
    const float half = degrees * kDegreesToRadians * 0.5f;
    const float sine = std::sin(half);
    return Normalize({ axis.x * sine, axis.y * sine, axis.z * sine,
        std::cos(half) });
}

Vector3 ResolveHingeAxis(
    const Vector3& tentacleDirection,
    const Vector3& targetDirection,
    const Matrix4x4& modelWorldMatrix) {
    Vector3 axis = MatrixMath::Cross(tentacleDirection, targetDirection);
    const Vector3 fallbacks[] = {
        { modelWorldMatrix.m[1][0], modelWorldMatrix.m[1][1],
            modelWorldMatrix.m[1][2] },
        { modelWorldMatrix.m[0][0], modelWorldMatrix.m[0][1],
            modelWorldMatrix.m[0][2] },
        { 0.0f, 1.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f },
    };
    for (const Vector3& fallback : fallbacks) {
        if (Length(axis) > kEpsilon) {
            break;
        }
        axis = MatrixMath::Cross(tentacleDirection, fallback);
    }
    return Normalize(axis);
}

Vector3 WorldAxisToParentLocal(
    const Vector3& worldAxis,
    const Joint& parent,
    const Matrix4x4& modelWorldMatrix) {
    const Matrix4x4 parentWorld = MatrixMath::Multipty(
        parent.skeletonSpaceMatrix, modelWorldMatrix);
    if (!IsFinite(parentWorld)) {
        return {};
    }
    return Normalize(TransformDirection(
        worldAxis, MatrixMath::Inverse(parentWorld)));
}

KrakenTentacleWholeSlamTargetSettings SanitizeTargetSettings(
    const KrakenTentacleWholeSlamTargetSettings& settings) {
    KrakenTentacleWholeSlamTargetSettings result{};
    result.hingeGain = std::isfinite(settings.hingeGain)
        ? std::clamp(settings.hingeGain, 0.0f, 2.0f)
        : result.hingeGain;
    result.minimumHingeDegrees = std::isfinite(settings.minimumHingeDegrees)
        ? std::clamp(settings.minimumHingeDegrees, 0.0f, 100.0f)
        : result.minimumHingeDegrees;
    result.maximumHingeDegrees = std::isfinite(settings.maximumHingeDegrees)
        ? std::clamp(settings.maximumHingeDegrees,
            result.minimumHingeDegrees, 100.0f)
        : result.maximumHingeDegrees;
    return result;
}

float RemapMainHingeDegrees(
    KrakenTentacleAttackPreviewPhase phase,
    const KrakenTentacleAttackSettings& settings,
    const KrakenTentacleAttackPoseTotals& totals,
    float targetHingeDegrees,
    float& outSlamProgress) {
    outSlamProgress = 0.0f;
    switch (phase) {
    case KrakenTentacleAttackPreviewPhase::Windup:
    case KrakenTentacleAttackPreviewPhase::WindupHold:
        return totals.primaryDegrees;
    case KrakenTentacleAttackPreviewPhase::Slam: {
        const float range = settings.slamPrimaryTotalDegrees -
            settings.windupPrimaryTotalDegrees;
        outSlamProgress = std::fabs(range) > kEpsilon
            ? std::clamp((totals.primaryDegrees -
                settings.windupPrimaryTotalDegrees) / range, 0.0f, 1.0f)
            : 1.0f;
        return settings.windupPrimaryTotalDegrees +
            (targetHingeDegrees - settings.windupPrimaryTotalDegrees) *
                outSlamProgress;
    }
    case KrakenTentacleAttackPreviewPhase::ImpactHold:
        outSlamProgress = 1.0f;
        return targetHingeDegrees;
    case KrakenTentacleAttackPreviewPhase::Recovery: {
        const float retained = std::fabs(settings.slamPrimaryTotalDegrees) >
            kEpsilon
            ? std::clamp(totals.primaryDegrees /
                settings.slamPrimaryTotalDegrees, 0.0f, 1.0f)
            : 0.0f;
        outSlamProgress = retained;
        return targetHingeDegrees * retained;
    }
    case KrakenTentacleAttackPreviewPhase::Completed:
    default:
        return 0.0f;
    }
}

bool Fail(
    KrakenTentacleAttackPoseResult& pose,
    KrakenTentacleWholeSlamPoseDiagnostics& diagnostics,
    const char* message) {
    pose = {};
    pose.errorMessage = message;
    diagnostics.valid = false;
    return false;
}
}

bool BuildKrakenTentacleWholeSlamPose(
    const KrakenTentacleAttackSettings& settings,
    const KrakenTentacleWholeSlamTargetSettings& targetSettings,
    KrakenTentacleAttackPreviewPhase phase,
    const KrakenTentacleAttackPoseTotals& totals,
    const std::vector<int>& chainJoints,
    const std::vector<Vector3>& bindLocalEulerRadians,
    const Skeleton& bindSkeleton,
    const Matrix4x4& modelWorldMatrix,
    const Vector3& attackTargetWorldPosition,
    KrakenTentacleAttackPoseResult& outPose,
    KrakenTentacleWholeSlamPoseDiagnostics& outDiagnostics) {
    outPose = {};
    outDiagnostics = {};
    outDiagnostics.configuredWindupDegrees =
        settings.windupPrimaryTotalDegrees;
    outDiagnostics.configuredSlamDegrees =
        settings.slamPrimaryTotalDegrees;
    const KrakenTentacleWholeSlamTargetSettings sanitizedTarget =
        SanitizeTargetSettings(targetSettings);
    outDiagnostics.hingeGain = sanitizedTarget.hingeGain;
    outDiagnostics.minimumHingeDegrees =
        sanitizedTarget.minimumHingeDegrees;
    outDiagnostics.maximumHingeDegrees =
        sanitizedTarget.maximumHingeDegrees;
    outDiagnostics.targetValid = IsFinite(attackTargetWorldPosition);
    if (!totals.finite || !IsFinite(modelWorldMatrix) ||
        !outDiagnostics.targetValid || chainJoints.size() < 2 ||
        bindLocalEulerRadians.size() != bindSkeleton.joints.size() ||
        !std::isfinite(settings.tipBias) || settings.tipBias <= 0.0f) {
        return Fail(outPose, outDiagnostics,
            "触手全体叩きつけの入力値が不正です。");
    }

    const std::size_t fixedCount = (std::min)(
        static_cast<std::size_t>(settings.fixedLeadingBoneCount),
        chainJoints.size());
    if (fixedCount == 0 || fixedCount >= chainJoints.size()) {
        return Fail(outPose, outDiagnostics,
            "触手全体叩きつけは固定ボーンと可動ボーンが必要です。");
    }
    const std::size_t hingeBone = fixedCount;
    const int hingeJointIndex = chainJoints[hingeBone];
    const int tipJointIndex = chainJoints.back();
    if (hingeJointIndex < 0 || tipJointIndex < 0 ||
        static_cast<std::size_t>(hingeJointIndex) >= bindSkeleton.joints.size() ||
        static_cast<std::size_t>(tipJointIndex) >= bindSkeleton.joints.size()) {
        return Fail(outPose, outDiagnostics,
            "触手全体叩きつけの支点または先端ジョイントが範囲外です。");
    }
    const Joint& hingeJoint = bindSkeleton.joints[
        static_cast<std::size_t>(hingeJointIndex)];
    if (hingeJoint.parentIndex < 0 ||
        static_cast<std::size_t>(hingeJoint.parentIndex) >=
            bindSkeleton.joints.size()) {
        return Fail(outPose, outDiagnostics,
            "触手全体叩きつけの支点親ジョイントが不正です。");
    }

    outDiagnostics.pivotJointIndex = hingeJointIndex;
    outDiagnostics.pivotParentJointIndex = hingeJoint.parentIndex;
    outDiagnostics.pivotWorldPosition = TransformPosition(
        hingeJoint.worldTranslate, modelWorldMatrix);
    outDiagnostics.bindTipWorldPosition = TransformPosition(
        bindSkeleton.joints[static_cast<std::size_t>(tipJointIndex)].worldTranslate,
        modelWorldMatrix);
    outDiagnostics.tipWorldPosition = outDiagnostics.bindTipWorldPosition;
    outDiagnostics.bindTentacleWorldDirection = Normalize(Subtract(
        outDiagnostics.bindTipWorldPosition,
        outDiagnostics.pivotWorldPosition));
    outDiagnostics.tentacleWorldDirection =
        outDiagnostics.bindTentacleWorldDirection;
    outDiagnostics.targetWorldDirection = Normalize(Subtract(
        attackTargetWorldPosition,
        outDiagnostics.pivotWorldPosition));
    if (Length(outDiagnostics.bindTentacleWorldDirection) <= kEpsilon ||
        Length(outDiagnostics.targetWorldDirection) <= kEpsilon) {
        return Fail(outPose, outDiagnostics,
            "触手方向または攻撃対象方向を計算できませんでした。");
    }
    outDiagnostics.hingeWorldAxis = ResolveHingeAxis(
        outDiagnostics.bindTentacleWorldDirection,
        outDiagnostics.targetWorldDirection,
        modelWorldMatrix);
    outDiagnostics.hingeParentLocalAxis = WorldAxisToParentLocal(
        outDiagnostics.hingeWorldAxis,
        bindSkeleton.joints[static_cast<std::size_t>(hingeJoint.parentIndex)],
        modelWorldMatrix);
    if (Length(outDiagnostics.hingeWorldAxis) <= kEpsilon ||
        Length(outDiagnostics.hingeParentLocalAxis) <= kEpsilon) {
        return Fail(outPose, outDiagnostics,
            "触手全体叩きつけの回転軸をLocal空間へ変換できませんでした。");
    }

    const float directionDot = std::clamp(Dot(
        outDiagnostics.bindTentacleWorldDirection,
        outDiagnostics.targetWorldDirection), -1.0f, 1.0f);
    outDiagnostics.targetAlignmentDegrees =
        std::acos(directionDot) * kRadiansToDegrees;
    outDiagnostics.requiredHingeDegrees =
        outDiagnostics.targetAlignmentDegrees;
    outDiagnostics.targetHingeDegrees = std::clamp(
        outDiagnostics.requiredHingeDegrees * sanitizedTarget.hingeGain,
        sanitizedTarget.minimumHingeDegrees,
        sanitizedTarget.maximumHingeDegrees);
    outDiagnostics.fixedHingeOvershootDegrees =
        settings.slamPrimaryTotalDegrees -
            outDiagnostics.requiredHingeDegrees;
    outDiagnostics.targetHingeOvershootDegrees =
        outDiagnostics.targetHingeDegrees -
            outDiagnostics.requiredHingeDegrees;
    outDiagnostics.appliedMainHingeDegrees = RemapMainHingeDegrees(
        phase, settings, totals, outDiagnostics.targetHingeDegrees,
        outDiagnostics.slamProgress);
    outDiagnostics.appliedUpperBendDegrees = totals.secondaryDegrees *
        (settings.secondarySign < 0.0f ? -1.0f : 1.0f);

    const std::size_t upperBoneCount =
        chainJoints.size() - hingeBone - 1;
    float upperWeightSum = 0.0f;
    for (std::size_t index = 0; index < upperBoneCount; ++index) {
        const float t = static_cast<float>(index + 1) /
            static_cast<float>((std::max)(upperBoneCount, std::size_t{ 1 }));
        upperWeightSum += std::pow(t, settings.tipBias);
    }

    outPose.joints.reserve(chainJoints.size());
    outPose.fixedBoneCount = fixedCount;
    outPose.movableBoneCount = chainJoints.size() - fixedCount;
    for (std::size_t chainBone = 0;
        chainBone < chainJoints.size(); ++chainBone) {
        const int jointIndex = chainJoints[chainBone];
        if (jointIndex < 0 || static_cast<std::size_t>(jointIndex) >=
            bindLocalEulerRadians.size()) {
            return Fail(outPose, outDiagnostics,
                "触手全体叩きつけに範囲外のジョイントがあります。");
        }
        const Vector3 bindEuler = bindLocalEulerRadians[
            static_cast<std::size_t>(jointIndex)];
        KrakenTentacleAttackJointPose pose{};
        pose.jointIndex = jointIndex;
        pose.chainBoneIndex = chainBone;
        pose.fixed = chainBone < fixedCount;
        pose.absoluteLocalRotation = EulerXyzToQuaternion(bindEuler);
        pose.absoluteLocalEulerRadians = bindEuler;
        if (chainBone == hingeBone) {
            pose.normalizedWeight = 1.0f;
            pose.primaryDegrees =
                outDiagnostics.appliedMainHingeDegrees;
            pose.attackOffset = AxisRotation(
                outDiagnostics.hingeParentLocalAxis,
                pose.primaryDegrees);
            pose.absoluteLocalRotation = Multiply(
                pose.attackOffset, pose.absoluteLocalRotation);
            pose.absoluteLocalEulerRadians = ConvertQuaternionToEulerXYZ(
                pose.absoluteLocalRotation);
        } else if (chainBone > hingeBone && upperWeightSum > kEpsilon &&
            std::fabs(outDiagnostics.appliedUpperBendDegrees) > kEpsilon) {
            const std::size_t upperIndex = chainBone - hingeBone - 1;
            const float t = static_cast<float>(upperIndex + 1) /
                static_cast<float>(upperBoneCount);
            const float weight = std::pow(t, settings.tipBias) /
                upperWeightSum;
            pose.secondaryDegrees =
                outDiagnostics.appliedUpperBendDegrees * weight;
            const Joint& joint = bindSkeleton.joints[
                static_cast<std::size_t>(jointIndex)];
            if (joint.parentIndex < 0 || static_cast<std::size_t>(
                joint.parentIndex) >= bindSkeleton.joints.size()) {
                return Fail(outPose, outDiagnostics,
                    "上側しなりの親ジョイントが不正です。");
            }
            const Vector3 localAxis = WorldAxisToParentLocal(
                outDiagnostics.hingeWorldAxis,
                bindSkeleton.joints[static_cast<std::size_t>(joint.parentIndex)],
                modelWorldMatrix);
            pose.attackOffset = AxisRotation(localAxis, pose.secondaryDegrees);
            pose.absoluteLocalRotation = Multiply(
                pose.attackOffset, pose.absoluteLocalRotation);
            pose.absoluteLocalEulerRadians = ConvertQuaternionToEulerXYZ(
                pose.absoluteLocalRotation);
        }
        pose.finite = IsFinite(pose.attackOffset) &&
            IsFinite(pose.absoluteLocalRotation) &&
            IsFinite(pose.absoluteLocalEulerRadians);
        if (!pose.finite) {
            return Fail(outPose, outDiagnostics,
                "触手全体叩きつけの回転結果が有限値ではありません。");
        }
        outPose.joints.push_back(pose);
    }

    outPose.normalizedWeightSum = 1.0f;
    outPose.valid = true;
    outDiagnostics.mainHingeApplied =
        std::fabs(outDiagnostics.appliedMainHingeDegrees) > kEpsilon;
    outDiagnostics.finite = IsFinite(outDiagnostics.pivotWorldPosition) &&
        IsFinite(outDiagnostics.tipWorldPosition) &&
        IsFinite(outDiagnostics.hingeWorldAxis) &&
        IsFinite(outDiagnostics.hingeParentLocalAxis) &&
        std::isfinite(outDiagnostics.targetAlignmentDegrees) &&
        std::isfinite(outDiagnostics.targetHingeDegrees) &&
        std::isfinite(outDiagnostics.fixedHingeOvershootDegrees) &&
        std::isfinite(outDiagnostics.appliedMainHingeDegrees);
    outDiagnostics.valid = outDiagnostics.finite;
    return outDiagnostics.valid;
}

bool KrakenTentacleMidbossController::Impl::
ApplyWholeSlamPoseToSkeleton(
    Skeleton& targetSkeleton,
    std::size_t chainIndex,
    KrakenTentacleAttackPreviewPhase phase,
    const KrakenTentacleAttackPoseTotals& totals,
    const Vector3& attackTarget,
    KrakenTentacleWholeSlamPoseDiagnostics* poseDiagnostics) const {
    if (chainIndex >= chains.size()) {
        return false;
    }
    UpdateSkeletonWorldTransforms(targetSkeleton);
    KrakenTentacleAttackPoseResult pose{};
    KrakenTentacleWholeSlamPoseDiagnostics diagnostics{};
    if (!BuildKrakenTentacleWholeSlamPose(
            attackSettings, wholeSlamDiagnostics.targetSettings, phase,
            totals, chains[chainIndex].joints,
            bindLocalEulerRadians, targetSkeleton, worldMatrix,
            attackTarget, pose, diagnostics) || !pose.valid) {
        return false;
    }
    for (const KrakenTentacleAttackJointPose& jointPose : pose.joints) {
        targetSkeleton.joints[static_cast<std::size_t>(
            jointPose.jointIndex)].localRotate =
            jointPose.absoluteLocalEulerRadians;
    }
    UpdateSkeletonWorldTransforms(targetSkeleton);
    const int hingeJointIndex = chains[chainIndex].joints[
        (std::min)(static_cast<std::size_t>(attackSettings.fixedLeadingBoneCount),
            chains[chainIndex].joints.size() - 1)];
    const int tipJointIndex = chains[chainIndex].joints.back();
    diagnostics.pivotWorldPosition = TransformPosition(
        targetSkeleton.joints[static_cast<std::size_t>(
            hingeJointIndex)].worldTranslate, worldMatrix);
    diagnostics.tipWorldPosition = TransformPosition(
        targetSkeleton.joints[static_cast<std::size_t>(
            tipJointIndex)].worldTranslate, worldMatrix);
    diagnostics.tentacleWorldDirection = Normalize(Subtract(
        diagnostics.tipWorldPosition, diagnostics.pivotWorldPosition));
    if (poseDiagnostics) {
        *poseDiagnostics = diagnostics;
    }
    return true;
}

bool KrakenTentacleMidbossController::Impl::CaptureAttackTargetSnapshot() {
    if (!collisionPlayer) {
        lastWarning =
            "プレイヤー判定が未接続のため攻撃対象を保存できません。";
        return false;
    }
    const Vector3 target = collisionPlayer->GetWorldPosition();
    if (!IsFinite(target)) {
        lastWarning =
            "プレイヤー判定中心が有限値ではないため攻撃を開始できません。";
        return false;
    }
    wholeSlamDiagnostics.attackTargetWorldPosition = target;
    wholeSlamDiagnostics.attackTargetSnapshotValid = true;
    wholeSlamDiagnostics.chainIndex = selectedAttackChainIndex;
    wholeSlamDiagnostics.pose = {};
    ++wholeSlamDiagnostics.targetCaptureCount;
    return true;
}

void KrakenTentacleMidbossController::Impl::ClearAttackTargetSnapshot() {
    wholeSlamDiagnostics.attackTargetWorldPosition = {};
    wholeSlamDiagnostics.attackSequenceId = 0;
    wholeSlamDiagnostics.attackTargetSnapshotValid = false;
}

void KrakenTentacleMidbossController::Impl::ResetWholeSlamDiagnostics() {
    wholeSlamDiagnostics = {};
}

void KrakenTentacleMidbossController::Impl::DrawWholeSlamDiagnosticsImGui() {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader(
            "触手全体叩きつけ診断##WholeTentacleSlamDiagnostics",
            ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    const auto drawVector = [](const char* label, const Vector3& value) {
        ImGui::Text("%s: (%.3f, %.3f, %.3f)",
            label, value.x, value.y, value.z);
    };
    const KrakenTentacleWholeSlamPoseDiagnostics& pose =
        wholeSlamDiagnostics.pose;
    ImGui::Text("攻撃対象保存済み: %s",
        wholeSlamDiagnostics.attackTargetSnapshotValid ? "はい" : "いいえ");
    drawVector("攻撃対象保存位置",
        wholeSlamDiagnostics.attackTargetWorldPosition);
    ImGui::Text("攻撃シーケンス識別子: %llu",
        static_cast<unsigned long long>(
            wholeSlamDiagnostics.attackSequenceId));
    ImGui::Text("対象保存回数: %llu",
        static_cast<unsigned long long>(
            wholeSlamDiagnostics.targetCaptureCount));
    ImGui::Text("対象チェーン: %zu", wholeSlamDiagnostics.chainIndex);
    ImGui::SeparatorText("主ヒンジ");
    ImGui::Text("支点ジョイント／親: %d／%d",
        pose.pivotJointIndex, pose.pivotParentJointIndex);
    drawVector("支点位置", pose.pivotWorldPosition);
    drawVector("バインド先端位置", pose.bindTipWorldPosition);
    drawVector("先端位置", pose.tipWorldPosition);
    drawVector("バインド触手方向", pose.bindTentacleWorldDirection);
    drawVector("触手方向", pose.tentacleWorldDirection);
    drawVector("対象方向", pose.targetWorldDirection);
    drawVector("ヒンジ世界軸", pose.hingeWorldAxis);
    drawVector("ヒンジ親ローカル軸", pose.hingeParentLocalAxis);
    ImGui::Text("振りかぶりヒンジ設定: %.2f 度",
        pose.configuredWindupDegrees);
    ImGui::Text("旧固定叩きつけ角度: %.2f 度",
        pose.configuredSlamDegrees);
    ImGui::Text("対象整列角度: %.2f 度",
        pose.targetAlignmentDegrees);
    ImGui::Text("必要ヒンジ角度: %.2f 度",
        pose.requiredHingeDegrees);
    ImGui::Text("目標ヒンジ角度: %.2f 度",
        pose.targetHingeDegrees);
    ImGui::Text("旧80度超過量: %.2f 度",
        pose.fixedHingeOvershootDegrees);
    ImGui::Text("目標角度超過量: %.2f 度",
        pose.targetHingeOvershootDegrees);
    ImGui::Text("ヒンジ倍率: %.2f", pose.hingeGain);
    ImGui::Text("最小／最大ヒンジ: %.2f／%.2f 度",
        pose.minimumHingeDegrees, pose.maximumHingeDegrees);
    ImGui::Text("叩きつけ進行率: %.3f", pose.slamProgress);
    ImGui::Text("主ヒンジ適用角度: %.2f 度",
        pose.appliedMainHingeDegrees);
    ImGui::Text("主ヒンジ適用中: %s",
        pose.mainHingeApplied ? "はい" : "いいえ");
    ImGui::Text("上側しなり合計: %.2f 度",
        pose.appliedUpperBendDegrees);
    ImGui::Text("姿勢有効: %s", pose.valid ? "はい" : "いいえ");
    ImGui::SeparatorText("到達と回避");
    const std::size_t chain = wholeSlamDiagnostics.chainIndex <
        attackReachDiagnostics.chains.size()
        ? wholeSlamDiagnostics.chainIndex : 0;
    ImGui::Text("先端到達距離: %.3f",
        Length(Subtract(pose.tipWorldPosition, pose.pivotWorldPosition)));
    ImGui::Text("支点と対象の高さ差: %.3f",
        wholeSlamDiagnostics.attackTargetWorldPosition.y -
            pose.pivotWorldPosition.y);
    ImGui::Text("支点から対象までの距離: %.3f",
        Length(Subtract(wholeSlamDiagnostics.attackTargetWorldPosition,
            pose.pivotWorldPosition)));
    ImGui::Text("表面間隔: %.3f",
        attackReachDiagnostics.chains[chain].minimumActiveDistance -
            attackReachDiagnostics.chains[chain].combinedRadius);
    ImGui::Text("到達可能: %s",
        attackReachDiagnostics.chains[chain].reachesMovementRegion
            ? "はい" : "いいえ");
    ImGui::Text("回避可能: %s",
        attackReachDiagnostics.chains[chain].avoidableInMovementRegion
            ? "はい" : "いいえ");
#endif
}
