#include "LevelEventRuntime.h"
#include "LevelSceneData.h"
#include "LevelTransformConverter.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>
#include <string>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr size_t kMaxEventLogCount = 128;

    struct EventRuntimeWorldTransform {
        Vector3 translation{ 0.0f, 0.0f, 0.0f };
        Vector3 rotationDegrees{ 0.0f, 0.0f, 0.0f };
        Vector3 scaling{ 1.0f, 1.0f, 1.0f };
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

    float LengthSquared(const Vector3& value) {
        return value.x * value.x + value.y * value.y + value.z * value.z;
    }

    float MaxComponent(const Vector3& value) {
        return (std::max)(value.x, (std::max)(value.y, value.z));
    }

    std::string ToLowerString(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
            });
        return text;
    }

    bool IsSphereShape(const std::string& shapeType) {
        return ToLowerString(shapeType) == "sphere";
    }

    std::string GetFlagId(const LevelObject& object) {
        if (!object.eventFlag.id.empty()) {
            return object.eventFlag.id;
        }
        return object.eventFlagId;
    }

    EventRuntimeWorldTransform CombineTransform(
        const EventRuntimeWorldTransform& parent,
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

    Vector3 ConvertEventSize(const Vector3& size, bool axisConversionEnabled) {
        return AbsVector3(axisConversionEnabled ? BlenderToEngineScale(size) : size);
    }

    bool ContainsAabb(const Vector3& point, const Vector3& center, const Vector3& halfExtents) {
        return std::fabs(point.x - center.x) <= halfExtents.x &&
            std::fabs(point.y - center.y) <= halfExtents.y &&
            std::fabs(point.z - center.z) <= halfExtents.z;
    }

    bool ContainsSphere(const Vector3& point, const Vector3& center, float radius) {
        return LengthSquared(SubtractVector3(point, center)) <= radius * radius;
    }

    Model* GetDebugActorModel() {
        auto modelManager = ModelManager::GetInstance();
        if (Model* model = modelManager->FindModel("LevelEventDebugActorSphere")) {
            return model;
        }
        return modelManager->CreateSphere("LevelEventDebugActorSphere", 12);
    }

    void ApplyDebugActorMaterial(Model* model) {
        if (!model) {
            return;
        }
        if (Model::Material* material = model->GetMaterialData()) {
            material->color = { 0.25f, 1.0f, 0.25f, 1.0f };
            material->enableLighting = 0;
            material->alphaReference = 0.0f;
        }
    }
}

struct LevelEventRuntimeFlagState {
    std::string id;
    std::string displayName;
    std::string triggerType;
    std::string shapeType;
    Vector3 center{ 0.0f, 0.0f, 0.0f };
    Vector3 halfExtents{ 0.5f, 0.5f, 0.5f };
    float radius = 0.5f;
    bool oneShot = true;
    bool initiallyEnabled = true;
    bool active = true;
    bool fired = false;
    bool wasInside = false;
    std::vector<std::string> nextFlagIds;
    std::vector<LevelEventObjectAction> objectActions;
};

namespace {
    void CollectFlagsRecursive(
        const LevelObject& object,
        const EventRuntimeWorldTransform& parentTransform,
        bool axisConversionEnabled,
        std::vector<std::unique_ptr<LevelEventRuntimeFlagState>>& flags) {
        const EventRuntimeWorldTransform objectWorld = CombineTransform(
            parentTransform,
            ConvertTransform(object.transform, axisConversionEnabled));

        if (object.isEventFlag) {
            const Vector3 eventSize = ConvertEventSize(object.eventFlag.size, axisConversionEnabled);
            const Vector3 worldScale = AbsVector3(objectWorld.scaling);
            const Vector3 halfExtents = ScaleVector3(MultiplyVector3(worldScale, eventSize), 0.5f);

            auto flag = std::make_unique<LevelEventRuntimeFlagState>();
            flag->id = GetFlagId(object);
            flag->displayName = object.eventFlag.displayName;
            flag->triggerType = object.eventFlag.triggerType;
            flag->shapeType = object.eventFlag.shapeType;
            flag->center = objectWorld.translation;
            flag->halfExtents = {
                (std::max)(halfExtents.x, 0.001f),
                (std::max)(halfExtents.y, 0.001f),
                (std::max)(halfExtents.z, 0.001f),
            };
            flag->radius = (std::max)(MaxComponent(flag->halfExtents), 0.001f);
            flag->oneShot = object.eventFlag.oneShot;
            flag->initiallyEnabled = object.eventFlag.initiallyEnabled;
            flag->active = object.eventFlag.initiallyEnabled;
            flag->nextFlagIds = object.eventFlag.nextFlagIds;
            flag->objectActions = object.eventFlag.objectActions;
            flags.push_back(std::move(flag));
        }

        for (const LevelObject& child : object.children) {
            CollectFlagsRecursive(child, objectWorld, axisConversionEnabled, flags);
        }
    }
}

