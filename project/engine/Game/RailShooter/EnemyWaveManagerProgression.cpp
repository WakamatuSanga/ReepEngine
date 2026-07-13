#include "EnemyWaveManager.h"
#include "Engine/Game/UI/WarningUIController.h"

#include <algorithm>
#include <utility>

void EnemyWaveManager::NotifyWaveStartWarning(const EnemyWaveDefinition& wave) {
    lastStartedWaveId_ = wave.waveId.empty() ? "(none)" : wave.waveId;
    lastStartedWaveShowWarning_ = wave.showWarningOnStart;
    lastStartedWaveWaitForWarning_ = wave.waitForWarningBeforeSpawn;
    lastWaveWarningText_ = wave.warningText.empty() ? "WARNING" : wave.warningText;
    lastWaveWarningDuration_ = wave.warningDuration > 0.0f ? wave.warningDuration : 1.5f;

    if (!wave.showWarningOnStart) {
        return;
    }
    if (!warningUIController_) {
        AddLog("Wave start WARNING skipped: WarningUIController missing wave=" + lastStartedWaveId_);
        return;
    }

    warningUIController_->ShowWarning(
        lastWaveWarningText_,
        lastWaveWarningDuration_,
        "EnemyWave:" + lastStartedWaveId_);
    ++waveStartWarningCount_;
    AddLog(
        "Wave start WARNING shown: wave=" + lastStartedWaveId_ +
        " text=" + lastWaveWarningText_ +
        " duration=" + std::to_string(lastWaveWarningDuration_));
}

bool EnemyWaveManager::QueueWaveStartWarning(size_t waveIndex, std::string& resultMessage) {
    if (waveIndex >= waves_.size()) {
        resultMessage = "Invalid pending warning wave index.";
        lastResult_ = resultMessage;
        ++failedWaveCount_;
        AddLog("SpawnWave failed: " + resultMessage);
        return false;
    }

    const EnemyWaveDefinition& wave = waves_[waveIndex];
    if (pendingStartWarningActive_ && pendingStartWaveId_ == wave.waveId) {
        resultMessage = "Wave already waiting for start WARNING: " + wave.waveId;
        lastResult_ = resultMessage;
        AddLog(resultMessage);
        return true;
    }
    if (pendingStartWarningActive_) {
        AddLog("Replaced pending start WARNING wave=" + pendingStartWaveId_ + " with wave=" + wave.waveId);
        ClearPendingStartWarning();
    }

    pendingStartWarningActive_ = true;
    pendingStartWaveIndex_ = waveIndex;
    pendingStartWaveId_ = wave.waveId;
    pendingStartWarningDuration_ = wave.warningDuration > 0.0f ? wave.warningDuration : 1.5f;
    pendingStartPostDelay_ = (std::max)(0.0f, wave.postWarningDelay);
    pendingStartWarningTimer_ = pendingStartWarningDuration_ + pendingStartPostDelay_;
    lastWaveElapsedTime_ = 0.0f;
    lastWaveSpawnedCount_ = 0;
    lastWaveEnemyCount_ = wave.enemies.size();

    NotifyWaveStartWarning(wave);
    resultMessage =
        "Queued wave start WARNING " + wave.waveId +
        " warningDuration=" + std::to_string(pendingStartWarningDuration_) +
        " postDelay=" + std::to_string(pendingStartPostDelay_);
    lastResult_ = resultMessage;
    AddLog(resultMessage);
    return true;
}

bool EnemyWaveManager::StartWaveNow(size_t waveIndex, std::string& resultMessage, bool showStartWarning) {
    if (!enemyManager_) {
        resultMessage = "EnemyManager is missing.";
        lastResult_ = resultMessage;
        ++failedWaveCount_;
        AddLog("SpawnWave failed: " + resultMessage);
        return false;
    }
    if (waveIndex >= waves_.size()) {
        resultMessage = "Loaded wave index is invalid.";
        lastResult_ = resultMessage;
        ++failedWaveCount_;
        AddLog("SpawnWave failed: " + resultMessage);
        return false;
    }

    const EnemyWaveDefinition& wave = waves_[waveIndex];
    lastWaveId_ = wave.waveId.empty() ? "(none)" : wave.waveId;
    lastWaveElapsedTime_ = 0.0f;
    lastWaveSpawnedCount_ = 0;
    lastWaveEnemyCount_ = wave.enemies.size();

    ActiveWave activeWave;
    activeWave.waveIndex = waveIndex;
    activeWave.elapsedTime = -std::max(0.0f, wave.delay);
    activeWave.spawned.assign(wave.enemies.size(), false);
    activeWaves_.push_back(std::move(activeWave));

    ++startedWaveCount_;
    resultMessage = "Started wave " + wave.waveId + " enemies=" + std::to_string(wave.enemies.size());
    lastResult_ = resultMessage;
    AddLog(resultMessage);
    if (showStartWarning) {
        NotifyWaveStartWarning(wave);
    }
    return true;
}

void EnemyWaveManager::UpdatePendingStartWarning(float deltaTime) {
    if (!pendingStartWarningActive_) {
        return;
    }

    pendingStartWarningTimer_ -= (std::max)(0.0f, deltaTime);
    if (pendingStartWarningTimer_ > 0.0f) {
        return;
    }

    const size_t waveIndex = pendingStartWaveIndex_;
    ClearPendingStartWarning();

    std::string result;
    StartWaveNow(waveIndex, result, false);
    lastResult_ = result;
}

