#pragma once
#include <algorithm>
#include <cmath>

namespace TitleForegroundMotion {
    constexpr float kDepth = 34.0f; // Keep the rotated roots inside the existing lower cloud volume.
    constexpr float kCloudFarFace = 38.0f;
    constexpr float kCloudScreenLift = 0.05f;
    constexpr float kStorageDrop = 0.50f; // Fully below the frame while remaining inside the unchanged cloud box.
    constexpr float kCloudScreenMargin = 0.005f - kCloudScreenLift;
    constexpr float kCloudExitSeconds = 0.30f;
    constexpr float kHalfCycle = 7.0f;
    constexpr float kRetreatSeconds = 0.22f;
    constexpr float kTentacleFadeSeconds = 0.15f;
    inline float Smooth(float t) { t = std::clamp(t, 0.0f, 1.0f); return t * t * (3.0f - 2.0f * t); }
    inline float RisePulse(float time, int side) {
        float t = std::fmod(time, kHalfCycle * 2.0f) - side * kHalfCycle;
        if (t < 0.7f || t >= 5.8f) return 0.0f;
        if (t < 2.7f) return Smooth((t - 0.7f) / 2.0f);
        if (t < 4.0f) return 1.0f;
        return 1.0f - Smooth((t - 4.0f) / 1.8f);
    }
    // Keep both side silhouettes present; retain the existing alternating cycle as a small bob.
    inline float Rise(float time, int side) { return 0.96f + 0.04f * RisePulse(time, side); }
    inline float SwayX(float time, int side) {
        return 0.005f * std::sin(time * 0.30f + side * 2.1f);
    }
}
