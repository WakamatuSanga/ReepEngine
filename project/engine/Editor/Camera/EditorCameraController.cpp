#include "EditorCameraController.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Input/Input.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kMinVectorLength = 0.00001f;
    constexpr float kMinPitch = -1.48f;
    constexpr float kMaxPitch = 1.48f;
    constexpr float kDefaultDeltaTime = 1.0f / 60.0f;

    Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 ScaleVector3(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    float Length(const Vector3& value) {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    }

    Vector3 Normalize(const Vector3& value, const Vector3& fallback) {
        const float length = Length(value);
        if (length <= kMinVectorLength ||
            !std::isfinite(value.x) ||
            !std::isfinite(value.y) ||
            !std::isfinite(value.z)) {
            return fallback;
        }
        return { value.x / length, value.y / length, value.z / length };
    }

    Vector3 Cross(const Vector3& lhs, const Vector3& rhs) {
        return {
            lhs.y * rhs.z - lhs.z * rhs.y,
            lhs.z * rhs.x - lhs.x * rhs.z,
            lhs.x * rhs.y - lhs.y * rhs.x,
        };
    }

    bool IsShiftDown(const Input& input) {
        return input.PushKey(DIK_LSHIFT) || input.PushKey(DIK_RSHIFT);
    }
}

EditorCameraController::EditorCameraController() = default;

EditorCameraController::~EditorCameraController() = default;

void EditorCameraController::Initialize(Camera* camera) {
    camera_ = camera;
    CaptureEditorCameraHome();
}

void EditorCameraController::Finalize() {
    camera_ = nullptr;
}

void EditorCameraController::Update(
    float deltaTime,
    Input* input,
    bool isGameViewHovered,
    bool isGameViewFocused,
    bool isImGuiInputActive,
    bool isGizmoInteracting,
    bool isCameraRigControllingCamera) {
    usingKeyboardInput_ = false;
    rightMouseFlyActive_ = false;
    isCameraRigControllingCamera_ = isCameraRigControllingCamera;
    isGameViewActive_ = IsGameViewActive(isGameViewHovered, isGameViewFocused);
    inputBlockedReason_ = "None";

    if (!enableNavigation_) {
        inputBlockedReason_ = "Navigation disabled";
        return;
    }
    if (!camera_ || !input) {
        inputBlockedReason_ = "Camera or Input missing";
        return;
    }
    if (isCameraRigControllingCamera_) {
        inputBlockedReason_ = "CameraRig controlling camera";
        return;
    }
    if (!isGameViewActive_) {
        inputBlockedReason_ = "Game View inactive";
        return;
    }
    if (isGizmoInteracting) {
        inputBlockedReason_ = "Gizmo interacting";
        return;
    }
    if (isImGuiInputActive) {
        inputBlockedReason_ = "ImGui input active";
        return;
    }

    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    const bool shiftDown = IsShiftDown(*input);
    bool changedCamera = false;

    if (wheelDollyEnabled_) {
        const float wheelSteps = static_cast<float>(input->MouseWheelDelta()) / 120.0f;
        if (wheelSteps != 0.0f) {
            ApplyTranslation(ScaleVector3(GetForward(), wheelSteps * wheelDollySpeed_));
            changedCamera = true;
        }
    }

    if (middleMouseOrbitEnabled_ && input->MouseDown(Input::MouseMiddle) && !shiftDown) {
        ApplyRotationDelta(static_cast<float>(input->MouseDeltaX()), static_cast<float>(input->MouseDeltaY()));
        changedCamera = true;
    }

    if (shiftMiddlePanEnabled_ && input->MouseDown(Input::MouseMiddle) && shiftDown) {
        const Vector3 right = GetRight();
        const Vector3 up = GetUp();
        const Vector3 pan = AddVector3(
            ScaleVector3(right, -static_cast<float>(input->MouseDeltaX()) * panSpeed_),
            ScaleVector3(up, static_cast<float>(input->MouseDeltaY()) * panSpeed_));
        ApplyTranslation(pan);
        changedCamera = true;
    }

    rightMouseFlyActive_ = rightMouseFlyEnabled_ && input->MouseDown(Input::MouseRight);
    if (rightMouseFlyActive_) {
        ApplyRotationDelta(static_cast<float>(input->MouseDeltaX()), static_cast<float>(input->MouseDeltaY()));
        Vector3 move{ 0.0f, 0.0f, 0.0f };
        if (input->PushKey(DIK_W)) { move = AddVector3(move, GetForward()); }
        if (input->PushKey(DIK_S)) { move = AddVector3(move, ScaleVector3(GetForward(), -1.0f)); }
        if (input->PushKey(DIK_D)) { move = AddVector3(move, GetRight()); }
        if (input->PushKey(DIK_A)) { move = AddVector3(move, ScaleVector3(GetRight(), -1.0f)); }
        if (input->PushKey(DIK_E)) { move = AddVector3(move, { 0.0f, 1.0f, 0.0f }); }
        if (input->PushKey(DIK_Q)) { move = AddVector3(move, { 0.0f, -1.0f, 0.0f }); }

        const float moveLength = Length(move);
        if (moveLength > kMinVectorLength) {
            usingKeyboardInput_ = true;
            const float speed = moveSpeed_ * (shiftDown ? fastMoveMultiplier_ : 1.0f);
            ApplyTranslation(ScaleVector3(Normalize(move, { 0.0f, 0.0f, 1.0f }), speed * safeDeltaTime));
        }
        changedCamera = true;
    }

    if (changedCamera) {
        camera_->Update();
    }
}

