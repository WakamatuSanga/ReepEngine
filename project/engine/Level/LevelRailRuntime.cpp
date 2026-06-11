#include "LevelRailRuntime.h"
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

    Vector3 NormalizeOrForward(const Vector3& value) {
        const float length = Length(value);
        if (length <= kMinSegmentLength) {
            return { 0.0f, 0.0f, 1.0f };
        }
        return { value.x / length, value.y / length, value.z / length };
    }

    float WrapDistance(float distance, float totalLength) {
        if (totalLength <= kMinSegmentLength) {
            return 0.0f;
        }

        float wrapped = std::fmod(distance, totalLength);
        if (wrapped < 0.0f) {
            wrapped += totalLength;
        }
        return wrapped;
    }

    float ClampDistance(float distance, float totalLength) {
        return std::clamp(distance, 0.0f, (std::max)(0.0f, totalLength));
    }

    float NormalizeT(float distance, float totalLength) {
        if (totalLength <= kMinSegmentLength) {
            return 0.0f;
        }
        return std::clamp(distance / totalLength, 0.0f, 1.0f);
    }

    Vector3 ConvertRailPoint(const Vector3& point, bool axisConversionEnabled) {
        return axisConversionEnabled ? BlenderToEnginePosition(point) : point;
    }

    Model* GetActorModel() {
        auto modelManager = ModelManager::GetInstance();
        if (Model* model = modelManager->FindModel("LevelRailRuntimeActor")) {
            return model;
        }
        return modelManager->CreateSphere("LevelRailRuntimeActor", 12);
    }

    Model* GetForwardModel() {
        auto modelManager = ModelManager::GetInstance();
        if (Model* model = modelManager->FindModel("LevelRailRuntimeForward")) {
            return model;
        }
        return modelManager->CreateBox("LevelRailRuntimeForward");
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

    Vector3 MakeLineRotation(const Vector3& direction) {
        const Vector3 normalized = NormalizeOrForward(direction);
        const float yaw = std::atan2(normalized.x, normalized.z);
        const float horizontal = std::sqrt(normalized.x * normalized.x + normalized.z * normalized.z);
        const float pitch = std::atan2(-normalized.y, horizontal);
        return { pitch, yaw, 0.0f };
    }
}

struct LevelRailRuntimeRail {
    std::string railId;
    std::string name;
    std::string railType;
    bool loop = false;
    bool visibleInEditor = true;
    float speed = 1.0f;
    float totalLength = 0.0f;
    std::vector<Vector3> points;
    LevelRailSampleTable sampleTable;
};

namespace {
    LevelRailEvaluation ToRuntimeEvaluation(const LevelRailSampleEvaluation& evaluation) {
        return {
            evaluation.valid,
            evaluation.position,
            evaluation.forward,
            evaluation.segmentIndex,
            evaluation.totalLength,
            evaluation.distance,
            evaluation.t,
        };
    }

    LevelRailEvaluation EvaluateRailByDistance(
        const LevelRailRuntimeRail& rail,
        float distance,
        bool loopEnabled,
        float forwardLookAheadDistance) {
        LevelRailEvaluation result = ToRuntimeEvaluation(
            LevelRailEvaluator::EvaluateByDistance(rail.sampleTable, distance, loopEnabled));
        if (!result.valid || forwardLookAheadDistance <= 0.0f || result.totalLength <= kMinSegmentLength) {
            return result;
        }

        const LevelRailSampleEvaluation lookAhead = LevelRailEvaluator::EvaluateByDistance(
            rail.sampleTable,
            result.distance + forwardLookAheadDistance,
            loopEnabled);
        if (!lookAhead.valid) {
            return result;
        }

        result.forward = NormalizeOrForward(SubtractVector3(lookAhead.position, result.position));
        return result;
    }
}

LevelRailRuntime::LevelRailRuntime() = default;

LevelRailRuntime::~LevelRailRuntime() = default;

void LevelRailRuntime::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    object3dCommon_ = object3dCommon;
    camera_ = camera;
    EnsureDebugActorVisual();
}

void LevelRailRuntime::Clear() {
    rails_.clear();
    currentEvaluation_ = {};
    selectedRailIndex_ = 0;
    previousSelectedRailIndex_ = -1;
    railT_ = 0.0f;
    railDistance_ = 0.0f;
}

