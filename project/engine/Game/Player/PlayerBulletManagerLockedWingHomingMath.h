#pragma once

#include "Engine/math/Matrix4x4.h"

namespace PlayerLockedWingHomingMath {

enum class RotateDirectionStatus {
    Success,
    InvalidCurrentDirection,
    InvalidDesiredDirection,
    InvalidMaximumTurn,
};

struct RotateDirectionResult {
    Vector3 direction{ 0.0f, 0.0f, 1.0f };
    float directionDot = 1.0f;
    float angleRadians = 0.0f;
    float appliedTurnRadians = 0.0f;
    bool usedOppositeFallback = false;
    bool clampedByMaximumTurn = false;
    RotateDirectionStatus status = RotateDirectionStatus::Success;
};

RotateDirectionResult RotateDirectionTowards(
    const Vector3& currentDirection,
    const Vector3& desiredDirection,
    float maximumTurnRadians,
    const Vector3& preferredUp);

} // namespace PlayerLockedWingHomingMath
