#pragma once

#include "Engine/Game/Boss/Kraken/KrakenTentaclePoseEvaluator.h"

#include <cstddef>
#include <cstdint>

struct Skeleton;

// Small distributed lag, zero at phase boundaries and at the targeted Slam endpoint.
float EvaluateKrakenTentacleSlamFlex(
    KrakenTentacleAttackPreviewPhase phase, float progress, float upperPosition);

struct KrakenTentacleWholeSlamTargetSettings {
    float hingeGain = 1.0f;
    float minimumHingeDegrees = 0.0f;
    float maximumHingeDegrees = 85.0f;
};

struct KrakenTentacleWholeSlamPoseDiagnostics {
    Vector3 pivotWorldPosition{};
    Vector3 bindTipWorldPosition{};
    Vector3 tipWorldPosition{};
    Vector3 bindTentacleWorldDirection{};
    Vector3 tentacleWorldDirection{};
    Vector3 targetWorldDirection{};
    Vector3 hingeWorldAxis{};
    Vector3 hingeParentLocalAxis{};
    int pivotJointIndex = -1;
    int pivotParentJointIndex = -1;
    float targetAlignmentDegrees = 0.0f;
    float configuredWindupDegrees = -25.0f;
    float configuredSlamDegrees = 80.0f;
    float requiredHingeDegrees = 0.0f;
    float targetHingeDegrees = 0.0f;
    float fixedHingeOvershootDegrees = 0.0f;
    float targetHingeOvershootDegrees = 0.0f;
    float hingeGain = 1.0f;
    float minimumHingeDegrees = 0.0f;
    float maximumHingeDegrees = 85.0f;
    float slamProgress = 0.0f;
    float appliedMainHingeDegrees = 0.0f;
    float appliedUpperBendDegrees = 0.0f;
    bool targetValid = false;
    bool mainHingeApplied = false;
    bool finite = false;
    bool valid = false;
};

struct KrakenTentacleWholeSlamRuntimeDiagnostics {
    KrakenTentacleWholeSlamTargetSettings targetSettings{};
    KrakenTentacleWholeSlamPoseDiagnostics pose{};
    Vector3 attackTargetWorldPosition{};
    std::uint64_t attackSequenceId = 0;
    std::uint64_t targetCaptureCount = 0;
    std::size_t chainIndex = 0;
    bool attackTargetSnapshotValid = false;
};

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
    KrakenTentacleWholeSlamPoseDiagnostics& outDiagnostics);
