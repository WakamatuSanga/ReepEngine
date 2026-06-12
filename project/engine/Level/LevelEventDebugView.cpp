#include "LevelEventDebugView.h"
#include "LevelEventConnectionVisualizer.h"
#include "LevelEventData.h"
#include "LevelEventLabelVisualizer.h"
#include "LevelEventObjectActionVisualizer.h"
#include "LevelEventVisualizer.h"
#include "LevelSceneData.h"
#include <cstdio>
#include <string>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    std::string FormatVector3(const Vector3& value) {
        char buffer[96]{};
        std::snprintf(
            buffer,
            sizeof(buffer),
            "(%.3f, %.3f, %.3f)",
            value.x,
            value.y,
            value.z);
        return buffer;
    }
}

void DrawLevelObjectEventDetailsImGui(const LevelObject& object) {
#ifdef _DEBUG
    ImGui::Text("オブジェクトID (objectId): %s", object.objectId.empty() ? "(none)" : object.objectId.c_str());
    ImGui::Text("エディタラベル (editorLabel): %s", object.editorLabel.empty() ? "(none)" : object.editorLabel.c_str());
    ImGui::Text(
        "エディタ説明 (editorDescription): %s",
        object.editorDescription.empty() ? "(none)" : object.editorDescription.c_str());
    ImGui::Text("イベントフラグか (isEventFlag): %s", object.isEventFlag ? "true" : "false");
    ImGui::Text("イベントフラグID (eventFlagId): %s", object.eventFlagId.empty() ? "(none)" : object.eventFlagId.c_str());

    if (!object.isEventFlag) {
        return;
    }

    ImGui::Separator();
    ImGui::Text("表示名 (event.displayName): %s", object.eventFlag.displayName.c_str());
    ImGui::TextWrapped("説明 (event.description): %s", object.eventFlag.description.c_str());
    ImGui::Text("発火種類 (event.triggerType): %s", object.eventFlag.triggerType.c_str());
    ImGui::Text("形状種類 (event.shapeType): %s", object.eventFlag.shapeType.c_str());
    ImGui::Text("一度だけ (event.oneShot): %s", object.eventFlag.oneShot ? "true" : "false");
    ImGui::Text("初期有効 (event.initiallyEnabled): %s", object.eventFlag.initiallyEnabled ? "true" : "false");
    ImGui::Text("エディタ表示 (event.visibleInEditor): %s", object.eventFlag.visibleInEditor ? "true" : "false");
    ImGui::Text("サイズ (event.size): %s", FormatVector3(object.eventFlag.size).c_str());

    if (object.eventFlag.nextFlagIds.empty()) {
        ImGui::TextUnformatted("次フラグID (event.nextFlagIds): (none)");
    } else if (ImGui::TreeNode("次フラグID (event.nextFlagIds)")) {
        for (const std::string& nextFlagId : object.eventFlag.nextFlagIds) {
            ImGui::BulletText("%s", nextFlagId.c_str());
        }
        ImGui::TreePop();
    }

    if (object.eventFlag.objectActions.empty()) {
        ImGui::TextUnformatted("対象オブジェクト操作 (event.objectActions): (none)");
    } else if (ImGui::TreeNode("対象オブジェクト操作 (event.objectActions)")) {
        for (const LevelEventObjectAction& action : object.eventFlag.objectActions) {
            ImGui::BulletText(
                "%s / %s / %s",
                action.targetObjectId.empty() ? action.targetObjectName.c_str() : action.targetObjectId.c_str(),
                action.actionType.c_str(),
                action.actionDescription.c_str());
        }
        ImGui::TreePop();
    }
#else
    (void)object;
#endif
}

