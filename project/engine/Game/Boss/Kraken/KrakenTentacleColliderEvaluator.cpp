#include "Engine/Game/Boss/Kraken/KrakenTentacleColliderEvaluator.h"

#include "Engine/Animation/Skeleton.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace {
    constexpr float kMinimumColliderLength = 0.00001f;
    constexpr float kMinimumValidSlamDuration = 0.001f;
    constexpr float kDefaultAttackActiveStartRatio = 0.65f;
    constexpr std::array<std::array<float, 2>, 4> kNormalizedSegments = {{
        { 0.15f, 0.35f },
        { 0.35f, 0.55f },
        { 0.55f, 0.75f },
        { 0.75f, 0.95f },
    }};
    constexpr std::array<float, 4> kRecommendedRadii = {
        0.45f, 0.35f, 0.28f, 0.22f,
    };

    bool IsFinite(const Vector3& value) {
        return std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
    }

    bool IsJointIndexValid(
        int jointIndex,
        const Skeleton& skeleton,
        int skeletonRootJointIndex) {
        return jointIndex >= 0 &&
            static_cast<std::size_t>(jointIndex) < skeleton.joints.size() &&
            jointIndex != skeletonRootJointIndex;
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

    Vector3 Subtract(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    }

    float Length(const Vector3& value) {
        return std::sqrt(
            value.x * value.x +
            value.y * value.y +
            value.z * value.z);
    }

    float Distance(const Vector3& lhs, const Vector3& rhs) {
        return Length(Subtract(lhs, rhs));
    }

    float ExtractMaximumScale(const Matrix4x4& matrix) {
        const float scaleX = std::sqrt(
            matrix.m[0][0] * matrix.m[0][0] +
            matrix.m[0][1] * matrix.m[0][1] +
            matrix.m[0][2] * matrix.m[0][2]);
        const float scaleY = std::sqrt(
            matrix.m[1][0] * matrix.m[1][0] +
            matrix.m[1][1] * matrix.m[1][1] +
            matrix.m[1][2] * matrix.m[1][2]);
        const float scaleZ = std::sqrt(
            matrix.m[2][0] * matrix.m[2][0] +
            matrix.m[2][1] * matrix.m[2][1] +
            matrix.m[2][2] * matrix.m[2][2]);
        return (std::max)({ scaleX, scaleY, scaleZ });
    }

    std::size_t FindNearestChainBone(
        float normalizedPosition,
        std::size_t chainBoneCount) {
        if (chainBoneCount <= 1) {
            return 0;
        }
        const float scaled = std::clamp(
            normalizedPosition, 0.0f, 1.0f) *
            static_cast<float>(chainBoneCount - 1);
        return static_cast<std::size_t>(std::lround(scaled));
    }

    bool IsKnownPhase(KrakenTentacleColliderAttackPhase phase) {
        switch (phase) {
        case KrakenTentacleColliderAttackPhase::Windup:
        case KrakenTentacleColliderAttackPhase::WindupHold:
        case KrakenTentacleColliderAttackPhase::Slam:
        case KrakenTentacleColliderAttackPhase::ImpactHold:
        case KrakenTentacleColliderAttackPhase::Recovery:
        case KrakenTentacleColliderAttackPhase::Completed:
            return true;
        case KrakenTentacleColliderAttackPhase::Invalid:
        default:
            return false;
        }
    }
}

