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

struct FiredEventAction {
    std::string eventFlagId;
    std::string eventDisplayName;
    std::string actionType;
    std::string targetObjectName;
    std::string targetObjectId;
    std::string actionDescription;
    std::string postEffectType;
    std::string waveId;
};

enum class LevelEventTriggerSource {
    DebugActor,
    Player,
    Both,
};

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
    size_t GetPendingActionCount() const { return pendingActions_.size(); }
    std::vector<FiredEventAction> ConsumePendingActions();
    void SetPlayerTriggerState(bool available, const Vector3& position, float radius);
    bool IsPlayerInsideEventFlag() const { return isPlayerInsideEventFlag_; }
    const Vector3& GetPlayerTriggerPosition() const { return playerTriggerPosition_; }
    float GetPlayerTriggerRadius() const { return playerTriggerRadius_; }
    const std::string& GetLastTriggeredBy() const { return lastTriggeredBy_; }

private:
    void ResetRuntimeState();
    void FireFlag(LevelEventRuntimeFlagState& flag, const std::string& triggerSourceName);
    void EnableFlag(const std::string& flagId, const std::string& sourceFlagId);
    void AddLog(const std::string& message);
    bool TestFlagIntersection(const LevelEventRuntimeFlagState& flag, const Vector3& position, float radius) const;
    void UpdateDebugActorVisual(uint64_t frameCounter);
    void EnsureDebugActorVisual();

    std::vector<std::unique_ptr<LevelEventRuntimeFlagState>> flags_;
    std::unordered_map<std::string, size_t> flagIndexById_;
    std::vector<std::string> eventLog_;
    std::vector<FiredEventAction> pendingActions_;
    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    std::unique_ptr<Object3d> debugActorObject_;
    Model* debugActorModel_ = nullptr;
    Vector3 debugActorPosition_{ 0.0f, 0.5f, 0.0f };
    Vector3 playerTriggerPosition_{ 0.0f, 0.0f, 0.0f };
    bool runtimeEnabled_ = false;
    bool showDebugActor_ = true;
    bool showFiredFlags_ = true;
    bool playerTriggerAvailable_ = false;
    bool isPlayerInsideEventFlag_ = false;
    float debugActorRadius_ = 0.18f;
    float playerTriggerRadius_ = 0.25f;
    LevelEventTriggerSource triggerSource_ = LevelEventTriggerSource::DebugActor;
    std::string lastTriggeredBy_ = "(none)";
    uint64_t lastUpdateFrame_ = 0;
    uint64_t lastMatrixUpdateFrame_ = 0;
};
