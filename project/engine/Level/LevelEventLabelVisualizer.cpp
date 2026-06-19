#include "LevelEventLabelVisualizer.h"
#include "LevelSceneData.h"
#include "LevelTransformConverter.h"
#include "Engine/Graphics/Camera/Camera.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

struct LevelEventLabelItem {
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    std::string text;
    bool isAction = false;
};

namespace {
    struct EventLabelSource {
        std::string flagId;
        Vector3 position{ 0.0f, 0.0f, 0.0f };
        std::vector<LevelEventObjectAction> objectActions;
    };

    struct ObjectLabelTarget {
        std::string objectId;
        std::string name;
        Vector3 position{ 0.0f, 0.0f, 0.0f };
    };

    struct EventLabelWorldTransform {
        Vector3 translation{ 0.0f, 0.0f, 0.0f };
        Vector3 rotationDegrees{ 0.0f, 0.0f, 0.0f };
        Vector3 scaling{ 1.0f, 1.0f, 1.0f };
    };

    struct LabelViewportRect {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    struct LabelScreenPoint {
        float x = 0.0f;
        float y = 0.0f;
    };

    enum class LabelProjectionResult {
        Visible,
        Clipped,
        Failed,
    };

    Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 SubtractVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    }

    Vector3 MultiplyVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z };
    }

    Vector3 ScaleVector3(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    Vector3 AbsVector3(const Vector3& value) {
        return { std::fabs(value.x), std::fabs(value.y), std::fabs(value.z) };
    }

    float Length(const Vector3& value) {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    }

    Vector4 TransformPoint(const Vector3& value, const Matrix4x4& matrix) {
        return {
            value.x * matrix.m[0][0] + value.y * matrix.m[1][0] + value.z * matrix.m[2][0] + matrix.m[3][0],
            value.x * matrix.m[0][1] + value.y * matrix.m[1][1] + value.z * matrix.m[2][1] + matrix.m[3][1],
            value.x * matrix.m[0][2] + value.y * matrix.m[1][2] + value.z * matrix.m[2][2] + matrix.m[3][2],
            value.x * matrix.m[0][3] + value.y * matrix.m[1][3] + value.z * matrix.m[2][3] + matrix.m[3][3],
        };
    }

    EventLabelWorldTransform CombineTransform(
        const EventLabelWorldTransform& parent,
        const LevelTransform& local) {
        return {
            AddVector3(parent.translation, MultiplyVector3(local.translation, parent.scaling)),
            AddVector3(parent.rotationDegrees, local.rotation),
            MultiplyVector3(parent.scaling, local.scaling),
        };
    }

    LevelTransform ConvertTransform(const LevelTransform& transform, bool axisConversionEnabled) {
        return axisConversionEnabled ? BlenderToEngineTransform(transform) : transform;
    }

    Vector3 ConvertScale(const Vector3& scale, bool axisConversionEnabled) {
        return AbsVector3(axisConversionEnabled ? BlenderToEngineScale(scale) : scale);
    }

    std::string GetFlagId(const LevelObject& object) {
        if (!object.eventFlag.id.empty()) {
            return object.eventFlag.id;
        }
        return object.eventFlagId;
    }

    void AppendLine(std::string& text, const std::string& line) {
        if (line.empty()) {
            return;
        }
        if (!text.empty()) {
            text += "\n";
        }
        text += line;
    }

    std::string BuildEventText(const LevelObject& object, bool showName, bool showDescription) {
        std::string text;
        if (showName) {
            AppendLine(text, object.eventFlag.displayName.empty() ? GetFlagId(object) : object.eventFlag.displayName);
        }
        if (showDescription) {
            AppendLine(text, "Trigger: " + object.eventFlag.triggerType);
            AppendLine(text, object.eventFlag.description);
        }
        return text;
    }

    std::string BuildActionText(
        const LevelEventObjectAction& action,
        const ObjectLabelTarget& target,
        bool showActionDescription) {
        if (!showActionDescription) {
            return {};
        }

        std::string text;
        AppendLine(text, action.actionType);
        AppendLine(text, action.actionDescription);
        AppendLine(text, "-> " + (target.name.empty() ? action.targetObjectName : target.name));
        return text;
    }

    void CollectLabelsRecursive(
        const LevelObject& object,
        const EventLabelWorldTransform& parentTransform,
        bool axisConversionEnabled,
        bool showEventNames,
        bool showEventDescriptions,
        std::vector<EventLabelSource>& actionSources,
        std::vector<ObjectLabelTarget>& actionTargets,
        std::vector<std::unique_ptr<LevelEventLabelItem>>& labels) {
        const EventLabelWorldTransform objectWorld = CombineTransform(
            parentTransform,
            ConvertTransform(object.transform, axisConversionEnabled));

        actionTargets.push_back({ object.objectId, object.name, objectWorld.translation });
        if (object.isEventFlag) {
            actionSources.push_back({ GetFlagId(object), objectWorld.translation, object.eventFlag.objectActions });

            const std::string labelText = BuildEventText(object, showEventNames, showEventDescriptions);
            if (!labelText.empty()) {
                const Vector3 eventSize = ConvertScale(object.eventFlag.size, axisConversionEnabled);
                const Vector3 objectScale = AbsVector3(objectWorld.scaling);
                const float yOffset = (objectScale.y * eventSize.y * 0.5f) + 0.35f;

                auto label = std::make_unique<LevelEventLabelItem>();
                label->position = AddVector3(objectWorld.translation, { 0.0f, yOffset, 0.0f });
                label->text = labelText;
                label->isAction = false;
                labels.push_back(std::move(label));
            }
        }

        for (const LevelObject& child : object.children) {
            CollectLabelsRecursive(
                child,
                objectWorld,
                axisConversionEnabled,
                showEventNames,
                showEventDescriptions,
                actionSources,
                actionTargets,
                labels);
        }
    }

    LabelProjectionResult ProjectToScreen(
        const Vector3& worldPosition,
        const Camera* camera,
        const LabelViewportRect& viewportRect,
        LabelScreenPoint& outScreen) {
        if (!camera || viewportRect.width <= 1.0f || viewportRect.height <= 1.0f) {
            return LabelProjectionResult::Failed;
        }

        const Vector4 clipPosition = TransformPoint(worldPosition, camera->GetViewProjectionMatrix());
        if (clipPosition.w <= 0.0001f) {
            return LabelProjectionResult::Failed;
        }

        const float invW = 1.0f / clipPosition.w;
        const float ndcX = clipPosition.x * invW;
        const float ndcY = clipPosition.y * invW;
        const float ndcZ = clipPosition.z * invW;
        if (ndcZ < 0.0f || ndcZ > 1.0f) {
            return LabelProjectionResult::Failed;
        }
        if (ndcX < -1.0f || ndcX > 1.0f || ndcY < -1.0f || ndcY > 1.0f) {
            return LabelProjectionResult::Clipped;
        }

        outScreen.x = viewportRect.x + ((ndcX * 0.5f) + 0.5f) * viewportRect.width;
        outScreen.y = viewportRect.y + ((-ndcY * 0.5f) + 0.5f) * viewportRect.height;
        return LabelProjectionResult::Visible;
    }
}

