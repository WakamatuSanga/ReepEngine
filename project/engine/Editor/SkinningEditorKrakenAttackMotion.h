#pragma once

#include "Engine/Game/Boss/Kraken/KrakenTentaclePoseEvaluator.h"

#include <cstddef>
#include <cstdint>
#include <string>

class SkinningEditorKrakenAttackMotion {
public:
    SkinningEditorKrakenAttackMotion();

    void Reset();
    void Stop();
    bool PlayOnce(std::size_t detectedChainCount);
    bool PlayLoop(std::size_t detectedChainCount);
    bool Restart(std::size_t detectedChainCount);
    void SetLoopEnabled(bool enabled);
    void Pause();
    void Resume();
    void Update(float unscaledDeltaTime, std::size_t detectedChainCount);

    bool SelectChain(
        std::size_t chainIndex,
        std::size_t detectedChainCount);
    bool RevalidateSelectedChain(std::size_t detectedChainCount);
    bool JumpToPhase(
        KrakenTentacleAttackPreviewPhase phase,
        std::size_t detectedChainCount);
    bool SeekPhaseNormalizedTime(
        KrakenTentacleAttackPreviewPhase phase,
        float normalizedTime,
        std::size_t detectedChainCount);

    void SetSettings(const KrakenTentacleAttackPreviewSettings& settings);
    void ResetRecommendedSettings();
    const KrakenTentacleAttackPreviewSettings& GetSettings() const {
        return settings_;
    }

    KrakenTentacleAttackPoseTotals EvaluatePoseTotals() const;
    float GetMotionDuration() const;
    float GetPhaseDuration(
        KrakenTentacleAttackPreviewPhase phase) const;

    KrakenTentacleAttackPreviewPhase GetPhase() const { return phase_; }
    float GetElapsedTime() const { return elapsedTime_; }
    float GetPhaseElapsedTime() const { return phaseElapsedTime_; }
    float GetLoopWaitElapsedTime() const { return loopWaitElapsedTime_; }
    std::size_t GetSelectedChainIndex() const { return selectedChainIndex_; }
    std::uint32_t GetImpactStartCount() const { return impactStartCount_; }
    std::uint32_t GetCompletedCycleCount() const {
        return completedCycleCount_;
    }
    float GetLastImpactStartTime() const { return lastImpactStartTime_; }
    bool IsPlaying() const { return playing_; }
    bool IsPaused() const { return paused_; }
    bool IsLoopEnabled() const { return loop_; }
    bool IsWaitingForLoop() const { return waitingForLoop_; }
    bool HasAttackPose() const {
        return phase_ != KrakenTentacleAttackPreviewPhase::Completed;
    }
    const std::string& GetLastError() const { return lastError_; }
    void ClearLastError() { lastError_.clear(); }

private:
    bool Start(std::size_t detectedChainCount, bool loop);
    void BeginCycle();
    void AdvancePhase();
    void EnterImpactHold();
    void CompleteCycle();
    void SetStoppedState();
    void SetError(const std::string& errorMessage);
    float GetPhaseStartTime(
        KrakenTentacleAttackPreviewPhase phase) const;

    KrakenTentacleAttackPreviewSettings settings_{};
    KrakenTentacleAttackPreviewPhase phase_ =
        KrakenTentacleAttackPreviewPhase::Windup;
    float elapsedTime_ = 0.0f;
    float phaseElapsedTime_ = 0.0f;
    float loopWaitElapsedTime_ = 0.0f;
    float lastImpactStartTime_ = 0.0f;
    std::size_t selectedChainIndex_ = 0;
    std::uint32_t impactStartCount_ = 0;
    std::uint32_t completedCycleCount_ = 0;
    bool playing_ = false;
    bool paused_ = false;
    bool loop_ = false;
    bool waitingForLoop_ = false;
    std::string lastError_;
};

const char* GetKrakenTentacleAttackPhaseJapaneseLabel(
    KrakenTentacleAttackPreviewPhase phase);

const char* GetKrakenTentacleAttackAxisLabel(
    KrakenTentacleAttackLocalAxis axis);
