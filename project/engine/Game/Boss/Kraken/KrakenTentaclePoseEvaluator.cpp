#include "Engine/Game/Boss/Kraken/KrakenTentaclePoseEvaluator.h"

#include "Engine/Animation/AnimationClip.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
    constexpr float kEpsilon = 0.000001f;
    constexpr float kDegreesToRadians =
        std::numbers::pi_v<float> / 180.0f;
    constexpr float kMinimumWindupDuration = 0.10f;
    constexpr float kMinimumSlamDuration = 0.05f;
    constexpr float kMinimumRecoveryDuration = 0.10f;
    constexpr float kMaximumLongPhaseDuration = 2.00f;
    constexpr float kMaximumShortPhaseDuration = 1.00f;
    constexpr float kMaximumLoopInterval = 2.00f;
    constexpr float kMinimumTipBias = 0.10f;
    constexpr float kMaximumTipBias = 8.00f;

    bool IsFinite(const Vector3& value) {
        return std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
    }

    bool IsFinite(const Quaternion& value) {
        return std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z) &&
            std::isfinite(value.w);
    }

    float ClampFinite(
        float value,
        float minimum,
        float maximum,
        float fallback) {
        return std::isfinite(value)
            ? std::clamp(value, minimum, maximum)
            : fallback;
    }

    float NormalizeSign(float value) {
        if (!std::isfinite(value) || value == 0.0f) {
            return 1.0f;
        }
        return value < 0.0f ? -1.0f : 1.0f;
    }

    KrakenTentacleAttackLocalAxis SanitizeAxis(
        KrakenTentacleAttackLocalAxis axis,
        KrakenTentacleAttackLocalAxis fallback) {
        switch (axis) {
        case KrakenTentacleAttackLocalAxis::X:
        case KrakenTentacleAttackLocalAxis::Y:
        case KrakenTentacleAttackLocalAxis::Z:
            return axis;
        default:
            return fallback;
        }
    }

    Quaternion NormalizeQuaternion(const Quaternion& value) {
        if (!IsFinite(value)) {
            return {};
        }
        const float lengthSquared =
            (value.x * value.x) + (value.y * value.y) +
            (value.z * value.z) + (value.w * value.w);
        if (!std::isfinite(lengthSquared) ||
            lengthSquared <= kEpsilon * kEpsilon) {
            return {};
        }
        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        return {
            value.x * inverseLength,
            value.y * inverseLength,
            value.z * inverseLength,
            value.w * inverseLength,
        };
    }

    Quaternion Multiply(const Quaternion& lhs, const Quaternion& rhs) {
        return NormalizeQuaternion({
            (lhs.w * rhs.x) + (lhs.x * rhs.w) +
                (lhs.y * rhs.z) - (lhs.z * rhs.y),
            (lhs.w * rhs.y) - (lhs.x * rhs.z) +
                (lhs.y * rhs.w) + (lhs.z * rhs.x),
            (lhs.w * rhs.z) + (lhs.x * rhs.y) -
                (lhs.y * rhs.x) + (lhs.z * rhs.w),
            (lhs.w * rhs.w) - (lhs.x * rhs.x) -
                (lhs.y * rhs.y) - (lhs.z * rhs.z),
        });
    }

    Quaternion EulerXyzToQuaternion(const Vector3& eulerRadians) {
        const float halfX = eulerRadians.x * 0.5f;
        const float halfY = eulerRadians.y * 0.5f;
        const float halfZ = eulerRadians.z * 0.5f;
        const Quaternion rotateX{
            std::sin(halfX), 0.0f, 0.0f, std::cos(halfX) };
        const Quaternion rotateY{
            0.0f, std::sin(halfY), 0.0f, std::cos(halfY) };
        const Quaternion rotateZ{
            0.0f, 0.0f, std::sin(halfZ), std::cos(halfZ) };
        return Multiply(Multiply(rotateZ, rotateY), rotateX);
    }

    Quaternion MakeAxisRotation(
        KrakenTentacleAttackLocalAxis axis,
        float radians) {
        const float halfAngle = radians * 0.5f;
        const float sine = std::sin(halfAngle);
        const float cosine = std::cos(halfAngle);
        switch (axis) {
        case KrakenTentacleAttackLocalAxis::X:
            return NormalizeQuaternion({ sine, 0.0f, 0.0f, cosine });
        case KrakenTentacleAttackLocalAxis::Y:
            return NormalizeQuaternion({ 0.0f, sine, 0.0f, cosine });
        case KrakenTentacleAttackLocalAxis::Z:
            return NormalizeQuaternion({ 0.0f, 0.0f, sine, cosine });
        default:
            return {};
        }
    }

    bool FailIdle(
        KrakenTentacleIdlePoseResult& result,
        const char* errorMessage) {
        result.joints.clear();
        result.valid = false;
        result.errorMessage = errorMessage;
        return false;
    }

    bool FailAttack(
        KrakenTentacleAttackPoseResult& result,
        const char* errorMessage) {
        result.joints.clear();
        result.fixedBoneCount = 0;
        result.movableBoneCount = 0;
        result.normalizedWeightSum = 0.0f;
        result.valid = false;
        result.errorMessage = errorMessage;
        return false;
    }

    float Lerp(float start, float end, float t) {
        return start + ((end - start) * t);
    }

    float SmoothStep(float t) {
        const float safeT = std::clamp(t, 0.0f, 1.0f);
        return safeT * safeT * (3.0f - (2.0f * safeT));
    }
}

