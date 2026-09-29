#pragma once

#include <chrono>
#include <array>
#include <cstddef>
#include <cstdint>

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

    enum class BulletMetric : size_t {
        FrameMs, PlayerAlive, EnemyAlive, PlayerSpawnCount, EnemySpawnCount,
        PlayerSpawnMs, EnemySpawnMs, PlayerUpdateMs, EnemyUpdateMs,
        EnemyHitMs, PlayerHitMs, KrakenHitMs, CancelHitMs,
        PlayerDrawMs, EnemyDrawMs, PlayerDrawCount, EnemyDrawCount,
        GameUpdateMs, RenderCommandsMs, SubmitMs, PresentMs, GpuWaitMs,
        FixedFpsMs, CommandResetMs, MessagesMs, UnaccountedMs, Count
    };
    using BulletValues = std::array<double, static_cast<size_t>(BulletMetric::Count)>;
    struct BulletReport {
        BulletValues last{};
        BulletValues average{};
        BulletValues maximum{};
        uint64_t frames = 0;
        double seconds = 0.0;
    };
    // Single render thread only. OFF skips clock reads and metric accumulation.
    class BulletScope {
    public:
        explicit BulletScope(BulletMetric metric);
        ~BulletScope();
        BulletScope(const BulletScope&) = delete;
        BulletScope& operator=(const BulletScope&) = delete;
    private:
        FrameTimer* timer_ = nullptr;
        BulletMetric metric_;
        BulletMetric previousDraw_ = BulletMetric::Count;
        std::chrono::steady_clock::time_point start_{};
    };
    bool IsBulletMeasurementEnabled() const { return bulletMeasurementEnabled_; }
    void SetBulletMeasurementEnabled(bool enabled);
    void ResetBulletMeasurement();
    void SetBulletMeasurementGameMode(bool gameMode);
    void AddBulletMetric(BulletMetric metric, double value = 1.0);
    void SetBulletCounts(size_t player, size_t enemy);
    void CountBulletDraw();
    const BulletReport& GetBulletReport() const { return bulletReport_; }
    const BulletReport& GetLastGameBulletReport() const { return lastGameBulletReport_; }

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
    bool bulletMeasurementEnabled_ = false;
    bool bulletMeasurementGameMode_ = false;
    bool bulletFrameActive_ = false;
    BulletMetric activeBulletDraw_ = BulletMetric::Count;
    BulletValues bulletFrame_{};
    BulletValues bulletSum_{};
    BulletValues bulletMaximum_{};
    BulletReport bulletReport_{};
    BulletReport lastGameBulletReport_{};
    uint64_t bulletWindowFrames_ = 0;
    double bulletWindowSeconds_ = 0.0;
    void FinishBulletFrame();
    void ResetBulletWindow();
};
