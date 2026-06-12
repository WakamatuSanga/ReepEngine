#pragma once
#include <cstddef>
#include <string>
#include <vector>

class LevelEventRuntime;
class RailShooterCameraRig;
struct FiredEventAction;

class RailShooterEventActionBridge {
public:
    RailShooterEventActionBridge();
    ~RailShooterEventActionBridge();

    void Initialize(LevelEventRuntime* eventRuntime, RailShooterCameraRig* cameraRig);
    void Finalize();
    void Update();
    void DrawImGui();
    bool HandleAction(const FiredEventAction& action, std::string& resultMessage);

private:
    void AddLog(const std::string& message);
    static bool IsStartCameraRailAction(const std::string& actionType);

    LevelEventRuntime* eventRuntime_ = nullptr;
    RailShooterCameraRig* cameraRig_ = nullptr;
    std::vector<std::string> actionLog_;
    std::string lastActionType_ = "(none)";
    std::string lastTarget_ = "(none)";
    std::string lastResult_ = "(none)";
    bool enableBridge_ = true;
    size_t consumedActionCount_ = 0;
};
