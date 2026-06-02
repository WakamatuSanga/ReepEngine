#pragma once

#include "Matrix4x4.h"

#include <array>
#include <cstdint>

class CloudVolume {
public:
    struct Parameters {
        Vector3 center = { 0.0f, 4.0f, 6.0f };
        float density = 0.45f;

        Vector3 halfExtents = { 10.0f, 4.0f, 10.0f };
        float absorption = 1.2f;

        Vector3 windDirection = { 1.0f, 0.0f, 0.2f };
        float windSpeed = 0.35f;

        Vector3 sunDirection = { 0.0f, -1.0f, 0.0f };
        float lightAbsorption = 1.0f;

        Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

        float noiseScale = 0.18f;
        float detailNoiseScale = 0.55f;
        float detailWeight = 0.35f;
        float edgeFade = 0.2f;

        float ambientLighting = 0.35f;
        float sunIntensity = 1.0f;
        uint32_t viewStepCount = 64;
        uint32_t lightStepCount = 6;
    };

public:
    CloudVolume();

    void Update(float deltaTime);

    Parameters& GetParameters() { return parameters_; }
    const Parameters& GetParameters() const { return parameters_; }

    float GetElapsedTime() const { return elapsedTime_; }
    const Vector3& GetWindOffset() const { return windOffset_; }
    Vector3 GetMin() const;
    Vector3 GetMax() const;
    bool ContainsPoint(const Vector3& point) const;
    std::array<Vector3, 8> GetCorners() const;

private:
    static Vector3 Normalize(const Vector3& value);

private:
    Parameters parameters_{};
    float elapsedTime_ = 0.0f;
    Vector3 windOffset_{ 0.0f, 0.0f, 0.0f };
};