LevelEventRuntime::LevelEventRuntime() = default;

LevelEventRuntime::~LevelEventRuntime() = default;

void LevelEventRuntime::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    object3dCommon_ = object3dCommon;
    camera_ = camera;
    EnsureDebugActorVisual();
}

void LevelEventRuntime::Clear() {
    flags_.clear();
    flagIndexById_.clear();
    eventLog_.clear();
}

void LevelEventRuntime::Rebuild(const LevelSceneData& sceneData, bool axisConversionEnabled) {
    std::unordered_map<std::string, LevelEventRuntimeFlagState> previousStates;
    for (const auto& flag : flags_) {
        if (flag && !flag->id.empty()) {
            previousStates[flag->id] = *flag;
        }
    }

    flags_.clear();
    flagIndexById_.clear();
    const EventRuntimeWorldTransform identity{};
    for (const LevelObject& object : sceneData.objects) {
        CollectFlagsRecursive(object, identity, axisConversionEnabled, flags_);
    }

    for (size_t index = 0; index < flags_.size(); ++index) {
        LevelEventRuntimeFlagState& flag = *flags_[index];
        if (!flag.id.empty()) {
            const auto previousIt = previousStates.find(flag.id);
            if (previousIt != previousStates.end()) {
                flag.active = previousIt->second.active;
                flag.fired = previousIt->second.fired;
                flag.wasInside = previousIt->second.wasInside;
            }
            flagIndexById_[flag.id] = index;
        }
    }
}

void LevelEventRuntime::Update(uint64_t frameCounter) {
    lastUpdateFrame_ = frameCounter;
    if (!runtimeEnabled_) {
        UpdateDebugActorVisual(frameCounter);
        return;
    }

    for (const auto& flagPtr : flags_) {
        if (!flagPtr) {
            continue;
        }
        LevelEventRuntimeFlagState& flag = *flagPtr;
        if (!flag.active || (flag.oneShot && flag.fired)) {
            flag.wasInside = false;
            continue;
        }

        const bool inside = IsSphereShape(flag.shapeType)
            ? ContainsSphere(debugActorPosition_, flag.center, flag.radius)
            : ContainsAabb(debugActorPosition_, flag.center, flag.halfExtents);
        if (inside && !flag.wasInside) {
            FireFlag(flag);
        }
        flag.wasInside = inside;
    }

    UpdateDebugActorVisual(frameCounter);
}

void LevelEventRuntime::Draw(uint64_t frameCounter) {
    if (!showDebugActor_ || !object3dCommon_ || !debugActorObject_) {
        return;
    }

    UpdateDebugActorVisual(frameCounter);
    object3dCommon_->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    ApplyDebugActorMaterial(debugActorModel_);
    debugActorObject_->Draw();
}

void LevelEventRuntime::DrawImGui() {
#ifdef _DEBUG
    ImGui::Checkbox("Runtime有効 (Enable Event Runtime)", &runtimeEnabled_);
    ImGui::Checkbox("Debug Actorを表示 (Show Debug Actor)", &showDebugActor_);
    ImGui::Checkbox("発火済みフラグを表示 (Show Fired Flags)", &showFiredFlags_);
    ImGui::DragFloat3("Debug Actor Position", &debugActorPosition_.x, 0.05f);
    if (ImGui::Button("Reset Runtime State")) {
        ResetRuntimeState();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Log")) {
        eventLog_.clear();
    }

    ImGui::Text("Fired Event Count: %zu", GetFiredEventCount());
    ImGui::Text("Active Flag Count: %zu", GetActiveFlagCount());
    ImGui::Text("Event Flag Runtime Count: %zu", flags_.size());
    ImGui::Text("Last Runtime Update Frame: %llu", static_cast<unsigned long long>(lastUpdateFrame_));
    ImGui::Text("Last Debug Actor Matrix Update Frame: %llu", static_cast<unsigned long long>(lastMatrixUpdateFrame_));

    if (ImGui::TreeNode("Event Log")) {
        if (eventLog_.empty()) {
            ImGui::TextDisabled("ログはまだありません。 (No event log yet.)");
        } else {
            for (const std::string& line : eventLog_) {
                ImGui::TextWrapped("%s", line.c_str());
            }
        }
        ImGui::TreePop();
    }
#endif
}

