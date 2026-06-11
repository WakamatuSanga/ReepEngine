#pragma once
#include "Engine/math/Matrix4x4.h"
#include <string>

class Camera;
class Input;

class EditorCameraController {
public:
    EditorCameraController();
    ~EditorCameraController();

    void Initialize(Camera* camera);
    void Finalize();
    void Update(
        float deltaTime,
        Input* input,
        bool isGameViewHovered,
        bool isGameViewFocused,
        bool isImGuiInputActive,
        bool isGizmoInteracting,
        bool isCameraRigControllingCamera);
    void DrawImGui();

    bool IsUsingKeyboardInput() const { return usingKeyboardInput_; }
    bool IsRightMouseFlyActive() const { return rightMouseFlyActive_; }

private:
    void CaptureEditorCameraHome();
    void RestoreEditorCameraHome();
    void ResetEditorCamera();
    void ApplyRotationDelta(float deltaX, float deltaY);
    void ApplyTranslation(const Vector3& delta);
    Vector3 GetForward() const;
    Vector3 GetRight() const;
    Vector3 GetUp() const;
    bool IsGameViewActive(bool isGameViewHovered, bool isGameViewFocused) const;

    Camera* camera_ = nullptr;
    Vector3 homePosition_{ 0.0f, 2.0f, -10.0f };
    Vector3 homeRotation_{ 0.1f, 0.0f, 0.0f };
    std::string inputBlockedReason_ = "Not updated";
    bool enableNavigation_ = true;
    bool requireGameViewHover_ = true;
    bool wheelDollyEnabled_ = true;
    bool middleMouseOrbitEnabled_ = true;
    bool shiftMiddlePanEnabled_ = true;
    bool rightMouseFlyEnabled_ = true;
    bool hasHomePose_ = false;
    bool isGameViewActive_ = false;
    bool isCameraRigControllingCamera_ = false;
    bool usingKeyboardInput_ = false;
    bool rightMouseFlyActive_ = false;
    float moveSpeed_ = 8.0f;
    float fastMoveMultiplier_ = 3.0f;
    float mouseSensitivity_ = 0.0025f;
    float wheelDollySpeed_ = 1.2f;
    float panSpeed_ = 0.015f;
};
