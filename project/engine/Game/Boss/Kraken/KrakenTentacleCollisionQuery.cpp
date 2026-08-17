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
