#include "LevelEventConnectionVisualizer.h"
#include "LevelSceneData.h"
#include "LevelTransformConverter.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kLineThickness = 0.035f;
    constexpr float kMinLineLength = 0.0001f;

    struct EventFlagEndpoint {
        std::string id;
        Vector3 position{ 0.0f, 0.0f, 0.0f };
        std::vector<std::string> nextFlagIds;
    };

    struct EventConnectionTransform {
        Vector3 translation{ 0.0f, 0.0f, 0.0f };
        Vector3 rotationRadians{ 0.0f, 0.0f, 0.0f };
        Vector3 scaling{ 1.0f, 1.0f, 1.0f };
    };

    struct EventConnectionWorldTransform {
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

    float Length(const Vector3& value) {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    }

    Vector3 Normalize(const Vector3& value) {
        const float length = Length(value);
        if (length <= kMinLineLength) {
            return { 0.0f, 0.0f, 1.0f };
        }
        return { value.x / length, value.y / length, value.z / length };
    }

    EventConnectionWorldTransform CombineTransform(
        const EventConnectionWorldTransform& parent,
        const LevelTransform& local) {
        return {
            AddVector3(parent.translation, MultiplyVector3(local.translation, parent.scaling)),
            AddVector3(parent.rotationDegrees, local.rotation),
            MultiplyVector3(parent.scaling, local.scaling),
        };
    }

    LevelTransform MakeEventLocalTransform(const LevelObject& object) {
        return {
            object.eventFlag.position,
            object.eventFlag.rotation,
            object.eventFlag.scale,
        };
    }

    LevelTransform ConvertTransform(const LevelTransform& transform, bool axisConversionEnabled) {
        return axisConversionEnabled ? BlenderToEngineTransform(transform) : transform;
    }

    std::string GetFlagId(const LevelObject& object) {
        if (!object.eventFlag.id.empty()) {
            return object.eventFlag.id;
        }
        return object.eventFlagId;
    }

    void CollectEndpointsRecursive(
        const LevelObject& object,
        const EventConnectionWorldTransform& parentTransform,
        bool axisConversionEnabled,
        std::vector<EventFlagEndpoint>& endpoints) {
        const EventConnectionWorldTransform objectWorld = CombineTransform(
            parentTransform,
            ConvertTransform(object.transform, axisConversionEnabled));

        if (object.isEventFlag) {
            const EventConnectionWorldTransform eventWorld = CombineTransform(
                parentTransform,
                ConvertTransform(MakeEventLocalTransform(object), axisConversionEnabled));
            endpoints.push_back({
                GetFlagId(object),
                eventWorld.translation,
                object.eventFlag.nextFlagIds,
                });
        }

        for (const LevelObject& child : object.children) {
            CollectEndpointsRecursive(child, objectWorld, axisConversionEnabled, endpoints);
        }
    }

    EventConnectionTransform MakeLineTransform(const Vector3& from, const Vector3& to) {
        const Vector3 diff = SubtractVector3(to, from);
        const float length = Length(diff);
        const Vector3 direction = Normalize(diff);
        const float yaw = std::atan2(direction.x, direction.z);
        const float horizontal = std::sqrt(direction.x * direction.x + direction.z * direction.z);
        const float pitch = std::atan2(-direction.y, horizontal);

        return {
            ScaleVector3(AddVector3(from, to), 0.5f),
            { pitch, yaw, 0.0f },
            { kLineThickness, kLineThickness, length * 0.5f },
        };
    }

    Model* GetLineModel() {
        auto modelManager = ModelManager::GetInstance();
        if (Model* model = modelManager->FindModel("LevelEventFlagLinkLine")) {
            return model;
        }
        return modelManager->CreateBox("LevelEventFlagLinkLine");
    }

    void ApplyModelMaterial(Model* model, const std::array<float, 4>& color) {
        if (!model) {
            return;
        }

        if (Model::Material* material = model->GetMaterialData()) {
            material->color = { color[0], color[1], color[2], color[3] };
            material->enableLighting = 0;
            material->alphaReference = 0.0f;
        }
    }

    void ApplyTransform(Object3d& object, const EventConnectionTransform& transform) {
        object.SetTranslate(transform.translation);
        object.SetRotate(transform.rotationRadians);
        object.SetScale(transform.scaling);
    }
}

