#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossDefeat.h"

#include <algorithm>
#include <cmath>

namespace {
bool IsFinite(const Vector3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}
}

bool IsKrakenTentacleDefeatSettingsValid(
    const KrakenTentacleDefeatSettings& settings) {
    return std::isfinite(settings.holdTime) && settings.holdTime >= 0.0f &&
        std::isfinite(settings.retreatDuration) &&
        settings.retreatDuration > 0.0f &&
        std::isfinite(settings.retreatDistance) &&
        settings.retreatDistance >= 0.0f;
}

KrakenTentacleDefeatSettings SanitizeKrakenTentacleDefeatSettings(
    const KrakenTentacleDefeatSettings& settings) {
    KrakenTentacleDefeatSettings result = settings;
    result.holdTime = std::isfinite(result.holdTime)
        ? std::clamp(result.holdTime, 0.0f, 1.0f)
        : KrakenTentacleDefeatSettings::kRecommendedHoldTime;
    result.retreatDuration = std::isfinite(result.retreatDuration)
        ? std::clamp(result.retreatDuration, 0.1f, 3.0f)
        : KrakenTentacleDefeatSettings::kRecommendedRetreatDuration;
    result.retreatDistance = std::isfinite(result.retreatDistance)
        ? std::clamp(result.retreatDistance, 1.0f, 100.0f)
        : KrakenTentacleDefeatSettings::kRecommendedRetreatDistance;
    return result;
}

KrakenTentacleDefeatMotionSample EvaluateKrakenTentacleDefeatMotion(
    const Vector3& startWorldPosition,
    float elapsedTime,
    const KrakenTentacleDefeatSettings& settings) {
    KrakenTentacleDefeatMotionSample result{};
    result.worldPosition = startWorldPosition;
    if (!IsFinite(startWorldPosition) || !std::isfinite(elapsedTime) ||
        elapsedTime < 0.0f || !IsKrakenTentacleDefeatSettingsValid(settings)) {
        return result;
    }

    const float duration = (std::max)(settings.retreatDuration, 0.001f);
    result.normalizedTime = std::clamp(elapsedTime / duration, 0.0f, 1.0f);
    const float inverseTime = 1.0f - result.normalizedTime;
    result.easedTime = 1.0f - inverseTime * inverseTime * inverseTime;
    result.worldPosition.y = startWorldPosition.y -
        settings.retreatDistance * result.easedTime;
    result.completed = result.normalizedTime >= 1.0f;
    result.valid = IsFinite(result.worldPosition) &&
        std::isfinite(result.easedTime);
    return result;
}

KrakenTentacleDefeatAdvanceResult AdvanceKrakenTentacleDefeatMotion(
    KrakenTentacleDefeatMotionPhase phase,
    float stateElapsedTime,
    float deltaTime,
    const Vector3& startWorldPosition,
    const KrakenTentacleDefeatSettings& settings) {
    KrakenTentacleDefeatAdvanceResult result{};
    result.phase = phase;
    result.stateElapsedTime = stateElapsedTime;
    result.motion.worldPosition = startWorldPosition;
    if (!IsFinite(startWorldPosition) || !std::isfinite(stateElapsedTime) ||
        stateElapsedTime < 0.0f || !std::isfinite(deltaTime) ||
        deltaTime < 0.0f || !IsKrakenTentacleDefeatSettingsValid(settings)) {
        return result;
    }

    float remaining = deltaTime;
    if (phase == KrakenTentacleDefeatMotionPhase::Defeated) {
        const float available = (std::max)(
            settings.holdTime - stateElapsedTime, 0.0f);
        const float consumed = (std::min)(remaining, available);
        result.stateElapsedTime += consumed;
        remaining -= consumed;
        result.motion.valid = true;
        if (result.stateElapsedTime + 0.000001f < settings.holdTime) {
            result.valid = true;
            return result;
        }
        result.phase = KrakenTentacleDefeatMotionPhase::Retreating;
        result.stateElapsedTime = 0.0f;
        result.beganRetreat = true;
    } else if (phase == KrakenTentacleDefeatMotionPhase::Completed) {
        result.motion = EvaluateKrakenTentacleDefeatMotion(
            startWorldPosition, settings.retreatDuration, settings);
        result.valid = result.motion.valid;
        return result;
    } else if (phase != KrakenTentacleDefeatMotionPhase::Retreating) {
        return result;
    }

    result.stateElapsedTime += remaining;
    result.motion = EvaluateKrakenTentacleDefeatMotion(
        startWorldPosition, result.stateElapsedTime, settings);
    if (!result.motion.valid) {
        return result;
    }
    if (result.motion.completed) {
        result.phase = KrakenTentacleDefeatMotionPhase::Completed;
        result.stateElapsedTime = 0.0f;
        result.completedNow = true;
    }
    result.valid = true;
    return result;
}
