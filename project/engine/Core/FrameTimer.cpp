#include "FrameTimer.h"

#include <algorithm>
#include <cmath>

FrameTimer& FrameTimer::GetInstance() {
    static FrameTimer instance;
    return instance;
}

void FrameTimer::BeginFrame() {
    const Clock::time_point now = Clock::now();
    if (hasPreviousTime_) {
        const std::chrono::duration<float> elapsed = now - previousTime_;
        rawDeltaTime_ = elapsed.count();
        if (!std::isfinite(rawDeltaTime_) || rawDeltaTime_ < 0.0f) {
            rawDeltaTime_ = 1.0f / 60.0f;
        }
    } else {
        rawDeltaTime_ = 1.0f / 60.0f;
        hasPreviousTime_ = true;
    }

    previousTime_ = now;
    gameplayDeltaTime_ = useDeltaTimeClamp_ ? std::min(rawDeltaTime_, maxDeltaTime_) : rawDeltaTime_;
    gameplayDeltaTime_ = std::max(0.0f, gameplayDeltaTime_);
    ++frameIndex_;
}

float FrameTimer::GetFps() const {
    if (rawDeltaTime_ <= 0.000001f || !std::isfinite(rawDeltaTime_)) {
        return 0.0f;
    }
    return 1.0f / rawDeltaTime_;
}

void FrameTimer::SetMaxDeltaTime(float maxDeltaTime) {
    if (!std::isfinite(maxDeltaTime)) {
        return;
    }
    maxDeltaTime_ = std::clamp(maxDeltaTime, 1.0f / 240.0f, 0.25f);
}
