#pragma once

#include "Engine/math/Matrix4x4.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Camera;
class Model;
class Object3d;
class Object3dCommon;

struct RailSplineNode {
    Vector3 position{};
};

struct RailArcLengthSample {
    float cumulativeDistance = 0.0f;
    uint32_t segmentIndex = 0;
    float segmentT = 0.0f;
    Vector3 position{};
};

struct RailPathSample {
    Vector3 position{};
    Vector3 tangent{ 0.0f, 0.0f, 1.0f };
    float distance = 0.0f;
    float normalizedDistance = 0.0f;
    uint32_t segmentIndex = 0;
    float segmentT = 0.0f;
    bool valid = false;
};

class RailPathRuntimeV2 {
public:
    RailPathRuntimeV2();
    ~RailPathRuntimeV2();

    void InitializeDebug(Object3dCommon* object3dCommon, Camera* camera);
    void Finalize();
    bool Build(const std::vector<RailSplineNode>& nodes);
    bool Rebuild();
    void Clear();

    bool IsValid() const { return valid_; }
    bool IsRuntimeEnabled() const { return runtimeEnabled_; }
    float GetTotalLength() const { return totalLength_; }
    size_t GetNodeCount() const { return nodes_.size(); }
    size_t GetSegmentCount() const { return nodes_.size() >= 2 ? nodes_.size() - 1 : 0; }
    size_t GetArcLengthSampleCount() const { return arcLengthTable_.size(); }
    size_t GetDuplicateNodeSkipCount() const { return duplicateNodeSkipCount_; }
    int GetSamplesPerSegment() const { return samplesPerSegment_; }
    const std::vector<RailSplineNode>& GetNodes() const { return nodes_; }
    const std::vector<RailArcLengthSample>& GetArcLengthTable() const { return arcLengthTable_; }

    RailPathSample SampleByDistance(float distance) const;
    RailPathSample SampleByNormalizedDistance(float normalizedDistance) const;
    Vector3 EvaluatePosition(uint32_t segmentIndex, float segmentT) const;
    Vector3 EvaluateTangent(uint32_t segmentIndex, float segmentT) const;

    void SetLegacyComparisonData(
        const std::vector<Vector3>& sampledPoints,
        const std::vector<float>& cumulativeDistances,
        float totalLength,
        const std::string& adapterMode);
    void SetExternalDebugHidden(bool hidden);
    void DrawDebug();
    void DrawImGui();

private:
    bool BuildArcLengthTable();
    bool ValidateBuild();
    void RunDistanceValidation();
    void RunLegacyComparison();
    Vector3 EvaluateGlobalParameter(float globalParameter) const;
    Vector3 SampleLegacyByNormalizedDistance(float normalizedDistance) const;
    static bool IsFinite(const Vector3& value);

    void ClearDebugObjects();
    void RebuildDebugObjects();
    void DrawNodeIndexOverlay() const;

    std::vector<RailSplineNode> sourceNodes_;
    std::vector<RailSplineNode> nodes_;
    std::vector<RailArcLengthSample> arcLengthTable_;
    std::vector<Vector3> legacySampledPoints_;
    std::vector<float> legacyCumulativeDistances_;

    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    std::vector<std::unique_ptr<Object3d>> runtimeLineObjects_;
    std::vector<std::unique_ptr<Object3d>> legacyLineObjects_;
    std::vector<std::unique_ptr<Object3d>> nodeObjects_;
    std::vector<std::unique_ptr<Object3d>> distanceMarkerObjects_;
    std::unique_ptr<Object3d> tangentObject_;
    Model* runtimeLineModel_ = nullptr;
    Model* legacyLineModel_ = nullptr;
    Model* nodeModel_ = nullptr;
    Model* distanceMarkerModel_ = nullptr;
    Model* tangentModel_ = nullptr;

    std::string lastBuildResult_ = "未構築";
    std::string lastError_;
    std::string adapterMode_ = "未接続";
    float totalLength_ = 0.0f;
    float duplicateNodeDistanceEpsilon_ = 0.001f;
    float alpha_ = 0.5f;
    int samplesPerSegment_ = 32;
    size_t duplicateNodeSkipCount_ = 0;
    bool runtimeEnabled_ = true;
    bool valid_ = false;
    bool arcTableValid_ = false;
    bool cumulativeDistanceMonotonic_ = false;
    bool openRail_ = true;

    float previewDistance_ = 0.0f;
    float previewNormalizedDistance_ = 0.0f;
    RailPathSample previewSample_{};

    float validationInterval_ = 1.0f;
    size_t validationSampleCount_ = 0;
    float validationMinimumStep_ = 0.0f;
    float validationMaximumStep_ = 0.0f;
    float validationAverageStep_ = 0.0f;
    float validationMaximumError_ = 0.0f;
    bool validationSucceeded_ = false;

    float legacyTotalLength_ = 0.0f;
    int comparisonSampleCount_ = 64;
    float comparisonStartDifference_ = 0.0f;
    float comparisonEndDifference_ = 0.0f;
    float comparisonLengthDifference_ = 0.0f;
    float comparisonMaximumDifference_ = 0.0f;
    float comparisonAverageDifference_ = 0.0f;

    bool debugDrawEnabled_ = false;
    bool externalDebugHidden_ = false;
    bool showNodes_ = true;
    bool showNodeIndices_ = false;
    bool showRuntimeLine_ = true;
    bool showLegacyLine_ = false;
    bool showDistanceMarkers_ = true;
    bool showTangent_ = true;
    float tangentLength_ = 2.0f;
    float distanceMarkerInterval_ = 5.0f;
    float lineThickness_ = 0.035f;
    float nodeScale_ = 0.10f;
    float markerScale_ = 0.075f;
    int maximumDebugDrawCount_ = 512;
    size_t lastDebugDrawCount_ = 0;
};
