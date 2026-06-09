#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Camera;
class Model;
class Object3d;
class Object3dCommon;
struct LevelSceneData;
struct LevelEventRuntimeFlagState;

class LevelEventRuntime {
public:
    LevelEventRuntime();
    ~LevelEventRuntime();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Clear();
    void Rebuild(const LevelSceneData& sceneData, bool axisConversionEnabled);
    void Update(uint64_t frameCounter);
    void Draw(uint64_t frameCounter);
    void DrawImGui();

    bool IsFlagActive(const std::string& flagId) const;
    bool IsFlagFired(const std::string& flagId) const;
    bool ShouldShowFiredFlags() const { return showFiredFlags_; }
    size_t GetFiredEventCount() const;
    size_t GetActiveFlagCount() const;

private:
    void ResetRuntimeState();
    void FireFlag(LevelEventRuntimeFlagState& flag);
    void EnableFlag(const std::string& flagId, const std::string& sourceFlagId);
    void AddLog(const std::string& message);
    void UpdateDebugActorVisual(uint64_t frameCounter);
    void EnsureDebugActorVisual();

    std::vector<std::unique_ptr<LevelEventRuntimeFlagState>> flags_;
    std::unordered_map<std::string, size_t> flagIndexById_;
    std::vector<std::string> eventLog_;
    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    std::unique_ptr<Object3d> debugActorObject_;
    Model* debugActorModel_ = nullptr;
    Vector3 debugActorPosition_{ 0.0f, 0.5f, 0.0f };
    bool runtimeEnabled_ = false;
    bool showDebugActor_ = true;
    bool showFiredFlags_ = true;
    float debugActorRadius_ = 0.18f;
    uint64_t lastUpdateFrame_ = 0;
    uint64_t lastMatrixUpdateFrame_ = 0;
};
