#include "PlayerDeathSequenceController.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Core/WinApp.h"
#include "Engine/Game/Camera/CameraShakeController.h"
#include "Engine/Graphics/Sprite/Sprite.h"
#include "Engine/Graphics/Sprite/SpriteCommon.h"
#include <algorithm>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

PlayerDeathSequenceController::PlayerDeathSequenceController() = default;

PlayerDeathSequenceController::~PlayerDeathSequenceController() = default;

void PlayerDeathSequenceController::Initialize(DirectXCommon* dxCommon, SpriteCommon* spriteCommon, CameraShakeController* cameraShake) {
    dxCommon_ = dxCommon;
    spriteCommon_ = spriteCommon;
    cameraShake_ = cameraShake;
    fadeSprite_ = std::make_unique<Sprite>();
    fadeSprite_->Initialize(spriteCommon_);
    fadeSprite_->SetTexture("resources/obj/axis/uvChecker.png");
    fadeSprite_->SetPosition({ 0.0f, 0.0f });
    fadeSprite_->SetSize({ static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) });
    fadeSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
    fadeSprite_->Update();
}

void PlayerDeathSequenceController::Finalize() {
    RestorePostEffect();
    fadeSprite_.reset();
    dxCommon_ = nullptr;
    spriteCommon_ = nullptr;
    cameraShake_ = nullptr;
}

void PlayerDeathSequenceController::StartDeathSequence() {
    if (isPlaying_ || isFinished_) {
        return;
    }

    if (dxCommon_ && !savedPostEffect_) {
        const DirectXCommon::PostEffectParameters& params = dxCommon_->GetPostEffectParameters();
        previousGrayscaleEnabled_ = params.grayscaleEnabled;
        previousGrayscaleIntensity_ = params.grayscaleIntensity;
        savedPostEffect_ = true;
    }

    isPlaying_ = true;
    isFinished_ = false;
    finishConsumed_ = false;
    elapsedTime_ = 0.0f;
    fadeAlpha_ = 0.0f;

    if (cameraShake_) {
        cameraShake_->Start(shakeDuration_, shakeAmplitude_, shakeFrequency_);
    }
}

void PlayerDeathSequenceController::Update(float deltaTime) {
    if (!isPlaying_) {
        return;
    }

    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    elapsedTime_ += safeDeltaTime;
    fadeAlpha_ = CalculateFadeAlpha();
    ApplyPostEffect();

    if (elapsedTime_ >= deathSequenceDuration_) {
        isPlaying_ = false;
        isFinished_ = true;
        fadeAlpha_ = 1.0f;
    }
}

void PlayerDeathSequenceController::Draw() {
    if (!spriteCommon_ || !fadeSprite_ || fadeAlpha_ <= 0.001f) {
        return;
    }

    fadeSprite_->SetColor({ 0.0f, 0.0f, 0.0f, std::clamp(fadeAlpha_, 0.0f, 1.0f) });
    fadeSprite_->SetSize({ static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) });
    fadeSprite_->Update();
    spriteCommon_->CommonDrawSetting();
    fadeSprite_->Draw();
}

void PlayerDeathSequenceController::DrawImGui() {
#ifdef _DEBUG
    ImGui::SetNextWindowSize(ImVec2(360.0f, 340.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("死亡演出確認 (Death Effect Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Playing: %s / Finished: %s", isPlaying_ ? "true" : "false", isFinished_ ? "true" : "false");
    ImGui::Text("Elapsed Time: %.2f", elapsedTime_);
    ImGui::Checkbox("Enable Grayscale", &enableGrayscale_);
    ImGui::DragFloat("Death Sequence Duration", &deathSequenceDuration_, 0.05f, 0.1f, 10.0f, "%.2f");
    ImGui::DragFloat("Grayscale Duration", &grayscaleDuration_, 0.05f, 0.1f, 10.0f, "%.2f");
    ImGui::DragFloat("Fade Start Time", &fadeStartTime_, 0.05f, 0.0f, 10.0f, "%.2f");
    ImGui::DragFloat("Fade Duration", &fadeDuration_, 0.05f, 0.1f, 10.0f, "%.2f");
    ImGui::DragFloat("Shake Duration", &shakeDuration_, 0.05f, 0.01f, 10.0f, "%.2f");
    ImGui::DragFloat("Shake Amplitude", &shakeAmplitude_, 0.005f, 0.0f, 5.0f, "%.3f");
    ImGui::DragFloat("Shake Frequency", &shakeFrequency_, 0.1f, 0.1f, 100.0f, "%.1f");
    ImGui::Text("Fade Alpha: %.2f", fadeAlpha_);
    if (ImGui::Button("Trigger Test Death")) {
        StartDeathSequence();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Death Sequence")) {
        Reset();
    }

    ImGui::End();
#endif
}

void PlayerDeathSequenceController::Reset() {
    isPlaying_ = false;
    isFinished_ = false;
    finishConsumed_ = false;
    elapsedTime_ = 0.0f;
    fadeAlpha_ = 0.0f;
    RestorePostEffect();
}

bool PlayerDeathSequenceController::ConsumeFinished() {
    if (!isFinished_ || finishConsumed_) {
        return false;
    }

    finishConsumed_ = true;
    RestorePostEffect();
    return true;
}

void PlayerDeathSequenceController::ApplyPostEffect() {
    if (!dxCommon_ || !enableGrayscale_) {
        return;
    }

    DirectXCommon::PostEffectParameters& params = dxCommon_->GetPostEffectParameters();
    params.grayscaleEnabled = 1;
    params.grayscaleIntensity = std::clamp(elapsedTime_ / (std::max)(0.01f, grayscaleDuration_), 0.0f, 1.0f);
}

void PlayerDeathSequenceController::RestorePostEffect() {
    if (!dxCommon_ || !savedPostEffect_) {
        return;
    }

    DirectXCommon::PostEffectParameters& params = dxCommon_->GetPostEffectParameters();
    params.grayscaleEnabled = previousGrayscaleEnabled_;
    params.grayscaleIntensity = previousGrayscaleIntensity_;
    savedPostEffect_ = false;
}

float PlayerDeathSequenceController::CalculateFadeAlpha() const {
    if (elapsedTime_ <= fadeStartTime_) {
        return 0.0f;
    }

    return std::clamp((elapsedTime_ - fadeStartTime_) / (std::max)(0.01f, fadeDuration_), 0.0f, 1.0f);
}
