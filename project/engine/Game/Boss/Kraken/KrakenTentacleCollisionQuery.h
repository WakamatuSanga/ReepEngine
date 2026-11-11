#pragma once

#include "Matrix4x4.h"

struct KrakenTentacleCollisionQueryResult {
    Vector3 closestPoint{};
    float centerDistance = 0.0f;
    float radiusSum = 0.0f;
    bool valid = false;
    bool intersecting = false;
};

// Let an incoming projectile reach a weak sphere recessed inside a damage capsule.
bool ShouldDeferKrakenBodyHitForWeakPoint(
    const KrakenTentacleCollisionQueryResult& bodyHit,
    const Vector3& projectilePosition,
    const Vector3& projectileVelocity,
    float projectileRadius,
    const Vector3& weakPointCenter,
    float weakPointRadius);

KrakenTentacleCollisionQueryResult QueryKrakenCapsuleSphereIntersection(
    const Vector3& capsuleStart,
    const Vector3& capsuleEnd,
    float capsuleRadius,
    const Vector3& sphereCenter,
    float sphereRadius);

KrakenTentacleCollisionQueryResult QueryKrakenSphereSphereIntersection(
    const Vector3& firstCenter,
    float firstRadius,
    const Vector3& secondCenter,
    float secondRadius);

KrakenTentacleCollisionQueryResult QueryKrakenCapsuleSweptSphereIntersection(
    const Vector3& capsuleStart, const Vector3& capsuleEnd, float capsuleRadius,
    const Vector3& sweepStart, const Vector3& sweepEnd, float sphereRadius);

KrakenTentacleCollisionQueryResult QueryKrakenMovingCapsuleSphereIntersection(
    const Vector3& previousStart, const Vector3& previousEnd,
    const Vector3& currentStart, const Vector3& currentEnd, float capsuleRadius,
    const Vector3& previousSphere, const Vector3& currentSphere, float sphereRadius);
