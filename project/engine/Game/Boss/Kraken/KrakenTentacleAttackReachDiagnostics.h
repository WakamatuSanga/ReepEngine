#pragma once

#include "Matrix4x4.h"

#include <array>
#include <cstddef>
#include <cstdint>

constexpr std::size_t kKrakenTentacleReachChainCount = 4;

enum class KrakenTentacleReachPoseSample : std::uint8_t {
    Bind,
    Windup,
    SlamStart,
    SlamThreeQuarter,
    SlamThreshold,
    SlamEnd,
    ImpactHold,
    Recovery,
    Count,
};

struct KrakenTentacleReachPhaseDiagnostics {
    Vector3 tipWorldPosition{};
    Vector3 finalMovableBoneWorldPosition{};
    Vector3 attackCapsuleStart{};
    Vector3 attackCapsuleEnd{};
    Vector3 attackCapsuleDirection{};
    Vector3 nearestPlayerPosition{};
    float playerCenterDistance = 0.0f;
    float movementRegionDistance = 0.0f;
    bool valid = false;
};

struct KrakenTentacleReachChainDiagnostics {
    std::array<KrakenTentacleReachPhaseDiagnostics,
        static_cast<std::size_t>(KrakenTentacleReachPoseSample::Count)>
        phaseSamples{};
    Vector3 rootWorldPosition{};
    Vector3 currentTipWorldPosition{};
    Vector3 currentFinalMovableBoneWorldPosition{};
    Vector3 currentAttackCapsuleStart{};
    Vector3 currentAttackCapsuleEnd{};
    Vector3 currentAttackCapsuleDirection{};
    Vector3 bindToSlamTipDirection{};
    Vector3 bindTipToPlayerDirection{};
    Vector3 nearestReachablePlayerPosition{};
    Vector3 farthestReachablePlayerPosition{};
    float tipPlayerDirectionDot = 0.0f;
    float minimumActiveDistance = 0.0f;
    float maximumActiveDistance = 0.0f;
    float combinedRadius = 0.0f;
    float attackCapsuleRadius = 0.0f;
    bool currentValid = false;
    bool predictionValid = false;
    bool reachesMovementRegion = false;
    bool avoidableInMovementRegion = false;
};

struct KrakenTentacleAttackReachDiagnostics {
    std::array<KrakenTentacleReachChainDiagnostics,
        kKrakenTentacleReachChainCount> chains{};
    Vector3 playerMovementMinimum{};
    Vector3 playerMovementMaximum{};
    Vector3 playerMovementCenter{};
    Vector3 playerMovementLeft{};
    Vector3 playerMovementRight{};
    Vector3 playerMovementTop{};
    Vector3 playerMovementBottom{};
    Vector3 cameraRight{};
    Vector3 cameraUp{};
    Vector3 predictedAttackTargetPosition{};
    Matrix4x4 predictionWorldMatrix{};
    float playerMoveLimitX = 0.0f;
    float playerMoveLimitY = 0.0f;
    float playerRadius = 0.0f;
    float predictedPrimarySign = 0.0f;
    float predictedSecondarySign = 0.0f;
    float predictedSlamPrimaryDegrees = 0.0f;
    float predictedSlamSecondaryDegrees = 0.0f;
    float predictedTipBias = 0.0f;
    std::uint32_t predictedFixedLeadingBoneCount = 0;
    std::uint32_t reachableChainMask = 0;
    std::uint32_t avoidableChainMask = 0;
    bool playerMovementRegionValid = false;
    bool predictionValid = false;
};
