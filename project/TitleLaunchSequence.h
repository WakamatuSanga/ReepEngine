#pragma once
#include <algorithm>
#include <cmath>

// Title-only timings and path distances. No gameplay input or simulation.
class TitleLaunchSequence {
public:
    enum class Phase { Idle, Align, Fly, Cover };
    static constexpr float kOrbitSeconds = 60.0f;
    static constexpr float kAlignSeconds = 0.45f;
    static constexpr float kFlySeconds = 0.8f;
    static constexpr float kInitialYaw = 0.70f;
    static constexpr float kFlightDistance = 36.0f;
    static constexpr float kTwoPi = 6.28318530718f;

    void Start(float aircraftYaw) {
        if (phase != Phase::Idle) return;
        phase = Phase::Align;
        elapsed = 0.0f;
        startYaw = yaw;
        turnAngle = std::remainder(aircraftYaw - startYaw, kTwoPi);
        alignDuration = kAlignSeconds; // Bounded wait even at the opposite heading.
    }
    void Update(float dt) {
        dt = (std::max)(0.0f, dt);
        if (phase == Phase::Idle) {
            yaw = std::remainder(yaw + kTwoPi * dt / kOrbitSeconds, kTwoPi);
            return;
        }
        elapsed += dt;
        if (phase == Phase::Align) {
            const float t = GetAlignmentProgress();
            // Hermite arc: retain the orbit's initial angular velocity and stop
            // at the nearest rear heading, without wrapping the interpolated yaw.
            yaw = startYaw + turnAngle * t * t * (3.0f - 2.0f * t) +
                (kTwoPi / kOrbitSeconds) * alignDuration * t * (1.0f - t) * (1.0f - t);
            if (elapsed >= alignDuration) {
                yaw = startYaw + turnAngle;
                phase = Phase::Fly;
                elapsed = 0.0f; // First flight frame is the unchanged waiting pose.
            }
        } else {
            if (phase == Phase::Fly && elapsed >= kFlySeconds) phase = Phase::Cover;
        }
    }
    float GetAlignmentProgress() const {
        if (phase == Phase::Idle) return 0.0f;
        return phase == Phase::Align ? std::clamp(elapsed / alignDuration, 0.0f, 1.0f) : 1.0f;
    }
    float GetFlightProgress() const {
        return phase == Phase::Fly || phase == Phase::Cover ? std::clamp(elapsed / kFlySeconds, 0.0f, 1.0f) : 0.0f;
    }
    float GetFlightDistance() const {
        if (phase != Phase::Fly && phase != Phase::Cover) return 0.0f;
        const float t = elapsed / kFlySeconds;
        // Accelerate from rest, then continue at the final speed under cloud cover.
        return kFlightDistance * (t <= 1.0f ? t * t : 1.0f + 2.0f * (t - 1.0f));
    }
    Phase phase = Phase::Idle;
    float yaw = kInitialYaw;
private:
    float elapsed = 0.0f;
    float startYaw = 0.0f;
    float turnAngle = 0.0f;
    float alignDuration = kAlignSeconds;
};
