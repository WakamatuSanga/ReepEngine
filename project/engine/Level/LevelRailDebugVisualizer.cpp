#include "LevelRailDebugVisualizer.h"
#include "LevelSceneData.h"
#include "LevelTransformConverter.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kMinSegmentLength = 0.0001f;

    struct RailVisualTransform {
        Vector3 translation{ 0.0f, 0.0f, 0.0f };
        Vector3 rotationRadians{ 0.0f, 0.0f, 0.0f };
        Vector3 scaling{ 1.0f, 1.0f, 1.0f };
    };

    Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 SubtractVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    }

    Vector3 ScaleVector3(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    float Length(const Vector3& value) {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    }

    Vector3 Normalize(const Vector3& value) {
        const float length = Length(value);
        if (length <= kMinSegmentLength) {
            return { 0.0f, 0.0f, 1.0f };
        }
        return { value.x / length, value.y / length, value.z / length };
    }

    Vector3 ConvertPoint(const Vector3& point, bool axisConversionEnabled) {
        return axisConversionEnabled ? BlenderToEnginePosition(point) : point;
    }

    RailVisualTransform MakeLineTransform(const Vector3& from, const Vector3& to, float thickness) {
        const Vector3 diff = SubtractVector3(to, from);
        const float length = Length(diff);
        const Vector3 direction = Normalize(diff);
        const float yaw = std::atan2(direction.x, direction.z);
        const float horizontal = std::sqrt(direction.x * direction.x + direction.z * direction.z);
        const float pitch = std::atan2(-direction.y, horizontal);

        return {
            ScaleVector3(AddVector3(from, to), 0.5f),
            { pitch, yaw, 0.0f },
            { thickness, thickness, length * 0.5f },
        };
    }

    Model* GetLineModel() {
        auto modelManager = ModelManager::GetInstance();
        if (Model* model = modelManager->FindModel("LevelRailDebugLine")) {
            return model;
        }
        return modelManager->CreateBox("LevelRailDebugLine");
    }

    Model* GetPointModel() {
        auto modelManager = ModelManager::GetInstance();
        if (Model* model = modelManager->FindModel("LevelRailDebugPoint")) {
            return model;
        }
        return modelManager->CreateSphere("LevelRailDebugPoint", 10);
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

    void ApplyTransform(Object3d& object, const RailVisualTransform& transform) {
        object.SetTranslate(transform.translation);
        object.SetRotate(transform.rotationRadians);
        object.SetScale(transform.scaling);
    }
}

struct LevelRailDebugVisualObject {
    std::unique_ptr<Object3d> object;
    Model* model = nullptr;
};

LevelRailDebugVisualizer::LevelRailDebugVisualizer() = default;

LevelRailDebugVisualizer::~LevelRailDebugVisualizer() = default;

void LevelRailDebugVisualizer::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    object3dCommon_ = object3dCommon;
    camera_ = camera;
}

void LevelRailDebugVisualizer::Clear() {
    lineObjects_.clear();
    pointObjects_.clear();
    railSummaries_.clear();
    railPointCount_ = 0;
}

void LevelRailDebugVisualizer::Rebuild(
    const LevelSceneData& sceneData,
    bool axisConversionEnabled,
    uint64_t frameCounter) {
    if (pauseRailRebuild_) {
        return;
    }

    Clear();
    ++rebuildCount_;
    lastRebuildFrame_ = frameCounter;
    if (!object3dCommon_ || !camera_) {
        return;
    }

    Model* lineModel = GetLineModel();
    Model* pointModel = GetPointModel();
    if (!lineModel || !pointModel) {
        return;
    }

    for (const LevelRail& rail : sceneData.rails) {
        if (!rail.visibleInEditor) {
            continue;
        }

        std::vector<Vector3> points;
        points.reserve(rail.points.size());
        for (const Vector3& point : rail.points) {
            points.push_back(ConvertPoint(point, axisConversionEnabled));
        }

        LevelRailSummary summary;
        summary.railId = rail.railId;
        summary.name = rail.name;
        summary.railType = rail.railType;
        summary.loop = rail.loop;
        summary.speed = rail.speed;
        summary.pointCount = points.size();
        railPointCount_ += points.size();

        for (const Vector3& point : points) {
            auto pointObject = std::make_unique<LevelRailDebugVisualObject>();
            pointObject->model = pointModel;
            pointObject->object = std::make_unique<Object3d>();
            pointObject->object->Initialize(object3dCommon_);
            pointObject->object->SetModel(pointModel);
            pointObject->object->SetCamera(camera_);
            pointObject->object->SetEnvironmentMapEnabled(false);
            ApplyTransform(*pointObject->object, { point, { 0.0f, 0.0f, 0.0f }, { railPointScale_, railPointScale_, railPointScale_ } });
            pointObjects_.push_back(std::move(pointObject));
        }

        if (points.size() >= 2) {
            const size_t segmentCount = rail.loop ? points.size() : points.size() - 1;
            for (size_t index = 0; index < segmentCount; ++index) {
                const Vector3& from = points[index];
                const Vector3& to = points[(index + 1) % points.size()];
                if (Length(SubtractVector3(to, from)) <= kMinSegmentLength) {
                    continue;
                }

                auto lineObject = std::make_unique<LevelRailDebugVisualObject>();
                lineObject->model = lineModel;
                lineObject->object = std::make_unique<Object3d>();
                lineObject->object->Initialize(object3dCommon_);
                lineObject->object->SetModel(lineModel);
                lineObject->object->SetCamera(camera_);
                lineObject->object->SetEnvironmentMapEnabled(false);
                ApplyTransform(*lineObject->object, MakeLineTransform(from, to, railLineThickness_));
                lineObjects_.push_back(std::move(lineObject));
                ++summary.segmentCount;
            }
        }

        railSummaries_.push_back(std::move(summary));
    }

    selectedRailIndex_ = std::clamp(
        selectedRailIndex_,
        0,
        (std::max)(0, static_cast<int>(railSummaries_.size()) - 1));
}

