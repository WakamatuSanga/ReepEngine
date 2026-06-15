#pragma once
#include "Engine/Level/LevelRailEvaluator.h"
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
struct LevelRailRuntimeRail;
struct LevelSceneData;

struct LevelRailEvaluation {
    bool valid = false;
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    Vector3 forward{ 0.0f, 0.0f, 1.0f };
    size_t segmentIndex = 0;
    float totalLength = 0.0f;
    float distance = 0.0f;
    float t = 0.0f;
    float distanceToPoint = 0.0f;
};

struct LevelRailRuntimeRailInfo {
    std::string railId;
    std::string name;
    std::string railType;
    bool loop = false;
    bool reverseDirection = false;
    float speed = 1.0f;
    float totalLength = 0.0f;
    size_t pointCount = 0;
    size_t sampledPointCount = 0;
};

class LevelRailRuntime {
public:
    LevelRailRuntime();
    ~LevelRailRuntime();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Clear();
    void Rebuild(const LevelSceneData& sceneData, bool axisConversionEnabled, uint64_t frameCounter = 0);
    void Update(float deltaTime, uint64_t frameCounter);
    void Draw(uint64_t frameCounter);
    bool DrawImGui();
    void SetExternalDebugActorHidden(bool hidden);

    LevelRailEvaluation EvaluateByT(const std::string& railId, float t) const;
    LevelRailEvaluation EvaluateByDistance(const std::string& railId, float distance) const;
    LevelRailEvaluation EvaluateByDistance(const std::string& railId, float distance, bool loopEnabled) const;
    LevelRailEvaluation FindClosestEvaluation(const std::string& railId, const Vector3& position, bool loopEnabled) const;
    size_t GetRailCount() const;
    bool GetRailInfo(size_t index, LevelRailRuntimeRailInfo& outInfo) const;

private:
    const LevelRailRuntimeRail* FindRailById(const std::string& railId) const;
    const LevelRailRuntimeRail* GetSelectedRail() const;
    void SyncSelectionDefaults();
    void UpdateCurrentEvaluation();
    void UpdateDebugActorVisual(uint64_t frameCounter);
    void EnsureDebugActorVisual();
    void ResetRailActor();

    std::vector<std::unique_ptr<LevelRailRuntimeRail>> rails_;
    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    std::unique_ptr<Object3d> actorObject_;
    std::unique_ptr<Object3d> forwardObject_;
    Model* actorModel_ = nullptr;
    Model* forwardModel_ = nullptr;
    LevelRailEvaluation currentEvaluation_;
    bool runtimeEnabled_ = true;
    bool showDebugRailActor_ = true;
    bool externalHideDebugActor_ = false;
    bool autoPlay_ = false;
    bool debugLoop_ = false;
    bool updateMatricesWithLatestCamera_ = true;
    int selectedRailIndex_ = 0;
    int previousSelectedRailIndex_ = -1;
    int moveMode_ = 0;
    float railT_ = 0.0f;
    float railDistance_ = 0.0f;
    float playSpeed_ = 1.0f;
    float forwardLookAheadDistance_ = 0.25f;
    float actorScale_ = 0.16f;
    float forwardLength_ = 0.55f;
    float forwardThickness_ = 0.045f;
    RailInterpolationMode interpolationMode_ = RailInterpolationMode::CatmullRom;
    bool useSmoothedRailEvaluation_ = true;
    int smoothingSubdivisionsPerSegment_ = 8;
    uint64_t rebuildCount_ = 0;
    uint64_t lastRebuildFrame_ = 0;
    uint64_t lastUpdateFrame_ = 0;
    uint64_t lastMatrixUpdateFrame_ = 0;
};