LevelEventLabelVisualizer::LevelEventLabelVisualizer() = default;

LevelEventLabelVisualizer::~LevelEventLabelVisualizer() = default;

void LevelEventLabelVisualizer::Initialize(Camera* camera) {
    camera_ = camera;
}

void LevelEventLabelVisualizer::Clear() {
    labels_.clear();
}

void LevelEventLabelVisualizer::Rebuild(const LevelSceneData& sceneData, bool axisConversionEnabled) {
    Clear();

    std::vector<EventLabelSource> actionSources;
    std::vector<ObjectLabelTarget> actionTargets;
    const EventLabelWorldTransform identity{};
    for (const LevelObject& object : sceneData.objects) {
        CollectLabelsRecursive(
            object,
            identity,
            axisConversionEnabled,
            showEventNames_,
            showEventDescriptions_,
            actionSources,
            actionTargets,
            labels_);
    }

    std::unordered_map<std::string, const ObjectLabelTarget*> targetById;
    std::unordered_map<std::string, const ObjectLabelTarget*> targetByName;
    for (const ObjectLabelTarget& target : actionTargets) {
        if (!target.objectId.empty()) {
            targetById[target.objectId] = &target;
        }
        if (!target.name.empty()) {
            targetByName[target.name] = &target;
        }
    }

    for (const EventLabelSource& source : actionSources) {
        for (const LevelEventObjectAction& action : source.objectActions) {
            const ObjectLabelTarget* target = nullptr;
            if (!action.targetObjectId.empty()) {
                const auto targetIt = targetById.find(action.targetObjectId);
                target = targetIt == targetById.end() ? nullptr : targetIt->second;
            } else if (!action.targetObjectName.empty()) {
                const auto targetIt = targetByName.find(action.targetObjectName);
                target = targetIt == targetByName.end() ? nullptr : targetIt->second;
            }
            if (!target) {
                continue;
            }

            const std::string labelText = BuildActionText(action, *target, showActionDescriptions_);
            if (labelText.empty()) {
                continue;
            }

            auto label = std::make_unique<LevelEventLabelItem>();
            label->position = AddVector3(ScaleVector3(AddVector3(source.position, target->position), 0.5f), { 0.0f, 0.2f, 0.0f });
            label->text = labelText;
            label->isAction = true;
            labels_.push_back(std::move(label));
        }
    }
}