void LevelRailDebugVisualizer::Update(uint64_t frameCounter) {
    for (const auto& line : lineObjects_) {
        if (line && line->object) {
            line->object->Update();
        }
    }
    for (const auto& point : pointObjects_) {
        if (point && point->object) {
            point->object->Update();
        }
    }
    lastMatrixUpdateFrame_ = frameCounter;
}

void LevelRailDebugVisualizer::Draw(uint64_t frameCounter) {
    if (!showRails_ || !object3dCommon_) {
        return;
    }

    if (updateMatricesWithLatestCamera_) {
        Update(frameCounter);
    }

    object3dCommon_->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    for (const auto& line : lineObjects_) {
        if (!line || !line->object) {
            continue;
        }
        ApplyModelMaterial(line->model, railLineColor_);
        line->object->Draw();
    }

    if (!showRailPoints_) {
        return;
    }
    for (const auto& point : pointObjects_) {
        if (!point || !point->object) {
            continue;
        }
        ApplyModelMaterial(point->model, railPointColor_);
        point->object->Draw();
    }
}

bool LevelRailDebugVisualizer::DrawImGui() {
#ifdef _DEBUG
    bool needsRebuild = false;
    ImGui::Checkbox("レール表示 (Show Rails)", &showRails_);
    ImGui::Checkbox("レール点表示 (Show Rail Points)", &showRailPoints_);
    ImGui::ColorEdit4("レール線の色 (Rail Line Color)", railLineColor_.data());
    ImGui::SliderFloat("レール線透明度 (Rail Line Alpha)", &railLineColor_[3], 0.05f, 1.0f, "%.2f");
    ImGui::ColorEdit4("レール点の色 (Rail Point Color)", railPointColor_.data());
    if (ImGui::SliderFloat("レール線の太さ (Rail Thickness)", &railLineThickness_, 0.01f, 0.20f, "%.3f")) {
        needsRebuild = true;
    }
    if (ImGui::SliderFloat("レール点サイズ (Rail Point Scale)", &railPointScale_, 0.03f, 0.35f, "%.2f")) {
        needsRebuild = true;
    }
    ImGui::Checkbox("レール再構築を一時停止 (Pause Rail Rebuild)", &pauseRailRebuild_);
    ImGui::Checkbox("最新カメラでレール行列更新 (Update Matrices With Latest Camera)", &updateMatricesWithLatestCamera_);
    if (ImGui::Button("レールを再構築 (Rebuild Rails)")) {
        needsRebuild = true;
    }

    ImGui::Text("レール数 (Rail Count): %zu", railSummaries_.size());
    ImGui::Text("レール点数 (Rail Point Count): %zu", railPointCount_);
    ImGui::Text("レール線分数 (Rail Segment Count): %zu", lineObjects_.size());
    ImGui::Text("レール再構築回数 (Rail Rebuild Count): %llu", static_cast<unsigned long long>(rebuildCount_));
    ImGui::Text("最後のレール再構築フレーム (Last Rail Rebuild Frame): %llu", static_cast<unsigned long long>(lastRebuildFrame_));
    ImGui::Text("最後のレール行列更新フレーム (Last Rail Matrix Update Frame): %llu", static_cast<unsigned long long>(lastMatrixUpdateFrame_));

    if (ImGui::TreeNode("選択中レール情報 (Selected Rail Info)")) {
        if (railSummaries_.empty()) {
            ImGui::TextDisabled("レールがありません。 (No rails.)");
        } else {
            ImGui::SliderInt("Selected Rail Index", &selectedRailIndex_, 0, static_cast<int>(railSummaries_.size()) - 1);
            const LevelRailSummary& rail = railSummaries_[static_cast<size_t>(selectedRailIndex_)];
            ImGui::Text("rail_id: %s", rail.railId.empty() ? "(none)" : rail.railId.c_str());
            ImGui::Text("name: %s", rail.name.empty() ? "(none)" : rail.name.c_str());
            ImGui::Text("rail_type: %s", rail.railType.empty() ? "(none)" : rail.railType.c_str());
            ImGui::Text("loop: %s", rail.loop ? "true" : "false");
            ImGui::Text("speed: %.3f", rail.speed);
            ImGui::Text("points: %zu", rail.pointCount);
            ImGui::Text("segments: %zu", rail.segmentCount);
        }
        ImGui::TreePop();
    }

    return needsRebuild;
#else
    return false;
#endif
}
