#include "GameViewport.h"
#include "Engine/Core/WinApp.h"
#include "Engine/Graphics/Camera/Camera.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kMinViewportSize = 1.0f;
    constexpr float kMinVectorLength = 0.00001f;

    const char* ToModeLabel(GameViewport::Mode mode) {
        switch (mode) {
        case GameViewport::Mode::FullscreenGame:
            return "FullscreenGame";
        case GameViewport::Mode::ImGuiGameView:
        default:
            return "ImGuiGameView";
        }
    }

    Vector3 SubtractVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    }

    float Length(const Vector3& value) {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    }

    Vector3 Normalize(const Vector3& value, const Vector3& fallback) {
        const float length = Length(value);
        if (length <= kMinVectorLength || !std::isfinite(length)) {
            return fallback;
        }
        return { value.x / length, value.y / length, value.z / length };
    }

    Vector4 TransformClipPoint(const Vector4& value, const Matrix4x4& matrix) {
        return {
            value.x * matrix.m[0][0] + value.y * matrix.m[1][0] + value.z * matrix.m[2][0] + value.w * matrix.m[3][0],
            value.x * matrix.m[0][1] + value.y * matrix.m[1][1] + value.z * matrix.m[2][1] + value.w * matrix.m[3][1],
            value.x * matrix.m[0][2] + value.y * matrix.m[1][2] + value.z * matrix.m[2][2] + value.w * matrix.m[3][2],
            value.x * matrix.m[0][3] + value.y * matrix.m[1][3] + value.z * matrix.m[2][3] + value.w * matrix.m[3][3],
        };
    }

    bool ToWorldPosition(const Vector4& clip, const Matrix4x4& inverseViewProjection, Vector3& outPosition) {
        const Vector4 world = TransformClipPoint(clip, inverseViewProjection);
        if (std::fabs(world.w) <= kMinVectorLength || !std::isfinite(world.w)) {
            return false;
        }
        outPosition = { world.x / world.w, world.y / world.w, world.z / world.w };
        return
            std::isfinite(outPosition.x) &&
            std::isfinite(outPosition.y) &&
            std::isfinite(outPosition.z);
    }
}

void GameViewport::Initialize(WinApp* winApp) {
    winApp_ = winApp;
    ClearImGuiGameViewRect();
}

void GameViewport::BeginFrame(bool fullscreenGameMode) {
    mode_ = fullscreenGameMode ? Mode::FullscreenGame : Mode::ImGuiGameView;
    if (mode_ == Mode::FullscreenGame) {
        SetFullscreenGameRect();
        gameViewHovered_ = true;
        gameViewFocused_ = true;
    } else if (!hasImGuiGameViewRect_) {
        gameViewportRect_ = {};
        gameViewHovered_ = false;
        gameViewFocused_ = false;
    }

    UpdateMouseState();
}

void GameViewport::SetRenderTargetSize(float width, float height) {
    renderTargetSize_ = {
        (std::max)(0.0f, width),
        (std::max)(0.0f, height),
    };
}

void GameViewport::SetImGuiGameViewRect(float left, float top, float width, float height, bool hovered, bool focused) {
    hasImGuiGameViewRect_ = width > kMinViewportSize && height > kMinViewportSize;
    gameViewportRect_ = {
        left,
        top,
        (std::max)(0.0f, width),
        (std::max)(0.0f, height),
    };
    gameViewHovered_ = hovered;
    gameViewFocused_ = focused;
    if (mode_ == Mode::ImGuiGameView) {
        UpdateMouseState();
    }
}

void GameViewport::ClearImGuiGameViewRect() {
    hasImGuiGameViewRect_ = false;
    if (mode_ == Mode::ImGuiGameView) {
        gameViewportRect_ = {};
    }
    gameViewHovered_ = false;
    gameViewFocused_ = false;
    UpdateMouseState();
}

void GameViewport::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(360.0f, 260.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("ゲームビューポート確認 (GameViewport Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Viewport Mode: %s", ToModeLabel(mode_));
    ImGui::Text("Game View Rect: L %.1f T %.1f W %.1f H %.1f",
        gameViewportRect_.left,
        gameViewportRect_.top,
        gameViewportRect_.width,
        gameViewportRect_.height);
    ImGui::Text("Render Target Size: %.1f x %.1f", renderTargetSize_.x, renderTargetSize_.y);
    ImGui::Text("Mouse Screen: %.1f, %.1f", mouseScreenPosition_.x, mouseScreenPosition_.y);
    ImGui::Text("Mouse Local: %.1f, %.1f", mouseLocalPosition_.x, mouseLocalPosition_.y);
    ImGui::Text("Mouse Normalized: %.3f, %.3f", mouseNormalizedPosition_.x, mouseNormalizedPosition_.y);
    ImGui::Text("Mouse NDC: %.3f, %.3f", mouseNdcPosition_.x, mouseNdcPosition_.y);
    ImGui::Text("Mouse In Game View: %s", mouseInGameViewport_ ? "true" : "false");
    ImGui::Text("Game View Hovered / Focused: %s / %s",
        gameViewHovered_ ? "true" : "false",
        gameViewFocused_ ? "true" : "false");
    ImGui::TextWrapped("Input Blocked Reason: %s", inputBlockedReason_.c_str());
    ImGui::End();
#endif
}

