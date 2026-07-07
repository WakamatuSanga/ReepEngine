#pragma once
#include <cstddef>
#include <string>
#include <vector>

class LevelEventRuntime;
class LevelSceneRuntime;
class EnemySpawnActionBridge;
class EnemyWaveManager;
class PostEffectActionBridge;
class PrimitiveEffectSystem;
class RailShooterEventActionBridge;
class WarningUIController;
struct FiredEventAction;

class EventActionDispatcher {
public:
    EventActionDispatcher();
    ~EventActionDispatcher();

    void Initialize(
        LevelEventRuntime* eventRuntime,
        RailShooterEventActionBridge* cameraRailBridge,
        EnemySpawnActionBridge* enemySpawnBridge,
        EnemyWaveManager* enemyWaveManager,
        PostEffectActionBridge* postEffectBridge,
        PrimitiveEffectSystem* primitiveEffectSystem,
        LevelSceneRuntime* levelSceneRuntime,
        WarningUIController* warningUIController);
    void Finalize();
    void Update();
    void DrawImGui();

private:
    void AddLog(const std::string& message);
    bool DispatchPlayEffect(const FiredEventAction& action, std::string& resultMessage);
    static bool IsActionType(const std::string& actionType, const char* expectedLower);

    LevelEventRuntime* eventRuntime_ = nullptr;
    RailShooterEventActionBridge* cameraRailBridge_ = nullptr;
    EnemySpawnActionBridge* enemySpawnBridge_ = nullptr;
    EnemyWaveManager* enemyWaveManager_ = nullptr;
    PostEffectActionBridge* postEffectBridge_ = nullptr;
    PrimitiveEffectSystem* primitiveEffectSystem_ = nullptr;
    LevelSceneRuntime* levelSceneRuntime_ = nullptr;
    WarningUIController* warningUIController_ = nullptr;
    std::vector<std::string> dispatchLog_;
    std::string lastActionType_ = "(none)";
    std::string lastTarget_ = "(none)";
    std::string lastResult_ = "(none)";
    bool enabled_ = true;
    size_t consumedActionCount_ = 0;
    size_t dispatchedActionCount_ = 0;
    size_t missingTargetCount_ = 0;
};