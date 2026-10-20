#include "Engine/Core/DebugUiWorkspace.h"
#include "CameraShakeController.h"
#include "Engine/Game/Player/FlightTuning.h"
#include "Engine/Graphics/Camera/Camera.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
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
    SetGameplayEventsEnabled(false);
    currentRotationOffset_ = {};
    hasAppliedRotation_ = false;
    rotationClock_ = 0.0f;
    isPlaying_ = false;
    hasAppliedOffset_ = false;
    currentOffset_ = { 0.0f, 0.0f, 0.0f };
}

void CameraShakeController::Finalize() {
    SetGameplayEventsEnabled(false);
    isPlaying_ = false;
    hasAppliedOffset_ = false;
    currentOffset_ = { 0.0f, 0.0f, 0.0f };
}

void CameraShakeController::BeginFrame(Camera* camera) {
    RemoveAppliedOffset(camera);
}

void CameraShakeController::Start(float duration, float amplitude, float frequency) {
    // Dedicated translation shakes have priority over ordinary gameplay events.
    SetGameplayEventsEnabled(false);
    duration_ = (std::max)(0.01f, duration);
    amplitude_ = (std::max)(0.0f, amplitude);
    frequency_ = (std::max)(0.1f, frequency);
    elapsedTime_ = 0.0f;
    currentOffset_ = { 0.0f, 0.0f, 0.0f };
    isPlaying_ = true;
}