void LevelRailRuntime::Rebuild(const LevelSceneData& sceneData, bool axisConversionEnabled, uint64_t frameCounter) {
    rails_.clear();
    ++rebuildCount_;
    lastRebuildFrame_ = frameCounter;

    for (const LevelRail& sourceRail : sceneData.rails) {
        auto rail = std::make_unique<LevelRailRuntimeRail>();
        rail->railId = sourceRail.railId;
        rail->name = sourceRail.name;
        rail->railType = sourceRail.railType;
        rail->loop = sourceRail.loop;
        rail->visibleInEditor = sourceRail.visibleInEditor;
        rail->speed = sourceRail.speed;
        rail->points.reserve(sourceRail.points.size());
        for (const Vector3& point : sourceRail.points) {
            rail->points.push_back(ConvertRailPoint(point, axisConversionEnabled));
        }

        const RailInterpolationMode activeMode = useSmoothedRailEvaluation_
            ? interpolationMode_
            : RailInterpolationMode::Linear;
        LevelRailEvaluator::BuildSampleTable(
            rail->sampleTable,
            rail->points,
            rail->loop,
            activeMode,
            smoothingSubdivisionsPerSegment_);
        rail->totalLength = rail->sampleTable.totalLength;

        rails_.push_back(std::move(rail));
    }

    selectedRailIndex_ = std::clamp(selectedRailIndex_, 0, (std::max)(0, static_cast<int>(rails_.size()) - 1));
    previousSelectedRailIndex_ = -1;
    SyncSelectionDefaults();
    UpdateCurrentEvaluation();
}

void LevelRailRuntime::Update(float deltaTime, uint64_t frameCounter) {
    lastUpdateFrame_ = frameCounter;
    SyncSelectionDefaults();
    const LevelRailRuntimeRail* rail = GetSelectedRail();
    if (!rail) {
        currentEvaluation_ = {};
        UpdateDebugActorVisual(frameCounter);
        return;
    }

    if (runtimeEnabled_ && autoPlay_ && rail->totalLength > kMinSegmentLength) {
        railDistance_ += deltaTime * playSpeed_;
        const bool shouldLoop = debugLoop_ || rail->loop;
        if (shouldLoop) {
            railDistance_ = WrapDistance(railDistance_, rail->totalLength);
        } else {
            const float clampedDistance = ClampDistance(railDistance_, rail->totalLength);
            if (clampedDistance != railDistance_) {
                railDistance_ = clampedDistance;
                autoPlay_ = false;
            }
        }
        railT_ = NormalizeT(railDistance_, rail->totalLength);
    }

    UpdateCurrentEvaluation();
    UpdateDebugActorVisual(frameCounter);
}

void LevelRailRuntime::Draw(uint64_t frameCounter) {
    if (!runtimeEnabled_ || !showDebugRailActor_ || externalHideDebugActor_ || !object3dCommon_ || !currentEvaluation_.valid) {
        return;
    }

    if (updateMatricesWithLatestCamera_) {
        UpdateDebugActorVisual(frameCounter);
    }

    object3dCommon_->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    ApplyModelMaterial(actorModel_, { 1.0f, 0.28f, 0.08f, 1.0f });
    if (actorObject_) {
        actorObject_->Draw();
    }

    ApplyModelMaterial(forwardModel_, { 1.0f, 0.95f, 0.18f, 1.0f });
    if (forwardObject_) {
        forwardObject_->Draw();
    }
}

void LevelRailRuntime::SetExternalDebugActorHidden(bool hidden) {
    externalHideDebugActor_ = hidden;
}

