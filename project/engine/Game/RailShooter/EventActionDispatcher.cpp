#include "Engine/Core/DebugUiWorkspace.h"
#include "EventActionDispatcher.h"
#include "Engine/Game/RailShooter/EnemySpawnActionBridge.h"
#include "Engine/Game/RailShooter/EnemyWaveManager.h"
#include "Engine/Game/RailShooter/PostEffectActionBridge.h"
#include "Engine/Game/RailShooter/RailShooterEventActionBridge.h"
#include "Engine/Game/UI/WarningUIController.h"
#include "Engine/Graphics/Effect/PrimitiveEffectSystem.h"
#include "Engine/Level/LevelEventRuntime.h"
#include "Engine/Level/LevelSceneRuntime.h"
#include <algorithm>
#include <cctype>

#ifdef USE_IMGUI
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
        if (!action.waveId.empty()) {
            return action.waveId;
        }
        return "(none)";
    }
}

class EventActionDispatcher::ActionHandler {
public:
    struct Result {
        bool handled = false;
        bool missingEffectTarget = false;
    };

    virtual ~ActionHandler() = default;
    virtual Result Execute(const FiredEventAction& action, std::string& message) = 0;
};

template<class Target, auto HandleAction>
class EventActionDispatcher::BridgeActionHandler final : public ActionHandler {
public:
    explicit BridgeActionHandler(Target* target) : target_(target) {}

    Result Execute(const FiredEventAction& action, std::string& message) override {
        return { target_ && (target_->*HandleAction)(action, message), false };
    }

private:
    Target* target_;
};

class EventActionDispatcher::PlayEffectHandler final : public ActionHandler {
public:
    PlayEffectHandler(PrimitiveEffectSystem* effects, const LevelSceneRuntime* level)
        : effects_(effects), level_(level) {}

    Result Execute(const FiredEventAction& action, std::string& message) override;

private:
    PrimitiveEffectSystem* effects_;
    const LevelSceneRuntime* level_;
};

EventActionDispatcher::EventActionDispatcher() = default;

EventActionDispatcher::~EventActionDispatcher() = default;

void EventActionDispatcher::Initialize(
    LevelEventRuntime* eventRuntime,
    RailShooterEventActionBridge* cameraRailBridge,
    EnemySpawnActionBridge* enemySpawnBridge,
    EnemyWaveManager* enemyWaveManager,
    PostEffectActionBridge* postEffectBridge,
    PrimitiveEffectSystem* primitiveEffectSystem,
    LevelSceneRuntime* levelSceneRuntime,
    WarningUIController* warningUIController) {
    eventRuntime_ = eventRuntime;
    handlers_["startcamerarail"] = std::make_unique<
        BridgeActionHandler<RailShooterEventActionBridge, &RailShooterEventActionBridge::HandleAction>>(cameraRailBridge);
    handlers_["playeffect"] = std::make_unique<PlayEffectHandler>(primitiveEffectSystem, levelSceneRuntime);
    handlers_["playposteffect"] = std::make_unique<
        BridgeActionHandler<PostEffectActionBridge, &PostEffectActionBridge::HandleAction>>(postEffectBridge);
    handlers_["spawnenemy"] = std::make_unique<
        BridgeActionHandler<EnemySpawnActionBridge, &EnemySpawnActionBridge::HandleAction>>(enemySpawnBridge);
    handlers_["spawnwave"] = std::make_unique<
        BridgeActionHandler<EnemyWaveManager, &EnemyWaveManager::HandleSpawnWaveAction>>(enemyWaveManager);
    handlers_["showwarning"] = std::make_unique<
        BridgeActionHandler<WarningUIController, &WarningUIController::HandleAction>>(warningUIController);
}

void EventActionDispatcher::Finalize() {
    eventRuntime_ = nullptr;
    handlers_.clear();
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
        ActionHandler::Result outcome;

        const auto handler = handlers_.find(ToLowerString(action.actionType));
        if (handler != handlers_.end()) {
            outcome = handler->second->Execute(action, result);
        } else {
            result = "Unsupported actionType: " + lastActionType_;
        }

        if (outcome.missingEffectTarget) ++missingTargetCount_;
        if (outcome.handled) {
            ++dispatchedActionCount_;
        }
        lastResult_ = result;
        AddLog(
            std::string(outcome.handled ? "Dispatched " : "Dispatch failed ") +
            lastActionType_ +
            " target=" + lastTarget_ +
            " result=" + result);
    }
}

void EventActionDispatcher::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(430.0f, 360.0f), ImGuiCond_FirstUseEver);
    if (!DebugUiWorkspace::BeginPanel("イベントアクション振り分け (Event Action Dispatcher)")) {
        DebugUiWorkspace::EndPanel();
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
    DebugUiWorkspace::EndPanel();
#endif
}

void EventActionDispatcher::AddLog(const std::string& message) {
    dispatchLog_.push_back(message);
    if (dispatchLog_.size() > kMaxDispatchLogCount) {
        dispatchLog_.erase(dispatchLog_.begin(), dispatchLog_.begin() + (dispatchLog_.size() - kMaxDispatchLogCount));
    }
}

EventActionDispatcher::ActionHandler::Result EventActionDispatcher::PlayEffectHandler::Execute(
    const FiredEventAction& action, std::string& resultMessage) {
    if (!effects_) {
        resultMessage = "PrimitiveEffectSystem is missing.";
        return {};
    }
    if (!level_) {
        resultMessage = "LevelSceneRuntime is missing.";
        return {};
    }

    Vector3 targetPosition{};
    if (!level_->TryFindObjectWorldPosition(
        action.targetObjectId,
        action.targetObjectId.empty() ? action.targetObjectName : std::string{},
        targetPosition)) {
        resultMessage = "Effect target not found: " + PickTargetLabel(action);
        return { false, true };
    }

    std::string effectResult;
    const bool played = effects_->PlayPresetAt("HitRing", targetPosition, effectResult);
    resultMessage = effectResult + " position=(" +
        std::to_string(targetPosition.x) + ", " +
        std::to_string(targetPosition.y) + ", " +
        std::to_string(targetPosition.z) + ")";
    return { played, false };
}

