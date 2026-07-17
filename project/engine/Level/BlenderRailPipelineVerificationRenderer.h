#pragma once

#include <cstddef>
#include <memory>
#include <vector>

class Camera;
class Model;
class Object3d;
class Object3dCommon;
class RailPathRuntimeV2;
struct Vector3;

struct BlenderRailVerificationRenderConfig {
    bool enabled = true;
    bool showRuntimeLine = true;
    bool showNodes = true;
    bool showDistanceMarkers = true;
    bool showStartEnd = true;
    bool showLegacyLine = false;
    bool showTangents = false;
    float markerInterval = 50.0f;
    float tangentInterval = 50.0f;
    float tangentLength = 8.0f;
    float lineThickness = 0.035f;
    float nodeScale = 0.10f;
    float markerScale = 0.12f;
    float endpointScale = 0.20f;
    size_t maximumLineCount = 4096;
    size_t maximumNodeCount = 256;
    size_t maximumMarkerCount = 512;
    size_t maximumTangentCount = 256;
};

struct BlenderRailVerificationRenderStats {
    size_t lineCount = 0;
    size_t nodeCount = 0;
    size_t markerCount = 0;
    size_t tangentCount = 0;
    size_t endpointCount = 0;
    size_t skippedLineCount = 0;
    size_t skippedNodeCount = 0;
    size_t skippedMarkerCount = 0;
    size_t skippedTangentCount = 0;
};

class BlenderRailPipelineVerificationRenderer {
public:
    BlenderRailPipelineVerificationRenderer();
    ~BlenderRailPipelineVerificationRenderer();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Clear();
    void Rebuild(
        const RailPathRuntimeV2& runtime,
        const std::vector<Vector3>& legacyPoints);
    void Draw();

    BlenderRailVerificationRenderConfig& GetConfig() { return config_; }
    const BlenderRailVerificationRenderConfig& GetConfig() const { return config_; }
    const BlenderRailVerificationRenderStats& GetStats() const { return stats_; }

private:
    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    Model* runtimeLineModel_ = nullptr;
    Model* legacyLineModel_ = nullptr;
    Model* nodeModel_ = nullptr;
    Model* markerModel_ = nullptr;
    Model* tangentModel_ = nullptr;
    std::vector<std::unique_ptr<Object3d>> runtimeLines_;
    std::vector<std::unique_ptr<Object3d>> legacyLines_;
    std::vector<std::unique_ptr<Object3d>> nodes_;
    std::vector<std::unique_ptr<Object3d>> markers_;
    std::vector<std::unique_ptr<Object3d>> tangents_;
    std::unique_ptr<Object3d> startMarker_;
    std::unique_ptr<Object3d> endMarker_;
    BlenderRailVerificationRenderConfig config_{};
    BlenderRailVerificationRenderStats stats_{};
};
