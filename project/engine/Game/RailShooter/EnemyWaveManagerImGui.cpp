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
}
void EnemyWaveManager::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(430.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Enemy Wave Debug")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Enable SpawnWave Action", &enabled_);
    ImGui::Checkbox("Auto Load Missing Wave", &autoLoadMissingWave_);
    ImGui::Checkbox("GameMode開始時にWave 1を再生 (Auto Start Wave On GameMode)", &autoStartWaveOnGameMode_);
    ImGui::Checkbox("Auto Progress Enabled", &autoProgressEnabled_);
    ImGui::TextWrapped("Auto Start Wave ID: %s", autoStartWaveId_.c_str());
    ImGui::DragFloat("Spawn Width", &spawnWidth_, 0.1f, 1.0f, 100.0f);
    ImGui::DragFloat("Spawn Height", &spawnHeight_, 0.1f, 1.0f, 100.0f);
    ImGui::DragFloat("接近速度 (Approach Speed)", &approachSpeed_, 0.1f, 0.0f, 60.0f, "%.1f");
    ImGui::DragFloat("Boost接近速度加算 (Boost Approach Bonus)", &boostApproachBonus_, 0.1f, 0.0f, 30.0f, "%.1f");
    ImGui::DragFloat("接近停止距離 (Approach Stop Distance)", &approachStopDistance_, 0.1f, 0.0f, 40.0f, "%.1f");
    ImGui::Text("Current Boost Power: %.2f", currentBoostPower_);
    ImGui::Text("Current Approach Speed: %.2f", approachSpeed_ + currentBoostPower_ * boostApproachBonus_);
    ImGui::SeparatorText("Wave 2 Enemy Debug");
    ImGui::Checkbox("Screen Anchor Enabled", &screenAnchorEnabled_);
    ImGui::DragFloat("Drop Duration", &screenAnchorDropDuration_, 0.02f, 0.05f, 3.0f, "%.2f");
    ImGui::DragFloat("Spawn Screen Y", &screenAnchorSpawnScreenY_, 0.01f, 0.8f, 2.0f, "%.2f");
    ImGui::DragFloat("Enemy Scale", &screenAnchorEnemyScale_, 0.02f, 0.1f, 5.0f, "%.2f");
    ImGui::DragFloat("Rotation During Drop", &screenAnchorRotationDuringDrop_, 10.0f, 0.0f, 1440.0f, "%.0f");
    ImGui::DragFloat("First Warning Delay", &firstWarningDelay_, 0.02f, 0.0f, 5.0f, "%.2f");
    ImGui::DragFloat("Left First Warning Delay", &leftFirstWarningDelay_, 0.02f, 0.0f, 5.0f, "%.2f");
    ImGui::DragFloat("Right First Warning Delay", &rightFirstWarningDelay_, 0.02f, 0.0f, 5.0f, "%.2f");
    ImGui::DragFloat("Laser Cooldown", &laserCooldown_, 0.02f, 0.0f, 5.0f, "%.2f");
    ImGui::Text("Screen Anchor Enemy Count: %zu", screenAnchorEnemyCount_);
    ImGui::Text("Last Screen Anchor Pos: %.2f, %.2f, %.2f", lastScreenAnchorPosition_.x, lastScreenAnchorPosition_.y, lastScreenAnchorPosition_.z);
    ImGui::Text("Loaded Wave Count: %zu", waves_.size());
    ImGui::Text("Active Wave Count: %zu", activeWaves_.size());
    ImGui::TextWrapped("Wave State: %s", pendingNextWaveActive_ ? "Pending Next Wave" : (activeWaves_.empty() ? "Idle" : "Active"));
    ImGui::TextWrapped("Last Completed Wave ID: %s", lastCompletedWaveId_.c_str());
    ImGui::TextWrapped("Last Completed Reason: %s", lastCompletedReason_.c_str());
    ImGui::TextWrapped("Next Wave ID: %s", pendingNextWaveActive_ ? pendingNextWaveId_.c_str() : "(none)");
    ImGui::Text("Next Wave Countdown: %.2f", pendingNextWaveActive_ ? pendingNextWaveTimer_ : 0.0f);
    ImGui::Text("Started Wave Count: %zu", startedWaveCount_);
    ImGui::Text("Failed Wave Count: %zu", failedWaveCount_);
    ImGui::Text("Spawned Enemy Count: %zu", spawnedEnemyCount_);
    ImGui::Text("Tracked Wave Enemy Count: %zu", waveEnemies_.size());
    ImGui::Text("Despawned Out Of Camera Count: %zu", despawnedOutOfCameraCount_);
    ImGui::Text("Last Locked Direction: %.2f, %.2f, %.2f", lastLockedApproachDirection_.x, lastLockedApproachDirection_.y, lastLockedApproachDirection_.z);
    ImGui::Text("GameMode Auto Start Count: %zu", lastGameModeAutoStartCount_);
    ImGui::Text("Last Wave Elapsed Time: %.2f", lastWaveElapsedTime_);
    ImGui::Text("Last Wave Spawned Count: %zu / %zu", lastWaveSpawnedCount_, lastWaveEnemyCount_);
    ImGui::TextWrapped("Current / Last Wave ID: %s", lastWaveId_.c_str());
    ImGui::TextWrapped("Last Result: %s", lastResult_.c_str());
    ImGui::Text("Last Spawn Position: %.2f, %.2f, %.2f", lastSpawnPosition_.x, lastSpawnPosition_.y, lastSpawnPosition_.z);

    if (!activeWaves_.empty() && activeWaves_.front().waveIndex < waves_.size()) {
        const ActiveWave& waveState = activeWaves_.front();
        const EnemyWaveDefinition& wave = waves_[waveState.waveIndex];
        ImGui::SeparatorText("Current Active Wave");
        ImGui::TextWrapped("Wave: %s / %s", wave.waveId.c_str(), wave.name.c_str());
        ImGui::Text("Elapsed Time: %.2f", waveState.elapsedTime);
        ImGui::Text("Spawned: %zu / %zu", waveState.spawnedCount, wave.enemies.size());
    }

    ImGui::SeparatorText("Manual Play Wave");
    ImGui::InputText("Wave ID", manualWaveIdBuffer_.data(), manualWaveIdBuffer_.size());
    if (ImGui::Button("Manual Play Wave")) {
        std::string result;
        PlayWave(TrimCopyForWaveImGui(manualWaveIdBuffer_.data()), result);
        lastResult_ = result;
    }
    ImGui::SameLine();
    if (ImGui::Button("Manual Play wave_001")) {
        std::string result;
        PlayWave("wave_001", result);
        lastResult_ = result;
    }
    ImGui::SameLine();
    if (ImGui::Button("Manual Play wave_002")) {
        std::string result;
        PlayWave("wave_002", result);
        lastResult_ = result;
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop Wave")) {
        StopAllWaves();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Log")) {
        waveLog_.clear();
    }

    if (ImGui::TreeNode("Loaded Waves")) {
        if (waves_.empty()) {
            ImGui::TextDisabled("No loaded waves.");
        } else {
            for (const EnemyWaveDefinition& wave : waves_) {
                ImGui::TextWrapped("%s  name=%s  enemies=%zu  next=%s", wave.waveId.c_str(), wave.name.c_str(), wave.enemies.size(), wave.nextWaveId.empty() ? "(none)" : wave.nextWaveId.c_str());
            }
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Wave Log")) {
        if (waveLog_.empty()) {
            ImGui::TextDisabled("No wave log yet.");
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

