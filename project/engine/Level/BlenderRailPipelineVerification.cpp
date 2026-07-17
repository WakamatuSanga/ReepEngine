#include "BlenderRailPipelineVerification.h"

#include "BlenderRailPipelineVerificationRenderer.h"
#include "Engine/Game/RailShooter/RailPathRuntimeV2.h"
#include "Engine/Game/RailShooter/RailPathRuntimeV2Adapter.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Level/LevelRailEvaluator.h"
#include "Engine/Level/LevelRailRuntime.h"
#include "Engine/Level/LevelSceneData.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>

namespace {
constexpr float kDuplicateDistanceSquared = 0.000001f;

float DistanceSquared(const Vector3& a, const Vector3& b) {
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return x * x + y * y + z * z;
}

float Distance(const Vector3& a, const Vector3& b) {
    return std::sqrt(DistanceSquared(a, b));
}
}

BlenderRailPipelineVerification::BlenderRailPipelineVerification() = default;

BlenderRailPipelineVerification::~BlenderRailPipelineVerification() = default;

void BlenderRailPipelineVerification::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    camera_ = camera;
    previewRuntime_ = std::make_unique<RailPathRuntimeV2>();
    renderer_ = std::make_unique<BlenderRailPipelineVerificationRenderer>();
    renderer_->Initialize(object3dCommon, camera);
}

void BlenderRailPipelineVerification::Clear() {
    if (previewRuntime_) previewRuntime_->Clear();
    if (renderer_) renderer_->Clear();
    convertedPoints_.clear();
    legacySampledPoints_.clear();
    buildSucceeded_ = false;
    adapterSucceeded_ = false;
}

void BlenderRailPipelineVerification::Draw() {
    if (renderer_) renderer_->Draw();
}

void BlenderRailPipelineVerification::SetStageJsonDiagnostics(
    const std::string& requestedPath,
    const std::string& resolvedPath,
    const std::string& loadResult,
    bool success) {
    requestedJsonPath_ = requestedPath;
    std::error_code error;
    const std::filesystem::path absolutePath = std::filesystem::absolute(resolvedPath, error);
    resolvedJsonPath_ = resolvedPath.empty() ? std::string{}
        : (error ? resolvedPath : absolutePath.lexically_normal().generic_string());
    lastJsonLoadResult_ = loadResult;
    jsonLoadSucceeded_ = success;
    error.clear();
    jsonExists_ = !resolvedPath.empty() && std::filesystem::exists(resolvedPath, error);
}

void BlenderRailPipelineVerification::SetLiveSyncDiagnostics(
    const BlenderRailLiveSyncDiagnostics& diagnostics) {
    liveSync_ = diagnostics;
}

void BlenderRailPipelineVerification::OnStageRebuilt(
    const LevelSceneData& sceneData,
    const LevelRailRuntime& railRuntime,
    bool axisConversionEnabled,
    const std::string& applySource) {
    if (applySource.find("LiveSync") != std::string::npos || applySource.find("UDP") != std::string::npos) {
        dataSource_ = DataSource::UdpLiveSync;
        lastApplyReason_ = "UDP Live Sync反映";
    } else if (applySource.find("LoadJson") != std::string::npos) {
        dataSource_ = DataSource::StageJson;
        lastApplyReason_ = "Stage JSON読込";
    } else {
        lastApplyReason_ = "Stage Runtime再構築";
    }
    lastApplyTime_ = CurrentLocalTime();
    BuildPreview(sceneData, railRuntime, axisConversionEnabled, lastApplyReason_);
}

