#include "Engine/Game/Boss/Kraken/KrakenTentacleCollisionQuery.h"

#include <algorithm>
#include <cmath>

namespace {
    bool IsFinite(const Vector3& value) {
        return std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
    }

    bool IsValidRadius(float radius) {
        return std::isfinite(radius) && radius > 0.0f;
    }

    Vector3 AddScaled(
        const Vector3& origin,
        const Vector3& direction,
        double scale) {
        return {
            static_cast<float>(origin.x + direction.x * scale),
            static_cast<float>(origin.y + direction.y * scale),
            static_cast<float>(origin.z + direction.z * scale),
        };
    }

    double DistanceSquared(const Vector3& lhs, const Vector3& rhs) {
        const double x = static_cast<double>(lhs.x) - rhs.x;
        const double y = static_cast<double>(lhs.y) - rhs.y;
        const double z = static_cast<double>(lhs.z) - rhs.z;
        return x * x + y * y + z * z;
    }

    Vector3 Subtract(const Vector3& a, const Vector3& b) {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    }

    double Dot(const Vector3& a, const Vector3& b) {
        return static_cast<double>(a.x) * b.x + static_cast<double>(a.y) * b.y + static_cast<double>(a.z) * b.z;
    }

    KrakenTentacleCollisionQueryResult BuildResult(
        const Vector3& closestPoint,
        const Vector3& sphereCenter,
        float firstRadius,
        float secondRadius) {
        KrakenTentacleCollisionQueryResult result{};
        const double distanceSquared =
            DistanceSquared(closestPoint, sphereCenter);
        const double radiusSum =
            static_cast<double>(firstRadius) + secondRadius;
        if (!std::isfinite(distanceSquared) ||
            !std::isfinite(radiusSum)) {
            return result;
        }
        result.closestPoint = closestPoint;
        result.centerDistance =
            static_cast<float>(std::sqrt(distanceSquared));
        result.radiusSum = static_cast<float>(radiusSum);
        result.valid = std::isfinite(result.centerDistance) &&
            std::isfinite(result.radiusSum);
        result.intersecting = result.valid &&
            distanceSquared <= radiusSum * radiusSum;
        return result;
    }
}

bool ShouldDeferKrakenBodyHitForWeakPoint(
    const KrakenTentacleCollisionQueryResult& bodyHit,
    const Vector3& projectilePosition,
    const Vector3& projectileVelocity,
    float projectileRadius,
    const Vector3& weakPointCenter,
    float weakPointRadius) {
    if (!bodyHit.valid || !bodyHit.intersecting ||
        !IsFinite(projectilePosition) || !IsFinite(projectileVelocity) ||
        !IsFinite(weakPointCenter) || !IsValidRadius(projectileRadius) ||
        !IsValidRadius(weakPointRadius) ||
        DistanceSquared(bodyHit.closestPoint, weakPointCenter) >
            static_cast<double>(weakPointRadius) * weakPointRadius) {
        return false;
    }
    const double speedSquared = DistanceSquared(projectileVelocity, {});
    const Vector3 toWeakPoint = { weakPointCenter.x - projectilePosition.x,
        weakPointCenter.y - projectilePosition.y,
        weakPointCenter.z - projectilePosition.z };
    const double projection = static_cast<double>(toWeakPoint.x) * projectileVelocity.x +
        static_cast<double>(toWeakPoint.y) * projectileVelocity.y +
        static_cast<double>(toWeakPoint.z) * projectileVelocity.z;
    if (speedSquared <= 0.0 || projection <= 0.0) {
        return false;
    }
    const Vector3 closestOnRay = AddScaled(
        projectilePosition, projectileVelocity, projection / speedSquared);
    const auto currentHit = QueryKrakenSphereSphereIntersection(
        weakPointCenter, weakPointRadius, projectilePosition, projectileRadius);
    const auto rayHit = QueryKrakenSphereSphereIntersection(
        weakPointCenter, weakPointRadius, closestOnRay, projectileRadius);
    return currentHit.valid && !currentHit.intersecting && rayHit.intersecting;
}

KrakenTentacleCollisionQueryResult QueryKrakenCapsuleSphereIntersection(
    const Vector3& capsuleStart,
    const Vector3& capsuleEnd,
    float capsuleRadius,
    const Vector3& sphereCenter,
    float sphereRadius) {
    if (!IsFinite(capsuleStart) || !IsFinite(capsuleEnd) ||
        !IsFinite(sphereCenter) || !IsValidRadius(capsuleRadius) ||
        !IsValidRadius(sphereRadius)) {
        return {};
    }

    const Vector3 segment = {
        capsuleEnd.x - capsuleStart.x,
        capsuleEnd.y - capsuleStart.y,
        capsuleEnd.z - capsuleStart.z,
    };
    const Vector3 toSphere = {
        sphereCenter.x - capsuleStart.x,
        sphereCenter.y - capsuleStart.y,
        sphereCenter.z - capsuleStart.z,
    };
    const double segmentLengthSquared =
        static_cast<double>(segment.x) * segment.x +
        static_cast<double>(segment.y) * segment.y +
        static_cast<double>(segment.z) * segment.z;
    if (segmentLengthSquared <= 0.0) {
        return {};
    }
    const double projection =
        static_cast<double>(toSphere.x) * segment.x +
        static_cast<double>(toSphere.y) * segment.y +
        static_cast<double>(toSphere.z) * segment.z;
    const double normalizedDistance = std::clamp(
        projection / segmentLengthSquared, 0.0, 1.0);
    const Vector3 closestPoint = AddScaled(
        capsuleStart, segment, normalizedDistance);
    if (!IsFinite(closestPoint)) {
        return {};
    }
    return BuildResult(
        closestPoint, sphereCenter, capsuleRadius, sphereRadius);
}

