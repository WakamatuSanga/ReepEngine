#include "BlenderRailPipelineVerificationRenderer.h"

#include "Engine/Game/RailShooter/RailPathRuntimeV2.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kMinimumLength = 0.000001f;

Vector3 Add(const Vector3& a, const Vector3& b) {
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

Vector3 Subtract(const Vector3& a, const Vector3& b) {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

Vector3 Scale(const Vector3& value, float scale) {
    return { value.x * scale, value.y * scale, value.z * scale };
}

float Length(const Vector3& value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

void ApplyMaterial(Model* model, const Vector4& color) {
    if (!model) return;
    if (Model::Material* material = model->GetMaterialData()) {
        material->color = color;
        material->enableLighting = 0;
        material->alphaReference = 0.0f;
    }
}

void ApplyLineTransform(Object3d& object, const Vector3& from, const Vector3& to, float thickness) {
    const Vector3 direction = Subtract(to, from);
    const float length = Length(direction);
    if (length <= kMinimumLength) return;
    const Vector3 normalized = Scale(direction, 1.0f / length);
    const float horizontal = std::sqrt(normalized.x * normalized.x + normalized.z * normalized.z);
    object.SetTranslate(Scale(Add(from, to), 0.5f));
    object.SetRotate({ std::atan2(-normalized.y, horizontal), std::atan2(normalized.x, normalized.z), 0.0f });
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

size_t CalculateStride(size_t sourceCount, size_t maximumCount) {
    if (sourceCount == 0 || maximumCount == 0) return 1;
    return (sourceCount + maximumCount - 1) / maximumCount;
}

void AddLines(
    std::vector<std::unique_ptr<Object3d>>& destination,
    const std::vector<Vector3>& points,
    size_t maximumCount,
    float thickness,
    Object3dCommon* common,
    Camera* camera,
    Model* model,
    size_t& skippedCount) {
    if (!model || points.size() < 2 || maximumCount == 0) return;
    const size_t segmentCount = points.size() - 1;
    const size_t stride = CalculateStride(segmentCount, maximumCount);
    for (size_t begin = 0; begin < segmentCount && destination.size() < maximumCount; begin += stride) {
        const size_t end = (std::min)(begin + stride, points.size() - 1);
        auto object = CreateObject(common, camera, model);
        ApplyLineTransform(*object, points[begin], points[end], thickness);
        destination.push_back(std::move(object));
    }
    skippedCount = segmentCount > destination.size() ? segmentCount - destination.size() : 0;
}
}

BlenderRailPipelineVerificationRenderer::BlenderRailPipelineVerificationRenderer() = default;

BlenderRailPipelineVerificationRenderer::~BlenderRailPipelineVerificationRenderer() = default;

void BlenderRailPipelineVerificationRenderer::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    object3dCommon_ = object3dCommon;
    camera_ = camera;

    ModelManager* manager = ModelManager::GetInstance();
    runtimeLineModel_ = manager->FindModel("BlenderRailVerificationRuntimeLine");
    if (!runtimeLineModel_) runtimeLineModel_ = manager->CreateBox("BlenderRailVerificationRuntimeLine");
    legacyLineModel_ = manager->FindModel("BlenderRailVerificationLegacyLine");
    if (!legacyLineModel_) legacyLineModel_ = manager->CreateBox("BlenderRailVerificationLegacyLine");
    nodeModel_ = manager->FindModel("BlenderRailVerificationNode");
    if (!nodeModel_) nodeModel_ = manager->CreateSphere("BlenderRailVerificationNode", 10);
    markerModel_ = manager->FindModel("BlenderRailVerificationDistanceMarker");
    if (!markerModel_) markerModel_ = manager->CreateSphere("BlenderRailVerificationDistanceMarker", 8);
    tangentModel_ = manager->FindModel("BlenderRailVerificationTangent");
    if (!tangentModel_) tangentModel_ = manager->CreateBox("BlenderRailVerificationTangent");
}

void BlenderRailPipelineVerificationRenderer::Clear() {
    runtimeLines_.clear();
    legacyLines_.clear();
    nodes_.clear();
    markers_.clear();
    tangents_.clear();
    startMarker_.reset();
    endMarker_.reset();
    stats_ = {};
}

void BlenderRailPipelineVerificationRenderer::Rebuild(
    const RailPathRuntimeV2& runtime,
    const std::vector<Vector3>& legacyPoints) {
    Clear();
    if (!object3dCommon_ || !camera_ || !runtime.IsValid()) return;

    std::vector<Vector3> runtimePoints;
    runtimePoints.reserve(runtime.GetArcLengthTable().size());
    for (const RailArcLengthSample& sample : runtime.GetArcLengthTable()) {
        runtimePoints.push_back(sample.position);
    }
    if (config_.showRuntimeLine) {
        AddLines(runtimeLines_, runtimePoints, config_.maximumLineCount, config_.lineThickness,
            object3dCommon_, camera_, runtimeLineModel_, stats_.skippedLineCount);
    }
    stats_.lineCount = runtimeLines_.size();

    if (config_.showLegacyLine) {
        size_t legacySkipped = 0;
        const size_t remainingLineCount = config_.maximumLineCount > runtimeLines_.size()
            ? config_.maximumLineCount - runtimeLines_.size() : 0;
        AddLines(legacyLines_, legacyPoints, remainingLineCount, config_.lineThickness * 1.35f,
            object3dCommon_, camera_, legacyLineModel_, legacySkipped);
        stats_.lineCount += legacyLines_.size();
        stats_.skippedLineCount += legacySkipped;
    }

    if (config_.showNodes && nodeModel_) {
        const auto& sourceNodes = runtime.GetNodes();
        const size_t stride = CalculateStride(sourceNodes.size(), config_.maximumNodeCount);
        for (size_t index = 0; index < sourceNodes.size() && nodes_.size() < config_.maximumNodeCount; index += stride) {
            auto object = CreateObject(object3dCommon_, camera_, nodeModel_);
            object->SetTranslate(sourceNodes[index].position);
            object->SetScale({ config_.nodeScale, config_.nodeScale, config_.nodeScale });
            nodes_.push_back(std::move(object));
        }
        stats_.skippedNodeCount = sourceNodes.size() > nodes_.size() ? sourceNodes.size() - nodes_.size() : 0;
    }
    stats_.nodeCount = nodes_.size();

    if (config_.showDistanceMarkers && markerModel_ && config_.markerInterval > kMinimumLength) {
        const size_t requestedCount = static_cast<size_t>(std::floor(runtime.GetTotalLength() / config_.markerInterval)) + 1;
        const size_t stride = CalculateStride(requestedCount, config_.maximumMarkerCount);
        const float effectiveInterval = config_.markerInterval * static_cast<float>(stride);
        for (float distance = 0.0f; distance <= runtime.GetTotalLength() && markers_.size() < config_.maximumMarkerCount;
            distance += effectiveInterval) {
            auto object = CreateObject(object3dCommon_, camera_, markerModel_);
            object->SetTranslate(runtime.SampleByDistance(distance).position);
            object->SetScale({ config_.markerScale, config_.markerScale, config_.markerScale });
            markers_.push_back(std::move(object));
        }
        stats_.skippedMarkerCount = requestedCount > markers_.size() ? requestedCount - markers_.size() : 0;
    }
    stats_.markerCount = markers_.size();

    if (config_.showTangents && tangentModel_ && config_.tangentInterval > kMinimumLength) {
        const size_t requestedCount = static_cast<size_t>(std::floor(runtime.GetTotalLength() / config_.tangentInterval)) + 1;
        const size_t stride = CalculateStride(requestedCount, config_.maximumTangentCount);
        const float effectiveInterval = config_.tangentInterval * static_cast<float>(stride);
        for (float distance = 0.0f; distance <= runtime.GetTotalLength() && tangents_.size() < config_.maximumTangentCount;
            distance += effectiveInterval) {
            const RailPathSample sample = runtime.SampleByDistance(distance);
            if (!sample.valid) continue;
            auto object = CreateObject(object3dCommon_, camera_, tangentModel_);
            ApplyLineTransform(*object, sample.position, Add(sample.position, Scale(sample.tangent, config_.tangentLength)),
                config_.lineThickness * 1.5f);
            tangents_.push_back(std::move(object));
        }
        stats_.skippedTangentCount = requestedCount > tangents_.size() ? requestedCount - tangents_.size() : 0;
    }
    stats_.tangentCount = tangents_.size();

    if (config_.showStartEnd && nodeModel_) {
        const RailPathSample start = runtime.SampleByDistance(0.0f);
        const RailPathSample end = runtime.SampleByDistance(runtime.GetTotalLength());
        if (start.valid) {
            startMarker_ = CreateObject(object3dCommon_, camera_, nodeModel_);
            startMarker_->SetTranslate(start.position);
            startMarker_->SetScale({ config_.endpointScale, config_.endpointScale, config_.endpointScale });
            ++stats_.endpointCount;
        }
        if (end.valid) {
            endMarker_ = CreateObject(object3dCommon_, camera_, nodeModel_);
            endMarker_->SetTranslate(end.position);
            endMarker_->SetScale({ config_.endpointScale, config_.endpointScale, config_.endpointScale });
            ++stats_.endpointCount;
        }
    }
}

void BlenderRailPipelineVerificationRenderer::Draw() {
    if (!config_.enabled || !object3dCommon_) return;
    auto updateAll = [](const auto& objects) {
        for (const auto& object : objects) if (object) object->Update();
    };
    updateAll(runtimeLines_);
    updateAll(legacyLines_);
    updateAll(nodes_);
    updateAll(markers_);
    updateAll(tangents_);
    if (startMarker_) startMarker_->Update();
    if (endMarker_) endMarker_->Update();

    object3dCommon_->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    auto drawAll = [](const auto& objects, Model* model, const Vector4& color) {
        ApplyMaterial(model, color);
        for (const auto& object : objects) if (object) object->Draw();
    };
    drawAll(runtimeLines_, runtimeLineModel_, { 0.1f, 0.9f, 1.0f, 0.9f });
    drawAll(legacyLines_, legacyLineModel_, { 1.0f, 0.72f, 0.08f, 0.85f });
    drawAll(nodes_, nodeModel_, { 1.0f, 1.0f, 1.0f, 1.0f });
    drawAll(markers_, markerModel_, { 0.65f, 1.0f, 0.1f, 1.0f });
    drawAll(tangents_, tangentModel_, { 1.0f, 0.2f, 0.05f, 1.0f });
    if (startMarker_) {
        ApplyMaterial(nodeModel_, { 0.1f, 1.0f, 0.1f, 1.0f });
        startMarker_->Draw();
    }
    if (endMarker_) {
        ApplyMaterial(nodeModel_, { 1.0f, 0.1f, 0.1f, 1.0f });
        endMarker_->Draw();
    }
}
