#pragma once
#include <cstddef>
#include <cstdint>

class Camera;
class Input;
class WinApp;

enum class RuntimeMode {
    Debug,
    Game,
};

class RuntimeModeController {
public:
    struct PerformanceStats {
        int windowWidth = 0;
        int windowHeight = 0;
        uint32_t renderTextureWidth = 0;
        uint32_t renderTextureHeight = 0;
        uint32_t depthTextureWidth = 0;
        uint32_t depthTextureHeight = 0;
        float currentViewportWidth = 0.0f;
        float currentViewportHeight = 0.0f;
        int currentScissorWidth = 0;
        int currentScissorHeight = 0;
        float backBufferViewportWidth = 0.0f;
        float backBufferViewportHeight = 0.0f;
        int backBufferScissorWidth = 0;
        int backBufferScissorHeight = 0;
        float gameViewportWidth = 0.0f;
        float gameViewportHeight = 0.0f;
        float internalRenderScale = 1.0f;
        float cloudResolutionScale = 1.0f;
        bool cloudEnabled = false;
        bool lowResolutionCloudEnabled = false;
        bool cloudCompositeEnabled = false;
        bool depthAwareCloudUpsampleEnabled = false;
        size_t activeEnemyCount = 0;
        size_t enemyBulletCount = 0;
        size_t playerBulletCount = 0;
        size_t primitiveEffectCount = 0;
        uint32_t gpuParticleActiveEstimate = 0;
        uint32_t presentInterval = 1;
        bool fixedFpsWaitEnabled = true;
    };

    struct ShadowLikeDebugSettings {
        bool disableClouds = false;
        bool disableCloudComposite = false;
        bool disableDepthAwareUpsample = false;
        bool disablePostEffects = false;
        bool disableEffects = false;
        bool disableFakeShadow = false;
        bool disablePrimitiveEffect = false;
        bool disableGpuParticle = false;
        bool disablePbrLighting = false;
        bool clearColorTest = false;
    };

    RuntimeModeController();
    ~RuntimeModeController();

    void Initialize(WinApp* winApp);
    void Update(Input* input);
    void DrawImGui();
    void DrawMenuBarImGui();
    void BeginCameraModeSwitchDiagnostics(const Camera* camera);
    void EndCameraModeSwitchDiagnostics(Camera* camera, bool projectionUpdated);

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
    void SetPerformanceStats(const PerformanceStats& stats);
    float GetDesiredRenderScale() const;
    bool ShouldAutoApplyGameModePerformancePreset() const { return autoApplyGameModePerformancePreset_; }
    const ShadowLikeDebugSettings& GetShadowLikeDebugSettings() const { return shadowLikeDebugSettings_; }

    static RuntimeModeController* GetActiveController();
    static void DrawActiveMenuBarImGui();

private:
    struct CameraPoseSnapshot {
        float position[3]{};
        float rotation[3]{};
        float forward[3]{};
        float fovY = 0.0f;
        float aspectRatio = 0.0f;
        float nearClip = 0.0f;
        float farClip = 0.0f;
        bool valid = false;
    };

    void SetMode(RuntimeMode mode);
    CameraPoseSnapshot CaptureCameraPose(const Camera* camera) const;
    bool HasCameraPoseChanged(const CameraPoseSnapshot& before, const CameraPoseSnapshot& after) const;
    void RestoreCameraPose(Camera* camera, const CameraPoseSnapshot& pose) const;
    const char* GetCurrentSuspectedShadowSource() const;

    WinApp* winApp_ = nullptr;
    PerformanceStats performanceStats_{};
    RuntimeMode mode_ = RuntimeMode::Game;
    float gameModeRenderScale_ = 1.0f;
    bool requestedWindowFullscreen_ = false;
    bool useGameModeRenderScale_ = true;
    bool autoApplyGameModePerformancePreset_ = false;
    bool preserveCameraPoseOnModeSwitch_ = true;
    bool cameraModeSwitchChangedThisFrame_ = false;
    bool cameraProjectionUpdatedOnModeSwitch_ = false;
    bool cameraPoseRestoredOnModeSwitch_ = false;
    RuntimeMode modeBeforeCameraDiagnostics_ = RuntimeMode::Debug;
    CameraPoseSnapshot cameraPoseBeforeModeSwitch_{};
    CameraPoseSnapshot cameraPoseAfterModeSwitch_{};
    ShadowLikeDebugSettings shadowLikeDebugSettings_{};

    static RuntimeModeController* activeController_;
};