KrakenTentacleCollisionQueryResult QueryKrakenSphereSphereIntersection(
    const Vector3& firstCenter,
    float firstRadius,
    const Vector3& secondCenter,
    float secondRadius) {
    if (!IsFinite(firstCenter) || !IsFinite(secondCenter) ||
        !IsValidRadius(firstRadius) || !IsValidRadius(secondRadius)) {
        return {};
    }
    return BuildResult(
        firstCenter, secondCenter, firstRadius, secondRadius);
}

KrakenTentacleCollisionQueryResult QueryKrakenCapsuleSweptSphereIntersection(
    const Vector3& capsuleStart, const Vector3& capsuleEnd, float capsuleRadius,
    const Vector3& sweepStart, const Vector3& sweepEnd, float sphereRadius) {
    if (!IsFinite(capsuleStart) || !IsFinite(capsuleEnd) ||
        !IsFinite(sweepStart) || !IsFinite(sweepEnd) ||
        !IsValidRadius(capsuleRadius) || !IsValidRadius(sphereRadius)) {
        return {};
    }

    const Vector3 axis = Subtract(capsuleEnd, capsuleStart);
    const Vector3 step = Subtract(sweepEnd, sweepStart);
    const Vector3 offset = Subtract(capsuleStart, sweepStart);
    const double axisLengthSquared = Dot(axis, axis);
    const double stepLengthSquared = Dot(step, step);
    if (stepLengthSquared <= 0.0) {
        return axisLengthSquared > 0.0
            ? QueryKrakenCapsuleSphereIntersection(capsuleStart, capsuleEnd, capsuleRadius, sweepStart, sphereRadius)
            : BuildResult(capsuleStart, sweepStart, capsuleRadius, sphereRadius);
    }

    const double alongStep = Dot(step, offset);
    if (axisLengthSquared <= 0.0) {
        const double fraction = std::clamp(alongStep / stepLengthSquared, 0.0, 1.0);
        return BuildResult(capsuleStart, AddScaled(sweepStart, step, fraction), capsuleRadius, sphereRadius);
    }

    const double alongAxis = Dot(axis, offset);
    const double alignment = Dot(axis, step);
    const double denominator = axisLengthSquared * stepLengthSquared - alignment * alignment;
    double axisFraction = 0.0;
    if (denominator > 1.0e-12 * axisLengthSquared * stepLengthSquared) {
        axisFraction = std::clamp((alignment * alongStep - alongAxis * stepLengthSquared) / denominator, 0.0, 1.0);
    }
    const double unconstrainedStep = (alignment * axisFraction + alongStep) / stepLengthSquared;
    const double stepFraction = std::clamp(unconstrainedStep, 0.0, 1.0);
    if (stepFraction != unconstrainedStep) {
        axisFraction = std::clamp((alignment * stepFraction - alongAxis) / axisLengthSquared, 0.0, 1.0);
    }
    return BuildResult(AddScaled(capsuleStart, axis, axisFraction),
        AddScaled(sweepStart, step, stepFraction), capsuleRadius, sphereRadius);
}

KrakenTentacleCollisionQueryResult QueryKrakenMovingCapsuleSphereIntersection(
    const Vector3& previousStart, const Vector3& previousEnd,
    const Vector3& currentStart, const Vector3& currentEnd, float capsuleRadius,
    const Vector3& previousSphere, const Vector3& currentSphere, float sphereRadius) {
    const Vector3 startMotion = Subtract(currentStart, previousStart);
    const Vector3 endMotion = Subtract(currentEnd, previousEnd);
    const Vector3 sphereMotion = Subtract(currentSphere, previousSphere);
    const double maximumRelativeTravel = std::sqrt((std::max)(
        DistanceSquared(startMotion, sphereMotion), DistanceSquared(endMotion, sphereMotion)));
    if (!std::isfinite(maximumRelativeTravel)) return {};

    double t = 0.0;
    KrakenTentacleCollisionQueryResult closest{};
    for (int i = 0; i < 48; ++i) {
        auto sample = QueryKrakenCapsuleSphereIntersection(
            AddScaled(previousStart, startMotion, t), AddScaled(previousEnd, endMotion, t),
            capsuleRadius, AddScaled(previousSphere, sphereMotion, t), sphereRadius);
        if (!sample.valid) return closest;
        const double gap = static_cast<double>(sample.centerDistance) - sample.radiusSum;
        if (gap <= 0.0001) {
            sample.intersecting = true;
            return sample;
        }
        if (!closest.valid || sample.centerDistance < closest.centerDistance) closest = sample;
        if (t >= 1.0 || maximumRelativeTravel <= 0.0) break;
        // The nearer endpoint cannot close this gap any faster. Advance only
        // through time in which the moving capsule and sphere cannot touch.
        t = (std::min)(1.0, t + gap / maximumRelativeTravel);
    }
    return closest;
}