bool BuildKrakenTentacleIdlePose(
    const KrakenTentacleIdlePoseSettings& settings,
    float motionTime,
    const std::vector<KrakenTentacleChain>& chains,
    bool applyAllChains,
    std::size_t selectedChainIndex,
    std::size_t skeletonJointCount,
    int skeletonRootJointIndex,
    KrakenTentacleIdlePoseResult& outResult) {
    outResult.joints.clear();
    outResult.valid = false;
    outResult.errorMessage.clear();
    if (chains.empty()) {
        return FailIdle(outResult, "Idle Poseを適用する触手Chainがありません。");
    }
    if (!applyAllChains && selectedChainIndex >= chains.size()) {
        return FailIdle(outResult, "Idle Poseの選択Chainが範囲外です。");
    }
    if (skeletonRootJointIndex < 0 ||
        static_cast<std::size_t>(skeletonRootJointIndex) >=
            skeletonJointCount) {
        return FailIdle(outResult, "Skeleton Root Jointが不正です。");
    }
    if (!std::isfinite(motionTime) ||
        !std::isfinite(settings.frequencyHz) ||
        !std::isfinite(settings.rootAmplitudeDegrees) ||
        !std::isfinite(settings.tipAmplitudeDegrees) ||
        !std::isfinite(settings.secondaryAmplitudeDegrees) ||
        !std::isfinite(settings.chainPhaseRadians) ||
        !std::isfinite(settings.phaseAlongChainRadians) ||
        !std::isfinite(settings.startupBlendDuration) ||
        settings.startupBlendDuration <= 0.0f ||
        !std::isfinite(settings.secondaryFrequencyScale)) {
        return FailIdle(outResult, "Idle Pose設定に不正な値があります。");
    }

    const float angularSpeed =
        2.0f * std::numbers::pi_v<float> * settings.frequencyHz;
    const float startupBlend = std::clamp(
        motionTime / settings.startupBlendDuration,
        0.0f,
        1.0f);
    std::vector<bool> visited(skeletonJointCount, false);
    for (std::size_t chainIndex = 0;
        chainIndex < chains.size();
        ++chainIndex) {
        if (!applyAllChains && chainIndex != selectedChainIndex) {
            continue;
        }
        const KrakenTentacleChain& chain = chains[chainIndex];
        if (chain.joints.empty()) {
            return FailIdle(outResult, "Idle Poseを適用する触手Chainが空です。");
        }
        const int denominator =
            (std::max)(static_cast<int>(chain.joints.size()) - 1, 1);
        const float chainPhase =
            static_cast<float>(chainIndex) * settings.chainPhaseRadians;
        for (std::size_t boneIndex = 0;
            boneIndex < chain.joints.size();
            ++boneIndex) {
            const int jointIndex = chain.joints[boneIndex];
            if (jointIndex < 0 ||
                static_cast<std::size_t>(jointIndex) >= skeletonJointCount) {
                return FailIdle(outResult, "Idle Poseに範囲外のJoint Indexがあります。");
            }
            if (jointIndex == skeletonRootJointIndex) {
                return FailIdle(outResult, "Skeleton RootへIdle回転を適用できません。");
            }
            if (visited[static_cast<std::size_t>(jointIndex)]) {
                return FailIdle(outResult, "Idle PoseのChainに重複Jointがあります。");
            }
            visited[static_cast<std::size_t>(jointIndex)] = true;

            const float chainT = static_cast<float>(boneIndex) /
                static_cast<float>(denominator);
            const float amplitudeDegrees =
                settings.rootAmplitudeDegrees +
                (settings.tipAmplitudeDegrees -
                    settings.rootAmplitudeDegrees) * chainT;
            const float angleA = std::sin(
                motionTime * angularSpeed + chainPhase +
                chainT * settings.phaseAlongChainRadians) *
                amplitudeDegrees * startupBlend;
            const float angleB = std::cos(
                motionTime * angularSpeed *
                    settings.secondaryFrequencyScale + chainPhase) *
                settings.secondaryAmplitudeDegrees * chainT * startupBlend;

            KrakenTentacleIdleJointPose pose{};
            pose.jointIndex = jointIndex;
            pose.chainIndex = chainIndex;
            pose.chainBoneIndex = boneIndex;
            pose.normalizedChainPosition = chainT;
            pose.localEulerOffsetRadians = {
                angleA * kDegreesToRadians,
                0.0f,
                angleB * kDegreesToRadians,
            };
            pose.finite = IsFinite(pose.localEulerOffsetRadians);
            if (!pose.finite) {
                return FailIdle(outResult, "Idle Pose計算で非有限値を検出しました。");
            }
            outResult.joints.push_back(pose);
        }
    }
    if (outResult.joints.empty()) {
        return FailIdle(outResult, "Idle Poseを適用できるJointがありません。");
    }
    outResult.valid = true;
    return true;
}

