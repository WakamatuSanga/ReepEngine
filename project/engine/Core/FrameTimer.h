#pragma once

#include <chrono>

class FrameTimer {
public:
    static FrameTimer& GetInstance();

    void BeginFrame();

    float GetRawDeltaTime() const { return rawDeltaTime_; }
    float GetGameplayDeltaTime() const { return gameplayDeltaTime_; }
    float GetClampedDeltaTime() const { return gameplayDeltaTime_; }
    float GetFrameTimeMs() const { return rawDeltaTime_ * 1000.0f; }
    float GetFps() const;
    bool IsDeltaTimeClampEnabled() const { return useDeltaTimeClamp_; }
    void SetDeltaTimeClampEnabled(bool enabled) { useDeltaTimeClamp_ = enabled; }
    float GetMaxDeltaTime() const { return maxDeltaTime_; }
    void SetMaxDeltaTime(float maxDeltaTime);
    uint64_t GetFrameIndex() const { return frameIndex_; }

private:
    FrameTimer() = default;

    using Clock = std::chrono::steady_clock;

    Clock::time_point previousTime_{};
    float rawDeltaTime_ = 1.0f / 60.0f;
    float gameplayDeltaTime_ = 1.0f / 60.0f;
    float maxDeltaTime_ = 1.0f / 15.0f;
    uint64_t frameIndex_ = 0;
    bool hasPreviousTime_ = false;
    bool useDeltaTimeClamp_ = true;
};
