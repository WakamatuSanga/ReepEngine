#include "RailPathRuntimeV2.h"

#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kMinimumLength = 0.000001f;

Vector3 Add(const Vector3& a, const Vector3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
Vector3 Subtract(const Vector3& a, const Vector3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
Vector3 Scale(const Vector3& value, float scale) { return { value.x * scale, value.y * scale, value.z * scale }; }
float Length(const Vector3& value) { return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z); }

void ApplyMaterial(Model* model, const Vector4& color) {
    if (model) {
        if (Model::Material* material = model->GetMaterialData()) {
            material->color = color;
            material->enableLighting = 0;
            material->alphaReference = 0.0f;
        }
    }
}

void ApplyLineTransform(Object3d& object, const Vector3& from, const Vector3& to, float thickness) {
    const Vector3 direction = Subtract(to, from);
    const float length = Length(direction);
    if (length <= kMinimumLength) return;
    const Vector3 normalized = Scale(direction, 1.0f / length);
    const float yaw = std::atan2(normalized.x, normalized.z);
    const float horizontal = std::sqrt(normalized.x * normalized.x + normalized.z * normalized.z);
    object.SetTranslate(Scale(Add(from, to), 0.5f));
    object.SetRotate({ std::atan2(-normalized.y, horizontal), yaw, 0.0f });
    object.SetScale({ thickness, thickness, length * 0.5f });
}

std::unique_ptr<Object3d> CreateObject(Object3dCommon* common, Camera* camera, Model* model) {
    auto object = std::make_unique<Object3d>();
    object->Initialize(common);
    object->SetModel(model);
    object->SetCamera(camera);
    object->SetEnvironmentMapEnabled(false);
    return object;
}
}

RailPathRuntimeV2::RailPathRuntimeV2() = default;

RailPathRuntimeV2::~RailPathRuntimeV2() = default;

void RailPathRuntimeV2::InitializeDebug(Object3dCommon* object3dCommon, Camera* camera) {
    object3dCommon_ = object3dCommon;
    camera_ = camera;
    RebuildDebugObjects();
}

void RailPathRuntimeV2::Finalize() {
    Clear();
    object3dCommon_ = nullptr;
    camera_ = nullptr;
}

void RailPathRuntimeV2::ClearDebugObjects() {
    runtimeLineObjects_.clear();
    legacyLineObjects_.clear();
    nodeObjects_.clear();
    distanceMarkerObjects_.clear();
    tangentObject_.reset();
    lastDebugDrawCount_ = 0;
}