void EnemyWaveManager::ClearPendingStartWarning() {
    pendingStartWarningActive_ = false;
    pendingStartWaveId_.clear();
    pendingStartWaveIndex_ = 0;
    pendingStartWarningTimer_ = 0.0f;
    pendingStartWarningDuration_ = 0.0f;
    pendingStartPostDelay_ = 0.0f;
}

void EnemyWaveManager::UpdateWaveProgression(float deltaTime) {
    if (!autoProgressEnabled_) {
        ClearPendingNextWave();
        return;
    }

    for (ActiveWave& activeWave : activeWaves_) {
        if (activeWave.stopped || activeWave.completionQueued || activeWave.waveIndex >= waves_.size()) {
            continue;
        }

        const EnemyWaveDefinition& wave = waves_[activeWave.waveIndex];
        if (activeWave.spawnedCount < wave.enemies.size() || activeWave.endedCount < wave.enemies.size()) {
            continue;
        }

        activeWave.completionQueued = true;
        activeWave.stopped = true;
        lastCompletedWaveId_ = wave.waveId;

        WaveEnemyEndReason reason = WaveEnemyEndReason::Unknown;
        if (activeWave.deadCount > 0 && activeWave.escapedCount == 0) {
            reason = WaveEnemyEndReason::Dead;
            lastCompletedReason_ = "AllDead";
        } else if (activeWave.deadCount == 0 && activeWave.escapedCount > 0) {
            reason = WaveEnemyEndReason::Escaped;
            lastCompletedReason_ = "AllEscaped";
        } else if (activeWave.deadCount > 0 && activeWave.escapedCount > 0) {
            reason = WaveEnemyEndReason::Dead;
            lastCompletedReason_ = "Mixed";
        } else {
            lastCompletedReason_ = "Unknown";
        }

        AddLog("Wave completed: " + wave.waveId + " reason=" + lastCompletedReason_);
        if (!wave.nextWaveId.empty() && !pendingNextWaveActive_ && !IsWaveCurrentlyActive(wave.nextWaveId)) {
            pendingNextWaveId_ = wave.nextWaveId;
            pendingNextWaveTimer_ = reason == WaveEnemyEndReason::Escaped ?
                (std::max)(0.0f, wave.clearDelayAfterAllEscaped) :
                (std::max)(0.0f, wave.clearDelayAfterAllDead);
            pendingNextWaveActive_ = true;
            AddLog("Queued next wave: " + pendingNextWaveId_ + " delay=" + std::to_string(pendingNextWaveTimer_));
        }
    }

    if (!pendingNextWaveActive_) {
        return;
    }

    pendingNextWaveTimer_ -= (std::max)(0.0f, deltaTime);
    if (pendingNextWaveTimer_ > 0.0f) {
        return;
    }

    const std::string nextWaveId = pendingNextWaveId_;
    ClearPendingNextWave();
    if (nextWaveId.empty() || IsWaveCurrentlyActive(nextWaveId)) {
        return;
    }

    std::string result;
    PlayWave(nextWaveId, result);
    lastResult_ = result;
}

void EnemyWaveManager::MarkWaveEnemyEnded(WaveEnemyRuntime& runtime, WaveEnemyEndReason reason) {
    if (runtime.ended) {
        return;
    }
    runtime.ended = true;
    runtime.endReason = reason;

    if (ActiveWave* activeWave = FindActiveWave(runtime.waveIndex)) {
        ++activeWave->endedCount;
        if (reason == WaveEnemyEndReason::Dead) {
            ++activeWave->deadCount;
        } else if (reason == WaveEnemyEndReason::Escaped) {
            ++activeWave->escapedCount;
        }
    }
}

EnemyWaveManager::ActiveWave* EnemyWaveManager::FindActiveWave(size_t waveIndex) {
    for (ActiveWave& activeWave : activeWaves_) {
        if (!activeWave.stopped && activeWave.waveIndex == waveIndex) {
            return &activeWave;
        }
    }
    return nullptr;
}

bool EnemyWaveManager::IsWaveCurrentlyActive(const std::string& waveId) const {
    if (pendingStartWarningActive_ && pendingStartWaveId_ == waveId) {
        return true;
    }
    const auto found = waveIndexById_.find(waveId);
    if (found == waveIndexById_.end()) {
        return false;
    }
    for (const ActiveWave& activeWave : activeWaves_) {
        if (!activeWave.stopped && activeWave.waveIndex == found->second) {
            return true;
        }
    }
    return false;
}

void EnemyWaveManager::ClearPendingNextWave() {
    pendingNextWaveActive_ = false;
    pendingNextWaveId_.clear();
    pendingNextWaveTimer_ = 0.0f;
}

const char* EnemyWaveManager::ToWaveEndReasonText(WaveEnemyEndReason reason) {
    switch (reason) {
    case WaveEnemyEndReason::Dead:
        return "Dead";
    case WaveEnemyEndReason::Escaped:
        return "Escaped";
    default:
        return "Unknown";
    }
}