void LevelEventLabelVisualizer::SetViewportRect(float x, float y, float width, float height) {
    viewportX_ = x;
    viewportY_ = y;
    viewportWidth_ = width;
    viewportHeight_ = height;
    hasViewportRect_ = width > 1.0f && height > 1.0f;
}

void LevelEventLabelVisualizer::ClearViewportRect() {
    hasViewportRect_ = false;
    viewportX_ = 0.0f;
    viewportY_ = 0.0f;
    viewportWidth_ = 0.0f;
    viewportHeight_ = 0.0f;
    lastVisibleCount_ = 0;
    lastClippedCount_ = 0;
    lastProjectionFailedCount_ = 0;
}

void LevelEventLabelVisualizer::DrawOverlay() const {
#ifdef USE_IMGUI
    lastVisibleCount_ = 0;
    lastClippedCount_ = 0;
    lastProjectionFailedCount_ = 0;

    if (!showEventLabels_ || !camera_) {
        return;
    }
    if (useGameViewRectForLabels_ && !hasViewportRect_) {
        return;
    }

    const LabelViewportRect labelViewport = useGameViewRectForLabels_
        ? LabelViewportRect{ viewportX_, viewportY_, viewportWidth_, viewportHeight_ }
        : LabelViewportRect{ 0.0f, 0.0f, ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y };
    if (labelViewport.width <= 1.0f || labelViewport.height <= 1.0f) {
        return;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);
    ImFont* font = ImGui::GetFont();
    const Vector3 cameraPosition = camera_->GetTranslate();
    const ImU32 shadowColor = IM_COL32(0, 0, 0, 210);
    const ImVec2 clipMin(labelViewport.x, labelViewport.y);
    const ImVec2 clipMax(labelViewport.x + labelViewport.width, labelViewport.y + labelViewport.height);

    drawList->PushClipRect(clipMin, clipMax, true);

    for (const auto& label : labels_) {
        if (!label || label->text.empty()) {
            continue;
        }
        if (eventLabelMaxDistance_ > 0.0f &&
            Length(SubtractVector3(label->position, cameraPosition)) > eventLabelMaxDistance_) {
            ++lastClippedCount_;
            continue;
        }

        LabelScreenPoint projectedScreen{};
        const LabelProjectionResult projectionResult = ProjectToScreen(label->position, camera_, labelViewport, projectedScreen);
        ImVec2 screen(projectedScreen.x, projectedScreen.y);
        if (projectionResult == LabelProjectionResult::Failed) {
            ++lastProjectionFailedCount_;
            continue;
        }
        if (projectionResult == LabelProjectionResult::Clipped) {
            ++lastClippedCount_;
            continue;
        }

        const auto& color = label->isAction ? actionLabelColor_ : eventLabelColor_;
        const ImU32 textColor = ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], color[3]));
        const ImVec2 textSize = font->CalcTextSizeA(eventLabelFontSize_, std::numeric_limits<float>::max(), 0.0f, label->text.c_str());
        const ImVec2 textPos(screen.x - (textSize.x * 0.5f), screen.y - textSize.y);
        if (textPos.x > clipMax.x || textPos.y > clipMax.y ||
            textPos.x + textSize.x < clipMin.x || textPos.y + textSize.y < clipMin.y) {
            ++lastClippedCount_;
            continue;
        }

        drawList->AddText(font, eventLabelFontSize_, ImVec2(textPos.x + 1.0f, textPos.y + 1.0f), shadowColor, label->text.c_str());
        drawList->AddText(font, eventLabelFontSize_, textPos, textColor, label->text.c_str());
        if (showLabelDebugPoints_) {
            drawList->AddCircleFilled(screen, 3.0f, IM_COL32(255, 255, 0, 230));
        }
        ++lastVisibleCount_;
    }

    drawList->PopClipRect();
