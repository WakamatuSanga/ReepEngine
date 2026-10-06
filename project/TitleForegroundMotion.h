#pragma once
#include <algorithm>
#include <cmath>

namespace TitleForegroundMotion {
    constexpr float kDepth = 36.0f; // Cloud must also be in front of the lower tentacle, not only behind its depth.
    constexpr float kCloudFarFace = 38.0f;
    constexpr float kCloudScreenLift = 0.05f;
    // Move the roots/storage by the same camera-up translation as the cloud.
    constexpr float kRootY = 1.58f - kCloudScreenLift * kCloudFarFace / kDepth;
    constexpr float kHeight = 0.84f; // Preserve the enlarged angular size at the new depth.
    constexpr float kStorageDrop = 0.50f; // Fully below the frame while remaining inside the unchanged cloud box.
    constexpr float kCloudScreenMargin = 0.005f - kCloudScreenLift;
    constexpr float kCloudExitSeconds = 0.30f;
    constexpr float kHalfCycle = 7.0f;
    constexpr float kRetreatSeconds = 0.22f;
    constexpr float kTentacleFadeSeconds = 0.15f;
    inline float Smooth(float t) { t = std::clamp(t, 0.0f, 1.0f); return t * t * (3.0f - 2.0f * t); }
    inline float Rise(float time, int side) {
        float t = std::fmod(time, kHalfCycle * 2.0f) - side * kHalfCycle;
        if (t < 0.7f || t >= 5.8f) return 0.0f;
        if (t < 2.7f) return Smooth((t - 0.7f) / 2.0f);
        if (t < 4.0f) return 1.0f;
        return 1.0f - Smooth((t - 4.0f) / 1.8f);
    }
    inline float CenterX(float time, int side) {
        return (side == 0 ? 0.10f : 0.90f) + 0.012f * std::sin(time * 0.30f + side * 2.1f);
    }
}
