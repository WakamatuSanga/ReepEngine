#pragma once
#include "Engine/math/Matrix4x4.h"
#include <algorithm>
#include <cmath>

namespace TitleWaitingAircraft {
    constexpr float kScreenY = 0.68f;
    constexpr float kDepth = 8.0f;
    constexpr Vector3 kHullMin{ -0.790591f, -0.232374f, -0.947587f };
    constexpr Vector3 kHullMax{ 0.790639f, 0.233814f, 0.946203f };
    constexpr float kOrbitRadius = 12.5f / 1.4f; // Enlarge the projected aircraft, not its model.
    constexpr float kLaunchRadius = 8.0f; // Preserve the existing rear launch composition.
    constexpr float kOrbitPitch = 0.261799388f; // 15-degree elevation above the aircraft.
    constexpr float kOrbitScreenY = 0.70f;

    inline Transform MakeOrbitCamera(const Vector3& center, float yaw,
        float aspect, float fov, float aircraftScale, float alignmentProgress = 0.0f) {
        // A bounding sphere keeps every heading inside the horizontal frustum,
        // including narrow windows. The aircraft transform itself stays fixed.
        const float hullRadius = aircraftScale * std::sqrt(
            kHullMax.x * kHullMax.x + kHullMax.y * kHullMax.y + kHullMin.z * kHullMin.z);
        // Nozzle offset + normal outer-particle travel/spread, including billboard extent.
        // Keep this title framing envelope clear during side views as well as rear views.
        const float exhaustRadius = 3.45f;
        const float halfAngle = std::atan((std::max)(aspect, 0.1f) * std::tan(fov * 0.5f) * 0.90f);
        const float idleRadius = (std::max)(kOrbitRadius,
            (std::max)(hullRadius, exhaustRadius) / std::sin(halfAngle));
        const float launchRadius = (std::max)(kLaunchRadius, hullRadius + hullRadius /
            ((std::max)(aspect, 0.1f) * std::tan(fov * 0.5f) * 0.85f));
        const float t = std::clamp(alignmentProgress, 0.0f, 1.0f);
        const float remaining = 1.0f - t * t * (3.0f - 2.0f * t);
        const float radius = launchRadius + (idleRadius - launchRadius) * remaining;
        const float elevation = kOrbitPitch * remaining;
        // Aim above the center without changing the aircraft's world pose. The
        // camera-to-aircraft elevation stays 15 degrees; its image center is at 70%.
        const float framingAngle = std::atan((2.0f * kOrbitScreenY - 1.0f) * std::tan(fov * 0.5f));
        const float pitch = elevation - framingAngle * remaining;
        return { { 1, 1, 1 }, { pitch, yaw, 0 },
            { center.x - std::sin(yaw) * radius,
              center.y + radius * std::tan(elevation),
              center.z - std::cos(yaw) * radius } };
    }

    inline float InitialDepth(float aspect, float fov) {
        return (std::max)(kDepth, 1.422f + 1.186f /
            ((std::max)(aspect, 0.1f) * std::tan(fov * 0.5f) * 0.65f));
    }

    // Evaluate only on title entry. Subsequent camera rotation never changes this pose.
    inline Transform MakeInitialPose(const Vector3& cameraPosition, float yaw,
        float aspect, float fov, float scale) {
        const float depth = InitialDepth(aspect, fov);
        const float startUp = (1.0f - 2.0f * kScreenY) * depth * std::tan(fov * 0.5f);
        return { { scale, scale, scale }, { 0.0f, yaw, 0.0f },
            { cameraPosition.x + std::sin(yaw) * depth, cameraPosition.y + startUp,
              cameraPosition.z + std::cos(yaw) * depth } };
    }

}
