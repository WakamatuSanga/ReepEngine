#include "EnemyWaveManager.h"

#include <array>
#include <unordered_set>

void EnemyWaveManager::LoadStep9AWaves() {
    constexpr std::array<const char*, 3> kWaveIds = {
        "wave_003", "wave_004", "wave_005"
    };
    for (const char* waveId : kWaveIds) {
        std::string result;
        if (LoadWaveById(waveId, result) && HasLoadedWave(waveId)) {
            AddLog("進行確認用Waveを読み込みました: " + std::string(waveId));
        } else {
            ++failedWaveCount_;
            AddLog("進行確認用Waveの読み込みに失敗しました: " +
                std::string(waveId) + " / " + result);
        }
    }
}

bool EnemyWaveManager::HasLoadedWave(std::string_view waveId) const {
    if (waveId.empty()) {
        return false;
    }
    const auto found = waveIndexById_.find(std::string(waveId));
    return found != waveIndexById_.end() && found->second < waves_.size();
}

bool EnemyWaveManager::ValidateWaveResources() {
    ++waveValidationCount_;
    unresolvedNextWaveCount_ = 0;
    waveChainCycleCount_ = 0;

    for (const EnemyWaveDefinition& wave : waves_) {
        if (!wave.nextWaveId.empty() && !HasLoadedWave(wave.nextWaveId)) {
            ++unresolvedNextWaveCount_;
        }
    }

    std::unordered_set<std::string> fullyChecked;
    for (const EnemyWaveDefinition& startWave : waves_) {
        if (fullyChecked.contains(startWave.waveId)) {
            continue;
        }

        std::unordered_set<std::string> path;
        std::string waveId = startWave.waveId;
        while (!waveId.empty()) {
            if (path.contains(waveId)) {
                ++waveChainCycleCount_;
                break;
            }
            if (fullyChecked.contains(waveId)) {
                break;
            }

            const auto found = waveIndexById_.find(waveId);
            if (found == waveIndexById_.end() || found->second >= waves_.size()) {
                break;
            }
            path.insert(waveId);
            waveId = waves_[found->second].nextWaveId;
        }
        fullyChecked.insert(path.begin(), path.end());
    }

    waveResourcesValid_ = !waves_.empty() &&
        HasLoadedWave(autoStartWaveId_) &&
        waveDuplicateIdCount_ == 0 &&
        unresolvedNextWaveCount_ == 0 &&
        waveChainCycleCount_ == 0;
    lastWaveValidationResult_ = waveResourcesValid_
        ? "Waveリソースは正常です。"
        : "Waveリソースに重複、未解決、または循環があります。";
    return waveResourcesValid_;
}

bool EnemyWaveManager::IsWaveReachableFromAutoStart(
    std::string_view targetWaveId) const {
    if (targetWaveId.empty()) {
        return false;
    }

    std::unordered_set<std::string> visited;
    std::string waveId = autoStartWaveId_;
    while (!waveId.empty() && visited.insert(waveId).second) {
        if (waveId == targetWaveId) {
            return true;
        }
        const auto found = waveIndexById_.find(waveId);
        if (found == waveIndexById_.end() || found->second >= waves_.size()) {
            return false;
        }
        waveId = waves_[found->second].nextWaveId;
    }
    return false;
}
