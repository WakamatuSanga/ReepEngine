#include "EnemySpawnActionBridge.h"
#include "Engine/Game/Enemy/EnemyManager.h"
#include "Engine/Level/LevelEventRuntime.h"
#include "Engine/Level/LevelSceneRuntime.h"
#include <algorithm>
#include <cctype>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr size_t kMaxActionLogCount = 128;

    std::string ToLowerString(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
            });
        return text;
    }

    std::string PickTargetLabel(const FiredEventAction& action) {
        if (!action.targetObjectId.empty()) {
            return action.targetObjectId;
        }
        if (!action.targetObjectName.empty()) {
            return action.targetObjectName;
        }
        return "(none)";
    }
}

EnemySpawnActionBridge::EnemySpawnActionBridge() = default;

EnemySpawnActionBridge::~EnemySpawnActionBridge() = default;

void EnemySpawnActionBridge::Initialize(EnemyManager* enemyManager, LevelSceneRuntime* levelSceneRuntime) {
    enemyManager_ = enemyManager;
    levelSceneRuntime_ = levelSceneRuntime;
}

void EnemySpawnActionBridge::Finalize() {
    enemyManager_ = nullptr;
    levelSceneRuntime_ = nullptr;
    actionLog_.clear();
}

void EnemySpawnActionBridge::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(430.0f, 360.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("敵出現アクション接続 (Enemy Spawn Action Bridge Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("SpawnEnemy処理有効 (Enable SpawnEnemy Action)", &enableSpawnEnemyAction_);
    ImGui::TextWrapped("Last Target: %s", lastTarget_.c_str());
    ImGui::Text("Last Spawn Position: %.2f, %.2f, %.2f",
        lastSpawnPosition_.x,
        lastSpawnPosition_.y,
        lastSpawnPosition_.z);
    ImGui::TextWrapped("Last Result: %s", lastResult_.c_str());
    ImGui::Text("Spawn Count: %zu", spawnCount_);
    ImGui::Text("Failed Spawn Count: %zu", failedSpawnCount_);
    if (ImGui::Button("ログをクリア (Clear Log)##EnemySpawnActionBridge")) {
        actionLog_.clear();
    }
    if (ImGui::TreeNode("Action Log##EnemySpawnActionBridge")) {
        if (actionLog_.empty()) {
            ImGui::TextDisabled("ログはまだありません。 (No action log yet.)");
        } else {
            for (const std::string& line : actionLog_) {
                ImGui::TextWrapped("%s", line.c_str());
            }
        }
        ImGui::TreePop();
    }
    ImGui::End();
#endif
}

bool EnemySpawnActionBridge::HandleAction(const FiredEventAction& action, std::string& resultMessage) {
    lastTarget_ = PickTargetLabel(action);
    if (!enableSpawnEnemyAction_) {
        ++failedSpawnCount_;
        resultMessage = "EnemySpawnActionBridge is disabled.";
        lastResult_ = resultMessage;
        AddLog("SpawnEnemy failed: " + resultMessage + " target=" + lastTarget_);
        return false;
    }
    if (!IsSpawnEnemyAction(action.actionType)) {
        ++failedSpawnCount_;
        resultMessage = "Unsupported actionType for enemy spawn bridge: " + action.actionType;
        lastResult_ = resultMessage;
        AddLog("SpawnEnemy failed: " + resultMessage + " target=" + lastTarget_);
        return false;
    }
    if (!enemyManager_) {
        ++failedSpawnCount_;
        resultMessage = "EnemyManager is missing.";
        lastResult_ = resultMessage;
        AddLog("SpawnEnemy failed: " + resultMessage + " target=" + lastTarget_);
        return false;
    }
    if (!levelSceneRuntime_) {
        ++failedSpawnCount_;
        resultMessage = "LevelSceneRuntime is missing.";
        lastResult_ = resultMessage;
        AddLog("SpawnEnemy failed: " + resultMessage + " target=" + lastTarget_);
        return false;
    }
    if (action.targetObjectId.empty() && action.targetObjectName.empty()) {
        ++failedSpawnCount_;
        resultMessage = "Spawn target is empty.";
        lastResult_ = resultMessage;
        AddLog("SpawnEnemy failed: " + resultMessage);
        return false;
    }

    Vector3 spawnPosition{};
    if (!levelSceneRuntime_->TryFindObjectWorldPosition(
        action.targetObjectId,
        action.targetObjectId.empty() ? action.targetObjectName : std::string{},
        spawnPosition)) {
        ++failedSpawnCount_;
        resultMessage = "Spawn target not found: " + lastTarget_;
        lastResult_ = resultMessage;
        AddLog("SpawnEnemy failed: " + resultMessage);
        return false;
    }

    const std::string enemyType = action.actionDescription.empty() ? "Default" : "EventSpawn";
    if (!enemyManager_->SpawnEnemyAt(spawnPosition, enemyType)) {
        ++failedSpawnCount_;
        resultMessage = "EnemyManager failed to spawn enemy.";
        lastResult_ = resultMessage;
        AddLog("SpawnEnemy failed: " + resultMessage + " target=" + lastTarget_);
        return false;
    }

    ++spawnCount_;
    lastSpawnPosition_ = spawnPosition;
    resultMessage = "Spawned enemy at (" +
        std::to_string(spawnPosition.x) + ", " +
        std::to_string(spawnPosition.y) + ", " +
        std::to_string(spawnPosition.z) + ")";
    lastResult_ = resultMessage;
    AddLog(
        "SpawnEnemy success: target=" + lastTarget_ +
        " sourceFlag=" + action.eventFlagId +
        " description=" + action.actionDescription +
        " result=" + resultMessage);
    return true;
}

void EnemySpawnActionBridge::AddLog(const std::string& message) {
    actionLog_.push_back(message);
    if (actionLog_.size() > kMaxActionLogCount) {
        actionLog_.erase(actionLog_.begin(), actionLog_.begin() + (actionLog_.size() - kMaxActionLogCount));
    }
}

bool EnemySpawnActionBridge::IsSpawnEnemyAction(const std::string& actionType) {
    return ToLowerString(actionType) == "spawnenemy";
}

