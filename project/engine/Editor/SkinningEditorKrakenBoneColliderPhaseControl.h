#pragma once

#include "Engine/Game/Boss/Kraken/KrakenTentacleColliderEvaluator.h"

#include <cstddef>
#include <cstdint>
#include <string>

class SkinningEditorKrakenBoneColliderPreviewCollection;
enum class KrakenTentacleAttackPreviewPhase : std::uint8_t;

enum class KrakenColliderPhaseMotionMode : std::uint8_t {
    Manual,
    IdleSway,
    AttackSlamPreview,
};

struct KrakenColliderPhaseMotionSnapshot {
    KrakenColliderPhaseMotionMode motionMode =
        KrakenColliderPhaseMotionMode::Manual;
    KrakenTentacleAttackPreviewPhase phase =
        static_cast<KrakenTentacleAttackPreviewPhase>(0);
    std::size_t selectedChainIndex = 0;
    float motionElapsedTime = 0.0f;
    float phaseElapsedTime = 0.0f;
    float phaseDuration = 0.0f;
    float phaseNormalizedTime = 0.0f;
    float slamDuration = 0.0f;
    bool playing = false;
    bool paused = false;
    bool loop = false;
    bool waitingForLoop = false;
    bool connected = false;
    bool safetyRecovery = false;
    bool valid = false;
};

using KrakenBoneColliderPhaseControlSettings =
    KrakenTentacleColliderPhaseSettings;

struct KrakenBoneColliderPhaseDiagnostics {
    std::uint64_t evaluationPassCount = 0;
    std::uint64_t phaseEvaluationCount = 0;
    std::uint64_t attackEvaluationCount = 0;
    std::uint64_t damageEvaluationCount = 0;
    std::uint64_t weakPointEvaluationCount = 0;
    std::uint64_t activationTransitionCount = 0;
    std::uint64_t deactivationTransitionCount = 0;
    std::uint64_t invalidMotionSnapshotCount = 0;
    std::uint64_t disconnectedSnapshotCount = 0;
    std::uint64_t safetyRecoveryCount = 0;
    std::uint64_t outOfRangeChainCount = 0;
    std::uint64_t invalidPhaseCount = 0;
    std::uint64_t invalidSlamDurationCount = 0;
    std::uint64_t gameplayRegistrationDetectionCount = 0;
    std::uint64_t sameFrameDoubleTransitionCount = 0;
    std::uint32_t currentColliderCount = 0;
    std::uint32_t currentActiveCount = 0;
    std::uint32_t currentInactiveCount = 0;
    std::uint32_t currentPreviewVisibleCount = 0;
    std::uint32_t currentAttackColliderCount = 0;
    std::uint32_t currentAttackActiveCount = 0;
    std::uint32_t currentAttackInactiveCount = 0;
    std::uint32_t currentDamageColliderCount = 0;
    std::uint32_t currentDamageActiveCount = 0;
    std::uint32_t currentDamageInactiveCount = 0;
    std::uint32_t currentWeakPointColliderCount = 0;
    std::uint32_t currentWeakPointActiveCount = 0;
    std::uint32_t currentWeakPointInactiveCount = 0;
    std::uint32_t currentGameplayRegisteredCount = 0;
    std::uint32_t nullChainPreviewCount = 0;
    float slamProgress = 0.0f;
    float lastTransitionTime = 0.0f;
    std::uint64_t lastTransitionFrame = 0;
    KrakenTentacleAttackPreviewPhase lastActivatedPhase =
        static_cast<KrakenTentacleAttackPreviewPhase>(0);
    KrakenTentacleAttackPreviewPhase lastDeactivatedPhase =
        static_cast<KrakenTentacleAttackPreviewPhase>(0);
    bool hasLastActivation = false;
    bool hasLastDeactivation = false;
    bool hasLastTransition = false;
    KrakenColliderPhaseMotionSnapshot snapshot{};
    std::string lastWarning;
};

class SkinningEditorKrakenBoneColliderPhaseControl {
public:
    void Reset();
    void Reset(
        SkinningEditorKrakenBoneColliderPreviewCollection& collection);
    void Finalize(
        SkinningEditorKrakenBoneColliderPreviewCollection& collection);
    void Evaluate(
        SkinningEditorKrakenBoneColliderPreviewCollection& collection,
        const KrakenColliderPhaseMotionSnapshot& snapshot,
        std::uint64_t frameIndex);
    void DeactivateAll(
        SkinningEditorKrakenBoneColliderPreviewCollection& collection,
        KrakenColliderPhaseReason reason,
        std::uint64_t frameIndex);
    void ResetRecommendedSettings();
    void SetSettings(const KrakenBoneColliderPhaseControlSettings& settings);
    void ResetDiagnostics();

    KrakenBoneColliderPhaseControlSettings& GetSettings() {
        return settings_;
    }
    const KrakenBoneColliderPhaseControlSettings& GetSettings() const {
        return settings_;
    }
    const KrakenBoneColliderPhaseDiagnostics& GetDiagnostics() const {
        return diagnostics_;
    }

private:
    void ClearColliderStates(
        SkinningEditorKrakenBoneColliderPreviewCollection& collection);

    KrakenBoneColliderPhaseControlSettings settings_{};
    KrakenBoneColliderPhaseDiagnostics diagnostics_{};
};

const char* GetKrakenColliderPhaseMotionModeJapaneseLabel(
    KrakenColliderPhaseMotionMode mode);
const char* GetKrakenColliderPhaseReasonJapaneseLabel(
    KrakenColliderPhaseReason reason);
