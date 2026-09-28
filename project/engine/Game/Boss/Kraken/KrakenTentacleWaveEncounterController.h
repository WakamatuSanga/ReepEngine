#pragma once

#include "Engine/math/Matrix4x4.h"
#include "KrakenTentacleWaveEncounterConfig.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

class Camera;
class CameraShakeController;
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
    KrakenTentacleWaveEncounterController();
    ~KrakenTentacleWaveEncounterController();
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
    enum class EntrancePhase : std::uint8_t { None, Shake, CameraPullback, Tentacles };
    enum class FollowupPhase : std::uint8_t { None, AwaitRecovery, Interval };
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
    void UpdateEntrance(float gameplayDeltaTime);
    bool BeginCombat();
    void ClearEntrance();
    void DrawEntranceSettingsImGui();
    bool PrepareWave4Reentry(bool& rearmedThisUpdate);
    bool RearmForNewWave4Revision();
    void UpdateAttackScheduler(float gameplayDeltaTime);
    void CancelFollowupAttack();
    bool PublishCompletion();
    void CompleteWave5Transition();
    bool SetRailHold(bool hold);
    void DisableDamage();
    void HideKraken();
    void EnterError(const std::string& message);
    void ClearRuntimeDiagnostics();
    bool IsTargetWaveCurrent() const;
    bool IsNextWaveCurrent() const;
    bool IsNewRevisionWave4() const;
    bool IsRearmRequired() const;
    bool ProcessPendingDebugCommand();
    void BeginFovOverride();
    void EndFovOverride(bool immediate);
    void UpdateFovOverride(float gameplayDeltaTime);
    void RestoreFovImmediately();
    void RefreshFramingDiagnostics();

    EnemyWaveManager* waveManager_ = nullptr;
    KrakenTentacleMidbossController* kraken_ = nullptr;
    RailShooterCameraRig* railRig_ = nullptr;
    Camera* camera_ = nullptr;
    std::unique_ptr<CameraShakeController> entranceShake_;
    EntrancePhase entrancePhase_ = EntrancePhase::None;
    float entranceElapsed_ = 0.0f;
    // Session settings survive controller recreation on Restart; active values are frozen.
    inline static KrakenTentacleWaveEncounterConfig::EntranceSettings entranceSettings_{};
    KrakenTentacleWaveEncounterConfig::EntranceSettings activeEntranceSettings_{};

    KrakenTentacleWaveEncounterState state_ =
        KrakenTentacleWaveEncounterState::WaitingForWave4;
    std::string handledWaveId_;
    std::string observedWaveId_;
    std::string lastReentryOldWaveId_ = "なし";
    std::string lastReentryNewWaveId_ = "なし";
    std::string lastRearmFailureReason_ = "なし";
    std::string lastError_ = "なし";
    std::string lastWarning_ = "なし";

    Vector3 spawnWorldPosition_{};
    Vector3 spawnCameraPosition_{};
    Vector3 spawnCameraForward_{ 0.0f, 0.0f, 1.0f };

    std::uint64_t handledWaveRevision_ = 0;
    std::uint64_t observedWaveRevision_ = 0;
    std::uint64_t targetWaveRevision_ = 0;
    std::uint64_t nextWaveRevision_ = 0;
    std::uint64_t publishedDefeatSequenceId_ = 0;
    std::uint64_t lastReentryOldWaveRevision_ = 0;
    std::uint64_t lastReentryNewWaveRevision_ = 0;
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
    std::uint64_t newRevisionWave4ReentryDetectionCount_ = 0;
    std::uint64_t rearmSuccessCount_ = 0;
    std::uint64_t rearmFailureCount_ = 0;
    std::uint64_t sameRevisionReentrySuppressionCount_ = 0;
    std::uint64_t invalidStateReentryRejectionCount_ = 0;
    std::uint64_t objectiveIncompleteResyncSuccessCount_ = 0;
    std::uint64_t objectiveIncompleteResyncFailureCount_ = 0;
    std::uint64_t rearmStartingSuccessCount_ = 0;
    std::uint64_t rearmStartingFailureCount_ = 0;
    std::uint64_t bossResetCount_ = 0;
    std::uint64_t bossShowCount_ = 0;
    std::uint64_t bossPlacementCount_ = 0;
    std::uint64_t damageEnableCount_ = 0;
    std::uint64_t railRearmResyncCount_ = 0;
    std::uint64_t railHoldStartCount_ = 0;

    std::size_t nextAttackChain_ = 0;
    std::size_t firstAttackChain_ = 0;
    FollowupPhase followupPhase_ = FollowupPhase::None;
    bool nextAttackIsDouble_ = false;
    std::size_t detectedChainCount_ = 0;
    float attackTimer_ = 0.0f;
    float currentAttackDelay_ = 1.25f;
    float firstAttackDelay_ = 1.25f;
    float nextAttackInterval_ = 1.50f;
    float retryDelay_ = 0.25f;
    float waitingForWave5Timer_ = 0.0f;
    float wave5TransitionTimeout_ = 3.0f;
    float baseFovY_ = 0.0f;
    float wave4FovY_ = 0.0f;
    float fovBlendStartY_ = 0.0f;
    float fovBlendElapsed_ = 0.0f;
    float fovBlendDuration_ = 0.45f;
    float fovBlendProgress_ = 0.0f;
    float wave4FovAdditionDegrees_ = 10.0f;
    float wave4VerticalViewOffset_ = 0.0f;
    float wave4BackwardViewOffset_ = 2.0f;
    float framingCameraForwardDot_ = 1.0f;
    Vector3 baseViewTranslationOffset_{};
    Vector3 wave4ViewTranslationOffset_{};
    Vector3 viewBlendStartTranslationOffset_{};
    Vector3 compositionCameraForward_{ 0.0f, 0.0f, 1.0f };
    Vector3 framingScreenMinimum_{};
    Vector3 framingScreenMaximum_{};
    std::array<bool, 4> visibleTentacleChains_{};
    std::size_t visibleWeakPointCount_ = 0;
    std::size_t visibleTentacleCount_ = 0;

    bool initialized_ = false;
    bool encounterStartedForRevision_ = false;
    bool completionPublished_ = false;
    bool schedulerEnabled_ = false;
    bool firstAttackPending_ = true;
    bool railHoldEnabled_ = false;
    bool railStopSucceeded_ = false;
    bool railResumeSucceeded_ = false;
    bool errorRailResumeSucceeded_ = false;
    bool lastRearmAttempted_ = false;
    bool lastRearmSucceeded_ = false;
    bool objectiveIncompleteResyncAttempted_ = false;
    bool lastObjectiveIncompleteResyncSucceeded_ = false;
    bool baseFovCaptured_ = false;
    bool fovOverrideRequested_ = false;
    bool fovOverrideActive_ = false;
    bool fovBlendActive_ = false;
    bool fovRestoreVerified_ = false;
    bool viewTranslationRestoreVerified_ = false;
    bool framingCameraRotationUnchanged_ = true;
    bool framingScreenBoundsValid_ = false;
    bool framingRootSideHidden_ = false;
    bool framingNearPlaneWarning_ = false;
    float framingScreenHeightOccupancy_ = 0.0f;
    KrakenTentacleWaveEncounterState lastReentryStateBefore_ =
        KrakenTentacleWaveEncounterState::WaitingForWave4;
    KrakenTentacleWaveEncounterState lastReentryStateAfter_ =
        KrakenTentacleWaveEncounterState::WaitingForWave4;
    PendingDebugCommand pendingDebugCommand_ = PendingDebugCommand::None;
};