struct LevelEventConnectionVisualObject {
    std::unique_ptr<Object3d> object;
    Model* model = nullptr;
};

LevelEventConnectionVisualizer::LevelEventConnectionVisualizer() = default;

LevelEventConnectionVisualizer::~LevelEventConnectionVisualizer() = default;

void LevelEventConnectionVisualizer::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    object3dCommon_ = object3dCommon;
    camera_ = camera;
}

void LevelEventConnectionVisualizer::Clear() {
    links_.clear();
    missingFlagLinkCount_ = 0;
}

void LevelEventConnectionVisualizer::Rebuild(const LevelSceneData& sceneData, bool axisConversionEnabled) {
    Clear();
    if (!object3dCommon_ || !camera_) {
        return;
    }

    std::vector<EventFlagEndpoint> endpoints;
    const EventConnectionWorldTransform identity{};
    for (const LevelObject& object : sceneData.objects) {
        CollectEndpointsRecursive(object, identity, axisConversionEnabled, endpoints);
    }

    std::unordered_map<std::string, const EventFlagEndpoint*> endpointById;
    for (const EventFlagEndpoint& endpoint : endpoints) {
        if (!endpoint.id.empty()) {
            endpointById[endpoint.id] = &endpoint;
        }
    }

    Model* lineModel = GetLineModel();
    if (!lineModel) {
        return;
    }

    for (const EventFlagEndpoint& source : endpoints) {
        for (const std::string& targetId : source.nextFlagIds) {
            const auto targetIt = endpointById.find(targetId);
            if (targetIt == endpointById.end()) {
                ++missingFlagLinkCount_;
                continue;
            }

            const Vector3 diff = SubtractVector3(targetIt->second->position, source.position);
            if (Length(diff) <= kMinLineLength) {
                continue;
            }

            auto link = std::make_unique<LevelEventConnectionVisualObject>();
            link->model = lineModel;
            link->object = std::make_unique<Object3d>();
            link->object->Initialize(object3dCommon_);
            link->object->SetModel(lineModel);
            link->object->SetCamera(camera_);
            link->object->SetEnvironmentMapEnabled(false);
            ApplyTransform(*link->object, MakeLineTransform(source.position, targetIt->second->position));
            links_.push_back(std::move(link));
        }
    }
}

void LevelEventConnectionVisualizer::Update() {
    for (const auto& link : links_) {
        if (link && link->object) {
            link->object->Update();
        }
    }
}

void LevelEventConnectionVisualizer::Draw() {
    if (!showFlagLinks_ || !object3dCommon_) {
        return;
    }

    object3dCommon_->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    for (const auto& link : links_) {
        if (!link || !link->object) {
            continue;
        }

        ApplyModelMaterial(link->model, flagLinkColor_);
        link->object->Draw();
    }
}

bool LevelEventConnectionVisualizer::DrawImGui() {
#ifdef _DEBUG
    bool needsRebuild = false;
    ImGui::Checkbox("フラグ接続線を表示 (Show Flag Links)", &showFlagLinks_);
    ImGui::ColorEdit4("フラグ接続線の色 (Flag Link Color)", flagLinkColor_.data());
    ImGui::Text("フラグ接続線数 (Flag Link Count): %zu", links_.size());
    ImGui::Text("不明なフラグ接続数 (Missing Flag Link Count): %zu", missingFlagLinkCount_);
    if (ImGui::Button("フラグ接続線を再構築 (Rebuild Flag Links)")) {
        needsRebuild = true;
    }
    return needsRebuild;
#else
    return false;
#endif
}

size_t LevelEventConnectionVisualizer::GetLinkCount() const {
    return links_.size();
}