void BlenderRailPipelineVerification::RefreshStageInformation(
    const LevelSceneData& sceneData,
    const LevelRailRuntime& railRuntime,
    bool axisConversionEnabled,
    LevelRailSampleTable* outSampleTable) {
    railCount_ = sceneData.rails.size();
    statistics_ = {};
    for (const LevelRail& rail : sceneData.rails) {
        if (IsRailValid(rail)) ++statistics_.validRailCount;
        else ++statistics_.invalidRailCount;
    }

    if (!selectedRailId_.empty()) {
        size_t index = 0;
        const LevelRail* rail = ResolveSelectedRail(sceneData, &index);
        if (rail && IsRailValid(*rail)) selectedRailIndex_ = index;
        else selectedRailId_.clear();
    }
    if (selectedRailId_.empty()) SelectFirstValidRail(sceneData);

    size_t selectedIndex = 0;
    const LevelRail* selected = ResolveSelectedRail(sceneData, &selectedIndex);
    if (!selected) {
        selectedRailIndex_ = static_cast<size_t>(-1);
        selectedRailName_.clear();
        selectedRailType_.clear();
        selectedWaypointCount_ = 0;
        lastRailReadError_ = railCount_ == 0
            ? "現在のStageデータにRailがありません。"
            : "有効な確認対象Railがありません。";
        return;
    }

    selectedRailIndex_ = selectedIndex;
    selectedRailName_ = selected->name;
    selectedRailType_ = selected->railType;
    selectedWaypointCount_ = selected->points.size();
    selectedRailLoop_ = selected->loop;
    selectedRailReverse_ = selected->reverseDirection;
    selectedRailVisible_ = selected->visibleInEditor;
    selectedRailSpeed_ = selected->speed;
    statistics_.loaderPoints = MakePointSummary(selected->points);
    for (const Vector3& point : selected->points) {
        if (!IsFinite(point)) ++statistics_.nonFinitePointCount;
    }
    LevelRailSampleTable localSampleTable;
    LevelRailSampleTable& sampleTable = outSampleTable ? *outSampleTable : localSampleTable;
    if (!railRuntime.CopyRailBuildInput(selectedIndex, convertedPoints_, sampleTable)) {
        convertedPoints_.clear();
        legacySampledPoints_.clear();
        coordinateConversionApplied_ = false;
        lastRailReadError_ = "LevelRailRuntimeの変換後Railを取得できません。";
        return;
    }
    legacySampledPoints_ = sampleTable.sampledPoints;
    coordinateConversionApplied_ = axisConversionEnabled;
    for (size_t index = 1; index < convertedPoints_.size(); ++index) {
        if (DistanceSquared(convertedPoints_[index - 1], convertedPoints_[index]) <= kDuplicateDistanceSquared) {
            ++statistics_.consecutiveDuplicateCount;
        }
    }
    statistics_.zeroLengthCandidate = convertedPoints_.size() < 2 || sampleTable.totalLength <= 0.000001f;
    statistics_.convertedPoints = MakePointSummary(convertedPoints_);
    if (camera_ && statistics_.convertedPoints.valid) {
        statistics_.cameraToStartDistance = Distance(camera_->GetTranslate(), statistics_.convertedPoints.first);
        statistics_.cameraToCenterDistance = Distance(camera_->GetTranslate(), statistics_.convertedPoints.boundsCenter);
    }
    lastRailReadError_.clear();
}