bool LevelRailRuntime::DrawImGui() {
#ifdef _DEBUG
    bool needsRebuild = false;
    ImGui::Checkbox("レールランタイム有効 (Enable Rail Runtime)", &runtimeEnabled_);
    ImGui::Checkbox("Debug Rail Actorを表示 (Show Debug Rail Actor)", &showDebugRailActor_);
    ImGui::Checkbox("Auto Play", &autoPlay_);
    ImGui::Checkbox("ループ (Loop)", &debugLoop_);
    ImGui::Checkbox("最新カメラでActor行列更新 (Update Matrices With Latest Camera)", &updateMatricesWithLatestCamera_);
    if (ImGui::Checkbox("補間済みRail評価を使う (Use Smoothed Rail Evaluation)", &useSmoothedRailEvaluation_)) {
        needsRebuild = true;
    }
    const char* interpolationModeNames[] = { "Linear", "CatmullRom" };
    int interpolationModeIndex = interpolationMode_ == RailInterpolationMode::CatmullRom ? 1 : 0;
    if (ImGui::Combo("Rail Interpolation Mode", &interpolationModeIndex, interpolationModeNames, 2)) {
        interpolationMode_ = interpolationModeIndex == 1 ? RailInterpolationMode::CatmullRom : RailInterpolationMode::Linear;
        needsRebuild = true;
    }
    if (ImGui::SliderInt("Smoothing Subdivisions Per Segment", &smoothingSubdivisionsPerSegment_, 1, 32)) {
        smoothingSubdivisionsPerSegment_ = std::clamp(smoothingSubdivisionsPerSegment_, 1, 64);
        needsRebuild = true;
    }
    ImGui::DragFloat("Forward LookAhead Distance", &forwardLookAheadDistance_, 0.01f, 0.0f, 5.0f, "%.2f");
    if (ImGui::Button("補間Railを再構築 (Rebuild Smoothed Rails)")) {
        needsRebuild = true;
    }

    ImGui::Text("Rail Count: %zu", rails_.size());
    if (rails_.empty()) {
        ImGui::TextDisabled("レールがありません。 (No rails.)");
        return needsRebuild;
    }

    SyncSelectionDefaults();
    const LevelRailRuntimeRail* rail = GetSelectedRail();
    const std::string currentLabel = rail
        ? ((rail->name.empty() ? rail->railId : rail->name) + "##" + rail->railId)
        : "(none)";
    if (ImGui::BeginCombo("選択中レール (Selected Rail)", currentLabel.c_str())) {
        for (size_t index = 0; index < rails_.size(); ++index) {
            const LevelRailRuntimeRail& candidate = *rails_[index];
            const std::string label =
                (candidate.name.empty() ? candidate.railId : candidate.name) +
                " [" + std::to_string(candidate.points.size()) +
                " -> " + std::to_string(candidate.sampleTable.sampledPoints.size()) +
                " pts]##rail_" + std::to_string(index);
            const bool selected = selectedRailIndex_ == static_cast<int>(index);
            if (ImGui::Selectable(label.c_str(), selected)) {
                selectedRailIndex_ = static_cast<int>(index);
                previousSelectedRailIndex_ = -1;
                SyncSelectionDefaults();
                UpdateCurrentEvaluation();
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    rail = GetSelectedRail();
    if (!rail) {
        return needsRebuild;
    }

    ImGui::Text("Original Point Count: %zu", rail->points.size());
    ImGui::Text("Sampled Point Count: %zu", rail->sampleTable.sampledPoints.size());
    ImGui::Text("Selected Rail Total Length: %.3f", rail->totalLength);
    ImGui::Text("Interpolation Mode: %s", LevelRailEvaluator::GetModeName(rail->sampleTable.interpolationMode));
    ImGui::Text("rail_id: %s", rail->railId.empty() ? "(none)" : rail->railId.c_str());
    ImGui::Text("name: %s", rail->name.empty() ? "(none)" : rail->name.c_str());
    ImGui::Text("rail_loop: %s", rail->loop ? "true" : "false");
    ImGui::Text("rail_speed: %.3f", rail->speed);

    const char* moveModeNames[] = { "T", "Distance" };
    ImGui::Combo("Move Mode", &moveMode_, moveModeNames, 2);
    if (moveMode_ == 0) {
        if (ImGui::SliderFloat("Rail T 0.0-1.0", &railT_, 0.0f, 1.0f, "%.3f")) {
            railDistance_ = railT_ * rail->totalLength;
            UpdateCurrentEvaluation();
        }
    } else {
        const float maxDistance = (std::max)(rail->totalLength, 0.001f);
        if (ImGui::SliderFloat("Rail Distance", &railDistance_, 0.0f, maxDistance, "%.3f")) {
            railDistance_ = ClampDistance(railDistance_, rail->totalLength);
            railT_ = NormalizeT(railDistance_, rail->totalLength);
            UpdateCurrentEvaluation();
        }
    }
    ImGui::DragFloat("Play Speed", &playSpeed_, 0.05f, -50.0f, 50.0f, "%.2f");
    ImGui::DragFloat("Actor Scale", &actorScale_, 0.01f, 0.03f, 1.0f, "%.2f");
    ImGui::DragFloat("Forward Length", &forwardLength_, 0.02f, 0.1f, 5.0f, "%.2f");
    if (ImGui::Button("Reset Rail Actor")) {
        ResetRailActor();
    }
    ImGui::SameLine();
    if (ImGui::Button("Rail Runtimeを再構築 (Rebuild Rail Runtime)")) {
        needsRebuild = true;
    }

    ImGui::Text("Current Position: %.3f, %.3f, %.3f",
        currentEvaluation_.position.x,
        currentEvaluation_.position.y,
        currentEvaluation_.position.z);
    ImGui::Text("Current Forward: %.3f, %.3f, %.3f",
        currentEvaluation_.forward.x,
        currentEvaluation_.forward.y,
        currentEvaluation_.forward.z);
    ImGui::Text("Current Segment Index: %zu", currentEvaluation_.segmentIndex);
    ImGui::Text("Current Distance: %.3f", currentEvaluation_.distance);
    ImGui::Text("Current T: %.3f", currentEvaluation_.t);
    ImGui::Text("Evaluation Valid: %s", currentEvaluation_.valid ? "true" : "false");
    ImGui::Text("Rail Runtime Rebuild Count: %llu", static_cast<unsigned long long>(rebuildCount_));
    ImGui::Text("Last Rail Runtime Rebuild Frame: %llu", static_cast<unsigned long long>(lastRebuildFrame_));
    ImGui::Text("Last Rail Runtime Update Frame: %llu", static_cast<unsigned long long>(lastUpdateFrame_));
    ImGui::Text("Last Rail Actor Matrix Update Frame: %llu", static_cast<unsigned long long>(lastMatrixUpdateFrame_));
    return needsRebuild;
#else
    return false;
#endif
}

LevelRailEvaluation LevelRailRuntime::EvaluateByT(const std::string& railId, float t) const {
    const LevelRailRuntimeRail* rail = FindRailById(railId);
    if (!rail) {
        return {};
    }
    const float clampedT = std::clamp(t, 0.0f, 1.0f);
    return EvaluateRailByDistance(*rail, clampedT * rail->totalLength, rail->loop, forwardLookAheadDistance_);
}

LevelRailEvaluation LevelRailRuntime::EvaluateByDistance(const std::string& railId, float distance) const {
    const LevelRailRuntimeRail* rail = FindRailById(railId);
    if (!rail) {
        return {};
    }
    return EvaluateRailByDistance(*rail, distance, rail->loop, forwardLookAheadDistance_);
}

LevelRailEvaluation LevelRailRuntime::EvaluateByDistance(const std::string& railId, float distance, bool loopEnabled) const {
    const LevelRailRuntimeRail* rail = FindRailById(railId);
    if (!rail) {
        return {};
    }
    return EvaluateRailByDistance(*rail, distance, loopEnabled, forwardLookAheadDistance_);
}

size_t LevelRailRuntime::GetRailCount() const {
    return rails_.size();
}

bool LevelRailRuntime::GetRailInfo(size_t index, LevelRailRuntimeRailInfo& outInfo) const {
    if (index >= rails_.size() || !rails_[index]) {
        return false;
    }

    const LevelRailRuntimeRail& rail = *rails_[index];
    outInfo.railId = rail.railId;
    outInfo.name = rail.name;
    outInfo.railType = rail.railType;
    outInfo.loop = rail.loop;
    outInfo.speed = rail.speed;
    outInfo.totalLength = rail.totalLength;
    outInfo.pointCount = rail.points.size();
    outInfo.sampledPointCount = rail.sampleTable.sampledPoints.size();
    return true;
}

const LevelRailRuntimeRail* LevelRailRuntime::FindRailById(const std::string& railId) const {
    for (const auto& rail : rails_) {
        if (!rail) {
            continue;
        }
        if (rail->railId == railId || rail->name == railId) {
            return rail.get();
        }
    }
    return nullptr;
}

const LevelRailRuntimeRail* LevelRailRuntime::GetSelectedRail() const {
    if (selectedRailIndex_ < 0 || selectedRailIndex_ >= static_cast<int>(rails_.size())) {
        return nullptr;
    }
    return rails_[static_cast<size_t>(selectedRailIndex_)].get();
}

void LevelRailRuntime::SyncSelectionDefaults() {
    selectedRailIndex_ = std::clamp(selectedRailIndex_, 0, (std::max)(0, static_cast<int>(rails_.size()) - 1));
    if (selectedRailIndex_ == previousSelectedRailIndex_) {
        return;
    }

    const LevelRailRuntimeRail* rail = GetSelectedRail();
    if (rail) {
        playSpeed_ = rail->speed;
        debugLoop_ = rail->loop;
        railDistance_ = 0.0f;
        railT_ = 0.0f;
    }
    previousSelectedRailIndex_ = selectedRailIndex_;
}

void LevelRailRuntime::UpdateCurrentEvaluation() {
    const LevelRailRuntimeRail* rail = GetSelectedRail();
    if (!rail) {
        currentEvaluation_ = {};
        return;
    }

    const bool shouldLoop = debugLoop_ || rail->loop;
    if (moveMode_ == 0) {
        railT_ = std::clamp(railT_, 0.0f, 1.0f);
        railDistance_ = railT_ * rail->totalLength;
    } else {
        railDistance_ = shouldLoop
            ? WrapDistance(railDistance_, rail->totalLength)
            : ClampDistance(railDistance_, rail->totalLength);
        railT_ = NormalizeT(railDistance_, rail->totalLength);
    }
    currentEvaluation_ = EvaluateRailByDistance(*rail, railDistance_, shouldLoop, forwardLookAheadDistance_);
}

void LevelRailRuntime::UpdateDebugActorVisual(uint64_t frameCounter) {
    EnsureDebugActorVisual();
    if (!actorObject_ || !forwardObject_ || !currentEvaluation_.valid) {
        return;
    }

    actorObject_->SetTranslate(currentEvaluation_.position);
    actorObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
    actorObject_->SetScale({ actorScale_, actorScale_, actorScale_ });
    actorObject_->Update();

    const Vector3 forwardEnd = AddVector3(currentEvaluation_.position, ScaleVector3(currentEvaluation_.forward, forwardLength_));
    const Vector3 forwardCenter = ScaleVector3(AddVector3(currentEvaluation_.position, forwardEnd), 0.5f);
    forwardObject_->SetTranslate(forwardCenter);
    forwardObject_->SetRotate(MakeLineRotation(currentEvaluation_.forward));
    forwardObject_->SetScale({ forwardThickness_, forwardThickness_, forwardLength_ * 0.5f });
    forwardObject_->Update();
    lastMatrixUpdateFrame_ = frameCounter;
}

void LevelRailRuntime::EnsureDebugActorVisual() {
    if (!object3dCommon_ || !camera_) {
        return;
    }

    if (!actorModel_) {
        actorModel_ = GetActorModel();
    }
    if (!forwardModel_) {
        forwardModel_ = GetForwardModel();
    }
    if (!actorObject_ && actorModel_) {
        actorObject_ = std::make_unique<Object3d>();
        actorObject_->Initialize(object3dCommon_);
        actorObject_->SetModel(actorModel_);
        actorObject_->SetCamera(camera_);
        actorObject_->SetEnvironmentMapEnabled(false);
    }
    if (!forwardObject_ && forwardModel_) {
        forwardObject_ = std::make_unique<Object3d>();
        forwardObject_->Initialize(object3dCommon_);
        forwardObject_->SetModel(forwardModel_);
        forwardObject_->SetCamera(camera_);
        forwardObject_->SetEnvironmentMapEnabled(false);
    }
}

void LevelRailRuntime::ResetRailActor() {
    railT_ = 0.0f;
    railDistance_ = 0.0f;
    autoPlay_ = false;
    UpdateCurrentEvaluation();
}