void EditorCameraController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(380.0f, 430.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("エディタカメラ確認 (Editor Camera Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("エディタカメラ操作 (Enable Editor Camera Navigation)", &enableNavigation_);
    ImGui::Checkbox("Game Viewホバー必須 (Require Game View Hover)", &requireGameViewHover_);
    ImGui::Checkbox("ホイールドリー (Wheel Dolly Enabled)", &wheelDollyEnabled_);
    ImGui::Checkbox("中ボタン回転 (Middle Mouse Orbit Enabled)", &middleMouseOrbitEnabled_);
    ImGui::Checkbox("Shift中ボタンパン (Shift Middle Pan Enabled)", &shiftMiddlePanEnabled_);
    ImGui::Checkbox("右ボタンフライ (Right Mouse Fly Enabled)", &rightMouseFlyEnabled_);
    ImGui::DragFloat("Move Speed", &moveSpeed_, 0.05f, 0.0f, 100.0f, "%.2f");
    ImGui::DragFloat("Fast Move Multiplier", &fastMoveMultiplier_, 0.05f, 1.0f, 20.0f, "%.2f");
    ImGui::DragFloat("Mouse Sensitivity", &mouseSensitivity_, 0.0001f, 0.0001f, 0.05f, "%.4f");
    ImGui::DragFloat("Wheel Dolly Speed", &wheelDollySpeed_, 0.02f, 0.0f, 20.0f, "%.2f");
    ImGui::DragFloat("Pan Speed", &panSpeed_, 0.001f, 0.0f, 0.2f, "%.3f");

    if (ImGui::Button("Reset Editor Camera")) {
        ResetEditorCamera();
    }
    if (ImGui::Button("Capture Editor Camera Home")) {
        CaptureEditorCameraHome();
    }
    ImGui::SameLine();
    if (ImGui::Button("Restore Editor Camera Home")) {
        RestoreEditorCameraHome();
    }

    ImGui::SeparatorText("Diagnostics");
    ImGui::TextWrapped("Input Blocked Reason: %s", inputBlockedReason_.c_str());
    ImGui::Text("Is Game View Active: %s", isGameViewActive_ ? "true" : "false");
    ImGui::Text("Is CameraRig Controlling Camera: %s", isCameraRigControllingCamera_ ? "true" : "false");
    ImGui::Text("Right Mouse Fly Active: %s", rightMouseFlyActive_ ? "true" : "false");
    ImGui::Text("Using Keyboard Input: %s", usingKeyboardInput_ ? "true" : "false");
    ImGui::Text("Has Home Pose: %s", hasHomePose_ ? "true" : "false");
    if (camera_) {
        const Vector3& position = camera_->GetTranslate();
        const Vector3& rotation = camera_->GetRotate();
        ImGui::Text("Camera Position: %.3f, %.3f, %.3f", position.x, position.y, position.z);
        ImGui::Text("Camera Rotation: %.3f, %.3f, %.3f", rotation.x, rotation.y, rotation.z);
    }
    ImGui::End();
#endif
}

void EditorCameraController::CaptureEditorCameraHome() {
    if (!camera_) {
        return;
    }
    homePosition_ = camera_->GetTranslate();
    homeRotation_ = camera_->GetRotate();
    hasHomePose_ = true;
}

void EditorCameraController::RestoreEditorCameraHome() {
    if (!camera_ || !hasHomePose_) {
        return;
    }
    camera_->SetTranslate(homePosition_);
    camera_->SetRotate(homeRotation_);
    camera_->Update();
}

void EditorCameraController::ResetEditorCamera() {
    if (!camera_) {
        return;
    }
    camera_->SetTranslate({ 0.0f, 2.0f, -10.0f });
    camera_->SetRotate({ 0.1f, 0.0f, 0.0f });
    camera_->Update();
}

void EditorCameraController::ApplyRotationDelta(float deltaX, float deltaY) {
    if (!camera_) {
        return;
    }
    Vector3 rotation = camera_->GetRotate();
    rotation.y += deltaX * mouseSensitivity_;
    rotation.x = std::clamp(rotation.x + deltaY * mouseSensitivity_, kMinPitch, kMaxPitch);
    rotation.z = 0.0f;
    camera_->SetRotate(rotation);
}

void EditorCameraController::ApplyTranslation(const Vector3& delta) {
    if (!camera_) {
        return;
    }
    camera_->SetTranslate(AddVector3(camera_->GetTranslate(), delta));
}

Vector3 EditorCameraController::GetForward() const {
    if (!camera_) {
        return { 0.0f, 0.0f, 1.0f };
    }
    const Vector3& rotation = camera_->GetRotate();
    const float cosPitch = std::cos(rotation.x);
    return Normalize({
        std::sin(rotation.y) * cosPitch,
        -std::sin(rotation.x),
        std::cos(rotation.y) * cosPitch,
        }, { 0.0f, 0.0f, 1.0f });
}

Vector3 EditorCameraController::GetRight() const {
    const Vector3 forward = GetForward();
    return Normalize(Cross({ 0.0f, 1.0f, 0.0f }, forward), { 1.0f, 0.0f, 0.0f });
}

Vector3 EditorCameraController::GetUp() const {
    const Vector3 forward = GetForward();
    const Vector3 right = GetRight();
    return Normalize(Cross(forward, right), { 0.0f, 1.0f, 0.0f });
}

bool EditorCameraController::IsGameViewActive(bool isGameViewHovered, bool isGameViewFocused) const {
    return requireGameViewHover_ ? isGameViewHovered : (isGameViewHovered || isGameViewFocused);
}

