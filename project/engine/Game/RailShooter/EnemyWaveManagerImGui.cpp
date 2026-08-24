#include "EnemyWaveManager.h"
#include <algorithm>
#include <cctype>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
    std::string TrimCopyForWaveImGui(const std::string& value) {
        const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
        const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
        if (begin >= end) {
            return {};
        }
        return std::string(begin, end);
    }

    const char* WaveCompletionReasonLabel(const std::string& reason) {
        if (reason == "AllDead") {
            return "全滅";
        }
        if (reason == "AllEscaped") {
            return "全離脱";
        }
        if (reason == "Mixed") {
            return "混在";
        }
        if (reason == "Unknown") {
            return "不明";
        }
        if (reason == "(none)") {
            return "なし";
        }
        return reason.c_str();
    }

    const char* WaveTextOrNoneLabel(const std::string& value) {
        return value == "(none)" ? "なし" : value.c_str();
    }
}
void EnemyWaveManager::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(430.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("敵ウェーブデバッグ###Enemy Wave Debug")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("ウェーブ出現アクションを有効化##EnableSpawnWaveAction", &enabled_);
    ImGui::Checkbox("不足ウェーブを自動読込##AutoLoadMissingWave", &autoLoadMissingWave_);
    ImGui::Checkbox("ゲームモード開始時にウェーブ1を再生##AutoStartWaveOnGameMode", &autoStartWaveOnGameMode_);
    ImGui::Checkbox("自動進行を有効化##AutoProgressEnabled", &autoProgressEnabled_);
    ImGui::TextWrapped("自動開始ウェーブID: %s", autoStartWaveId_.c_str());
    DrawProgressionObjectiveImGui();
    ImGui::DragFloat("出現幅##SpawnWidth", &spawnWidth_, 0.1f, 1.0f, 100.0f);
    ImGui::DragFloat("出現高さ##SpawnHeight", &spawnHeight_, 0.1f, 1.0f, 100.0f);
    ImGui::DragFloat("接近速度##ApproachSpeed", &approachSpeed_, 0.1f, 0.0f, 60.0f, "%.1f");
    ImGui::DragFloat("接近停止距離##ApproachStopDistance", &approachStopDistance_, 0.1f, 0.0f, 40.0f, "%.1f");
    const float enemyFinalApproachSpeed = (std::max)(0.0f, approachSpeed_);
    ImGui::Text("加速による敵接近速度補正: 無効");
    ImGui::Text("敵の基本接近速度: %.2f", enemyFinalApproachSpeed);
    ImGui::Text("敵自身の最終接近速度: %.2f", enemyFinalApproachSpeed);
    ImGui::Text("加速による加算値: 0.0");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("加速中の相対接近速度はカメラのレール速度によって増加します。\n敵自身の接近速度には加速を加算していません。");
    }
    ImGui::SeparatorText("ウェーブ2敵診断");
    ImGui::Checkbox("画面固定を有効化##ScreenAnchorEnabled", &screenAnchorEnabled_);
    ImGui::DragFloat("落下時間##DropDuration", &screenAnchorDropDuration_, 0.02f, 0.05f, 3.0f, "%.2f");
    ImGui::DragFloat("出現画面Y座標##SpawnScreenY", &screenAnchorSpawnScreenY_, 0.01f, 0.8f, 2.0f, "%.2f");
    ImGui::DragFloat("敵の拡縮##EnemyScale", &screenAnchorEnemyScale_, 0.02f, 0.1f, 5.0f, "%.2f");
    ImGui::DragFloat("落下中回転角度##RotationDuringDrop", &screenAnchorRotationDuringDrop_, 10.0f, 0.0f, 1440.0f, "%.0f");
    ImGui::DragFloat("最初の警告待機##FirstWarningDelay", &firstWarningDelay_, 0.02f, 0.0f, 5.0f, "%.2f");
    ImGui::DragFloat("左側の最初の警告待機##LeftFirstWarningDelay", &leftFirstWarningDelay_, 0.02f, 0.0f, 5.0f, "%.2f");
    ImGui::DragFloat("右側の最初の警告待機##RightFirstWarningDelay", &rightFirstWarningDelay_, 0.02f, 0.0f, 5.0f, "%.2f");
    ImGui::DragFloat("レーザー再使用待機##LaserCooldown", &laserCooldown_, 0.02f, 0.0f, 5.0f, "%.2f");
    ImGui::Text("画面固定敵数: %zu", screenAnchorEnemyCount_);
    ImGui::Text("最後の画面固定位置: %.2f, %.2f, %.2f", lastScreenAnchorPosition_.x, lastScreenAnchorPosition_.y, lastScreenAnchorPosition_.z);
    ImGui::Text("読込済みウェーブ数: %zu", waves_.size());
    ImGui::Text("進行中ウェーブ数: %zu", activeWaves_.size());
    ImGui::TextWrapped("ウェーブ状態: %s", pendingNextWaveActive_ ? "次ウェーブ待機" : (activeWaves_.empty() ? "待機" : "進行中"));
    ImGui::TextWrapped("最後に完了したウェーブID: %s", WaveTextOrNoneLabel(lastCompletedWaveId_));
    ImGui::TextWrapped("最後の完了理由: %s", WaveCompletionReasonLabel(lastCompletedReason_));
    ImGui::TextWrapped("最後に開始したウェーブID: %s", WaveTextOrNoneLabel(lastStartedWaveId_));
    ImGui::Text("開始時警告を表示: %s", lastStartedWaveShowWarning_ ? "はい" : "いいえ");
    ImGui::TextWrapped("ウェーブ警告文: %s", WaveTextOrNoneLabel(lastWaveWarningText_));
    ImGui::Text("ウェーブ警告時間: %.2f", lastWaveWarningDuration_);
    ImGui::Text("ウェーブ開始警告回数: %zu", waveStartWarningCount_);
    ImGui::Text("開始警告待機中: %s", pendingStartWarningActive_ ? "はい" : "いいえ");
    ImGui::TextWrapped("開始待機ウェーブID: %s", pendingStartWarningActive_ ? pendingStartWaveId_.c_str() : "なし");
    ImGui::Text("開始警告時間: %.2f / %.2f", pendingStartWarningActive_ ? (pendingStartWarningDuration_ + pendingStartPostDelay_ - pendingStartWarningTimer_) : 0.0f, pendingStartWarningDuration_ + pendingStartPostDelay_);
    ImGui::Text("警告後待機時間: %.2f", pendingStartPostDelay_);
    ImGui::Text("開始警告残り時間: %.2f", pendingStartWarningActive_ ? pendingStartWarningTimer_ : 0.0f);
    ImGui::Text("出現前に警告完了を待機: %s", lastStartedWaveWaitForWarning_ ? "はい" : "いいえ");
    ImGui::TextWrapped("次ウェーブID: %s", pendingNextWaveActive_ ? pendingNextWaveId_.c_str() : "なし");
    ImGui::Text("次ウェーブ残り時間: %.2f", pendingNextWaveActive_ ? pendingNextWaveTimer_ : 0.0f);
    ImGui::Text("開始ウェーブ数: %zu", startedWaveCount_);
    ImGui::Text("開始失敗ウェーブ数: %zu", failedWaveCount_);
    ImGui::Text("出現済み敵数: %zu", spawnedEnemyCount_);
    ImGui::Text("追跡中ウェーブ敵数: %zu", waveEnemies_.size());
    ImGui::Text("画面外で消去した敵数: %zu", despawnedOutOfCameraCount_);
    ImGui::Text("最後の固定接近方向: %.2f, %.2f, %.2f", lastLockedApproachDirection_.x, lastLockedApproachDirection_.y, lastLockedApproachDirection_.z);
    ImGui::Text("ゲームモード自動開始回数: %zu", lastGameModeAutoStartCount_);
    ImGui::Text("最後のウェーブ経過時間: %.2f", lastWaveElapsedTime_);
    ImGui::Text("最後のウェーブ出現数: %zu / %zu", lastWaveSpawnedCount_, lastWaveEnemyCount_);
    ImGui::TextWrapped("現在または最後のウェーブID: %s", WaveTextOrNoneLabel(lastWaveId_));
    ImGui::TextWrapped("最後の結果: %s", WaveTextOrNoneLabel(lastResult_));
    ImGui::Text("最後の出現位置: %.2f, %.2f, %.2f", lastSpawnPosition_.x, lastSpawnPosition_.y, lastSpawnPosition_.z);

    if (!activeWaves_.empty() && activeWaves_.front().waveIndex < waves_.size()) {
        const ActiveWave& waveState = activeWaves_.front();
        const EnemyWaveDefinition& wave = waves_[waveState.waveIndex];
        ImGui::SeparatorText("現在進行中のウェーブ");
        ImGui::TextWrapped("ウェーブ: %s / %s", wave.waveId.c_str(), wave.name.c_str());
        ImGui::Text("経過時間: %.2f", waveState.elapsedTime);
        ImGui::Text("出現済み: %zu / %zu", waveState.spawnedCount, wave.enemies.size());
    }

    ImGui::SeparatorText("ウェーブ手動再生");
    ImGui::InputText("ウェーブID##ManualWaveId", manualWaveIdBuffer_.data(), manualWaveIdBuffer_.size());
    if (ImGui::Button("入力ウェーブを再生##ManualPlayWave")) {
        std::string result;
        PlayWave(TrimCopyForWaveImGui(manualWaveIdBuffer_.data()), result);
        lastResult_ = result;
    }
    ImGui::SameLine();
    if (ImGui::Button("wave_001を再生##ManualPlayWave001")) {
        std::string result;
        PlayWave("wave_001", result);
        lastResult_ = result;
    }
    ImGui::SameLine();
    if (ImGui::Button("wave_002を再生##ManualPlayWave002")) {
        std::string result;
        PlayWave("wave_002", result);
        lastResult_ = result;
    }
    ImGui::SameLine();
    if (ImGui::Button("ウェーブを停止##StopWave")) {
        StopAllWaves();
    }
    ImGui::SameLine();
    if (ImGui::Button("ログを消去##ClearWaveLog")) {
        waveLog_.clear();
    }

    if (ImGui::TreeNode("読込済みウェーブ##LoadedWaves")) {
        if (waves_.empty()) {
            ImGui::TextDisabled("読込済みウェーブはありません。");
        } else {
            for (const EnemyWaveDefinition& wave : waves_) {
                ImGui::TextWrapped("%s  名前=%s  敵数=%zu  次=%s", wave.waveId.c_str(), wave.name.c_str(), wave.enemies.size(), wave.nextWaveId.empty() ? "なし" : wave.nextWaveId.c_str());
            }
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("ウェーブログ##WaveLog")) {
        if (waveLog_.empty()) {
            ImGui::TextDisabled("ウェーブログはまだありません。");
        } else {
            for (const std::string& line : waveLog_) {
                ImGui::TextWrapped("%s", line.c_str());
            }
        }
        ImGui::TreePop();
    }

    ImGui::End();
#endif
}
