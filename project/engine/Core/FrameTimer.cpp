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

    FinishBulletFrame();
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

void FrameTimer::SetBulletMeasurementEnabled(bool enabled) {
    if (bulletMeasurementEnabled_ != enabled) {
        bulletMeasurementEnabled_ = enabled;
        ResetBulletMeasurement();
    }
}

void FrameTimer::ResetBulletMeasurement() {
    ResetBulletWindow();
    lastGameBulletReport_ = {};
}

void FrameTimer::SetBulletMeasurementGameMode(bool gameMode) {
    if (bulletMeasurementGameMode_ != gameMode) {
        bulletMeasurementGameMode_ = gameMode;
        // A mode switch can occur mid-frame. Discard that frame and the
        // unfinished window, but retain the last fully completed Game window.
        ResetBulletWindow();
    }
}

void FrameTimer::ResetBulletWindow() {
    bulletFrameActive_ = false; // Discard a partial frame when toggled in ImGui.
    activeBulletDraw_ = BulletMetric::Count;
    bulletFrame_ = {};
    bulletSum_ = {};
    bulletMaximum_ = {};
    bulletReport_ = {};
    bulletWindowFrames_ = 0;
    bulletWindowSeconds_ = 0.0;
}

void FrameTimer::FinishBulletFrame() {
    if (!bulletMeasurementEnabled_) {
        return;
    }
    if (bulletFrameActive_) {
        bulletFrame_[static_cast<size_t>(BulletMetric::FrameMs)] = GetFrameTimeMs();
        // Only the disjoint outer frame sections belong in this subtraction.
        // Bullet timings are nested inside them and must not be added again.
        double accountedMs = 0.0;
        for (size_t i = static_cast<size_t>(BulletMetric::GameUpdateMs);
             i <= static_cast<size_t>(BulletMetric::MessagesMs); ++i) {
            accountedMs += bulletFrame_[i];
        }
        bulletFrame_[static_cast<size_t>(BulletMetric::UnaccountedMs)] =
            static_cast<double>(GetFrameTimeMs()) - accountedMs;
        bulletReport_.last = bulletFrame_;
        for (size_t i = 0; i < bulletFrame_.size(); ++i) {
            bulletSum_[i] += bulletFrame_[i];
            bulletMaximum_[i] = bulletWindowFrames_ == 0
                ? bulletFrame_[i] : std::max(bulletMaximum_[i], bulletFrame_[i]);
        }
        ++bulletWindowFrames_;
        bulletWindowSeconds_ += rawDeltaTime_;
        if (bulletWindowSeconds_ >= 1.0) {
            for (size_t i = 0; i < bulletSum_.size(); ++i) {
                bulletReport_.average[i] = bulletSum_[i] / static_cast<double>(bulletWindowFrames_);
            }
            bulletReport_.maximum = bulletMaximum_;
            bulletReport_.frames = bulletWindowFrames_;
            bulletReport_.seconds = bulletWindowSeconds_;
            if (bulletMeasurementGameMode_) {
                lastGameBulletReport_ = bulletReport_;
            }
            bulletSum_ = {};
            bulletMaximum_ = {};
            bulletWindowFrames_ = 0;
            bulletWindowSeconds_ = 0.0;
        }
    }
    bulletFrame_ = {};
    bulletFrameActive_ = true;
}

void FrameTimer::AddBulletMetric(BulletMetric metric, double value) {
    if (bulletMeasurementEnabled_ && bulletFrameActive_) {
        bulletFrame_[static_cast<size_t>(metric)] += value;
    }
}

void FrameTimer::SetBulletCounts(size_t player, size_t enemy) {
    if (bulletMeasurementEnabled_ && bulletFrameActive_) {
        bulletFrame_[static_cast<size_t>(BulletMetric::PlayerAlive)] = static_cast<double>(player);
        bulletFrame_[static_cast<size_t>(BulletMetric::EnemyAlive)] = static_cast<double>(enemy);
    }
}

void FrameTimer::CountBulletDraw() {
    if (bulletMeasurementEnabled_ && bulletFrameActive_ && activeBulletDraw_ != BulletMetric::Count) {
        AddBulletMetric(activeBulletDraw_);
    }
}

FrameTimer::BulletScope::BulletScope(BulletMetric metric) : metric_(metric) {
    FrameTimer& timer = FrameTimer::GetInstance();
    if (!timer.bulletMeasurementEnabled_ || !timer.bulletFrameActive_) {
        return;
    }
    timer_ = &timer;
    previousDraw_ = timer.activeBulletDraw_;
    if (metric == BulletMetric::PlayerDrawMs) {
        timer.activeBulletDraw_ = BulletMetric::PlayerDrawCount;
    } else if (metric == BulletMetric::EnemyDrawMs) {
        timer.activeBulletDraw_ = BulletMetric::EnemyDrawCount;
    }
    start_ = Clock::now();
}

FrameTimer::BulletScope::~BulletScope() {
    if (timer_) {
        const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start_).count();
        timer_->AddBulletMetric(metric_, ms);
        timer_->activeBulletDraw_ = previousDraw_;
    }
}