void RailPathRuntimeV2::RebuildDebugObjects() {
    ClearDebugObjects();
    if (!debugDrawEnabled_ || !valid_ || !object3dCommon_ || !camera_) return;

    ModelManager* manager = ModelManager::GetInstance();
    runtimeLineModel_ = manager->FindModel("RailPathV2RuntimeLine");
    if (!runtimeLineModel_) runtimeLineModel_ = manager->CreateBox("RailPathV2RuntimeLine");
    legacyLineModel_ = manager->FindModel("RailPathV2LegacyLine");
    if (!legacyLineModel_) legacyLineModel_ = manager->CreateBox("RailPathV2LegacyLine");
    nodeModel_ = manager->FindModel("RailPathV2Node");
    if (!nodeModel_) nodeModel_ = manager->CreateSphere("RailPathV2Node", 10);
    distanceMarkerModel_ = manager->FindModel("RailPathV2DistanceMarker");
    if (!distanceMarkerModel_) distanceMarkerModel_ = manager->CreateSphere("RailPathV2DistanceMarker", 8);
    tangentModel_ = manager->FindModel("RailPathV2Tangent");
    if (!tangentModel_) tangentModel_ = manager->CreateBox("RailPathV2Tangent");

    const size_t limit = static_cast<size_t>((std::max)(maximumDebugDrawCount_, 1));
    size_t createdCount = 0;
    if (showRuntimeLine_ && runtimeLineModel_) {
        for (size_t index = 1; index < arcLengthTable_.size() && createdCount < limit; ++index) {
            auto object = CreateObject(object3dCommon_, camera_, runtimeLineModel_);
            ApplyLineTransform(*object, arcLengthTable_[index - 1].position, arcLengthTable_[index].position, lineThickness_);
            runtimeLineObjects_.push_back(std::move(object));
            ++createdCount;
        }
    }
    if (showLegacyLine_ && legacyLineModel_) {
        for (size_t index = 1; index < legacySampledPoints_.size() && createdCount < limit; ++index) {
            auto object = CreateObject(object3dCommon_, camera_, legacyLineModel_);
            ApplyLineTransform(*object, legacySampledPoints_[index - 1], legacySampledPoints_[index], lineThickness_ * 1.35f);
            legacyLineObjects_.push_back(std::move(object));
            ++createdCount;
        }
    }
    if (showNodes_ && nodeModel_) {
        for (size_t index = 0; index < nodes_.size() && createdCount < limit; ++index) {
            auto object = CreateObject(object3dCommon_, camera_, nodeModel_);
            object->SetTranslate(nodes_[index].position);
            object->SetScale({ nodeScale_, nodeScale_, nodeScale_ });
            nodeObjects_.push_back(std::move(object));
            ++createdCount;
        }
    }
    if (showDistanceMarkers_ && distanceMarkerModel_ && distanceMarkerInterval_ > kMinimumLength) {
        for (float distance = 0.0f; distance <= totalLength_ && createdCount < limit; distance += distanceMarkerInterval_) {
            auto object = CreateObject(object3dCommon_, camera_, distanceMarkerModel_);
            object->SetTranslate(SampleByDistance(distance).position);
            object->SetScale({ markerScale_, markerScale_, markerScale_ });
            distanceMarkerObjects_.push_back(std::move(object));
            ++createdCount;
        }
        if (!distanceMarkerObjects_.empty() && createdCount < limit) {
            auto object = CreateObject(object3dCommon_, camera_, distanceMarkerModel_);
            object->SetTranslate(SampleByDistance(totalLength_).position);
            object->SetScale({ markerScale_, markerScale_, markerScale_ });
            distanceMarkerObjects_.push_back(std::move(object));
            ++createdCount;
        }
    }
    if (showTangent_ && tangentModel_ && createdCount < limit) {
        tangentObject_ = CreateObject(object3dCommon_, camera_, tangentModel_);
        ApplyLineTransform(*tangentObject_, previewSample_.position,
            Add(previewSample_.position, Scale(previewSample_.tangent, tangentLength_)), lineThickness_ * 1.5f);
        ++createdCount;
    }
    lastDebugDrawCount_ = createdCount;
}

void RailPathRuntimeV2::SetExternalDebugHidden(bool hidden) {
    externalDebugHidden_ = hidden;
}

void RailPathRuntimeV2::DrawDebug() {
    if (!runtimeEnabled_ || !debugDrawEnabled_ || externalDebugHidden_ || !valid_ || !object3dCommon_) return;
    if (tangentObject_ && previewSample_.valid) {
        ApplyLineTransform(*tangentObject_, previewSample_.position,
            Add(previewSample_.position, Scale(previewSample_.tangent, tangentLength_)), lineThickness_ * 1.5f);
    }
    auto updateAll = [](const auto& objects) {
        for (const auto& object : objects) if (object) object->Update();
    };
    updateAll(runtimeLineObjects_);
    updateAll(legacyLineObjects_);
    updateAll(nodeObjects_);
    updateAll(distanceMarkerObjects_);
    if (tangentObject_) tangentObject_->Update();

    object3dCommon_->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    auto drawAll = [](const auto& objects, Model* model, const Vector4& color) {
        ApplyMaterial(model, color);
        for (const auto& object : objects) if (object) object->Draw();
    };
    drawAll(runtimeLineObjects_, runtimeLineModel_, { 0.1f, 0.9f, 1.0f, 0.9f });
    drawAll(legacyLineObjects_, legacyLineModel_, { 1.0f, 0.72f, 0.08f, 0.85f });
    drawAll(nodeObjects_, nodeModel_, { 1.0f, 1.0f, 1.0f, 1.0f });
    drawAll(distanceMarkerObjects_, distanceMarkerModel_, { 0.3f, 1.0f, 0.25f, 1.0f });
    if (tangentObject_) {
        ApplyMaterial(tangentModel_, { 1.0f, 0.2f, 0.05f, 1.0f });
        tangentObject_->Draw();
    }
    lastDebugDrawCount_ = runtimeLineObjects_.size() + legacyLineObjects_.size() + nodeObjects_.size()
        + distanceMarkerObjects_.size() + (tangentObject_ ? 1 : 0);
}
