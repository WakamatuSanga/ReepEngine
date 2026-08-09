#pragma once

#include "Matrix4x4.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class KrakenTentacleAttackPreviewPhase : std::uint8_t {
    Windup,
    WindupHold,
    Slam,
    ImpactHold,
    Recovery,
    Completed,
};

enum class KrakenTentacleAttackLocalAxis : std::uint8_t {
    X,
    Y,
    Z,
};

struct KrakenTentacleAttackPreviewSettings {
    float windupDuration = 0.60f;
    float windupHoldDuration = 0.12f;
    float slamDuration = 0.20f;
    float impactHoldDuration = 0.15f;
    float recoveryDuration = 0.55f;
    float loopInterval = 0.25f;

    float windupPrimaryTotalDegrees = -35.0f;
    float windupSecondaryTotalDegrees = 12.0f;
    float slamPrimaryTotalDegrees = 65.0f;
    float slamSecondaryTotalDegrees = -8.0f;
    float tipBias = 1.60f;
    std::uint32_t fixedLeadingBoneCount = 1;

    // The existing Idle Sway convention bends on local X and curls on local Z.
    KrakenTentacleAttackLocalAxis primaryAxis =
        KrakenTentacleAttackLocalAxis::X;
    KrakenTentacleAttackLocalAxis secondaryAxis =
        KrakenTentacleAttackLocalAxis::Z;
    float primarySign = 1.0f;
    float secondarySign = 1.0f;
};

struct KrakenTentacleAttackPoseTotals {
    float primaryDegrees = 0.0f;
    float secondaryDegrees = 0.0f;
    bool finite = true;
};

struct KrakenTentacleAttackJointPose {
    int jointIndex = -1;
    std::size_t chainBoneIndex = 0;
    float normalizedWeight = 0.0f;
    float primaryDegrees = 0.0f;
    float secondaryDegrees = 0.0f;
    Quaternion attackOffset{};
    Quaternion absoluteLocalRotation{};
    Vector3 absoluteLocalEulerRadians{};
    bool fixed = false;
    bool finite = false;
};

struct KrakenTentacleAttackPoseResult {
    std::vector<KrakenTentacleAttackJointPose> joints;
    std::size_t fixedBoneCount = 0;
    std::size_t movableBoneCount = 0;
    float normalizedWeightSum = 0.0f;
    bool valid = false;
    std::string errorMessage;
};

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

bool BuildKrakenTentacleAttackPose(
    const KrakenTentacleAttackPreviewSettings& settings,
    const KrakenTentacleAttackPoseTotals& totals,
    const std::vector<int>& chainJoints,
    const std::vector<Vector3>& bindLocalEulerRadians,
    int skeletonRootJointIndex,
    KrakenTentacleAttackPoseResult& outResult);

const char* GetKrakenTentacleAttackPhaseJapaneseLabel(
    KrakenTentacleAttackPreviewPhase phase);

const char* GetKrakenTentacleAttackAxisLabel(
    KrakenTentacleAttackLocalAxis axis);
