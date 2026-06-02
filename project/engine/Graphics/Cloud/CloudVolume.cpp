#include "CloudVolume.h"

#include <cmath>

CloudVolume::CloudVolume() = default;

void CloudVolume::Update(float deltaTime)
{
    if (deltaTime <= 0.0f) {
        return;
    }

    Vector3 direction = Normalize(parameters_.windDirection);
    float windDistance = parameters_.windSpeed * deltaTime;
    windOffset_.x += direction.x * windDistance;
    windOffset_.y += direction.y * windDistance;
    windOffset_.z += direction.z * windDistance;

    elapsedTime_ += deltaTime;
}

Vector3 CloudVolume::GetMin() const
{
    return {
        parameters_.center.x - parameters_.halfExtents.x,
        parameters_.center.y - parameters_.halfExtents.y,
        parameters_.center.z - parameters_.halfExtents.z
    };
}

Vector3 CloudVolume::GetMax() const
{
    return {
        parameters_.center.x + parameters_.halfExtents.x,
        parameters_.center.y + parameters_.halfExtents.y,
        parameters_.center.z + parameters_.halfExtents.z
    };
}

bool CloudVolume::ContainsPoint(const Vector3& point) const
{
    const Vector3 min = GetMin();
    const Vector3 max = GetMax();

    return
        point.x >= min.x && point.x <= max.x &&
        point.y >= min.y && point.y <= max.y &&
        point.z >= min.z && point.z <= max.z;
}

std::array<Vector3, 8> CloudVolume::GetCorners() const
{
    const Vector3 min = GetMin();
    const Vector3 max = GetMax();

    return {
        Vector3{ min.x, min.y, min.z },
        Vector3{ max.x, min.y, min.z },
        Vector3{ min.x, max.y, min.z },
        Vector3{ max.x, max.y, min.z },
        Vector3{ min.x, min.y, max.z },
        Vector3{ max.x, min.y, max.z },
        Vector3{ min.x, max.y, max.z },
        Vector3{ max.x, max.y, max.z }
    };
}

Vector3 CloudVolume::Normalize(const Vector3& value)
{
    float lengthSquared =
        value.x * value.x +
        value.y * value.y +
        value.z * value.z;

    if (lengthSquared <= 0.000001f) {
        return { 0.0f, 0.0f, 0.0f };
    }

    float inverseLength = 1.0f / std::sqrt(lengthSquared);
    return {
        value.x * inverseLength,
        value.y * inverseLength,
        value.z * inverseLength
    };
}
