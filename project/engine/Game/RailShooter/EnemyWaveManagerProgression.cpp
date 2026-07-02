#include "EnemyWaveManager.h"

#include <algorithm>

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