KrakenTentacleAttackPreviewSettings SanitizeKrakenTentacleAttackSettings(
    const KrakenTentacleAttackPreviewSettings& settings) {
    const KrakenTentacleAttackPreviewSettings defaults{};
    KrakenTentacleAttackPreviewSettings result = settings;
    result.windupDuration = ClampFinite(
        settings.windupDuration, kMinimumWindupDuration,
        kMaximumLongPhaseDuration, defaults.windupDuration);
    result.windupHoldDuration = ClampFinite(
        settings.windupHoldDuration, 0.0f,
        kMaximumShortPhaseDuration, defaults.windupHoldDuration);
    result.slamDuration = ClampFinite(
        settings.slamDuration, kMinimumSlamDuration,
        kMaximumShortPhaseDuration, defaults.slamDuration);
    result.impactHoldDuration = ClampFinite(
        settings.impactHoldDuration, 0.0f,
        kMaximumShortPhaseDuration, defaults.impactHoldDuration);
    result.recoveryDuration = ClampFinite(
        settings.recoveryDuration, kMinimumRecoveryDuration,
        kMaximumLongPhaseDuration, defaults.recoveryDuration);
    result.loopInterval = ClampFinite(
        settings.loopInterval, 0.0f,
        kMaximumLoopInterval, defaults.loopInterval);
    result.windupPrimaryTotalDegrees = ClampFinite(
        settings.windupPrimaryTotalDegrees, -120.0f, 120.0f,
        defaults.windupPrimaryTotalDegrees);
    result.slamPrimaryTotalDegrees = ClampFinite(
        settings.slamPrimaryTotalDegrees, -120.0f, 120.0f,
        defaults.slamPrimaryTotalDegrees);
    result.windupSecondaryTotalDegrees = ClampFinite(
        settings.windupSecondaryTotalDegrees, -60.0f, 60.0f,
        defaults.windupSecondaryTotalDegrees);
    result.slamSecondaryTotalDegrees = ClampFinite(
        settings.slamSecondaryTotalDegrees, -60.0f, 60.0f,
        defaults.slamSecondaryTotalDegrees);
    result.tipBias = ClampFinite(
        settings.tipBias, kMinimumTipBias,
        kMaximumTipBias, defaults.tipBias);
    result.fixedLeadingBoneCount = (std::min)(
        settings.fixedLeadingBoneCount, std::uint32_t{ 1024 });
    result.primaryAxis = SanitizeAxis(
        settings.primaryAxis, KrakenTentacleAttackLocalAxis::X);
    result.secondaryAxis = SanitizeAxis(
        settings.secondaryAxis, KrakenTentacleAttackLocalAxis::Z);
    result.primarySign = NormalizeSign(settings.primarySign);
    result.secondarySign = NormalizeSign(settings.secondarySign);
    return result;
}

