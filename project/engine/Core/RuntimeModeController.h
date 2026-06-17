#pragma once

class Input;
class WinApp;

enum class RuntimeMode {
    Debug,
    Game,
};

class RuntimeModeController {
public:
    RuntimeModeController();
    ~RuntimeModeController();

    void Initialize(WinApp* winApp);
    void Update(Input* input);
    void DrawImGui();
    void DrawMenuBarImGui();

    RuntimeMode GetMode() const;
    bool IsDebugMode() const;
    bool IsGameMode() const;
    bool ShouldDrawDebugUi() const;
    bool ShouldDrawLevelDebug() const;
    bool ShouldDrawEventDebug() const;
    bool ShouldDrawRailDebug() const;
    bool ShouldKeepDockSpaceAlive() const;
    bool ShouldUseGameViewFullscreenPanel() const;
    bool ShouldUseWindowFullscreen() const;
    void RequestWindowFullscreen(bool fullscreen);

    static RuntimeModeController* GetActiveController();
    static void DrawActiveMenuBarImGui();

private:
    void SetMode(RuntimeMode mode);

    WinApp* winApp_ = nullptr;
    RuntimeMode mode_ = RuntimeMode::Game;
    bool requestedWindowFullscreen_ = false;

    static RuntimeModeController* activeController_;
};