bool GameViewport::IsGameInputActive(bool editingImGuiInput, bool editorCameraFlyActive, bool gameplayBlocked) {
    if (gameViewportRect_.width <= kMinViewportSize || gameViewportRect_.height <= kMinViewportSize) {
        inputBlockedReason_ = "Game viewport rect is invalid";
        return false;
    }
    if (!mouseInGameViewport_) {
        inputBlockedReason_ = "Mouse is outside game viewport";
        return false;
    }
    if (editingImGuiInput) {
        inputBlockedReason_ = "ImGui text/input widget is active";
        return false;
    }
    if (editorCameraFlyActive) {
        inputBlockedReason_ = "Editor camera fly is active";
        return false;
    }
    if (gameplayBlocked) {
        inputBlockedReason_ = "Gameplay input is blocked";
        return false;
    }
    inputBlockedReason_ = "None";
    return true;
}

GameViewport::Ray GameViewport::GetMouseRayFromCamera(const Camera& camera) const {
    Ray ray{};
    if (!mouseInGameViewport_ || gameViewportRect_.width <= kMinViewportSize || gameViewportRect_.height <= kMinViewportSize) {
        return ray;
    }

    const Matrix4x4 inverseViewProjection = MatrixMath::Inverse(camera.GetViewProjectionMatrix());
    Vector3 nearWorld{};
    Vector3 farWorld{};
    if (!ToWorldPosition({ mouseNdcPosition_.x, mouseNdcPosition_.y, 0.0f, 1.0f }, inverseViewProjection, nearWorld) ||
        !ToWorldPosition({ mouseNdcPosition_.x, mouseNdcPosition_.y, 1.0f, 1.0f }, inverseViewProjection, farWorld)) {
        return ray;
    }

    ray.origin = camera.GetTranslate();
    ray.direction = Normalize(SubtractVector3(farWorld, nearWorld), { 0.0f, 0.0f, 1.0f });
    ray.valid = true;
    return ray;
}

void GameViewport::SetFullscreenGameRect() {
    float clientWidth = 0.0f;
    float clientHeight = 0.0f;
    if (winApp_) {
        clientWidth = static_cast<float>(winApp_->GetClientWidth());
        clientHeight = static_cast<float>(winApp_->GetClientHeight());
    }

    float menuBarHeight = 0.0f;
#ifdef USE_IMGUI
    if (ImGui::GetCurrentContext()) {
        menuBarHeight = ImGui::GetFrameHeight();
    }
#endif

    gameViewportRect_ = {
        0.0f,
        menuBarHeight,
        (std::max)(0.0f, clientWidth),
        (std::max)(0.0f, clientHeight - menuBarHeight),
    };
}

void GameViewport::UpdateMouseState() {
    mouseScreenPosition_ = { 0.0f, 0.0f };
    mouseLocalPosition_ = { 0.0f, 0.0f };
    mouseNormalizedPosition_ = { 0.0f, 0.0f };
    mouseNdcPosition_ = { 0.0f, 0.0f };
    mouseInGameViewport_ = false;

    if (!winApp_ || !winApp_->GetHwnd() ||
        gameViewportRect_.width <= kMinViewportSize ||
        gameViewportRect_.height <= kMinViewportSize) {
        return;
    }

    POINT cursor{};
    if (!GetCursorPos(&cursor)) {
        return;
    }
    if (!ScreenToClient(winApp_->GetHwnd(), &cursor)) {
        return;
    }

    mouseScreenPosition_ = {
        static_cast<float>(cursor.x),
        static_cast<float>(cursor.y),
    };
    mouseLocalPosition_ = {
        mouseScreenPosition_.x - gameViewportRect_.left,
        mouseScreenPosition_.y - gameViewportRect_.top,
    };
    mouseInGameViewport_ =
        mouseLocalPosition_.x >= 0.0f &&
        mouseLocalPosition_.y >= 0.0f &&
        mouseLocalPosition_.x <= gameViewportRect_.width &&
        mouseLocalPosition_.y <= gameViewportRect_.height;

    if (!mouseInGameViewport_) {
        return;
    }

    mouseNormalizedPosition_ = {
        std::clamp(mouseLocalPosition_.x / gameViewportRect_.width, 0.0f, 1.0f),
        std::clamp(mouseLocalPosition_.y / gameViewportRect_.height, 0.0f, 1.0f),
    };
    mouseNdcPosition_ = {
        mouseNormalizedPosition_.x * 2.0f - 1.0f,
        1.0f - mouseNormalizedPosition_.y * 2.0f,
    };
}