bool BlenderRailPipelineVerification::BuildPreview(
    const LevelSceneData& sceneData,
    const LevelRailRuntime& railRuntime,
    bool axisConversionEnabled,
    const std::string& reason) {
    ++buildCount_;
    lastBuildReason_ = reason;
    lastBuildTime_ = CurrentLocalTime();
    lastBuildError_.clear();
    adapterSucceeded_ = false;
    buildSucceeded_ = false;
    if (previewRuntime_) previewRuntime_->Clear();
    if (renderer_) renderer_->Clear();

    LevelRailSampleTable legacyTable;
    RefreshStageInformation(sceneData, railRuntime, axisConversionEnabled, &legacyTable);
    const LevelRail* rail = ResolveSelectedRail(sceneData);
    if (!rail) lastBuildError_ = "Railが選択されていません。";
    else if (rail->points.size() < 2) lastBuildError_ = "Waypoint数が2未満です。";
    else if (statistics_.nonFinitePointCount > 0) lastBuildError_ = "非有限値を検出しました。";
    if (!lastBuildError_.empty() || !previewRuntime_) {
        ++buildFailureCount_;
        lastBuildResult_ = "Runtime V2のBuildに失敗しました。";
        nextAction_ = lastBuildError_;
        return false;
    }
    adapterInputCount_ = convertedPoints_.size();
    adapterSucceeded_ = RailPathRuntimeV2Adapter::BuildFromWaypointPositions(
        *previewRuntime_, convertedPoints_, legacyTable, "診断Preview（軸変換・反転適用後）");
    buildSucceeded_ = adapterSucceeded_ && previewRuntime_->IsValid()
        && !previewRuntime_->GetArcLengthTable().empty() && previewRuntime_->GetTotalLength() > 0.0f;
    if (!buildSucceeded_) {
        ++buildFailureCount_;
        lastBuildResult_ = "Runtime V2のBuildに失敗しました。";
        lastBuildError_ = adapterSucceeded_
            ? "Arc Length Tableを構築できませんでした。"
            : "Adapter変換またはRuntime V2のBuildに失敗しました。";
        nextAction_ = "重複点、非有限値、Rail全長を確認してください。";
        return false;
    }

    ++buildSuccessCount_;
    buildRailId_ = rail->railId;
    buildRailName_ = rail->name;
    lastBuildResult_ = "Runtime V2 Build成功";
    nextAction_ = "ゲーム画面でシアン線、白いNode、黄緑Marker、緑Start、赤Endを確認してください。";
    std::vector<Vector3> runtimePoints;
    runtimePoints.reserve(previewRuntime_->GetNodes().size());
    for (const RailSplineNode& node : previewRuntime_->GetNodes()) runtimePoints.push_back(node.position);
    statistics_.runtimePoints = MakePointSummary(runtimePoints);
    RebuildRenderer();
    return true;
}

void BlenderRailPipelineVerification::SelectFirstValidRail(const LevelSceneData& sceneData) {
    for (size_t index = 0; index < sceneData.rails.size(); ++index) {
        if (!IsRailValid(sceneData.rails[index])) continue;
        selectedRailId_ = sceneData.rails[index].railId;
        if (selectedRailId_.empty()) selectedRailId_ = sceneData.rails[index].name;
        selectedRailIndex_ = index;
        return;
    }
    selectedRailId_.clear();
    selectedRailIndex_ = static_cast<size_t>(-1);
}

void BlenderRailPipelineVerification::SelectRelativeRail(const LevelSceneData& sceneData, int direction) {
    if (sceneData.rails.empty()) return;
    const size_t start = selectedRailIndex_ < sceneData.rails.size() ? selectedRailIndex_ : 0;
    for (size_t offset = 1; offset <= sceneData.rails.size(); ++offset) {
        const int raw = static_cast<int>(start) + direction * static_cast<int>(offset);
        const size_t index = static_cast<size_t>((raw % static_cast<int>(sceneData.rails.size())
            + static_cast<int>(sceneData.rails.size())) % static_cast<int>(sceneData.rails.size()));
        if (!IsRailValid(sceneData.rails[index])) continue;
        selectedRailId_ = sceneData.rails[index].railId.empty() ? sceneData.rails[index].name : sceneData.rails[index].railId;
        selectedRailIndex_ = index;
        return;
    }
}

const LevelRail* BlenderRailPipelineVerification::ResolveSelectedRail(
    const LevelSceneData& sceneData,
    size_t* outIndex) const {
    if (selectedRailId_.empty()) return nullptr;
    for (size_t index = 0; index < sceneData.rails.size(); ++index) {
        const LevelRail& rail = sceneData.rails[index];
        if (rail.railId != selectedRailId_ && rail.name != selectedRailId_) continue;
        if (outIndex) *outIndex = index;
        return &rail;
    }
    return nullptr;
}