void CameraShakeController::UpdateAndApply(float deltaTime, Camera* camera, bool viewOnly) {
    if (!camera) {
        return;
    }

    if (!isPlaying_) {
        currentOffset_ = { 0.0f, 0.0f, 0.0f };
        UpdateGameplayShake(deltaTime, camera);
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

    appliedToView_ = viewOnly;
    if (appliedToView_) {
        camera->SetViewTranslationOffset(AddVector3(camera->GetViewTranslationOffset(), currentOffset_));
    } else {
        camera->SetTranslate(AddVector3(camera->GetTranslate(), currentOffset_));
    }
    hasAppliedOffset_ = true;

    if (elapsedTime_ >= duration_) {
        isPlaying_ = false;
    }
}

void CameraShakeController::Reset(Camera* camera) {
    RemoveAppliedOffset(camera);
    isPlaying_ = false;
    elapsedTime_ = 0.0f;
    SetGameplayEventsEnabled(false);
}

void CameraShakeController::SetGameplayEventsEnabled(bool enabled) {
    gameplayEventsEnabled_ = enabled && !isPlaying_;
    if (!gameplayEventsEnabled_) {
        shotPulse_ = {};
        impactPulse_ = {};
        shotCooldown_ = impactCooldown_ = rotationAmplitude_ = 0.0f;
    }
}

void CameraShakeController::RequestGameplayShake(GameplayEvent event) {
    if (!gameplayEventsEnabled_ || isPlaying_) return;
    if (event == GameplayEvent::Shot) {
        // At most 12.5 Hz, including simultaneous missiles. Never stack shots.
        if (shotCooldown_ > 0.0f) return;
        shotPulse_ = { 0.0f, 0.08f, 0.04f };
        shotCooldown_ = 0.08f;
        return;
    }
    const auto& tuning = FlightTuning::Get();
    const float amplitude = event == GameplayEvent::Explosion ? 0.5f
        : event == GameplayEvent::BulletCancel ? tuning.cancelAmplitude : tuning.defeatAmplitude;
    const float duration = event == GameplayEvent::Explosion ? 0.20f
        : event == GameplayEvent::BulletCancel ? tuning.cancelDuration : tuning.defeatDuration;
    // Defeat + explosion and multiple simultaneous defeats share one pulse.
    if (impactCooldown_ > 0.0f && amplitude <= impactPulse_.amplitudeDegrees) return;
    impactPulse_ = { 0.0f, duration, amplitude };
    impactCooldown_ = 0.05f;
}

void CameraShakeController::UpdateGameplayShake(float deltaTime, Camera* camera) {
    const float dt = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    shotCooldown_ = (std::max)(0.0f, shotCooldown_ - dt);
    impactCooldown_ = (std::max)(0.0f, impactCooldown_ - dt);
    rotationClock_ = std::fmod(rotationClock_ + dt, 1000.0f);
    const auto envelope = [dt](RotationPulse& pulse) {
        pulse.age += dt;
        if (pulse.duration <= 0.0f || pulse.age >= pulse.duration) return 0.0f;
        const float t = std::clamp(pulse.age / pulse.duration, 0.0f, 1.0f);
        const float attack = std::clamp(pulse.age / 0.012f, 0.0f, 1.0f);
        return pulse.amplitudeDegrees * attack * attack * (3.0f - 2.0f * attack)
            * (1.0f - t * t * (3.0f - 2.0f * t));
    };
    const float target = gameplayEventsEnabled_
        ? (std::max)(envelope(shotPulse_), envelope(impactPulse_)) : 0.0f;
    rotationAmplitude_ += (target - rotationAmplitude_) * (1.0f - std::exp(-dt / 0.012f));
    if (target == 0.0f && rotationAmplitude_ < 0.00001f) rotationAmplitude_ = 0.0f;
    const float phase = rotationClock_ * 24.0f * kPi * 2.0f;
    const float amplitude = std::clamp(rotationAmplitude_, 0.0f, FlightTuning::Get().shakeLimit) * kPi / 180.0f;
    // sqrt(2) normalization bounds the combined pitch/yaw vector at the configured cap.
    currentRotationOffset_ = { std::sin(phase * 1.17f) * amplitude * 0.70710678f,
        std::sin(phase * 1.71f) * amplitude * 0.70710678f, 0.0f };
    camera->SetViewRotationOffset(AddVector3(camera->GetViewRotationOffset(), currentRotationOffset_));
    hasAppliedRotation_ = true;
}

void CameraShakeController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(320.0f, 220.0f), ImGuiCond_FirstUseEver);
    if (!DebugUiWorkspace::BeginPanel("カメラシェイク確認 (Camera Shake Debug)")) {
        DebugUiWorkspace::EndPanel();
        return;
    }

    ImGui::Text("Playing: %s", isPlaying_ ? "true" : "false");
    ImGui::Text("通常イベント振幅: %.4f度 / 上限 %.2f度", rotationAmplitude_, FlightTuning::Get().shakeLimit);
    ImGui::Text("撃破・弾無効化の調整は飛行調整へ。");
    ImGui::DragFloat("Shake Duration", &duration_, 0.05f, 0.01f, 10.0f, "%.2f");
    ImGui::DragFloat("Shake Amplitude", &amplitude_, 0.005f, 0.0f, 5.0f, "%.3f");
    ImGui::DragFloat("Shake Frequency", &frequency_, 0.1f, 0.1f, 100.0f, "%.1f");
    ImGui::Text("Offset: %.3f, %.3f, %.3f", currentOffset_.x, currentOffset_.y, currentOffset_.z);
    if (ImGui::Button("Test Camera Shake")) {
        Start(duration_, amplitude_, frequency_);
    }

    DebugUiWorkspace::EndPanel();
#endif
}

void CameraShakeController::RemoveAppliedOffset(Camera* camera) {
    if (camera && hasAppliedRotation_) {
        camera->SetViewRotationOffset(SubtractVector3(camera->GetViewRotationOffset(), currentRotationOffset_));
        currentRotationOffset_ = {};
        hasAppliedRotation_ = false;
    }
    if (!camera || !hasAppliedOffset_) {
        return;
    }

    if (appliedToView_) {
        camera->SetViewTranslationOffset(SubtractVector3(camera->GetViewTranslationOffset(), currentOffset_));
    } else {
        camera->SetTranslate(SubtractVector3(camera->GetTranslate(), currentOffset_));
    }
    currentOffset_ = { 0.0f, 0.0f, 0.0f };
    hasAppliedOffset_ = false;
}

