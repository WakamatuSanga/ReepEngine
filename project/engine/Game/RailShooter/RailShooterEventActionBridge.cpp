#include "RailShooterEventActionBridge.h"
#include "Engine/Game/Camera/RailShooterCameraRig.h"
#include "Engine/Level/LevelEventRuntime.h"
#include <algorithm>
#include <cctype>

#ifdef _DEBUG
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

    std::string PickTargetRailKey(const FiredEventAction& action) {
        if (!action.targetObjectId.empty()) {
            return action.targetObjectId;
        }
        return action.targetObjectName;
    }
}

RailShooterEventActionBridge::RailShooterEventActionBridge() = default;

RailShooterEventActionBridge::~RailShooterEventActionBridge() = default;

void RailShooterEventActionBridge::Initialize(LevelEventRuntime* eventRuntime, RailShooterCameraRig* cameraRig) {
    eventRuntime_ = eventRuntime;
    cameraRig_ = cameraRig;
}

void RailShooterEventActionBridge::Finalize() {
    eventRuntime_ = nullptr;
    cameraRig_ = nullptr;
    actionLog_.clear();
}

void RailShooterEventActionBridge::Update() {
    // EventActionDispatcher consumes pending actions once and calls HandleAction().
}

bool RailShooterEventActionBridge::HandleAction(const FiredEventAction& action, std::string& resultMessage) {
    if (!enableBridge_) {
        resultMessage = "RailShooterEventActionBridge is disabled.";
        return false;
    }

    ++consumedActionCount_;
    lastActionType_ = action.actionType.empty() ? "(none)" : action.actionType;
    lastTarget_ = PickTargetRailKey(action);
    if (lastTarget_.empty()) {
        lastTarget_ = "(none)";
    }

    if (!IsStartCameraRailAction(action.actionType)) {
        resultMessage = "Unsupported actionType for camera bridge: " + lastActionType_;
        lastResult_ = resultMessage;
        AddLog(resultMessage + " from " + action.eventFlagId);
        return false;
    }
    if (!cameraRig_) {
        resultMessage = "CameraRig is missing.";
        lastResult_ = resultMessage;
        AddLog("StartCameraRail failed: " + resultMessage);
        return false;
    }
    if (lastTarget_ == "(none)") {
        resultMessage = "Rail target is empty.";
        lastResult_ = resultMessage;
        AddLog("StartCameraRail failed: " + resultMessage);
        return false;
    }

    std::string result;
    const bool started = cameraRig_->StartRailByKey(
        lastTarget_,
        RailShooterCameraRig::CameraRailStartMode::FromRailStart,
        result);
    lastResult_ = result;
    resultMessage = result;
    AddLog(
        std::string(started ? "StartCameraRail success: " : "StartCameraRail failed: ") +
        result +
        " target=" + lastTarget_ +
        " sourceFlag=" + action.eventFlagId +
        " description=" + action.actionDescription);
    return started;
}

void RailShooterEventActionBridge::DrawImGui() {
#ifdef _DEBUG
    ImGui::SetNextWindowSize(ImVec2(430.0f, 360.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("レールシューターイベント接続 (Rail Shooter Event Action Bridge)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Bridge有効 (Enable Bridge)", &enableBridge_);
    ImGui::Text("Pending Action Count: %zu", eventRuntime_ ? eventRuntime_->GetPendingActionCount() : 0);
    ImGui::Text("Handled Action Count: %zu", consumedActionCount_);
    ImGui::TextWrapped("Last Action Type: %s", lastActionType_.c_str());
    ImGui::TextWrapped("Last Target: %s", lastTarget_.c_str());
    ImGui::TextWrapped("Last Result: %s", lastResult_.c_str());
    if (ImGui::Button("ログをクリア (Clear Log)")) {
        actionLog_.clear();
    }
    if (ImGui::TreeNode("Action Log")) {
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

void RailShooterEventActionBridge::AddLog(const std::string& message) {
    actionLog_.push_back(message);
    if (actionLog_.size() > kMaxActionLogCount) {
        actionLog_.erase(actionLog_.begin(), actionLog_.begin() + (actionLog_.size() - kMaxActionLogCount));
    }
}

bool RailShooterEventActionBridge::IsStartCameraRailAction(const std::string& actionType) {
    return ToLowerString(actionType) == "startcamerarail";
}