KrakenTentacleColliderDefinitionResult
BuildKrakenTentacleColliderDefinitions(
    std::uint32_t detectedChainCount,
    std::uint32_t chainIndex,
    const std::vector<int>& chainJoints,
    int skeletonRootJointIndex,
    std::size_t skeletonJointCount,
    const Vector3& bindTipSkeletonPosition) {
    KrakenTentacleColliderDefinitionResult result{};
    if (detectedChainCount == 0 || chainIndex >= detectedChainCount) {
        result.error =
            KrakenTentacleColliderDefinitionError::ChainOutOfRange;
        return result;
    }
    if (chainJoints.empty()) {
        result.error = KrakenTentacleColliderDefinitionError::ChainEmpty;
        return result;
    }
    if (!IsFinite(bindTipSkeletonPosition)) {
        result.error =
            KrakenTentacleColliderDefinitionError::BindTipPositionNotFinite;
        return result;
    }
    for (int jointIndex : chainJoints) {
        if (jointIndex < 0 ||
            static_cast<std::size_t>(jointIndex) >= skeletonJointCount ||
            jointIndex == skeletonRootJointIndex) {
            result.error =
                KrakenTentacleColliderDefinitionError::InvalidJointIndex;
            return result;
        }
    }

    std::vector<std::pair<int, int>> usedJointPairs;
    for (std::size_t segmentIndex = 0;
        segmentIndex < kNormalizedSegments.size();
        ++segmentIndex) {
        std::size_t startBone = FindNearestChainBone(
            kNormalizedSegments[segmentIndex][0],
            chainJoints.size());
        std::size_t endBone = FindNearestChainBone(
            kNormalizedSegments[segmentIndex][1],
            chainJoints.size());
        if (endBone <= startBone) {
            endBone = (std::min)(startBone + 1, chainJoints.size() - 1);
        }
        if (startBone >= chainJoints.size() || endBone <= startBone) {
            continue;
        }

        const std::pair<int, int> jointPair = {
            chainJoints[startBone], chainJoints[endBone] };
        if (std::find(
                usedJointPairs.begin(),
                usedJointPairs.end(),
                jointPair) != usedJointPairs.end()) {
            continue;
        }
        usedJointPairs.push_back(jointPair);

        KrakenTentacleCapsuleColliderDefinition definition{};
        definition.colliderIndex =
            static_cast<std::uint32_t>(result.capsules.size());
        definition.chainIndex = chainIndex;
        definition.role = segmentIndex + 1 == kNormalizedSegments.size()
            ? KrakenColliderPreviewRole::Attack
            : KrakenColliderPreviewRole::Damage;
        definition.startJointIndex = jointPair.first;
        definition.endJointIndex = jointPair.second;
        definition.normalizedStart = static_cast<float>(startBone) /
            static_cast<float>(chainJoints.size() - 1);
        definition.normalizedEnd = static_cast<float>(endBone) /
            static_cast<float>(chainJoints.size() - 1);
        definition.recommendedLocalRadius =
            kRecommendedRadii[segmentIndex];
        result.capsules.push_back(definition);
    }
    if (!result.capsules.empty()) {
        result.capsules.back().role = KrakenColliderPreviewRole::Attack;
    }
    result.tipSphere.chainIndex = chainIndex;
    result.tipSphere.role = KrakenColliderPreviewRole::WeakPoint;
    result.tipSphere.tipJointIndex = chainJoints.back();
    result.tipSphere.recommendedLocalRadius = 0.30f;
    result.tipSphere.bindTipSkeletonPosition = bindTipSkeletonPosition;
    result.valid = true;
    return result;
}

KrakenTentacleCapsuleColliderEvaluation
EvaluateKrakenTentacleCapsuleCollider(
    const Skeleton& skeleton,
    const Matrix4x4& worldMatrix,
    int skeletonRootJointIndex,
    int startJointIndex,
    int endJointIndex,
    float localRadius,
    float radiusScale,
    float globalRadiusScale) {
    KrakenTentacleCapsuleColliderEvaluation result{};
    result.worldRadius = localRadius * radiusScale * globalRadiusScale *
        ExtractMaximumScale(worldMatrix);
    result.startJointValid = IsJointIndexValid(
        startJointIndex, skeleton, skeletonRootJointIndex);
    result.endJointValid = IsJointIndexValid(
        endJointIndex, skeleton, skeletonRootJointIndex);
    if (result.startJointValid && result.endJointValid) {
        result.worldStart = TransformPosition(
            skeleton.joints[static_cast<std::size_t>(
                startJointIndex)].worldTranslate,
            worldMatrix);
        result.worldEnd = TransformPosition(
            skeleton.joints[static_cast<std::size_t>(
                endJointIndex)].worldTranslate,
            worldMatrix);
        result.worldCenter = {
            (result.worldStart.x + result.worldEnd.x) * 0.5f,
            (result.worldStart.y + result.worldEnd.y) * 0.5f,
            (result.worldStart.z + result.worldEnd.z) * 0.5f,
        };
        const Vector3 difference = Subtract(
            result.worldEnd, result.worldStart);
        result.worldLength = Length(difference);
        if (std::isfinite(result.worldLength) &&
            result.worldLength > kMinimumColliderLength) {
            const float inverseLength = 1.0f / result.worldLength;
            result.worldDirection = {
                difference.x * inverseLength,
                difference.y * inverseLength,
                difference.z * inverseLength,
            };
        }
    }

    result.positionsFinite =
        IsFinite(result.worldStart) &&
        IsFinite(result.worldEnd) &&
        IsFinite(result.worldCenter) &&
        IsFinite(result.worldDirection) &&
        std::isfinite(result.worldLength);
    result.radiusFinite = std::isfinite(localRadius) &&
        std::isfinite(radiusScale) &&
        std::isfinite(result.worldRadius);
    result.radiusPositive = localRadius > 0.0f &&
        radiusScale > 0.0f && result.worldRadius > 0.0f;
    result.zeroLength = result.startJointValid && result.endJointValid &&
        std::isfinite(result.worldLength) &&
        (startJointIndex == endJointIndex ||
            result.worldLength <= kMinimumColliderLength);
    result.valid = result.startJointValid && result.endJointValid &&
        result.positionsFinite && result.radiusFinite &&
        result.radiusPositive && !result.zeroLength;
    return result;
}

