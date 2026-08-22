#pragma once

#include "Matrix4x4.h"

#include <cstdint>
#include <string>

struct KrakenTentacleDefeatSettings {
    static constexpr float kRecommendedHoldTime = 0.08f;
    static constexpr float kRecommendedRetreatDuration = 0.50f;
    static constexpr float kRecommendedRetreatDistance = 14.0f;

    float holdTime = kRecommendedHoldTime;
    float retreatDuration = kRecommendedRetreatDuration;
    float retreatDistance = kRecommendedRetreatDistance;
};

struct KrakenTentacleDefeatMotionSample {
    Vector3 worldPosition{};
    float normalizedTime = 0.0f;
    float easedTime = 0.0f;
    bool completed = false;
    bool valid = false;
};

enum class KrakenTentacleDefeatMotionPhase : std::uint8_t {
    Defeated,
    Retreating,
    Completed,
};

struct KrakenTentacleDefeatAdvanceResult {
    KrakenTentacleDefeatMotionPhase phase =
        KrakenTentacleDefeatMotionPhase::Defeated;
    KrakenTentacleDefeatMotionSample motion{};
    float stateElapsedTime = 0.0f;
    bool beganRetreat = false;
    bool completedNow = false;
    bool valid = false;
};

struct KrakenTentacleDefeatDiagnostics {
    std::uint64_t beginCount = 0;
    std::uint64_t duplicateBeginSuppressionCount = 0;
    std::uint64_t retreatBeginCount = 0;
    std::uint64_t retreatCompleteCount = 0;
    std::uint64_t frozenPoseCaptureSuccessCount = 0;
    std::uint64_t frozenPoseCaptureFailureCount = 0;
    std::uint64_t nonFinitePositionCount = 0;
    std::uint64_t nonFiniteTimeCount = 0;
    std::uint64_t invalidDistanceCount = 0;
    std::uint64_t additionalDamageRejectionCount = 0;
    std::uint64_t postHpZeroBulletKillRequestCount = 0;
    std::uint64_t waveNotificationCount = 0;
    float retreatProgress = 0.0f;
    float easedRetreatProgress = 0.0f;
    std::string lastError;
    std::string lastWarning;
};

bool IsKrakenTentacleDefeatSettingsValid(
    const KrakenTentacleDefeatSettings& settings);

KrakenTentacleDefeatSettings SanitizeKrakenTentacleDefeatSettings(
    const KrakenTentacleDefeatSettings& settings);

KrakenTentacleDefeatMotionSample EvaluateKrakenTentacleDefeatMotion(
    const Vector3& startWorldPosition,
    float elapsedTime,
    const KrakenTentacleDefeatSettings& settings);

KrakenTentacleDefeatAdvanceResult AdvanceKrakenTentacleDefeatMotion(
    KrakenTentacleDefeatMotionPhase phase,
    float stateElapsedTime,
    float deltaTime,
    const Vector3& startWorldPosition,
    const KrakenTentacleDefeatSettings& settings);
