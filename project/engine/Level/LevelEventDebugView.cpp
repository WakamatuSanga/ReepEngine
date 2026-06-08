#include "LevelEventDebugView.h"
#include "LevelEventConnectionVisualizer.h"
#include "LevelEventData.h"
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
    ImGui::Text("objectId: %s", object.objectId.empty() ? "(none)" : object.objectId.c_str());
    ImGui::Text("editorLabel: %s", object.editorLabel.empty() ? "(none)" : object.editorLabel.c_str());
    ImGui::Text(
        "editorDescription: %s",
        object.editorDescription.empty() ? "(none)" : object.editorDescription.c_str());
    ImGui::Text("isEventFlag: %s", object.isEventFlag ? "true" : "false");
    ImGui::Text("eventFlagId: %s", object.eventFlagId.empty() ? "(none)" : object.eventFlagId.c_str());

    if (!object.isEventFlag) {
        return;
    }

    ImGui::Separator();
    ImGui::Text("event.displayName: %s", object.eventFlag.displayName.c_str());
    ImGui::TextWrapped("event.description: %s", object.eventFlag.description.c_str());
    ImGui::Text("event.triggerType: %s", object.eventFlag.triggerType.c_str());
    ImGui::Text("event.shapeType: %s", object.eventFlag.shapeType.c_str());
    ImGui::Text("event.oneShot: %s", object.eventFlag.oneShot ? "true" : "false");
    ImGui::Text("event.initiallyEnabled: %s", object.eventFlag.initiallyEnabled ? "true" : "false");
    ImGui::Text("event.visibleInEditor: %s", object.eventFlag.visibleInEditor ? "true" : "false");
    ImGui::Text("event.size: %s", FormatVector3(object.eventFlag.size).c_str());

    if (object.eventFlag.nextFlagIds.empty()) {
        ImGui::TextUnformatted("event.nextFlagIds: (none)");
    } else if (ImGui::TreeNode("event.nextFlagIds")) {
        for (const std::string& nextFlagId : object.eventFlag.nextFlagIds) {
            ImGui::BulletText("%s", nextFlagId.c_str());
        }
        ImGui::TreePop();
    }

    if (object.eventFlag.objectActions.empty()) {
        ImGui::TextUnformatted("event.objectActions: (none)");
    } else if (ImGui::TreeNode("event.objectActions")) {
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
    LevelEventConnectionVisualizer* connectionVisualizer) {
#ifdef _DEBUG
    bool needsRebuild = false;
    const LevelEventValidationResult validation = ValidateLevelEventLinks(sceneData);
    ImGui::Text("Event Flag Count: %zu", CountEventFlags(sceneData));
    ImGui::Text("Missing Link Count: %zu", validation.GetMissingLinkCount());

    if (ImGui::TreeNode("Selected Event Flag")) {
        if (selectedObject && selectedObject->isEventFlag) {
            DrawLevelObjectEventDetailsImGui(*selectedObject);
        } else {
            ImGui::TextDisabled("Select an EventFlag object in Object Tree.");
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Link Validation")) {
        if (validation.missingFlagLinks.empty()) {
            ImGui::TextUnformatted("Missing Flag Links: none");
        } else if (ImGui::TreeNode("Missing Flag Links")) {
            for (const LevelEventFlagLink& link : validation.missingFlagLinks) {
                ImGui::BulletText("%s -> %s", link.sourceFlagId.c_str(), link.targetFlagId.c_str());
            }
            ImGui::TreePop();
        }

        if (validation.missingObjectLinks.empty()) {
            ImGui::TextUnformatted("Missing Object Links: none");
        } else if (ImGui::TreeNode("Missing Object Links")) {
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

    if (eventVisualizer && ImGui::TreeNode("Event Flag Visuals")) {
        needsRebuild = eventVisualizer->DrawImGui() || needsRebuild;
        ImGui::TreePop();
    }

    if (connectionVisualizer && ImGui::TreeNode("Flag Links")) {
        needsRebuild = connectionVisualizer->DrawImGui() || needsRebuild;
        ImGui::TreePop();
    }
    return needsRebuild;
#else
    (void)sceneData;
    (void)selectedObject;
    (void)eventVisualizer;
    (void)connectionVisualizer;
    return false;
#endif
}