void BlenderRailPipelineVerification::RebuildRenderer() {
    if (renderer_ && previewRuntime_) renderer_->Rebuild(*previewRuntime_, legacySampledPoints_);
}

void BlenderRailPipelineVerification::ResetDiagnostics() {
    const std::string requestedPath = requestedJsonPath_;
    const std::string resolvedPath = resolvedJsonPath_;
    const std::string jsonResult = lastJsonLoadResult_;
    const bool exists = jsonExists_;
    const bool loadSucceeded = jsonLoadSucceeded_;
    Clear();
    statistics_ = {};
    selectedRailId_.clear();
    selectedRailName_.clear();
    selectedRailType_.clear();
    buildRailId_.clear();
    buildRailName_.clear();
    selectedRailIndex_ = static_cast<size_t>(-1);
    railCount_ = 0;
    selectedWaypointCount_ = 0;
    adapterInputCount_ = 0;
    buildCount_ = buildSuccessCount_ = buildFailureCount_ = 0;
    dataSource_ = DataSource::Unknown;
    lastApplyReason_ = "未確認";
    lastApplyTime_ = "未確認";
    lastBuildReason_ = "未構築";
    lastBuildTime_ = "未構築";
    lastBuildResult_ = "未構築";
    nextAction_ = "読込情報だけ更新、またはレール連携を一括確認してください。";
    requestedJsonPath_ = requestedPath;
    resolvedJsonPath_ = resolvedPath;
    lastJsonLoadResult_ = jsonResult;
    jsonExists_ = exists;
    jsonLoadSucceeded_ = loadSucceeded;
}

bool BlenderRailPipelineVerification::IsRailValid(const LevelRail& rail) {
    return rail.points.size() >= 2
        && std::all_of(rail.points.begin(), rail.points.end(), IsFinite);
}

bool BlenderRailPipelineVerification::IsFinite(const Vector3& point) {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

BlenderRailPipelineVerification::PointSummary BlenderRailPipelineVerification::MakePointSummary(
    const std::vector<Vector3>& points) {
    PointSummary result;
    if (points.empty()) return result;
    result.valid = true;
    result.first = points.front();
    result.middle = points[points.size() / 2];
    result.last = points.back();
    result.maximumX = result.maximumY = result.maximumZ = points.front();
    result.boundsMinimum = result.boundsMaximum = points.front();
    for (const Vector3& point : points) {
        if (point.x > result.maximumX.x) result.maximumX = point;
        if (point.y > result.maximumY.y) result.maximumY = point;
        if (point.z > result.maximumZ.z) result.maximumZ = point;
        result.boundsMinimum.x = (std::min)(result.boundsMinimum.x, point.x);
        result.boundsMinimum.y = (std::min)(result.boundsMinimum.y, point.y);
        result.boundsMinimum.z = (std::min)(result.boundsMinimum.z, point.z);
        result.boundsMaximum.x = (std::max)(result.boundsMaximum.x, point.x);
        result.boundsMaximum.y = (std::max)(result.boundsMaximum.y, point.y);
        result.boundsMaximum.z = (std::max)(result.boundsMaximum.z, point.z);
    }
    result.boundsCenter = {
        (result.boundsMinimum.x + result.boundsMaximum.x) * 0.5f,
        (result.boundsMinimum.y + result.boundsMaximum.y) * 0.5f,
        (result.boundsMinimum.z + result.boundsMaximum.z) * 0.5f,
    };
    return result;
}

std::string BlenderRailPipelineVerification::CurrentLocalTime() {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
    localtime_s(&local, &now);
    std::ostringstream stream;
    stream << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

const char* BlenderRailPipelineVerification::DataSourceName(DataSource source) {
    if (source == DataSource::StageJson) return "Stage JSON";
    if (source == DataSource::UdpLiveSync) return "UDP Live Sync";
    return "未確認";
}
