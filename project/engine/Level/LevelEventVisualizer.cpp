#include "LevelEventVisualizer.h"
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
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

    enum class EventVisualStyle {
        kEnabled,
        kOneShot,
        kDisabled,
        kFired,
    };

    struct EventVisualTransform {
        Vector3 translation{ 0.0f, 0.0f, 0.0f };
        Vector3 rotationDegrees{ 0.0f, 0.0f, 0.0f };
        Vector3 scaling{ 1.0f, 1.0f, 1.0f };
    };

    Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 MultiplyVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z };
    }

    Vector3 ScaleVector3(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    Vector3 AbsVector3(const Vector3& value) {
        return {
            std::fabs(value.x),
            std::fabs(value.y),
            std::fabs(value.z),
        };
    }

    Vector3 DegreesToRadians(const Vector3& degrees) {
        return {
            degrees.x * kDegToRad,
            degrees.y * kDegToRad,
            degrees.z * kDegToRad,
        };
    }

    std::string ToLowerString(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
            });
        return text;
    }

    EventVisualTransform CombineTransform(
        const EventVisualTransform& parent,
        const LevelTransform& local) {
        return {
            AddVector3(parent.translation, MultiplyVector3(local.translation, parent.scaling)),
            AddVector3(parent.rotationDegrees, local.rotation),
            MultiplyVector3(parent.scaling, local.scaling),
        };
    }

    LevelTransform GetObjectLocalTransform(const LevelObject& object, bool axisConversionEnabled) {
        return axisConversionEnabled
            ? BlenderToEngineTransform(object.transform)
            : object.transform;
    }

    Vector3 GetEventSize(const LevelObject& object, bool axisConversionEnabled) {
        const Vector3 size = axisConversionEnabled
            ? BlenderToEngineScale(object.eventFlag.size)
            : object.eventFlag.size;
        return AbsVector3(size);
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

    EventVisualStyle GetVisualStyle(const LevelObject& object) {
        if (!object.eventFlag.initiallyEnabled) {
            return EventVisualStyle::kDisabled;
        }
        if (object.eventFlag.oneShot) {
            return EventVisualStyle::kOneShot;
        }
        return EventVisualStyle::kEnabled;
    }

    const char* GetModelKey(bool sphere, EventVisualStyle style) {
        if (sphere) {
            switch (style) {
            case EventVisualStyle::kOneShot:
                return "LevelEventFlagSphereOneShot";
            case EventVisualStyle::kDisabled:
                return "LevelEventFlagSphereDisabled";
            case EventVisualStyle::kFired:
                return "LevelEventFlagSphereFired";
            case EventVisualStyle::kEnabled:
            default:
                return "LevelEventFlagSphereEnabled";
            }
        }

        switch (style) {
        case EventVisualStyle::kOneShot:
            return "LevelEventFlagBoxOneShot";
        case EventVisualStyle::kDisabled:
            return "LevelEventFlagBoxDisabled";
        case EventVisualStyle::kFired:
            return "LevelEventFlagBoxFired";
        case EventVisualStyle::kEnabled:
        default:
            return "LevelEventFlagBoxEnabled";
        }
    }

    Vector4 GetStyleColor(EventVisualStyle style, float alpha) {
        switch (style) {
        case EventVisualStyle::kOneShot:
            return { 1.0f, 0.72f, 0.18f, alpha };
        case EventVisualStyle::kDisabled:
            return { 0.45f, 0.48f, 0.52f, alpha * 0.65f };
        case EventVisualStyle::kFired:
            return { 0.18f, 1.0f, 0.25f, (std::max)(alpha, 0.45f) };
        case EventVisualStyle::kEnabled:
        default:
            return { 0.18f, 0.85f, 0.95f, alpha };
        }
    }

    Model* GetEventModel(bool sphere, EventVisualStyle style) {
        const char* key = GetModelKey(sphere, style);
        auto modelManager = ModelManager::GetInstance();
        if (Model* model = modelManager->FindModel(key)) {
            return model;
        }
        return sphere
            ? modelManager->CreateSphere(key, 16)
            : modelManager->CreateBox(key);
    }

    void ApplyModelMaterial(Model* model, const Vector4& color) {
        if (!model) {
            return;
        }

        if (Model::Material* material = model->GetMaterialData()) {
            material->color = color;
            material->enableLighting = 0;
            material->alphaReference = 0.0f;
        }
    }

    void ApplyTransform(Object3d& object, const EventVisualTransform& transform) {
        object.SetTranslate(transform.translation);
        object.SetRotate(DegreesToRadians(transform.rotationDegrees));
        object.SetScale(transform.scaling);
    }
}