#endif
}

bool LevelEventLabelVisualizer::DrawImGui() {
#ifdef USE_IMGUI
    bool needsRebuild = false;
    ImGui::Checkbox("イベントラベルを表示 (Show Event Labels)", &showEventLabels_);
    if (ImGui::Checkbox("イベント名を表示 (Show Event Names)", &showEventNames_)) {
        needsRebuild = true;
    }
    if (ImGui::Checkbox("イベント説明を表示 (Show Event Descriptions)", &showEventDescriptions_)) {
        needsRebuild = true;
    }
    if (ImGui::Checkbox("アクション説明を表示 (Show Action Descriptions)", &showActionDescriptions_)) {
        needsRebuild = true;
    }
    ImGui::Checkbox("Game View矩形を使う (Use Game View Rect For Labels)", &useGameViewRectForLabels_);
    ImGui::Checkbox("ラベルデバッグ点を表示 (Show Label Debug Points)", &showLabelDebugPoints_);
    ImGui::SliderFloat("ラベル文字サイズ (Event Label Font Size)", &eventLabelFontSize_, 10.0f, 28.0f, "%.1f");
    ImGui::SliderFloat("ラベル表示距離 (Event Label Max Distance)", &eventLabelMaxDistance_, 5.0f, 300.0f, "%.1f");
    ImGui::ColorEdit4("ラベル色 (Event Label Color)", eventLabelColor_.data());
    ImGui::ColorEdit4("アクションラベル色 (Action Label Color)", actionLabelColor_.data());
    ImGui::Text("イベントラベル数 (Event Label Count): %zu", labels_.size());
    ImGui::Text(
        "ラベル表示矩形 (Label Viewport Rect): %s x=%.1f y=%.1f w=%.1f h=%.1f",
        hasViewportRect_ ? "set" : "unset",
        viewportX_,
        viewportY_,
        viewportWidth_,
        viewportHeight_);
    ImGui::Text("表示ラベル数 (Label Visible Count): %zu", lastVisibleCount_);
    ImGui::Text("クリップされたラベル数 (Label Clipped Count): %zu", lastClippedCount_);
    ImGui::Text("投影失敗ラベル数 (Label Projection Failed Count): %zu", lastProjectionFailedCount_);
    return needsRebuild;
#else
    return false;
#endif
}

size_t LevelEventLabelVisualizer::GetLabelCount() const {
    return labels_.size();
}

