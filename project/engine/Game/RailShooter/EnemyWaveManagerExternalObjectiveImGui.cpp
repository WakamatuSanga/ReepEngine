#include "EnemyWaveManager.h"

#include <charconv>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
int ParseUserWaveNumber(const std::string& waveId) {
    constexpr std::string_view kPrefix = "wave_";
    if (!std::string_view(waveId).starts_with(kPrefix)) {
        return 0;
    }
    int number = 0;
    const char* begin = waveId.data() + kPrefix.size();
    const char* end = waveId.data() + waveId.size();
    const auto result = std::from_chars(begin, end, number);
    return result.ec == std::errc{} && result.ptr == end ? number : 0;
}
}

void EnemyWaveManager::DrawProgressionObjectiveImGui() {
#ifdef USE_IMGUI
    ImGui::SeparatorText("ウェーブ進行・外部完了条件診断");

    ImGui::Text("現在ウェーブID: %s",
        currentWaveId_.empty() ? "なし" : currentWaveId_.c_str());
    ImGui::Text("ユーザー向けウェーブ番号: %d",
        ParseUserWaveNumber(currentWaveId_));
    if (currentWaveIndex_ == kInvalidWaveIndex) {
        ImGui::Text("内部インデックス: 無効");
    } else {
        ImGui::Text("内部インデックス: %zu", currentWaveIndex_);
    }
    ImGui::Text("ウェーブ改訂番号: %llu",
        static_cast<unsigned long long>(currentWaveRevision_));
    ImGui::Text("ウェーブ開始回数: %zu", startedWaveCount_);
    ImGui::Text("ウェーブデータ有効: %s", currentWaveDataValid_ ? "はい" : "いいえ");
    ImGui::Text("次ウェーブID: %s",
        currentWaveNextWaveId_.empty() ? "なし" : currentWaveNextWaveId_.c_str());
    ImGui::Text("出現総数: %zu", currentWaveSpawnCount_);
    ImGui::Text("出現予定済み数: %zu", currentWaveScheduledSpawnCount_);
    ImGui::Text("生存中の敵数: %zu", currentWaveActiveEnemyCount_);
    ImGui::Text("基本完了条件: %s", currentWaveBaseComplete_ ? "成立" : "未成立");
    ImGui::Text("最終完了条件: %s", currentWaveFinalComplete_ ? "成立" : "未成立");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "外部完了条件:\n通常敵が0体でも、中ボスの撃破完了までは\n"
            "ウェーブを終了させないための条件です。");
    }
    ImGui::Text("ウェーブ改訂番号の説明");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "同じウェーブIDでも、新しく開始されたウェーブを識別する番号です。\n"
            "毎フレームは変化しません。");
    }

    ImGui::SeparatorText("ウェーブリソース");
    ImGui::Text("wave_001存在: %s", HasLoadedWave("wave_001") ? "はい" : "いいえ");
    ImGui::Text("wave_002存在: %s", HasLoadedWave("wave_002") ? "はい" : "いいえ");
    ImGui::Text("wave_003存在: %s", HasLoadedWave("wave_003") ? "はい" : "いいえ");
    ImGui::Text("wave_004存在: %s", HasLoadedWave("wave_004") ? "はい" : "いいえ");
    ImGui::Text("wave_005存在: %s", HasLoadedWave("wave_005") ? "はい" : "いいえ");
    ImGui::Text("読込ウェーブ数: %zu", waves_.size());
    ImGui::Text("ID重複数: %zu", waveDuplicateIdCount_);
    ImGui::Text("次ウェーブID未解決数: %zu", unresolvedNextWaveCount_);
    ImGui::Text("チェーン循環数: %zu", waveChainCycleCount_);
    ImGui::TextWrapped("検証結果: %s", lastWaveValidationResult_.c_str());

    ImGui::SeparatorText("外部完了条件");
    ImGui::Text("設定済み: %s", externalWaveObjective_.configured ? "はい" : "いいえ");
    ImGui::Text("対象ウェーブID: %s", externalWaveObjective_.targetWaveId.empty()
        ? "なし" : externalWaveObjective_.targetWaveId.c_str());
    ImGui::Text("現在ウェーブが対象: %s",
        externalWaveObjective_.configured &&
        externalWaveObjective_.targetWaveId == currentWaveId_ ? "はい" : "いいえ");
    ImGui::Text("外部完了条件完了: %s", externalWaveObjective_.completed ? "はい" : "いいえ");
    ImGui::Text("ウェーブ完了保留中: %s",
        IsCurrentWaveBlockedByExternalObjective() ? "はい" : "いいえ");
    ImGui::Text("保留回数: %llu", static_cast<unsigned long long>(
        externalWaveObjective_.blockedCompletionCount));
    ImGui::Text("完了設定回数: %llu", static_cast<unsigned long long>(
        externalWaveObjective_.completionPublishCount));
    ImGui::Text("二重完了設定抑制数: %llu", static_cast<unsigned long long>(
        externalWaveObjective_.duplicatePublishSuppressionCount));
    const auto wave4 = waveIndexById_.find("wave_004");
    const bool wave4Empty = wave4 != waveIndexById_.end() &&
        wave4->second < waves_.size() && waves_[wave4->second].enemies.empty();
    ImGui::Text("ウェーブ4の通常出現が空: %s", wave4Empty ? "はい" : "いいえ");
    ImGui::Text("ウェーブ4自動完了防止成功: %s",
        emptyTargetWaveBlockObserved_ ? "はい" : "未確認");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "ウェーブ4は通常出現が0件ですが、外部完了条件が未完了の間は"
            "ウェーブ5へ進みません。");
    }
    ImGui::Text("クラーケン未接続: はい");
    ImGui::Text("レール未接続: はい");
    ImGui::TextWrapped("最後のエラー: %s", lastExternalObjectiveError_.c_str());

    ImGui::SeparatorText("操作");
    if (ImGui::Button("未完了にする##ObjectiveIncomplete")) {
        SetExternalWaveObjectiveCompleted(false);
    }
    ImGui::SameLine();
    if (ImGui::Button("完了にする##ObjectiveComplete")) {
        SetExternalWaveObjectiveCompleted(true);
    }
    if (ImGui::Button("外部完了条件を再設定##ReconfigureObjective")) {
        const std::string targetWaveId = externalWaveObjective_.targetWaveId.empty()
            ? lastConfiguredExternalObjectiveWaveId_
            : externalWaveObjective_.targetWaveId;
        if (targetWaveId.empty()) {
            lastExternalObjectiveError_ = "再設定する対象ウェーブIDがありません。";
        } else if (ConfigureExternalWaveObjective(targetWaveId)) {
            SetExternalWaveObjectiveCompleted(false);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("外部完了条件を解除##ClearObjective")) {
        ClearExternalWaveObjective();
    }
    if (ImGui::Button("診断をリセット##ResetObjectiveDiagnostics")) {
        ResetFoundationDiagnostics();
    }
    ImGui::SameLine();
    if (ImGui::Button("ウェーブリソースを再検証##ValidateWaveResources")) {
        ValidateWaveResources();
    }
    if (ImGui::Button("現在ウェーブ情報を再取得##RefreshWaveInfo")) {
        RefreshCurrentWaveDiagnostics();
    }
#endif
}
