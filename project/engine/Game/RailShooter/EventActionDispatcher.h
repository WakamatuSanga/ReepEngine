#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class LevelEventRuntime;
class LevelSceneRuntime;
class EnemySpawnActionBridge;
class EnemyWaveManager;
class PostEffectActionBridge;
class PrimitiveEffectSystem;
class RailShooterEventActionBridge;
class WarningUIController;

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
    class ActionHandler;
    template<class Target, auto HandleAction>
    class BridgeActionHandler;
    class PlayEffectHandler;

    void AddLog(const std::string& message);

    LevelEventRuntime* eventRuntime_ = nullptr;
    std::unordered_map<std::string, std::unique_ptr<ActionHandler>> handlers_;
    std::vector<std::string> dispatchLog_;
    std::string lastActionType_ = "(none)";
    std::string lastTarget_ = "(none)";
    std::string lastResult_ = "(none)";
    bool enabled_ = true;
    size_t consumedActionCount_ = 0;
    size_t dispatchedActionCount_ = 0;
    size_t missingTargetCount_ = 0;
};
