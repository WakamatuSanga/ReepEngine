#include "PlayerBulletManager.h"
#include "LockedWingMissileExhaustController.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
constexpr float kHelixFullTurns = 1.6f;
constexpr float kHelixFullRadius = 1.15f;
constexpr float kHelixFullDistance = 30.0f;
constexpr float kFullTurn = 2.0f * std::numbers::pi_v<float>;

Vector3 Add(const Vector3& a, const Vector3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Subtract(const Vector3& a, const Vector3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 Scale(const Vector3& v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}

float Dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 Normalize(const Vector3& v, const Vector3& fallback) {
    const float lengthSquared = Dot(v, v);
    return std::isfinite(lengthSquared) && lengthSquared > 0.00000001f
        ? Scale(v, 1.0f / std::sqrt(lengthSquared)) : fallback;
}

float SmoothStep(float value) {
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
}

Vector3 PlayerBulletManager::AdvanceLockedWingHelix(
    LockedWingLaunchState& state, const Vector3& previousDirection,
    const Vector3& previousPosition, float cruiseDeltaTime) {
    const float travel = state.baseBulletSpeed * cruiseDeltaTime;
    if (!state.homingEnabled) {
        if (state.helixLength > 0.0f) {
            // Continue from the actual missile position and tangent when the lock is lost.
            state.currentFlightDirection = state.visualFlightDirection;
            state.centerPosition = previousPosition;
            state.helixLength = 0.0f;
        }
        state.visualFlightDirection = state.currentFlightDirection;
        state.endVelocity = Scale(state.currentFlightDirection, state.baseBulletSpeed);
        state.centerPosition = Add(state.centerPosition, Scale(state.currentFlightDirection, travel));
        state.sweep.points[state.sweep.count++] = state.centerPosition;
        return state.centerPosition;
    }

    const float gap = (std::max)(0.0f, state.targetDistance - state.targetRadius - 0.15f);
    if (state.helixLength == 0.0f) {
        state.helixLength = (std::max)(1.0f, gap);
        state.helixProgressDistance = 0.0f;
        state.previousHelixTargetDistance = state.targetDistance;
        state.closestHelixTargetDistance = gap;
        const Vector3 reference = std::abs(previousDirection.y) < 0.9f
            ? Vector3{0.0f, 1.0f, 0.0f} : Vector3{1.0f, 0.0f, 0.0f};
        const Vector3 fallback = Normalize(MatrixMath::Cross(reference, previousDirection), {1.0f, 0.0f, 0.0f});
        state.helixRight = Normalize(
            MatrixMath::Cross(Scale(state.currentEjectionDownDirection, -1.0f), previousDirection), fallback);
    }

    const float distanceScale = SmoothStep(state.helixLength / kHelixFullDistance);
    const float winding = kFullTurn * kHelixFullTurns * distanceScale;
    const float progressStart = state.helixProgressDistance;
    const float approach = std::clamp(
        state.previousHelixTargetDistance - state.targetDistance, travel, travel * 1.75f);
    const float progressEnd = (std::min)(state.helixLength, progressStart + approach);
    const float nearest = (std::min)(state.closestHelixTargetDistance, (std::max)(0.0f, gap - travel));
    const float openingDistance = (std::min)(7.0f, state.helixLength * 0.24f);
    const float closingDistance = (std::min)(10.0f, state.helixLength * 0.32f);
    const float phaseOffset = kFullTurn * static_cast<float>(state.sequence % kMaxLockAmmo) / kMaxLockAmmo;
    const int steps = std::clamp(
        static_cast<int>(std::ceil(winding * (progressEnd - progressStart) / state.helixLength / 0.25f)),
        1, static_cast<int>(state.sweep.points.size() - state.sweep.count));
    const Vector3 centerStart = state.centerPosition;
    const Vector3 centerStep = Scale(state.currentFlightDirection, travel);
    const Vector3 directionChange = Subtract(state.currentFlightDirection, previousDirection);
    for (int step = 1; step <= steps; ++step) {
        const float fraction = static_cast<float>(step) / static_cast<float>(steps);
        const Vector3 axis = Normalize(Add(previousDirection, Scale(directionChange, fraction)), state.currentFlightDirection);
        // Transport the prior right vector into the new plane to avoid a pole or a sign flip.
        state.helixRight = Normalize(Subtract(state.helixRight, Scale(axis, Dot(state.helixRight, axis))), state.helixRight);
        const Vector3 up = MatrixMath::Cross(axis, state.helixRight);
        const float progress = std::lerp(progressStart, progressEnd, fraction);
        const float gapAtStep = std::lerp(state.closestHelixTargetDistance, nearest, fraction);
        const float remaining = (std::min)(state.helixLength - progress, gapAtStep);
        const float radius = kHelixFullRadius * distanceScale
            * SmoothStep(progress / openingDistance) * SmoothStep(remaining / closingDistance);
        const float phase = phaseOffset + winding * progress / state.helixLength;
        const Vector3 radial = Add(Scale(state.helixRight, std::cos(phase)), Scale(up, std::sin(phase)));
        state.sweep.points[state.sweep.count++] = Add(Add(centerStart, Scale(centerStep, fraction)), Scale(radial, radius));
    }
    const Vector3 end = state.sweep.points[state.sweep.count - 1];
    const Vector3 tangent = Subtract(end, state.sweep.points[state.sweep.count - 2]);
    state.visualFlightDirection = Normalize(tangent, state.currentFlightDirection);
    state.endVelocity = Scale(tangent, static_cast<float>(steps) / cruiseDeltaTime);
    state.centerPosition = Add(centerStart, centerStep);
    state.helixProgressDistance = progressEnd;
    state.previousHelixTargetDistance = state.targetDistance;
    state.closestHelixTargetDistance = nearest;
    return end;
}

void PlayerBulletManager::DrawLockedWingMissileLaunchImGui() {
    DrawLockedWingMissileIgnitionImGui();
    DrawLockedWingHomingImGui();
    if (lockedWingMissileExhaustController_) {
        lockedWingMissileExhaustController_->DrawImGui();
    }
}
