#include "CameraShakeController.h"
#include "Engine/Graphics/Camera/Camera.h"
#include <algorithm>
#include <cmath>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kPi = 3.14159265358979323846f;

    Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 SubtractVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    }

    Vector3 ScaleVector3(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }
}

CameraShakeController::CameraShakeController() = default;

CameraShakeController::~CameraShakeController() = default;

void CameraShakeController::Initialize() {
    isPlaying_ = false;
    hasAppliedOffset_ = false;
    currentOffset_ = { 0.0f, 0.0f, 0.0f };
}

void CameraShakeController::Finalize() {
    isPlaying_ = false;
    hasAppliedOffset_ = false;
    currentOffset_ = { 0.0f, 0.0f, 0.0f };
}

void CameraShakeController::BeginFrame(Camera* camera) {
    RemoveAppliedOffset(camera);
}

void CameraShakeController::Start(float duration, float amplitude, float frequency) {
    duration_ = (std::max)(0.01f, duration);
    amplitude_ = (std::max)(0.0f, amplitude);
    frequency_ = (std::max)(0.1f, frequency);
    elapsedTime_ = 0.0f;
    currentOffset_ = { 0.0f, 0.0f, 0.0f };
    isPlaying_ = true;
}

void CameraShakeController::UpdateAndApply(float deltaTime, Camera* camera) {
    if (!camera) {
        return;
    }

    if (!isPlaying_) {
        currentOffset_ = { 0.0f, 0.0f, 0.0f };
        return;
    }

    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    elapsedTime_ += safeDeltaTime;
    const float progress = std::clamp(elapsedTime_ / duration_, 0.0f, 1.0f);
    const float envelope = 1.0f - progress;
    const float phase = elapsedTime_ * frequency_ * kPi * 2.0f;
    currentOffset_ = ScaleVector3({
        std::sin(phase * 1.17f),
        std::sin(phase * 1.71f),
        std::sin(phase * 1.43f) * 0.25f,
        }, amplitude_ * envelope);

    camera->SetTranslate(AddVector3(camera->GetTranslate(), currentOffset_));
    hasAppliedOffset_ = true;

    if (elapsedTime_ >= duration_) {
        isPlaying_ = false;
    }
}

void CameraShakeController::Reset(Camera* camera) {
    RemoveAppliedOffset(camera);
    isPlaying_ = false;
    elapsedTime_ = 0.0f;
}

void CameraShakeController::DrawImGui() {
#ifdef _DEBUG
    ImGui::SetNextWindowSize(ImVec2(320.0f, 220.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("カメラシェイク確認 (Camera Shake Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Playing: %s", isPlaying_ ? "true" : "false");
    ImGui::DragFloat("Shake Duration", &duration_, 0.05f, 0.01f, 10.0f, "%.2f");
    ImGui::DragFloat("Shake Amplitude", &amplitude_, 0.005f, 0.0f, 5.0f, "%.3f");
    ImGui::DragFloat("Shake Frequency", &frequency_, 0.1f, 0.1f, 100.0f, "%.1f");
    ImGui::Text("Offset: %.3f, %.3f, %.3f", currentOffset_.x, currentOffset_.y, currentOffset_.z);
    if (ImGui::Button("Test Camera Shake")) {
        Start(duration_, amplitude_, frequency_);
    }

    ImGui::End();
#endif
}

void CameraShakeController::RemoveAppliedOffset(Camera* camera) {
    if (!camera || !hasAppliedOffset_) {
        return;
    }

    camera->SetTranslate(SubtractVector3(camera->GetTranslate(), currentOffset_));
    currentOffset_ = { 0.0f, 0.0f, 0.0f };
    hasAppliedOffset_ = false;
}
