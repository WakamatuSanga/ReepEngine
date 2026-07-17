#pragma once

#include "Engine/math/Matrix4x4.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class BlenderRailPipelineVerificationRenderer;
class Camera;
class LevelRailRuntime;
class Object3dCommon;
class RailPathRuntimeV2;
struct LevelRail;
struct LevelRailSampleTable;
struct LevelSceneData;

struct BlenderRailLiveSyncDiagnostics {
    bool receiverRunning = false;
    bool autoApplyEnabled = true;
    uint64_t receivedPacketCount = 0;
    uint64_t appliedPacketCount = 0;
    std::string lastReceiveTime;
    std::string lastApplyStatus;
    std::string lastError;
};

class BlenderRailPipelineVerification {
public:
    BlenderRailPipelineVerification();
    ~BlenderRailPipelineVerification();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Clear();
    void Draw();
    void DrawImGui(
        const LevelSceneData& sceneData, const LevelRailRuntime& railRuntime, bool axisConversionEnabled);
    void OnStageRebuilt(
        const LevelSceneData& sceneData,
        const LevelRailRuntime& railRuntime,
        bool axisConversionEnabled,
        const std::string& applySource);
    void SetStageJsonDiagnostics(
        const std::string& requestedPath,
        const std::string& resolvedPath,
        const std::string& loadResult,
        bool success);
    void SetLiveSyncDiagnostics(const BlenderRailLiveSyncDiagnostics& diagnostics);

private:
    enum class DataSource { Unknown, StageJson, UdpLiveSync };
    enum class PipelineState { Unknown, Ok, Warning, Error };

    struct PointSummary {
        bool valid = false;
        Vector3 first{};
        Vector3 middle{};
        Vector3 last{};
        Vector3 maximumX{};
        Vector3 maximumY{};
        Vector3 maximumZ{};
        Vector3 boundsMinimum{};
        Vector3 boundsMaximum{};
        Vector3 boundsCenter{};
    };

    struct RailStatistics {
        size_t validRailCount = 0;
        size_t invalidRailCount = 0;
        size_t nonFinitePointCount = 0;
        size_t consecutiveDuplicateCount = 0;
        bool zeroLengthCandidate = false;
        PointSummary loaderPoints{};
        PointSummary convertedPoints{};
        PointSummary runtimePoints{};
        float cameraToStartDistance = 0.0f;
        float cameraToCenterDistance = 0.0f;
    };

    void RefreshStageInformation(
        const LevelSceneData& sceneData, const LevelRailRuntime& railRuntime, bool axisConversionEnabled,
        LevelRailSampleTable* outSampleTable = nullptr);
    bool BuildPreview(
        const LevelSceneData& sceneData,
        const LevelRailRuntime& railRuntime,
        bool axisConversionEnabled,
        const std::string& reason);
    void SelectFirstValidRail(const LevelSceneData& sceneData);
    void SelectRelativeRail(const LevelSceneData& sceneData, int direction);
    const LevelRail* ResolveSelectedRail(const LevelSceneData& sceneData, size_t* outIndex = nullptr) const;
    void RebuildRenderer();
    void ResetDiagnostics();
    static bool IsRailValid(const LevelRail& rail);
    static bool IsFinite(const Vector3& point);
    static PointSummary MakePointSummary(const std::vector<Vector3>& points);
    static std::string CurrentLocalTime();
    static const char* DataSourceName(DataSource source);

    std::unique_ptr<RailPathRuntimeV2> previewRuntime_;
    std::unique_ptr<BlenderRailPipelineVerificationRenderer> renderer_;
    Camera* camera_ = nullptr;
    DataSource dataSource_ = DataSource::Unknown;
    BlenderRailLiveSyncDiagnostics liveSync_{};
    RailStatistics statistics_{};
    std::vector<Vector3> convertedPoints_;
    std::vector<Vector3> legacySampledPoints_;
    std::string selectedRailId_;
    std::string selectedRailName_;
    std::string selectedRailType_;
    std::string buildRailId_;
    std::string buildRailName_;
    std::string requestedJsonPath_;
    std::string resolvedJsonPath_;
    std::string lastJsonLoadResult_;
    std::string lastApplyReason_ = "未確認";
    std::string lastApplyTime_ = "未確認";
    std::string lastBuildReason_ = "未構築";
    std::string lastBuildTime_ = "未構築";
    std::string lastBuildResult_ = "未構築";
    std::string lastBuildError_;
    std::string lastRailReadError_;
    std::string nextAction_ = "Stage JSONを読み込むか、UDP Live SyncでRailを送信してください。";
    size_t selectedRailIndex_ = static_cast<size_t>(-1);
    size_t railCount_ = 0;
    size_t selectedWaypointCount_ = 0;
    size_t adapterInputCount_ = 0;
    uint64_t buildCount_ = 0;
    uint64_t buildSuccessCount_ = 0;
    uint64_t buildFailureCount_ = 0;
    float selectedRailSpeed_ = 0.0f;
    bool jsonExists_ = false;
    bool jsonLoadSucceeded_ = false;
    bool selectedRailLoop_ = false;
    bool selectedRailReverse_ = false;
    bool selectedRailVisible_ = false;
    bool coordinateConversionApplied_ = false;
    bool adapterSucceeded_ = false;
    bool buildSucceeded_ = false;
};
