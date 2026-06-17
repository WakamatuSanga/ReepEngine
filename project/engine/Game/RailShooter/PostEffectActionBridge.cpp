#include "PostEffectActionBridge.h"
#include "Engine/Game/Effect/PostEffectController.h"
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
}

PostEffectActionBridge::PostEffectActionBridge() = default;

PostEffectActionBridge::~PostEffectActionBridge() = default;

void PostEffectActionBridge::Initialize(PostEffectController* postEffectController) {
    postEffectController_ = postEffectController;
}

void PostEffectActionBridge::Finalize() {
    postEffectController_ = nullptr;
    actionLog_.clear();
}

void PostEffectActionBridge::DrawImGui() {
#ifdef _DEBUG
    ImGui::SetNextWindowSize(ImVec2(430.0f, 340.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("ポストエフェクトアクション接続 (PostEffect Action Bridge Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("PlayPostEffect処理有効 (Enable PlayPostEffect)", &enablePlayPostEffect_);
    ImGui::TextWrapped("Last PostEffect Type: %s", lastPostEffectType_.c_str());
    ImGui::TextWrapped("Last Result: %s", lastResult_.c_str());
    ImGui::Text("Play Count: %zu", playCount_);
    ImGui::Text("Failed Count: %zu", failedCount_);
    if (ImGui::Button("ログをクリア (Clear Log)##PostEffectActionBridge")) {
        actionLog_.clear();
    }
    if (ImGui::TreeNode("Action Log##PostEffectActionBridge")) {
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

bool PostEffectActionBridge::HandleAction(const FiredEventAction& action, std::string& resultMessage) {
    lastPostEffectType_ = action.postEffectType.empty() ? "Flash" : action.postEffectType;
    if (!enablePlayPostEffect_) {
        ++failedCount_;
        resultMessage = "PostEffectActionBridge is disabled.";
        lastResult_ = resultMessage;
        AddLog("PlayPostEffect failed: " + resultMessage);
        return false;
    }
    if (!IsPlayPostEffectAction(action.actionType)) {
        ++failedCount_;
        resultMessage = "Unsupported actionType for post effect bridge: " + action.actionType;
        lastResult_ = resultMessage;
        AddLog("PlayPostEffect failed: " + resultMessage);
        return false;
    }
    if (!postEffectController_) {
        ++failedCount_;
        resultMessage = "PostEffectController is missing.";
        lastResult_ = resultMessage;
        AddLog("PlayPostEffect failed: " + resultMessage);
        return false;
    }

    const bool played = postEffectController_->PlayPostEffect(lastPostEffectType_, resultMessage);
    if (played) {
        ++playCount_;
    } else {
        ++failedCount_;
    }
    lastResult_ = resultMessage;
    AddLog(
        std::string(played ? "PlayPostEffect success: " : "PlayPostEffect failed: ") +
        "type=" + lastPostEffectType_ +
        " sourceFlag=" + action.eventFlagId +
        " description=" + action.actionDescription +
        " result=" + resultMessage);
    return played;
}

void PostEffectActionBridge::AddLog(const std::string& message) {
    actionLog_.push_back(message);
    if (actionLog_.size() > kMaxActionLogCount) {
        actionLog_.erase(actionLog_.begin(), actionLog_.begin() + (actionLog_.size() - kMaxActionLogCount));
    }
}

bool PostEffectActionBridge::IsPlayPostEffectAction(const std::string& actionType) {
    return ToLowerString(actionType) == "playposteffect";
}
