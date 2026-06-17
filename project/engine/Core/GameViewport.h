#pragma once
#include "Engine/math/Matrix4x4.h"
#include <string>

class Camera;
class WinApp;

class GameViewport {
public:
    enum class Mode {
        ImGuiGameView,
        FullscreenGame,
    };

    struct Rect {
        float left = 0.0f;
        float top = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    struct Ray {
        Vector3 origin{ 0.0f, 0.0f, 0.0f };
        Vector3 direction{ 0.0f, 0.0f, 1.0f };
        bool valid = false;
    };

    void Initialize(WinApp* winApp);
    void BeginFrame(bool fullscreenGameMode);
    void SetRenderTargetSize(float width, float height);
    void SetImGuiGameViewRect(float left, float top, float width, float height, bool hovered, bool focused);
    void ClearImGuiGameViewRect();
    void DrawImGui();

    bool IsGameInputActive(bool editingImGuiInput, bool editorCameraFlyActive, bool gameplayBlocked);
    Ray GetMouseRayFromCamera(const Camera& camera) const;

    const Rect& GetGameViewportRect() const { return gameViewportRect_; }
    Mode GetMode() const { return mode_; }
    bool IsMouseInGameViewport() const { return mouseInGameViewport_; }
    bool IsGameViewHovered() const { return gameViewHovered_; }
    bool IsGameViewFocused() const { return gameViewFocused_; }
    const Vector2& GetMouseScreenPosition() const { return mouseScreenPosition_; }
    const Vector2& GetMouseLocalPosition() const { return mouseLocalPosition_; }
    const Vector2& GetMouseNormalizedInGameViewport() const { return mouseNormalizedPosition_; }
    const Vector2& GetMouseNdcInGameViewport() const { return mouseNdcPosition_; }
    const std::string& GetInputBlockedReason() const { return inputBlockedReason_; }

private:
    void SetFullscreenGameRect();
    void UpdateMouseState();

    WinApp* winApp_ = nullptr;
    Rect gameViewportRect_{};
    Vector2 renderTargetSize_{ 0.0f, 0.0f };
    Vector2 mouseScreenPosition_{ 0.0f, 0.0f };
    Vector2 mouseLocalPosition_{ 0.0f, 0.0f };
    Vector2 mouseNormalizedPosition_{ 0.0f, 0.0f };
    Vector2 mouseNdcPosition_{ 0.0f, 0.0f };
    Mode mode_ = Mode::ImGuiGameView;
    bool mouseInGameViewport_ = false;
    bool gameViewHovered_ = false;
    bool gameViewFocused_ = false;
    bool hasImGuiGameViewRect_ = false;
    std::string inputBlockedReason_ = "Not updated";
};
