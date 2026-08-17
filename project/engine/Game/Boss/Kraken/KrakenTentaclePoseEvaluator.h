#pragma once

#include "Engine/Game/Boss/Kraken/KrakenTentacleChainUtility.h"
#include "Matrix4x4.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct KrakenTentacleIdlePoseSettings {
    float frequencyHz = 0.35f;
    float rootAmplitudeDegrees = 3.0f;
    float tipAmplitudeDegrees = 15.0f;
    float secondaryAmplitudeDegrees = 6.0f;
    float chainPhaseRadians = 0.75f;
    float phaseAlongChainRadians = 0.55f;
    float startupBlendDuration = 0.25f;
    float secondaryFrequencyScale = 0.73f;
};

struct KrakenTentacleIdleJointPose {
    int jointIndex = -1;
    std::size_t chainIndex = 0;
    std::size_t chainBoneIndex = 0;
    float normalizedChainPosition = 0.0f;
    Vector3 localEulerOffsetRadians{};
    bool finite = false;
};

struct KrakenTentacleIdlePoseResult {
    std::vector<KrakenTentacleIdleJointPose> joints;
    bool valid = false;
    std::string errorMessage;
};

bool BuildKrakenTentacleIdlePose(
    const KrakenTentacleIdlePoseSettings& settings,
    float motionTime,
    const std::vector<KrakenTentacleChain>& chains,
    bool applyAllChains,
    std::size_t selectedChainIndex,
    std::size_t skeletonJointCount,
    int skeletonRootJointIndex,
    KrakenTentacleIdlePoseResult& outResult);

enum class KrakenTentacleAttackPreviewPhase : std::uint8_t {
    Windup,
    WindupHold,
    Slam,
    ImpactHold,
    Recovery,
    Completed,
};

enum class KrakenTentacleAttackLocalAxis : std::uint8_t {
    X,
    Y,
    Z,
};

struct KrakenTentacleAttackPreviewSettings {
    float windupDuration = 0.60f;
    float windupHoldDuration = 0.12f;
    float slamDuration = 0.20f;
    float impactHoldDuration = 0.15f;
    float recoveryDuration = 0.55f;
    float loopInterval = 0.25f;

    float windupPrimaryTotalDegrees = -35.0f;
    float windupSecondaryTotalDegrees = 12.0f;
    float slamPrimaryTotalDegrees = 65.0f;
    float slamSecondaryTotalDegrees = -8.0f;
    float tipBias = 1.60f;
    std::uint32_t fixedLeadingBoneCount = 1;

    // Idle Sway bends on local X and curls on local Z.
    KrakenTentacleAttackLocalAxis primaryAxis =
        KrakenTentacleAttackLocalAxis::X;
    KrakenTentacleAttackLocalAxis secondaryAxis =
        KrakenTentacleAttackLocalAxis::Z;
    float primarySign = 1.0f;
    float secondarySign = 1.0f;
};

struct KrakenTentacleAttackPoseTotals {
    float primaryDegrees = 0.0f;
    float secondaryDegrees = 0.0f;
    bool finite = true;
};

struct KrakenTentacleAttackJointPose {
    int jointIndex = -1;
    std::size_t chainBoneIndex = 0;
    float normalizedWeight = 0.0f;
    float primaryDegrees = 0.0f;
    float secondaryDegrees = 0.0f;
    Quaternion attackOffset{};
    Quaternion absoluteLocalRotation{};
    Vector3 absoluteLocalEulerRadians{};
    bool fixed = false;
    bool finite = false;
};

struct KrakenTentacleAttackPoseResult {
    std::vector<KrakenTentacleAttackJointPose> joints;
    std::size_t fixedBoneCount = 0;
    std::size_t movableBoneCount = 0;
    float normalizedWeightSum = 0.0f;
    bool valid = false;
    std::string errorMessage;
};

using KrakenTentacleAttackPhase = KrakenTentacleAttackPreviewPhase;
using KrakenTentacleAttackSettings = KrakenTentacleAttackPreviewSettings;

KrakenTentacleAttackPreviewSettings SanitizeKrakenTentacleAttackSettings(
    const KrakenTentacleAttackPreviewSettings& settings);

float GetKrakenTentacleAttackMotionDuration(
    const KrakenTentacleAttackPreviewSettings& settings);

float GetKrakenTentacleAttackPhaseDuration(
    const KrakenTentacleAttackPreviewSettings& settings,
    KrakenTentacleAttackPreviewPhase phase);

float GetKrakenTentacleAttackPhaseStartTime(
    const KrakenTentacleAttackPreviewSettings& settings,
    KrakenTentacleAttackPreviewPhase phase);

KrakenTentacleAttackPoseTotals EvaluateKrakenTentacleAttackPoseTotals(
    const KrakenTentacleAttackPreviewSettings& settings,
    KrakenTentacleAttackPreviewPhase phase,
    float phaseElapsedTime);

bool BuildKrakenTentacleAttackPose(
    const KrakenTentacleAttackPreviewSettings& settings,
    const KrakenTentacleAttackPoseTotals& totals,
    const std::vector<int>& chainJoints,
    const std::vector<Vector3>& bindLocalEulerRadians,
    int skeletonRootJointIndex,
    KrakenTentacleAttackPoseResult& outResult);
