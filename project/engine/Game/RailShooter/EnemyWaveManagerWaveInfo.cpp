#include "EnemyWaveManager.h"

#include <algorithm>

namespace {
uint64_t AdvanceNonZeroRevision(uint64_t revision) {
    ++revision;
    if (revision == 0) {
        revision = 1;
    }
    return revision;
}
}

const std::string& EnemyWaveManager::GetCurrentWaveId() const {
    return currentWaveId_;
}

bool EnemyWaveManager::IsCurrentWave(std::string_view waveId) const {
    return !waveId.empty() && currentWaveId_ == waveId;
}

void EnemyWaveManager::InitializeWaveFoundationState() {
    currentWaveId_.clear();
    currentWaveNextWaveId_.clear();
    currentWaveIndex_ = kInvalidWaveIndex;
    currentWaveSpawnCount_ = 0;
    currentWaveScheduledSpawnCount_ = 0;
    currentWaveActiveEnemyCount_ = 0;
    currentWaveRevision_ = 0;
    currentWaveDataValid_ = false;
    currentWaveBaseComplete_ = false;
    currentWaveFinalComplete_ = false;
    waveDuplicateIdCount_ = 0;
    unresolvedNextWaveCount_ = 0;
    waveChainCycleCount_ = 0;
    waveValidationCount_ = 0;
    waveResourcesValid_ = false;
    emptyTargetWaveBlockObserved_ = false;
    externalWaveObjective_ = {};
    lastConfiguredExternalObjectiveWaveId_.clear();
    lastExternalObjectiveError_ = "なし";
    lastWaveValidationResult_ = "未検証";
    initialized_ = false;
    finalizing_ = false;
}

void EnemyWaveManager::FinalizeWaveFoundationState() {
    InitializeWaveFoundationState();
}

void EnemyWaveManager::ResetWaveFoundationForRestart() {
    currentWaveId_.clear();
    currentWaveNextWaveId_.clear();
    currentWaveIndex_ = kInvalidWaveIndex;
    currentWaveSpawnCount_ = 0;
    currentWaveScheduledSpawnCount_ = 0;
    currentWaveActiveEnemyCount_ = 0;
    currentWaveRevision_ = 0;
    currentWaveDataValid_ = false;
    currentWaveBaseComplete_ = false;
    currentWaveFinalComplete_ = false;
    emptyTargetWaveBlockObserved_ = false;
    externalWaveObjective_.completed = false;
    externalWaveObjective_.completionRevision = 0;
    externalWaveObjective_.blockedCompletionCount = 0;
    externalWaveObjective_.completionPublishCount = 0;
    externalWaveObjective_.duplicatePublishSuppressionCount = 0;
    lastExternalObjectiveError_ = "なし";
}

void EnemyWaveManager::ResetFoundationDiagnostics() {
    externalWaveObjective_.completionRevision = 0;
    externalWaveObjective_.blockedCompletionCount = 0;
    externalWaveObjective_.completionPublishCount = 0;
    externalWaveObjective_.duplicatePublishSuppressionCount = 0;
    emptyTargetWaveBlockObserved_ = false;
    lastExternalObjectiveError_ = "なし";
    waveValidationCount_ = 0;
}

void EnemyWaveManager::Reset() {
    StopAllWaves();
    ResetFoundationDiagnostics();
}

void EnemyWaveManager::PublishCurrentWaveStart(size_t waveIndex) {
    if (waveIndex >= waves_.size()) {
        currentWaveId_.clear();
        currentWaveIndex_ = kInvalidWaveIndex;
        currentWaveDataValid_ = false;
        return;
    }

    currentWaveIndex_ = waveIndex;
    currentWaveId_ = waves_[waveIndex].waveId;
    currentWaveNextWaveId_ = waves_[waveIndex].nextWaveId;
    currentWaveRevision_ = AdvanceNonZeroRevision(currentWaveRevision_);
    currentWaveDataValid_ = true;
    RefreshCurrentWaveDiagnostics();
}

void EnemyWaveManager::RefreshCurrentWaveDiagnostics() {
    const ActiveWave* current = nullptr;
    for (const ActiveWave& wave : activeWaves_) {
        if (!wave.stopped && wave.waveIndex == currentWaveIndex_) {
            current = &wave;
        }
    }

    if (!current || currentWaveIndex_ >= waves_.size()) {
        currentWaveId_.clear();
        currentWaveNextWaveId_.clear();
        currentWaveIndex_ = kInvalidWaveIndex;
        currentWaveSpawnCount_ = 0;
        currentWaveScheduledSpawnCount_ = 0;
        currentWaveActiveEnemyCount_ = 0;
        currentWaveDataValid_ = false;
        currentWaveBaseComplete_ = false;
        currentWaveFinalComplete_ = false;
        return;
    }

    const EnemyWaveDefinition& definition = waves_[currentWaveIndex_];
    currentWaveId_ = definition.waveId;
    currentWaveNextWaveId_ = definition.nextWaveId;
    currentWaveSpawnCount_ = definition.enemies.size();
    currentWaveScheduledSpawnCount_ = current->spawnedCount;
    currentWaveActiveEnemyCount_ =
        current->spawnedCount >= current->endedCount
        ? current->spawnedCount - current->endedCount
        : 0;
    currentWaveDataValid_ = true;
    currentWaveBaseComplete_ =
        current->spawnedCount >= definition.enemies.size() &&
        current->endedCount >= definition.enemies.size();
    currentWaveFinalComplete_ = currentWaveBaseComplete_ &&
        DoesExternalObjectiveAllowCompletion(definition.waveId);
}