struct LevelEventVisualObject {
    std::unique_ptr<Object3d> object;
    Model* model = nullptr;
    std::string flagId;
    bool sphere = false;
    EventVisualStyle style = EventVisualStyle::kEnabled;
};

namespace {
    void BuildVisualsRecursive(
        const LevelObject& object,
        const EventVisualTransform& parentTransform,
        bool axisConversionEnabled,
        bool showDisabledEventFlags,
        float scaleMultiplier,
        Object3dCommon* object3dCommon,
        Camera* camera,
        const LevelEventRuntime* runtimeStateProvider,
        std::vector<std::unique_ptr<LevelEventVisualObject>>& visuals) {
        const LevelTransform objectLocal = GetObjectLocalTransform(object, axisConversionEnabled);
        const EventVisualTransform objectWorld = CombineTransform(parentTransform, objectLocal);

        if (object.isEventFlag &&
            object.eventFlag.visibleInEditor &&
            (runtimeStateProvider || object.eventFlag.initiallyEnabled || showDisabledEventFlags)) {
            const EventVisualStyle style = GetVisualStyle(object);
            const bool sphere = IsSphereShape(object.eventFlag.shapeType);
            Model* model = GetEventModel(sphere, style);

            if (model && object3dCommon && camera) {
                EventVisualTransform eventWorld = objectWorld;
                const Vector3 eventSize = GetEventSize(object, axisConversionEnabled);
                eventWorld.scaling = ScaleVector3(
                    MultiplyVector3(AbsVector3(objectWorld.scaling), ScaleVector3(eventSize, 0.5f)),
                    scaleMultiplier);

                auto visual = std::make_unique<LevelEventVisualObject>();
                visual->model = model;
                visual->flagId = GetFlagId(object);
                visual->sphere = sphere;
                visual->style = style;
                visual->object = std::make_unique<Object3d>();
                visual->object->Initialize(object3dCommon);
                visual->object->SetModel(model);
                visual->object->SetCamera(camera);
                visual->object->SetEnvironmentMapEnabled(false);
                ApplyTransform(*visual->object, eventWorld);
                visuals.push_back(std::move(visual));
            }
        }

        for (const LevelObject& child : object.children) {
            BuildVisualsRecursive(
                child,
                objectWorld,
                axisConversionEnabled,
                showDisabledEventFlags,
                scaleMultiplier,
                object3dCommon,
                camera,
                runtimeStateProvider,
                visuals);
        }
    }
}

LevelEventVisualizer::LevelEventVisualizer() = default;

LevelEventVisualizer::~LevelEventVisualizer() = default;

void LevelEventVisualizer::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    object3dCommon_ = object3dCommon;
    camera_ = camera;
}

void LevelEventVisualizer::SetRuntimeStateProvider(const LevelEventRuntime* runtime) {
    runtimeStateProvider_ = runtime;
}

void LevelEventVisualizer::Clear() {
    visuals_.clear();
}

void LevelEventVisualizer::Rebuild(
    const LevelSceneData& sceneData,
    bool axisConversionEnabled,
    uint64_t frameCounter) {
    if (pauseEventVisualRebuild_) {
        return;
    }

    visuals_.clear();
    ++rebuildCount_;
    lastRebuildFrame_ = frameCounter;
    if (!object3dCommon_ || !camera_) {
        return;
    }

    const EventVisualTransform identity{};
    for (const LevelObject& object : sceneData.objects) {
        BuildVisualsRecursive(
            object,
            identity,
            axisConversionEnabled,
            showDisabledEventFlags_,
            eventFlagScaleMultiplier_,
            object3dCommon_,
            camera_,
            runtimeStateProvider_,
            visuals_);
    }
}