KrakenTentacleTipSphereColliderEvaluation
EvaluateKrakenTentacleTipSphereCollider(
    const Skeleton& skeleton,
    const Matrix4x4& worldMatrix,
    int skeletonRootJointIndex,
    int tipJointIndex,
    const Vector3& bindTipSkeletonPosition,
    bool hasBindTipPosition,
    float localRadius,
    float radiusScale,
    float globalRadiusScale) {
    KrakenTentacleTipSphereColliderEvaluation result{};
    result.worldRadius = localRadius * radiusScale * globalRadiusScale *
        ExtractMaximumScale(worldMatrix);
    result.jointValid = IsJointIndexValid(
        tipJointIndex, skeleton, skeletonRootJointIndex);
    if (result.jointValid && hasBindTipPosition) {
        result.worldPosition = TransformPosition(
            skeleton.joints[static_cast<std::size_t>(
                tipJointIndex)].worldTranslate,
            worldMatrix);
        result.bindWorldPosition = TransformPosition(
            bindTipSkeletonPosition,
            worldMatrix);
        result.distanceFromBind = Distance(
            result.worldPosition,
            result.bindWorldPosition);
    }
    result.positionsFinite =
        IsFinite(result.worldPosition) &&
        IsFinite(result.bindWorldPosition) &&
        std::isfinite(result.distanceFromBind);
    result.radiusFinite = std::isfinite(localRadius) &&
        std::isfinite(radiusScale) &&
        std::isfinite(result.worldRadius);
    result.radiusPositive = localRadius > 0.0f &&
        radiusScale > 0.0f && result.worldRadius > 0.0f;
    result.valid = result.jointValid && hasBindTipPosition &&
        result.positionsFinite && result.radiusFinite &&
        result.radiusPositive;
    return result;
}

KrakenTentacleColliderPhaseSettings
SanitizeKrakenTentacleColliderPhaseSettings(
    const KrakenTentacleColliderPhaseSettings& settings) {
    KrakenTentacleColliderPhaseSettings result = settings;
    if (!std::isfinite(result.attackActiveStartRatio)) {
        result.attackActiveStartRatio = kDefaultAttackActiveStartRatio;
    }
    result.attackActiveStartRatio = std::clamp(
        result.attackActiveStartRatio, 0.0f, 1.0f);
    return result;
}

KrakenTentacleColliderPhaseContext
BuildKrakenTentacleColliderPhaseContext(
    const KrakenTentacleColliderPhaseState& state,
    const KrakenTentacleColliderPhaseSettings& settings) {
    KrakenTentacleColliderPhaseContext context{};
    context.state = state;
    context.settings =
        SanitizeKrakenTentacleColliderPhaseSettings(settings);
    context.snapshotScalarsFinite =
        std::isfinite(state.motionElapsedTime) &&
        std::isfinite(state.phaseElapsedTime) &&
        std::isfinite(state.phaseDuration) &&
        std::isfinite(state.phaseNormalizedTime);
    context.phaseKnown = IsKnownPhase(state.phase);
    context.selectedChainValid =
        state.selectedChainIndex < state.chainCount;
    context.slamDurationValid = std::isfinite(state.slamDuration) &&
        state.slamDuration > kMinimumValidSlamDuration;
    if (context.slamDurationValid &&
        std::isfinite(state.phaseElapsedTime)) {
        context.slamProgress = std::clamp(
            state.phaseElapsedTime /
                (std::max)(state.slamDuration,
                    kMinimumValidSlamDuration),
            0.0f,
            1.0f);
    }
    return context;
}

