#pragma once

#include "Matrix4x4.h"

#include <cstddef>
#include <cstdint>
#include <vector>

struct Skeleton;

enum class KrakenColliderPreviewRole : std::uint8_t {
    Attack,
    Damage,
    WeakPoint,
};

enum class KrakenColliderPhaseReason : std::uint8_t {
    DamageAlwaysActive,
    WeakPointAlwaysActive,
    AttackSlamLateActive,
    AttackImpactHoldActive,
    ColliderDisabled,
    ColliderInvalid,
    MotionStateInvalid,
    PreviewDisconnected,
    SafetyRecovery,
    NotAttackMotionMode,
    AttackChainOutOfRange,
    DifferentAttackChain,
    WindupInactive,
    WindupHoldInactive,
    SlamBeforeThreshold,
    InvalidSlamDuration,
    ImpactHoldDisabled,
    LoopWaitInactive,
    RecoveryInactive,
    CompletedInactive,
    UnknownPhase,
    UnknownRole,
};

enum class KrakenTentacleColliderDefinitionError : std::uint8_t {
    None,
    ChainOutOfRange,
    ChainEmpty,
    BindTipPositionNotFinite,
    InvalidJointIndex,
};

struct KrakenTentacleCapsuleColliderDefinition {
    std::uint32_t colliderIndex = 0;
    std::uint32_t chainIndex = 0;
    KrakenColliderPreviewRole role = KrakenColliderPreviewRole::Damage;
    int startJointIndex = -1;
    int endJointIndex = -1;
    float normalizedStart = 0.0f;
    float normalizedEnd = 0.0f;
    float recommendedLocalRadius = 0.25f;
};

struct KrakenTentacleTipSphereColliderDefinition {
    std::uint32_t chainIndex = 0;
    KrakenColliderPreviewRole role = KrakenColliderPreviewRole::WeakPoint;
    int tipJointIndex = -1;
    float recommendedLocalRadius = 0.30f;
    Vector3 bindTipSkeletonPosition{};
};

struct KrakenTentacleColliderDefinitionResult {
    std::vector<KrakenTentacleCapsuleColliderDefinition> capsules;
    KrakenTentacleTipSphereColliderDefinition tipSphere{};
    KrakenTentacleColliderDefinitionError error =
        KrakenTentacleColliderDefinitionError::None;
    bool valid = false;
};

struct KrakenTentacleCapsuleColliderEvaluation {
    Vector3 worldStart{};
    Vector3 worldEnd{};
    Vector3 worldCenter{};
    Vector3 worldDirection{ 0.0f, 1.0f, 0.0f };
    float worldLength = 0.0f;
    float worldRadius = 0.0f;
    bool startJointValid = false;
    bool endJointValid = false;
    bool positionsFinite = false;
    bool radiusFinite = false;
    bool radiusPositive = false;
    bool zeroLength = false;
    bool valid = false;
};

struct KrakenTentacleTipSphereColliderEvaluation {
    Vector3 worldPosition{};
    Vector3 bindWorldPosition{};
    float distanceFromBind = 0.0f;
    float worldRadius = 0.0f;
    bool jointValid = false;
    bool positionsFinite = false;
    bool radiusFinite = false;
    bool radiusPositive = false;
    bool valid = false;
};

enum class KrakenTentacleColliderAttackPhase : std::uint8_t {
    Windup,
    WindupHold,
    Slam,
    ImpactHold,
    Recovery,
    Completed,
    Invalid = 0xff,
};

struct KrakenTentacleColliderPhaseState {
    KrakenTentacleColliderAttackPhase phase =
        KrakenTentacleColliderAttackPhase::Invalid;
    std::size_t chainCount = 0;
    std::size_t selectedChainIndex = 0;
    float motionElapsedTime = 0.0f;
    float phaseElapsedTime = 0.0f;
    float phaseDuration = 0.0f;
    float phaseNormalizedTime = 0.0f;
    float slamDuration = 0.0f;
    bool connected = false;
    bool safetyRecovery = false;
    bool motionStateValid = false;
    bool attackMotionActive = false;
    bool waitingForLoop = false;
};

struct KrakenTentacleColliderPhaseSettings {
    float attackActiveStartRatio = 0.65f;
    bool impactHoldActive = true;
};

struct KrakenTentacleColliderPhaseContext {
    KrakenTentacleColliderPhaseState state{};
    KrakenTentacleColliderPhaseSettings settings{};
    float slamProgress = 0.0f;
    bool snapshotScalarsFinite = false;
    bool phaseKnown = false;
    bool selectedChainValid = false;
    bool slamDurationValid = false;
};

struct KrakenTentacleColliderPhaseEvaluation {
    bool active = false;
    KrakenColliderPhaseReason reason =
        KrakenColliderPhaseReason::MotionStateInvalid;
};

KrakenTentacleColliderDefinitionResult
BuildKrakenTentacleColliderDefinitions(
    std::uint32_t detectedChainCount,
    std::uint32_t chainIndex,
    const std::vector<int>& chainJoints,
    int skeletonRootJointIndex,
    std::size_t skeletonJointCount,
    const Vector3& bindTipSkeletonPosition);

KrakenTentacleCapsuleColliderEvaluation
EvaluateKrakenTentacleCapsuleCollider(
    const Skeleton& skeleton,
    const Matrix4x4& worldMatrix,
    int skeletonRootJointIndex,
    int startJointIndex,
    int endJointIndex,
    float localRadius,
    float radiusScale,
    float globalRadiusScale);

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
    float globalRadiusScale);

KrakenTentacleColliderPhaseSettings
SanitizeKrakenTentacleColliderPhaseSettings(
    const KrakenTentacleColliderPhaseSettings& settings);

KrakenTentacleColliderPhaseContext
BuildKrakenTentacleColliderPhaseContext(
    const KrakenTentacleColliderPhaseState& state,
    const KrakenTentacleColliderPhaseSettings& settings);

KrakenTentacleColliderPhaseEvaluation
EvaluateKrakenTentacleColliderPhase(
    KrakenColliderPreviewRole role,
    bool enabled,
    bool valid,
    std::size_t colliderChainIndex,
    const KrakenTentacleColliderPhaseContext& context);

const char* GetKrakenTentacleColliderDefinitionErrorJapaneseLabel(
    KrakenTentacleColliderDefinitionError error);
