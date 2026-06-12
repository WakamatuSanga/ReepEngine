#pragma once
#include <cstddef>
#include <string>
#include <vector>

class LevelEventRuntime;
class LevelSceneRuntime;
class PrimitiveEffectSystem;
class RailShooterEventActionBridge;
struct FiredEventAction;

class EventActionDispatcher {
public:
    EventActionDispatcher();
    ~EventActionDispatcher();

    void Initialize(
        LevelEventRuntime* eventRuntime,
        RailShooterEventActionBridge* cameraRailBridge,
        PrimitiveEffectSystem* primitiveEffectSystem,
        LevelSceneRuntime* levelSceneRuntime);
    void Finalize();
    void Update();
    void DrawImGui();

private:
    void AddLog(const std::string& message);
    bool DispatchPlayEffect(const FiredEventAction& action, std::string& resultMessage);
    static bool IsActionType(const std::string& actionType, const char* expectedLower);

    LevelEventRuntime* eventRuntime_ = nullptr;
    RailShooterEventActionBridge* cameraRailBridge_ = nullptr;
    PrimitiveEffectSystem* primitiveEffectSystem_ = nullptr;
    LevelSceneRuntime* levelSceneRuntime_ = nullptr;
    std::vector<std::string> dispatchLog_;
    std::string lastActionType_ = "(none)";
    std::string lastResult_ = "(none)";
    bool enabled_ = true;
    size_t consumedActionCount_ = 0;
    size_t dispatchedActionCount_ = 0;
    size_t missingTargetCount_ = 0;
};
