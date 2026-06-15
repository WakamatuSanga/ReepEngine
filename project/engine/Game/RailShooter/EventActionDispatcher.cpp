#include "EventActionDispatcher.h"
#include "Engine/Game/RailShooter/EnemySpawnActionBridge.h"
#include "Engine/Game/RailShooter/RailShooterEventActionBridge.h"
#include "Engine/Graphics/Effect/PrimitiveEffectSystem.h"
#include "Engine/Level/LevelEventRuntime.h"
#include "Engine/Level/LevelSceneRuntime.h"
#include <algorithm>
#include <cctype>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr size_t kMaxDispatchLogCount = 128;

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

EventActionDispatcher::EventActionDispatcher() = default;

EventActionDispatcher::~EventActionDispatcher() = default;

void EventActionDispatcher::Initialize(
    LevelEventRuntime* eventRuntime,
    RailShooterEventActionBridge* cameraRailBridge,
    EnemySpawnActionBridge* enemySpawnBridge,
    PrimitiveEffectSystem* primitiveEffectSystem,
    LevelSceneRuntime* levelSceneRuntime) {
    eventRuntime_ = eventRuntime;
    cameraRailBridge_ = cameraRailBridge;
    enemySpawnBridge_ = enemySpawnBridge;
    primitiveEffectSystem_ = primitiveEffectSystem;
    levelSceneRuntime_ = levelSceneRuntime;
}

void EventActionDispatcher::Finalize() {
    eventRuntime_ = nullptr;
    cameraRailBridge_ = nullptr;
    enemySpawnBridge_ = nullptr;
    primitiveEffectSystem_ = nullptr;
    levelSceneRuntime_ = nullptr;
    dispatchLog_.clear();
}

void EventActionDispatcher::Update() {
    if (!enabled_ || !eventRuntime_) {
        return;
    }

    std::vector<FiredEventAction> actions = eventRuntime_->ConsumePendingActions();
    consumedActionCount_ += actions.size();
    for (const FiredEventAction& action : actions) {
        lastActionType_ = action.actionType.empty() ? "(none)" : action.actionType;
        lastTarget_ = PickTargetLabel(action);
        std::string result;
        bool handled = false;

        if (IsActionType(action.actionType, "startcamerarail")) {
            handled = cameraRailBridge_ && cameraRailBridge_->HandleAction(action, result);
        } else if (IsActionType(action.actionType, "playeffect")) {
            handled = DispatchPlayEffect(action, result);
        } else if (IsActionType(action.actionType, "spawnenemy")) {
            handled = enemySpawnBridge_ && enemySpawnBridge_->HandleAction(action, result);
        } else {
            result = "Unsupported actionType: " + lastActionType_;
        }

        if (handled) {
            ++dispatchedActionCount_;
        }
        lastResult_ = result;
        AddLog(
            std::string(handled ? "Dispatched " : "Dispatch failed ") +
            lastActionType_ +
            " target=" + lastTarget_ +
            " result=" + result);
    }
}

void EventActionDispatcher::DrawImGui() {
#ifdef _DEBUG
    ImGui::SetNextWindowSize(ImVec2(430.0f, 360.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("イベントアクション振り分け (Event Action Dispatcher)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Dispatcher有効 (Enable Dispatcher)", &enabled_);
    ImGui::Text("Pending Action Count: %zu", eventRuntime_ ? eventRuntime_->GetPendingActionCount() : 0);
    ImGui::Text("Consumed Action Count: %zu", consumedActionCount_);
    ImGui::Text("Dispatched Action Count: %zu", dispatchedActionCount_);
    ImGui::Text("Missing Target Count: %zu", missingTargetCount_);
    ImGui::TextWrapped("Last Action Type: %s", lastActionType_.c_str());
    ImGui::TextWrapped("Last Target: %s", lastTarget_.c_str());
    ImGui::TextWrapped("Last Result: %s", lastResult_.c_str());
    if (ImGui::Button("ログをクリア (Clear Log)##EventActionDispatcher")) {
        dispatchLog_.clear();
    }
    if (ImGui::TreeNode("Dispatch Log")) {
        if (dispatchLog_.empty()) {
            ImGui::TextDisabled("ログはまだありません。 (No dispatch log yet.)");
        } else {
            for (const std::string& line : dispatchLog_) {
                ImGui::TextWrapped("%s", line.c_str());
            }
        }
        ImGui::TreePop();
    }
    ImGui::End();
#endif
}

void EventActionDispatcher::AddLog(const std::string& message) {
    dispatchLog_.push_back(message);
    if (dispatchLog_.size() > kMaxDispatchLogCount) {
        dispatchLog_.erase(dispatchLog_.begin(), dispatchLog_.begin() + (dispatchLog_.size() - kMaxDispatchLogCount));
    }
}

bool EventActionDispatcher::DispatchPlayEffect(const FiredEventAction& action, std::string& resultMessage) {
    if (!primitiveEffectSystem_) {
        resultMessage = "PrimitiveEffectSystem is missing.";
        return false;
    }
    if (!levelSceneRuntime_) {
        resultMessage = "LevelSceneRuntime is missing.";
        return false;
    }

    Vector3 targetPosition{};
    if (!levelSceneRuntime_->TryFindObjectWorldPosition(
        action.targetObjectId,
        action.targetObjectId.empty() ? action.targetObjectName : std::string{},
        targetPosition)) {
        ++missingTargetCount_;
        resultMessage = "Effect target not found: " + PickTargetLabel(action);
        return false;
    }

    std::string effectResult;
    const bool played = primitiveEffectSystem_->PlayPresetAt("HitRing", targetPosition, effectResult);
    resultMessage = effectResult + " position=(" +
        std::to_string(targetPosition.x) + ", " +
        std::to_string(targetPosition.y) + ", " +
        std::to_string(targetPosition.z) + ")";
    return played;
}

bool EventActionDispatcher::IsActionType(const std::string& actionType, const char* expectedLower) {
    return ToLowerString(actionType) == expectedLower;
}