KrakenTentacleColliderPhaseEvaluation
EvaluateKrakenTentacleColliderPhase(
    KrakenColliderPreviewRole role,
    bool enabled,
    bool valid,
    std::size_t colliderChainIndex,
    const KrakenTentacleColliderPhaseContext& context) {
    if (!enabled) {
        return { false, KrakenColliderPhaseReason::ColliderDisabled };
    }
    if (!valid) {
        return { false, KrakenColliderPhaseReason::ColliderInvalid };
    }
    if (!context.state.connected) {
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
    if (context.state.safetyRecovery) {
        return { false, KrakenColliderPhaseReason::SafetyRecovery };
    }
    if (!context.state.motionStateValid ||
        !context.snapshotScalarsFinite) {
        return { false, KrakenColliderPhaseReason::MotionStateInvalid };
    }
    if (!context.state.attackMotionActive) {
        return { false, KrakenColliderPhaseReason::NotAttackMotionMode };
    }
    if (context.state.waitingForLoop) {
        return { false, KrakenColliderPhaseReason::LoopWaitInactive };
    }
    if (!context.selectedChainValid) {
        return { false, KrakenColliderPhaseReason::AttackChainOutOfRange };
    }
    if (colliderChainIndex != context.state.selectedChainIndex) {
        return { false, KrakenColliderPhaseReason::DifferentAttackChain };
    }
    if (!context.slamDurationValid) {
        return { false, KrakenColliderPhaseReason::InvalidSlamDuration };
    }
    if (!context.phaseKnown) {
        return { false, KrakenColliderPhaseReason::UnknownPhase };
    }

    switch (context.state.phase) {
    case KrakenTentacleColliderAttackPhase::Windup:
        return { false, KrakenColliderPhaseReason::WindupInactive };
    case KrakenTentacleColliderAttackPhase::WindupHold:
        return { false, KrakenColliderPhaseReason::WindupHoldInactive };
    case KrakenTentacleColliderAttackPhase::Slam:
        return context.slamProgress >=
            context.settings.attackActiveStartRatio
            ? KrakenTentacleColliderPhaseEvaluation{
                true, KrakenColliderPhaseReason::AttackSlamLateActive }
            : KrakenTentacleColliderPhaseEvaluation{
                false, KrakenColliderPhaseReason::SlamBeforeThreshold };
    case KrakenTentacleColliderAttackPhase::ImpactHold:
        return context.settings.impactHoldActive
            ? KrakenTentacleColliderPhaseEvaluation{
                true, KrakenColliderPhaseReason::AttackImpactHoldActive }
            : KrakenTentacleColliderPhaseEvaluation{
                false, KrakenColliderPhaseReason::ImpactHoldDisabled };
    case KrakenTentacleColliderAttackPhase::Recovery:
        return { false, KrakenColliderPhaseReason::RecoveryInactive };
    case KrakenTentacleColliderAttackPhase::Completed:
        return { false, KrakenColliderPhaseReason::CompletedInactive };
    case KrakenTentacleColliderAttackPhase::Invalid:
    default:
        return { false, KrakenColliderPhaseReason::UnknownPhase };
    }
}

const char* GetKrakenTentacleColliderDefinitionErrorJapaneseLabel(
    KrakenTentacleColliderDefinitionError error) {
    switch (error) {
    case KrakenTentacleColliderDefinitionError::None:
        return "";
    case KrakenTentacleColliderDefinitionError::ChainOutOfRange:
        return "選択中の触手チェーンが範囲外です。";
    case KrakenTentacleColliderDefinitionError::ChainEmpty:
        return "触手チェーンのボーン数が不足しています。";
    case KrakenTentacleColliderDefinitionError::BindTipPositionNotFinite:
        return "先端のバインド位置が有限値ではありません。";
    case KrakenTentacleColliderDefinitionError::InvalidJointIndex:
        return "触手チェーンに無効なジョイント番号があります。";
    default:
        return "触手コライダー定義を生成できませんでした。";
    }
}