bool DrawLevelEventDebugImGui(
    const LevelSceneData& sceneData,
    const LevelObject* selectedObject,
    LevelEventVisualizer* eventVisualizer,
    LevelEventConnectionVisualizer* connectionVisualizer,
    LevelEventObjectActionVisualizer* objectActionVisualizer,
    LevelEventLabelVisualizer* labelVisualizer) {
#ifdef _DEBUG
    bool needsRebuild = false;
    const LevelEventValidationResult validation = ValidateLevelEventLinks(sceneData);
    ImGui::Text("イベントフラグ数 (Event Flag Count): %zu", CountEventFlags(sceneData));
    ImGui::Text("不明な接続数 (Missing Link Count): %zu", validation.GetMissingLinkCount());
    if (eventVisualizer) {
        ImGui::SeparatorText("EventFlag Visibility Diagnostics");
        ImGui::Text("EventFlag total count: %zu", eventVisualizer->GetTotalEventFlagCount());
        ImGui::Text("Visible EventFlag count: %zu", eventVisualizer->GetVisibleEventFlagCount());
        ImGui::Text("Hidden EventFlag count: %zu", eventVisualizer->GetHiddenEventFlagCount());
        ImGui::TextWrapped("Hidden reason: %s", eventVisualizer->GetHiddenReasonSummary().c_str());
        ImGui::Text("visible_in_editor false: %zu", eventVisualizer->GetHiddenVisibleInEditorFalseCount());
        ImGui::Text("disabled hidden: %zu", eventVisualizer->GetHiddenDisabledCount());
        ImGui::Text("camera rig hide debug: %zu", eventVisualizer->GetHiddenCameraRigDebugCount());
        ImGui::Text("no visual object: %zu", eventVisualizer->GetHiddenNoVisualObjectCount());
        ImGui::Text("runtime inactive but hidden: %zu", eventVisualizer->GetHiddenRuntimeInactiveCount());
    }

    if (ImGui::TreeNode("選択中イベントフラグ (Selected Event Flag)")) {
        if (selectedObject && selectedObject->isEventFlag) {
            DrawLevelObjectEventDetailsImGui(*selectedObject);
        } else {
            ImGui::TextDisabled("オブジェクトツリーでEventFlagを選択してください。 (Select an EventFlag object in Object Tree.)");
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("接続チェック (Link Validation)")) {
        if (validation.missingFlagLinks.empty()) {
            ImGui::TextUnformatted("不明なフラグ接続: なし (Missing Flag Links: none)");
        } else if (ImGui::TreeNode("不明なフラグ接続 (Missing Flag Links)")) {
            for (const LevelEventFlagLink& link : validation.missingFlagLinks) {
                ImGui::BulletText("%s -> %s", link.sourceFlagId.c_str(), link.targetFlagId.c_str());
            }
            ImGui::TreePop();
        }

        if (validation.missingObjectLinks.empty()) {
            ImGui::TextUnformatted("不明なオブジェクト接続: なし (Missing Object Links: none)");
        } else if (ImGui::TreeNode("不明なオブジェクト接続 (Missing Object Links)")) {
            for (const LevelEventObjectActionLink& link : validation.missingObjectLinks) {
                const char* target = link.targetObjectId.empty()
                    ? link.targetObjectName.c_str()
                    : link.targetObjectId.c_str();
                ImGui::BulletText("%s -> %s (%s)", link.sourceFlagId.c_str(), target, link.actionType.c_str());
            }
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }

    if (eventVisualizer && ImGui::TreeNode("イベント表示 (Event Flag Visuals)")) {
        needsRebuild = eventVisualizer->DrawImGui() || needsRebuild;
        ImGui::TreePop();
    }

    if (connectionVisualizer && ImGui::TreeNode("フラグ接続線 (Flag Links)")) {
        needsRebuild = connectionVisualizer->DrawImGui() || needsRebuild;
        ImGui::TreePop();
    }

    if (objectActionVisualizer && ImGui::TreeNode("オブジェクト影響線 (Object Action Links)")) {
        needsRebuild = objectActionVisualizer->DrawImGui() || needsRebuild;
        ImGui::TreePop();
    }

    if (labelVisualizer && ImGui::TreeNode("イベントラベル (Event Labels)")) {
        needsRebuild = labelVisualizer->DrawImGui() || needsRebuild;
        ImGui::TreePop();
    }
    return needsRebuild;
#else
    (void)sceneData;
    (void)selectedObject;
    (void)eventVisualizer;
    (void)connectionVisualizer;
    (void)objectActionVisualizer;
    (void)labelVisualizer;
    return false;
#endif
}
