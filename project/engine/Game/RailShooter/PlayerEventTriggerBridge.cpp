#include "PlayerEventTriggerBridge.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Level/LevelEventRuntime.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

PlayerEventTriggerBridge::PlayerEventTriggerBridge() = default;

PlayerEventTriggerBridge::~PlayerEventTriggerBridge() = default;

void PlayerEventTriggerBridge::Initialize(Player* player, LevelEventRuntime* eventRuntime) {
    player_ = player;
    eventRuntime_ = eventRuntime;
}

void PlayerEventTriggerBridge::Finalize() {
    if (eventRuntime_) {
        eventRuntime_->SetPlayerTriggerState(false, {}, 0.0f);
    }
    player_ = nullptr;
    eventRuntime_ = nullptr;
}

void PlayerEventTriggerBridge::Update() {
    if (!eventRuntime_) {
        return;
    }
    if (!enabled_ || !player_) {
        eventRuntime_->SetPlayerTriggerState(false, {}, 0.0f);
        return;
    }

    eventRuntime_->SetPlayerTriggerState(
        true,
        player_->GetWorldPosition(),
        player_->GetEventTriggerRadius());
}

void PlayerEventTriggerBridge::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(360.0f, 220.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("プレイヤーイベント判定確認 (Player Event Trigger Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Player Event Trigger Bridge有効 (Enable)", &enabled_);
    ImGui::Text("Player: %s", player_ ? "connected" : "missing");
    ImGui::Text("EventRuntime: %s", eventRuntime_ ? "connected" : "missing");
    if (player_) {
        const Vector3& position = player_->GetWorldPosition();
        ImGui::Text("Player Trigger Position: %.3f, %.3f, %.3f", position.x, position.y, position.z);
        ImGui::Text("Player Trigger Radius: %.3f", player_->GetEventTriggerRadius());
    }
    if (eventRuntime_) {
        ImGui::Text("Runtime Player Available: %s", enabled_ && player_ ? "true" : "false");
        ImGui::Text("Is Player Inside EventFlag: %s", eventRuntime_->IsPlayerInsideEventFlag() ? "true" : "false");
        ImGui::TextWrapped("Last Triggered By: %s", eventRuntime_->GetLastTriggeredBy().c_str());
    }
    ImGui::End();
#endif
}

