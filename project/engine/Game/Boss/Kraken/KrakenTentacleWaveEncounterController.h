#pragma once

#include "Engine/math/Matrix4x4.h"

#include <cstddef>
#include <cstdint>
#include <string>

class Camera;
class EnemyWaveManager;
class KrakenTentacleMidbossController;
class RailShooterCameraRig;

enum class KrakenTentacleWaveEncounterState : std::uint8_t {
    WaitingForWave4,
    Starting,
    Active,
    Defeating,
    CompletionPublished,
    WaitingForWave5,
    Completed,
    Error,
};

class KrakenTentacleWaveEncounterController {
public:
    bool Initialize(
        EnemyWaveManager* waveManager,
        KrakenTentacleMidbossController* kraken,
        RailShooterCameraRig* railRig,
        Camera* camera);
    void Reset();
    void Finalize();

    void PreRailUpdate(float gameplayDeltaTime);
    void PostKrakenUpdate();
    void PostWaveUpdate(float gameplayDeltaTime);
    void DrawImGui();

    bool IsControllingKraken() const;
    KrakenTentacleWaveEncounterState GetState() const { return state_; }

private:
    enum class PendingDebugCommand : std::uint8_t {
        None,
        Refresh,
        ForceStart,
        SchedulerOn,
        SchedulerOff,
        AttackNow,
        RailResync,
        ObjectiveResync,
        Reset,
        ClearError,
        SimulateCompletion,
    };

    bool ValidateContexts();
    bool ValidateObjective();
    bool BeginEncounter();
    void UpdateAttackScheduler(float gameplayDeltaTime);
    bool PublishCompletion();
    void CompleteWave5Transition();
    bool SetRailHold(bool hold);
    void DisableDamage();
    void HideKraken();
    void EnterError(const std::string& message);
    void ClearRuntimeDiagnostics();
    bool IsTargetWaveCurrent() const;
    bool IsNextWaveCurrent() const;
    bool ProcessPendingDebugCommand();

    EnemyWaveManager* waveManager_ = nullptr;
    KrakenTentacleMidbossController* kraken_ = nullptr;
    RailShooterCameraRig* railRig_ = nullptr;
    Camera* camera_ = nullptr;

    KrakenTentacleWaveEncounterState state_ =
        KrakenTentacleWaveEncounterState::WaitingForWave4;
    std::string handledWaveId_;
    std::string lastError_ = "なし";
    std::string lastWarning_ = "なし";

    Vector3 spawnWorldPosition_{};
    Vector3 spawnCameraPosition_{};
    Vector3 spawnCameraForward_{ 0.0f, 0.0f, 1.0f };

    std::uint64_t handledWaveRevision_ = 0;
    std::uint64_t targetWaveRevision_ = 0;
    std::uint64_t nextWaveRevision_ = 0;
    std::uint64_t publishedDefeatSequenceId_ = 0;
    std::uint64_t wave4StartCount_ = 0;
    std::uint64_t wave5TransitionCount_ = 0;
    std::uint64_t attackStartCount_ = 0;
    std::uint64_t attackRejectedCount_ = 0;
    std::uint64_t defeatAttackSuppressionCount_ = 0;
    std::uint64_t completionPublishCount_ = 0;
    std::uint64_t duplicateCompletionSuppressionCount_ = 0;
    std::uint64_t manualObjectiveCompletionCount_ = 0;
    std::uint64_t managerMissingCount_ = 0;
    std::uint64_t krakenMissingCount_ = 0;
    std::uint64_t cameraMissingCount_ = 0;
    std::uint64_t railMissingCount_ = 0;
    std::uint64_t invalidObjectiveCount_ = 0;
    std::uint64_t invalidRevisionCount_ = 0;
    std::uint64_t zeroChainCount_ = 0;
    std::uint64_t spawnFailureCount_ = 0;
    std::uint64_t railHoldFailureCount_ = 0;
    std::uint64_t railResumeFailureCount_ = 0;
    std::uint64_t damageSetupFailureCount_ = 0;
    std::uint64_t objectiveCompletionFailureCount_ = 0;
    std::uint64_t waveTransitionTimeoutCount_ = 0;
    std::uint64_t duplicateStartSuppressionCount_ = 0;
    std::uint64_t duplicateSpawnSuppressionCount_ = 0;
    std::uint64_t unexpectedWaveChangeCount_ = 0;

    std::size_t nextAttackChain_ = 0;
    std::size_t detectedChainCount_ = 0;
    float attackTimer_ = 0.0f;
    float currentAttackDelay_ = 1.25f;
    float firstAttackDelay_ = 1.25f;
    float nextAttackInterval_ = 1.50f;
    float retryDelay_ = 0.25f;
    float waitingForWave5Timer_ = 0.0f;
    float wave5TransitionTimeout_ = 3.0f;

    bool initialized_ = false;
    bool encounterStartedForRevision_ = false;
    bool completionPublished_ = false;
    bool schedulerEnabled_ = false;
    bool firstAttackPending_ = true;
    bool railHoldEnabled_ = false;
    bool railStopSucceeded_ = false;
    bool railResumeSucceeded_ = false;
    bool errorRailResumeSucceeded_ = false;
    PendingDebugCommand pendingDebugCommand_ = PendingDebugCommand::None;
};