bool LevelEventRuntime::IsFlagActive(const std::string& flagId) const {
    const auto it = flagIndexById_.find(flagId);
    if (it == flagIndexById_.end() || it->second >= flags_.size() || !flags_[it->second]) {
        return false;
    }
    return flags_[it->second]->active;
}

bool LevelEventRuntime::IsFlagFired(const std::string& flagId) const {
    const auto it = flagIndexById_.find(flagId);
    if (it == flagIndexById_.end() || it->second >= flags_.size() || !flags_[it->second]) {
        return false;
    }
    return flags_[it->second]->fired;
}

size_t LevelEventRuntime::GetFiredEventCount() const {
    size_t count = 0;
    for (const auto& flag : flags_) {
        if (flag && flag->fired) {
            ++count;
        }
    }
    return count;
}

size_t LevelEventRuntime::GetActiveFlagCount() const {
    size_t count = 0;
    for (const auto& flag : flags_) {
        if (flag && flag->active) {
            ++count;
        }
    }
    return count;
}

void LevelEventRuntime::ResetRuntimeState() {
    for (const auto& flag : flags_) {
        if (!flag) {
            continue;
        }
        flag->active = flag->initiallyEnabled;
        flag->fired = false;
        flag->wasInside = false;
    }
    AddLog("Runtime state reset.");
}

void LevelEventRuntime::FireFlag(LevelEventRuntimeFlagState& flag) {
    flag.fired = true;
    if (flag.oneShot) {
        flag.active = false;
    }

    AddLog(
        "Fired EventFlag id=" + flag.id +
        " name=" + (flag.displayName.empty() ? "(none)" : flag.displayName) +
        " trigger=" + (flag.triggerType.empty() ? "(none)" : flag.triggerType));

    for (const LevelEventObjectAction& action : flag.objectActions) {
        AddLog(
            "  ObjectAction target=" +
            (action.targetObjectName.empty() ? action.targetObjectId : action.targetObjectName) +
            " actionType=" + action.actionType +
            " description=" + action.actionDescription);
    }

    for (const std::string& nextFlagId : flag.nextFlagIds) {
        EnableFlag(nextFlagId, flag.id);
    }
}

void LevelEventRuntime::EnableFlag(const std::string& flagId, const std::string& sourceFlagId) {
    const auto it = flagIndexById_.find(flagId);
    if (it == flagIndexById_.end() || it->second >= flags_.size() || !flags_[it->second]) {
        AddLog("  Missing nextFlagId from " + sourceFlagId + " -> " + flagId);
        return;
    }

    LevelEventRuntimeFlagState& target = *flags_[it->second];
    if (!target.active) {
        target.active = true;
        AddLog("  Enabled next EventFlag " + flagId + " from " + sourceFlagId);
    } else {
        AddLog("  next EventFlag already active " + flagId + " from " + sourceFlagId);
    }
}

void LevelEventRuntime::AddLog(const std::string& message) {
    eventLog_.push_back(message);
    if (eventLog_.size() > kMaxEventLogCount) {
        eventLog_.erase(eventLog_.begin(), eventLog_.begin() + (eventLog_.size() - kMaxEventLogCount));
    }
}

void LevelEventRuntime::UpdateDebugActorVisual(uint64_t frameCounter) {
    EnsureDebugActorVisual();
    if (!debugActorObject_) {
        return;
    }

    debugActorObject_->SetTranslate(debugActorPosition_);
    debugActorObject_->SetScale({ debugActorRadius_, debugActorRadius_, debugActorRadius_ });
    debugActorObject_->Update();
    lastMatrixUpdateFrame_ = frameCounter;
}

void LevelEventRuntime::EnsureDebugActorVisual() {
    if (!object3dCommon_ || !camera_) {
        return;
    }
    if (!debugActorModel_) {
        debugActorModel_ = GetDebugActorModel();
    }
    if (!debugActorObject_ && debugActorModel_) {
        debugActorObject_ = std::make_unique<Object3d>();
        debugActorObject_->Initialize(object3dCommon_);
        debugActorObject_->SetModel(debugActorModel_);
        debugActorObject_->SetCamera(camera_);
        debugActorObject_->SetEnvironmentMapEnabled(false);
    }
}