void LevelEventVisualizer::Update(uint64_t frameCounter) {
    if (freezeEventVisuals_) {
        return;
    }

    for (const auto& visual : visuals_) {
        if (visual && visual->object) {
            visual->object->Update();
        }
    }
    lastMatrixUpdateFrame_ = frameCounter;
}

void LevelEventVisualizer::Draw(uint64_t frameCounter) {
    if (!showEventFlags_ || !object3dCommon_) {
        return;
    }

    if (updateMatricesWithLatestCamera_) {
        Update(frameCounter);
    }

    object3dCommon_->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    for (const auto& visual : visuals_) {
        if (!visual || !visual->object) {
            continue;
        }

        EventVisualStyle drawStyle = visual->style;
        if (runtimeStateProvider_ && !visual->flagId.empty()) {
            if (runtimeStateProvider_->IsFlagFired(visual->flagId)) {
                if (!runtimeStateProvider_->ShouldShowFiredFlags()) {
                    continue;
                }
                drawStyle = EventVisualStyle::kFired;
            } else if (!runtimeStateProvider_->IsFlagActive(visual->flagId)) {
                if (!showDisabledEventFlags_) {
                    continue;
                }
                drawStyle = EventVisualStyle::kDisabled;
            }
        }

        visual->model = GetEventModel(visual->sphere, drawStyle);
        visual->object->SetModel(visual->model);
        ApplyModelMaterial(visual->model, GetStyleColor(drawStyle, eventFlagAlpha_));
        visual->object->Draw();
    }
}

bool LevelEventVisualizer::DrawImGui() {
#ifdef _DEBUG
    bool needsRebuild = false;
    ImGui::Checkbox("イベントフラグ表示 (Show Event Flags)", &showEventFlags_);
    if (ImGui::SliderFloat("イベントフラグ透明度 (Event Flag Alpha)", &eventFlagAlpha_, 0.05f, 0.85f, "%.2f")) {
        eventFlagAlpha_ = std::clamp(eventFlagAlpha_, 0.05f, 0.85f);
    }
    if (ImGui::SliderFloat("イベントフラグ表示倍率 (Event Flag Scale Multiplier)", &eventFlagScaleMultiplier_, 0.1f, 4.0f, "%.2f")) {
        eventFlagScaleMultiplier_ = std::clamp(eventFlagScaleMultiplier_, 0.1f, 4.0f);
        needsRebuild = true;
    }
    if (ImGui::Checkbox("無効イベントフラグも表示 (Show Disabled Event Flags)", &showDisabledEventFlags_)) {
        needsRebuild = true;
    }
    ImGui::Checkbox("イベント表示を固定 (Freeze Event Visuals)", &freezeEventVisuals_);
    ImGui::Checkbox("イベント表示再構築を一時停止 (Pause Event Visual Rebuild)", &pauseEventVisualRebuild_);
    ImGui::Checkbox("最新カメラで行列更新 (Update Matrices With Latest Camera)", &updateMatricesWithLatestCamera_);
    if (ImGui::Button("イベント表示を再構築 (Rebuild Event Visuals)")) {
        needsRebuild = true;
    }
    ImGui::Text("イベント表示オブジェクト数 (Event Visual Object Count): %zu", visuals_.size());
    ImGui::Text("イベント表示再構築回数 (Event Visual Rebuild Count): %llu", static_cast<unsigned long long>(rebuildCount_));
    ImGui::Text("最後のイベント表示再構築フレーム (Last Event Visual Rebuild Frame): %llu", static_cast<unsigned long long>(lastRebuildFrame_));
    ImGui::Text("最後のイベント表示行列更新フレーム (Last Event Visual Matrix Update Frame): %llu", static_cast<unsigned long long>(lastMatrixUpdateFrame_));
    return needsRebuild;
#else
    return false;
#endif
}

size_t LevelEventVisualizer::GetVisualCount() const {
    return visuals_.size();
}
