#include "LevelEventObjectActionVisualizer.h"
#include "LevelSceneData.h"
#include "LevelTransformConverter.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kLineThickness = 0.04f;
    constexpr float kMinLineLength = 0.0001f;

    struct EventFlagActionSource {
        std::string id;
        Vector3 position{ 0.0f, 0.0f, 0.0f };
        std::vector<LevelEventObjectAction> objectActions;
    };

    struct EventObjectEndpoint {
        std::string objectId;
        std::string name;
        Vector3 position{ 0.0f, 0.0f, 0.0f };
    };

    struct EventObjectActionTransform {
        Vector3 translation{ 0.0f, 0.0f, 0.0f };
        Vector3 rotationRadians{ 0.0f, 0.0f, 0.0f };
        Vector3 scaling{ 1.0f, 1.0f, 1.0f };
    };

    struct EventObjectWorldTransform {
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

    EventObjectWorldTransform CombineTransform(
        const EventObjectWorldTransform& parent,
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

    std::string GetFlagId(const LevelObject& object) {
        if (!object.eventFlag.id.empty()) {
            return object.eventFlag.id;
        }
        return object.eventFlagId;
    }

    void CollectEndpointsRecursive(
        const LevelObject& object,
        const EventObjectWorldTransform& parentTransform,
        bool axisConversionEnabled,
        std::vector<EventFlagActionSource>& sources,
        std::vector<EventObjectEndpoint>& targets) {
        const EventObjectWorldTransform objectWorld = CombineTransform(
            parentTransform,
            ConvertTransform(object.transform, axisConversionEnabled));

        targets.push_back({ object.objectId, object.name, objectWorld.translation });
        if (object.isEventFlag) {
            sources.push_back({ GetFlagId(object), objectWorld.translation, object.eventFlag.objectActions });
        }

        for (const LevelObject& child : object.children) {
            CollectEndpointsRecursive(child, objectWorld, axisConversionEnabled, sources, targets);
        }
    }

    EventObjectActionTransform MakeLineTransform(const Vector3& from, const Vector3& to) {
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
        if (Model* model = modelManager->FindModel("LevelEventObjectActionLinkLine")) {
            return model;
        }
        return modelManager->CreateBox("LevelEventObjectActionLinkLine");
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

    void ApplyTransform(Object3d& object, const EventObjectActionTransform& transform) {
        object.SetTranslate(transform.translation);
        object.SetRotate(transform.rotationRadians);
        object.SetScale(transform.scaling);
    }
}

struct LevelEventObjectActionVisualObject {
    std::unique_ptr<Object3d> object;
    Model* model = nullptr;
};

LevelEventObjectActionVisualizer::LevelEventObjectActionVisualizer() = default;

LevelEventObjectActionVisualizer::~LevelEventObjectActionVisualizer() = default;

void LevelEventObjectActionVisualizer::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    object3dCommon_ = object3dCommon;
    camera_ = camera;
}

void LevelEventObjectActionVisualizer::Clear() {
    links_.clear();
    missingObjectLinkCount_ = 0;
}

void LevelEventObjectActionVisualizer::Rebuild(
    const LevelSceneData& sceneData,
    bool axisConversionEnabled,
    uint64_t frameCounter) {
    if (pauseObjectActionLinkRebuild_) {
        return;
    }

    Clear();
    ++rebuildCount_;
    lastRebuildFrame_ = frameCounter;
    if (!object3dCommon_ || !camera_) {
        return;
    }

    std::vector<EventFlagActionSource> sources;
    std::vector<EventObjectEndpoint> targets;
    const EventObjectWorldTransform identity{};
    for (const LevelObject& object : sceneData.objects) {
        CollectEndpointsRecursive(object, identity, axisConversionEnabled, sources, targets);
    }

    std::unordered_map<std::string, const EventObjectEndpoint*> targetById;
    std::unordered_map<std::string, const EventObjectEndpoint*> targetByName;
    for (const EventObjectEndpoint& target : targets) {
        if (!target.objectId.empty()) {
            targetById[target.objectId] = &target;
        }
        if (!target.name.empty()) {
            targetByName[target.name] = &target;
        }
    }

    Model* lineModel = GetLineModel();
    if (!lineModel) {
        return;
    }

    for (const EventFlagActionSource& source : sources) {
        for (const LevelEventObjectAction& action : source.objectActions) {
            const EventObjectEndpoint* target = nullptr;
            if (!action.targetObjectId.empty()) {
                const auto targetIt = targetById.find(action.targetObjectId);
                target = targetIt == targetById.end() ? nullptr : targetIt->second;
            } else if (!action.targetObjectName.empty()) {
                const auto targetIt = targetByName.find(action.targetObjectName);
                target = targetIt == targetByName.end() ? nullptr : targetIt->second;
            }

            if (!target) {
                ++missingObjectLinkCount_;
                continue;
            }

            const Vector3 diff = SubtractVector3(target->position, source.position);
            if (Length(diff) <= kMinLineLength) {
                continue;
            }

            auto link = std::make_unique<LevelEventObjectActionVisualObject>();
            link->model = lineModel;
            link->object = std::make_unique<Object3d>();
            link->object->Initialize(object3dCommon_);
            link->object->SetModel(lineModel);
            link->object->SetCamera(camera_);
            link->object->SetEnvironmentMapEnabled(false);
            ApplyTransform(*link->object, MakeLineTransform(source.position, target->position));
            links_.push_back(std::move(link));
        }
    }
}

void LevelEventObjectActionVisualizer::Update(uint64_t frameCounter) {
    for (const auto& link : links_) {
        if (link && link->object) {
            link->object->Update();
        }
    }
    lastMatrixUpdateFrame_ = frameCounter;
}

void LevelEventObjectActionVisualizer::Draw(uint64_t frameCounter) {
    if (!showObjectActionLinks_ || !object3dCommon_) {
        return;
    }

    if (updateMatricesWithLatestCamera_) {
        Update(frameCounter);
    }

    object3dCommon_->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    for (const auto& link : links_) {
        if (!link || !link->object) {
            continue;
        }

        ApplyModelMaterial(link->model, objectActionLinkColor_);
        link->object->Draw();
    }
}

bool LevelEventObjectActionVisualizer::DrawImGui() {
#ifdef _DEBUG
    bool needsRebuild = false;
    ImGui::Checkbox("オブジェクト影響線を表示 (Show Object Action Links)", &showObjectActionLinks_);
    ImGui::ColorEdit4("オブジェクト影響線の色 (Object Action Link Color)", objectActionLinkColor_.data());
    ImGui::Text("オブジェクト影響線数 (Object Action Link Count): %zu", links_.size());
    ImGui::Text("不明なオブジェクト接続数 (Missing Object Link Count): %zu", missingObjectLinkCount_);
    ImGui::Checkbox("オブジェクト影響線再構築を一時停止 (Pause Object Action Link Rebuild)", &pauseObjectActionLinkRebuild_);
    ImGui::Checkbox("最新カメラで影響線行列更新 (Update Matrices With Latest Camera)", &updateMatricesWithLatestCamera_);
    ImGui::Text("オブジェクト影響線再構築回数 (Object Action Link Rebuild Count): %llu", static_cast<unsigned long long>(rebuildCount_));
    ImGui::Text("最後のオブジェクト影響線再構築フレーム (Last Object Action Link Rebuild Frame): %llu", static_cast<unsigned long long>(lastRebuildFrame_));
    ImGui::Text("最後のオブジェクト影響線行列更新フレーム (Last Object Action Link Matrix Update Frame): %llu", static_cast<unsigned long long>(lastMatrixUpdateFrame_));
    if (ImGui::Button("オブジェクト影響線を再構築 (Rebuild Object Action Links)")) {
        needsRebuild = true;
    }
    return needsRebuild;
#else
    return false;
#endif
}

size_t LevelEventObjectActionVisualizer::GetLinkCount() const {
    return links_.size();
}