float GetKrakenTentacleAttackMotionDuration(
    const KrakenTentacleAttackPreviewSettings& settings) {
    return settings.windupDuration + settings.windupHoldDuration +
        settings.slamDuration + settings.impactHoldDuration +
        settings.recoveryDuration;
}

float GetKrakenTentacleAttackPhaseDuration(
    const KrakenTentacleAttackPreviewSettings& settings,
    KrakenTentacleAttackPreviewPhase phase) {
    switch (phase) {
    case KrakenTentacleAttackPreviewPhase::Windup:
        return settings.windupDuration;
    case KrakenTentacleAttackPreviewPhase::WindupHold:
        return settings.windupHoldDuration;
    case KrakenTentacleAttackPreviewPhase::Slam:
        return settings.slamDuration;
    case KrakenTentacleAttackPreviewPhase::ImpactHold:
        return settings.impactHoldDuration;
    case KrakenTentacleAttackPreviewPhase::Recovery:
        return settings.recoveryDuration;
    case KrakenTentacleAttackPreviewPhase::Completed:
    default:
        return 0.0f;
    }
}

float GetKrakenTentacleAttackPhaseStartTime(
    const KrakenTentacleAttackPreviewSettings& settings,
    KrakenTentacleAttackPreviewPhase phase) {
    switch (phase) {
    case KrakenTentacleAttackPreviewPhase::Windup:
        return 0.0f;
    case KrakenTentacleAttackPreviewPhase::WindupHold:
        return settings.windupDuration;
    case KrakenTentacleAttackPreviewPhase::Slam:
        return settings.windupDuration + settings.windupHoldDuration;
    case KrakenTentacleAttackPreviewPhase::ImpactHold:
        return settings.windupDuration + settings.windupHoldDuration +
            settings.slamDuration;
    case KrakenTentacleAttackPreviewPhase::Recovery:
        return settings.windupDuration + settings.windupHoldDuration +
            settings.slamDuration + settings.impactHoldDuration;
    case KrakenTentacleAttackPreviewPhase::Completed:
    default:
        return GetKrakenTentacleAttackMotionDuration(settings);
    }
}

KrakenTentacleAttackPoseTotals EvaluateKrakenTentacleAttackPoseTotals(
    const KrakenTentacleAttackPreviewSettings& settings,
    KrakenTentacleAttackPreviewPhase phase,
    float phaseElapsedTime) {
    KrakenTentacleAttackPoseTotals result{};
    const float duration =
        GetKrakenTentacleAttackPhaseDuration(settings, phase);
    const float t = duration > kEpsilon
        ? std::clamp(phaseElapsedTime / duration, 0.0f, 1.0f)
        : 1.0f;
    switch (phase) {
    case KrakenTentacleAttackPreviewPhase::Windup: {
        const float smoothT = SmoothStep(t);
        result.primaryDegrees = Lerp(
            0.0f, settings.windupPrimaryTotalDegrees, smoothT);
        result.secondaryDegrees = Lerp(
            0.0f, settings.windupSecondaryTotalDegrees, smoothT);
        break;
    }
    case KrakenTentacleAttackPreviewPhase::WindupHold:
        result.primaryDegrees = settings.windupPrimaryTotalDegrees;
        result.secondaryDegrees = settings.windupSecondaryTotalDegrees;
        break;
    case KrakenTentacleAttackPreviewPhase::Slam: {
        const float cubicT = t * t * t;
        result.primaryDegrees = Lerp(
            settings.windupPrimaryTotalDegrees,
            settings.slamPrimaryTotalDegrees,
            cubicT);
        result.secondaryDegrees = Lerp(
            settings.windupSecondaryTotalDegrees,
            settings.slamSecondaryTotalDegrees,
            cubicT);
        break;
    }
    case KrakenTentacleAttackPreviewPhase::ImpactHold:
        result.primaryDegrees = settings.slamPrimaryTotalDegrees;
        result.secondaryDegrees = settings.slamSecondaryTotalDegrees;
        break;
    case KrakenTentacleAttackPreviewPhase::Recovery: {
        const float smoothT = SmoothStep(t);
        result.primaryDegrees = Lerp(
            settings.slamPrimaryTotalDegrees, 0.0f, smoothT);
        result.secondaryDegrees = Lerp(
            settings.slamSecondaryTotalDegrees, 0.0f, smoothT);
        break;
    }
    case KrakenTentacleAttackPreviewPhase::Completed:
    default:
        break;
    }
    result.finite = std::isfinite(result.primaryDegrees) &&
        std::isfinite(result.secondaryDegrees);
    if (!result.finite) {
        result.primaryDegrees = 0.0f;
        result.secondaryDegrees = 0.0f;
    }
    return result;
}

