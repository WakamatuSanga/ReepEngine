#include "PlayerBulletManagerLockedWingHomingMath.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kMinimumVectorLength = 0.00001f;
constexpr float kMinimumAngle = 0.000001f;

Vector3 Add(const Vector3& lhs, const Vector3& rhs) {
    return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

Vector3 Scale(const Vector3& value, float scale) {
    return { value.x * scale, value.y * scale, value.z * scale };
}

float Dot(const Vector3& lhs, const Vector3& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vector3 Cross(const Vector3& lhs, const Vector3& rhs) {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

bool IsFinite(const Vector3& value) {
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

bool TryNormalize(const Vector3& value, Vector3& normalized) {
    if (!IsFinite(value)) {
        return false;
    }
    const float lengthSquared = Dot(value, value);
    if (!std::isfinite(lengthSquared)
        || lengthSquared <= kMinimumVectorLength * kMinimumVectorLength) {
        return false;
    }
    normalized = Scale(value, 1.0f / std::sqrt(lengthSquared));
    return IsFinite(normalized);
}

bool TryMakeRotationAxis(
    const Vector3& currentDirection,
    const Vector3& referenceDirection,
    Vector3& axis) {
    return TryNormalize(Cross(currentDirection, referenceDirection), axis);
}

bool TryMakeStableOppositeAxis(
    const Vector3& currentDirection,
    const Vector3& preferredUp,
    Vector3& axis) {
    if (TryMakeRotationAxis(currentDirection, preferredUp, axis)) {
        return true;
    }
    if (TryMakeRotationAxis(
            currentDirection, { 0.0f, 1.0f, 0.0f }, axis)) {
        return true;
    }
    if (TryMakeRotationAxis(
            currentDirection, { 1.0f, 0.0f, 0.0f }, axis)) {
        return true;
    }
    return TryMakeRotationAxis(
        currentDirection, { 0.0f, 0.0f, 1.0f }, axis);
}

Vector3 RotateAroundAxis(
    const Vector3& direction,
    const Vector3& axis,
    float angleRadians) {
    const float cosine = std::cos(angleRadians);
    const float sine = std::sin(angleRadians);
    return Add(
        Add(
            Scale(direction, cosine),
            Scale(Cross(axis, direction), sine)),
        Scale(axis, Dot(axis, direction) * (1.0f - cosine)));
}
} // namespace

namespace PlayerLockedWingHomingMath {

RotateDirectionResult RotateDirectionTowards(
    const Vector3& currentDirection,
    const Vector3& desiredDirection,
    float maximumTurnRadians,
    const Vector3& preferredUp) {
    RotateDirectionResult result;
    Vector3 current{};
    if (!TryNormalize(currentDirection, current)) {
        result.status = RotateDirectionStatus::InvalidCurrentDirection;
        return result;
    }
    result.direction = current;

    Vector3 desired{};
    if (!TryNormalize(desiredDirection, desired)) {
        result.status = RotateDirectionStatus::InvalidDesiredDirection;
        return result;
    }
    if (!std::isfinite(maximumTurnRadians) || maximumTurnRadians < 0.0f) {
        result.status = RotateDirectionStatus::InvalidMaximumTurn;
        return result;
    }

    result.directionDot = std::clamp(Dot(current, desired), -1.0f, 1.0f);
    result.angleRadians = std::acos(result.directionDot);
    if (result.angleRadians <= kMinimumAngle) {
        result.direction = desired;
        return result;
    }

    const float appliedTurn = (std::min)(
        maximumTurnRadians, result.angleRadians);
    result.appliedTurnRadians = appliedTurn;
    result.clampedByMaximumTurn =
        maximumTurnRadians + kMinimumAngle < result.angleRadians;
    if (!result.clampedByMaximumTurn) {
        result.direction = desired;
        return result;
    }
    if (appliedTurn <= kMinimumAngle) {
        return result;
    }

    Vector3 axis{};
    if (!TryMakeRotationAxis(current, desired, axis)) {
        result.usedOppositeFallback = true;
        if (!TryMakeStableOppositeAxis(current, preferredUp, axis)) {
            result.status = RotateDirectionStatus::InvalidCurrentDirection;
            return result;
        }
    }

    Vector3 rotated{};
    if (!TryNormalize(
            RotateAroundAxis(current, axis, appliedTurn), rotated)) {
        result.status = RotateDirectionStatus::InvalidCurrentDirection;
        return result;
    }
    result.direction = rotated;
    return result;
}

} // namespace PlayerLockedWingHomingMath
