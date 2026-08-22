#include "EnemyWaveManager.h"

namespace {
uint64_t AdvanceNonZeroRevision(uint64_t revision) {
    ++revision;
    if (revision == 0) {
        revision = 1;
    }
    return revision;
}
}

bool EnemyWaveManager::ConfigureExternalWaveObjective(
    std::string_view targetWaveId) {
    if (!initialized_ || finalizing_) {
        externalWaveObjective_ = {};
        lastExternalObjectiveError_ =
            "Wave Managerが利用可能な状態ではありません。";
        return false;
    }
    if (targetWaveId.empty()) {
        externalWaveObjective_ = {};
        lastExternalObjectiveError_ = "対象Wave IDが空です。";
        AddLog(lastExternalObjectiveError_);
        return false;
    }
    if (!ValidateWaveResources()) {
        externalWaveObjective_ = {};
        lastExternalObjectiveError_ =
            "Waveリソース検証に失敗したため外部Objectiveを設定できません。";
        AddLog(lastExternalObjectiveError_);
        return false;
    }
    if (!HasLoadedWave(targetWaveId)) {
        externalWaveObjective_ = {};
        lastExternalObjectiveError_ =
            "対象Wave IDが読み込まれていません: " + std::string(targetWaveId);
        AddLog(lastExternalObjectiveError_);
        return false;
    }
    if (!IsWaveReachableFromAutoStart(targetWaveId)) {
        externalWaveObjective_ = {};
        lastExternalObjectiveError_ =
            "対象Wave IDが開始Waveからの進行経路にありません: " +
            std::string(targetWaveId);
        AddLog(lastExternalObjectiveError_);
        return false;
    }

    externalWaveObjective_ = {};
    externalWaveObjective_.targetWaveId = targetWaveId;
    externalWaveObjective_.configured = true;
    lastConfiguredExternalObjectiveWaveId_ = targetWaveId;
    lastExternalObjectiveError_ = "なし";
    AddLog("外部Objectiveを設定しました: " + std::string(targetWaveId));
    return true;
}

bool EnemyWaveManager::SetExternalWaveObjectiveCompleted(bool completed) {
    if (!initialized_ || finalizing_ || !externalWaveObjective_.configured) {
        lastExternalObjectiveError_ =
            "外部Objectiveが設定されていないため完了状態を変更できません。";
        return false;
    }
    if (externalWaveObjective_.completed == completed) {
        if (completed) {
            ++externalWaveObjective_.duplicatePublishSuppressionCount;
        }
        return true;
    }

    externalWaveObjective_.completed = completed;
    if (completed) {
        externalWaveObjective_.completionRevision = AdvanceNonZeroRevision(
            externalWaveObjective_.completionRevision);
        ++externalWaveObjective_.completionPublishCount;
        AddLog("外部Objectiveを完了に設定しました: " +
            externalWaveObjective_.targetWaveId);
    } else {
        AddLog("外部Objectiveを未完了に戻しました: " +
            externalWaveObjective_.targetWaveId);
    }
    lastExternalObjectiveError_ = "なし";
    RefreshCurrentWaveDiagnostics();
    return true;
}

void EnemyWaveManager::ClearExternalWaveObjective() {
    externalWaveObjective_ = {};
    emptyTargetWaveBlockObserved_ = false;
    lastExternalObjectiveError_ = "なし";
    RefreshCurrentWaveDiagnostics();
}

bool EnemyWaveManager::IsExternalWaveObjectiveConfigured() const {
    return externalWaveObjective_.configured;
}

bool EnemyWaveManager::IsExternalWaveObjectiveCompleted() const {
    return externalWaveObjective_.completed;
}

bool EnemyWaveManager::IsCurrentWaveBlockedByExternalObjective() const {
    return currentWaveBaseComplete_ &&
        !DoesExternalObjectiveAllowCompletion(currentWaveId_);
}

const std::string& EnemyWaveManager::GetExternalWaveObjectiveTargetWaveId() const {
    return externalWaveObjective_.targetWaveId;
}

uint64_t EnemyWaveManager::GetExternalWaveObjectiveCompletionRevision() const {
    return externalWaveObjective_.completionRevision;
}

uint64_t EnemyWaveManager::GetExternalWaveObjectiveBlockedCompletionCount() const {
    return externalWaveObjective_.blockedCompletionCount;
}

uint64_t EnemyWaveManager::GetExternalWaveObjectiveCompletionPublishCount() const {
    return externalWaveObjective_.completionPublishCount;
}

uint64_t EnemyWaveManager::GetExternalWaveObjectiveDuplicatePublishSuppressionCount() const {
    return externalWaveObjective_.duplicatePublishSuppressionCount;
}

bool EnemyWaveManager::DoesExternalObjectiveAllowCompletion(
    std::string_view waveId) const {
    if (!externalWaveObjective_.configured ||
        externalWaveObjective_.targetWaveId != waveId) {
        return true;
    }
    return externalWaveObjective_.completed;
}

bool EnemyWaveManager::PassExternalObjectiveCompletionGate(
    std::string_view waveId) {
    if (DoesExternalObjectiveAllowCompletion(waveId)) {
        return true;
    }

    ++externalWaveObjective_.blockedCompletionCount;
    const auto found = waveIndexById_.find(std::string(waveId));
    if (found != waveIndexById_.end() && found->second < waves_.size() &&
        waves_[found->second].enemies.empty()) {
        emptyTargetWaveBlockObserved_ = true;
    }
    return false;
}
