#pragma once

#include "Matrix4x4.h"

struct KrakenTentacleCollisionQueryResult {
    Vector3 closestPoint{};
    float centerDistance = 0.0f;
    float radiusSum = 0.0f;
    bool valid = false;
    bool intersecting = false;
};

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