bool BuildKrakenTentacleAttackPose(
    const KrakenTentacleAttackPreviewSettings& settings,
    const KrakenTentacleAttackPoseTotals& totals,
    const std::vector<int>& chainJoints,
    const std::vector<Vector3>& bindLocalEulerRadians,
    int skeletonRootJointIndex,
    KrakenTentacleAttackPoseResult& outResult) {
    outResult = {};
    if (!totals.finite ||
        !std::isfinite(totals.primaryDegrees) ||
        !std::isfinite(totals.secondaryDegrees)) {
        return FailAttack(
            outResult,
            "攻撃Poseの全体角度が有限値ではありません。");
    }
    if (chainJoints.empty()) {
        return FailAttack(outResult, "攻撃Poseを適用する触手Chainが空です。");
    }
    if (bindLocalEulerRadians.empty()) {
        return FailAttack(outResult, "Bind Local Rotationを取得できません。");
    }
    if (!std::isfinite(settings.tipBias) || settings.tipBias <= 0.0f) {
        return FailAttack(outResult, "Tip Biasは0より大きい有限値が必要です。");
    }

    const std::size_t fixedBoneCount = (std::min)(
        static_cast<std::size_t>(settings.fixedLeadingBoneCount),
        chainJoints.size());
    if (fixedBoneCount >= chainJoints.size()) {
        return FailAttack(
            outResult,
            "固定先頭Bone数がChain Bone数以上のため、動かせるBoneがありません。");
    }
    const std::size_t movableBoneCount =
        chainJoints.size() - fixedBoneCount;
    for (std::size_t chainIndex = 0;
        chainIndex < chainJoints.size();
        ++chainIndex) {
        const int jointIndex = chainJoints[chainIndex];
        for (std::size_t previousIndex = 0;
            previousIndex < chainIndex;
            ++previousIndex) {
            if (chainJoints[previousIndex] == jointIndex) {
                return FailAttack(
                    outResult,
                    "触手Chainに重複したJoint Indexがあります。");
            }
        }
        if (jointIndex < 0 ||
            static_cast<std::size_t>(jointIndex) >=
                bindLocalEulerRadians.size()) {
            return FailAttack(
                outResult,
                "触手Chainに範囲外のJoint Indexがあります。");
        }
        if (jointIndex == skeletonRootJointIndex) {
            return FailAttack(
                outResult,
                "Skeleton Rootへ攻撃回転を適用することはできません。");
        }
        if (!IsFinite(bindLocalEulerRadians[
            static_cast<std::size_t>(jointIndex)])) {
            return FailAttack(
                outResult,
                "Bind Local Rotationに非有限値があります。");
        }
    }

    float rawWeightSum = 0.0f;
    for (std::size_t movableIndex = 0;
        movableIndex < movableBoneCount;
        ++movableIndex) {
        const float chainT = static_cast<float>(movableIndex + 1) /
            static_cast<float>(movableBoneCount);
        const float rawWeight = std::pow(chainT, settings.tipBias);
        if (!std::isfinite(rawWeight) || rawWeight < 0.0f) {
            return FailAttack(
                outResult,
                "Chain角度の配分Weightを計算できませんでした。");
        }
        rawWeightSum += rawWeight;
    }
    if (!std::isfinite(rawWeightSum) || rawWeightSum <= kEpsilon) {
        return FailAttack(
            outResult,
            "Chain角度の配分Weight合計が不正です。");
    }

    const bool hasZeroTotals = totals.primaryDegrees == 0.0f &&
        totals.secondaryDegrees == 0.0f;
    outResult.joints.reserve(chainJoints.size());
    outResult.fixedBoneCount = fixedBoneCount;
    outResult.movableBoneCount = movableBoneCount;
    const float primarySign = NormalizeSign(settings.primarySign);
    const float secondarySign = NormalizeSign(settings.secondarySign);
    for (std::size_t chainBoneIndex = 0;
        chainBoneIndex < chainJoints.size();
        ++chainBoneIndex) {
        const int jointIndex = chainJoints[chainBoneIndex];
        const Vector3& bindEuler = bindLocalEulerRadians[
            static_cast<std::size_t>(jointIndex)];
        KrakenTentacleAttackJointPose pose{};
        pose.jointIndex = jointIndex;
        pose.chainBoneIndex = chainBoneIndex;
        pose.fixed = chainBoneIndex < fixedBoneCount;
        if (pose.fixed) {
            pose.attackOffset = {};
            pose.absoluteLocalRotation = EulerXyzToQuaternion(bindEuler);
            pose.absoluteLocalEulerRadians = bindEuler;
        } else {
            const std::size_t movableIndex =
                chainBoneIndex - fixedBoneCount;
            const float chainT =
                static_cast<float>(movableIndex + 1) /
                static_cast<float>(movableBoneCount);
            pose.normalizedWeight =
                std::pow(chainT, settings.tipBias) / rawWeightSum;
            pose.primaryDegrees = totals.primaryDegrees *
                primarySign * pose.normalizedWeight;
            pose.secondaryDegrees = totals.secondaryDegrees *
                secondarySign * pose.normalizedWeight;
            if (hasZeroTotals) {
                pose.attackOffset = {};
                pose.absoluteLocalRotation =
                    EulerXyzToQuaternion(bindEuler);
                pose.absoluteLocalEulerRadians = bindEuler;
            } else {
                const Quaternion primaryRotation = MakeAxisRotation(
                    settings.primaryAxis,
                    pose.primaryDegrees * kDegreesToRadians);
                const Quaternion secondaryRotation = MakeAxisRotation(
                    settings.secondaryAxis,
                    pose.secondaryDegrees * kDegreesToRadians);
                pose.attackOffset = Multiply(
                    secondaryRotation,
                    primaryRotation);
                pose.absoluteLocalRotation = Multiply(
                    EulerXyzToQuaternion(bindEuler),
                    pose.attackOffset);
                pose.absoluteLocalEulerRadians =
                    ConvertQuaternionToEulerXYZ(
                        pose.absoluteLocalRotation);
            }
        }
        pose.finite = IsFinite(pose.attackOffset) &&
            IsFinite(pose.absoluteLocalRotation) &&
            IsFinite(pose.absoluteLocalEulerRadians) &&
            std::isfinite(pose.normalizedWeight) &&
            std::isfinite(pose.primaryDegrees) &&
            std::isfinite(pose.secondaryDegrees);
        if (!pose.finite) {
            return FailAttack(
                outResult,
                "攻撃PoseのQuaternion合成で非有限値を検出しました。");
        }
        outResult.normalizedWeightSum += pose.normalizedWeight;
        outResult.joints.push_back(pose);
    }

    if (!std::isfinite(outResult.normalizedWeightSum) ||
        std::fabs(outResult.normalizedWeightSum - 1.0f) > 0.0001f) {
        return FailAttack(
            outResult,
            "Chain角度の正規化Weight合計が1になりませんでした。");
    }
    outResult.valid = true;
    return true;
}
