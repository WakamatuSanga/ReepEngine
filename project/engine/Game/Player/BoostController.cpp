#include "BoostController.h"

#include "Engine/Game/Effect/PostEffectController.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Cloud/VolumetricCloudPass.h"
#include "Player.h"
#include "Engine/Input/Input.h"
#include "MyGame.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

BoostController::BoostController() = default;

BoostController::~BoostController() = default;

void BoostController::Initialize(VolumetricCloudPass* cloudPass) {
    cloudPass_ = cloudPass;
    currentCloudFlowMultiplier_ = 1.0f;
    targetCloudFlowMultiplier_ = 1.0f;
    PushMultiplierToCloud();
}

void BoostController::SetPostEffectContext(Player* player, Camera* camera, PostEffectController* postEffectController) {
    player_ = player;
    camera_ = camera;
    postEffectController_ = postEffectController;
}

void BoostController::Finalize() {
    ResetBoost();
    PushMultiplierToCloud();
    cloudPass_ = nullptr;
}

void BoostController::Update(float deltaTime) {
    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    lastInputTriggered_ = false;
    lastCanBoost_ = false;
    inputBlockedReason_ = "None";

    if (state_ == State::Boosting) {
        boostTimer_ += safeDeltaTime;
        const float safeDuration = (std::max)(boostDuration_, 0.001f);
        const float easeIn = (std::max)(boostEaseInTime_, 0.001f);
        const float easeOut = (std::max)(boostEaseOutTime_, 0.001f);
        const float easeInPower = SmoothStep(boostTimer_ / easeIn);
        const float easeOutPower = SmoothStep((safeDuration - boostTimer_) / easeOut);
        currentBoostPower_ = Saturate((std::min)(easeInPower, easeOutPower));

        if (boostTimer_ >= safeDuration) {
            state_ = boostCooldown_ > 0.0f ? State::Cooldown : State::Idle;
            boostTimer_ = 0.0f;
            boostCooldownTimer_ = (std::max)(0.0f, boostCooldown_);
            currentBoostPower_ = 0.0f;
        }
    } else if (state_ == State::Cooldown) {
        boostCooldownTimer_ = (std::max)(0.0f, boostCooldownTimer_ - safeDeltaTime);
        currentBoostPower_ = 0.0f;
        if (boostCooldownTimer_ <= 0.0f) {
            state_ = State::Idle;
        }
    } else {
        currentBoostPower_ = 0.0f;
    }

    Input* input = MyGame::GetInstance()->GetInput();
    if (!enableBoost_) {
        inputBlockedReason_ = "Boost disabled";
    } else if (!gameViewInputActive_) {
        inputBlockedReason_ = "Game View input inactive";
    } else if (!input) {
        inputBlockedReason_ = "Input missing";
    } else {
        lastInputTriggered_ = input->TriggerKey(ResolveInputKey());
        lastCanBoost_ = state_ == State::Idle && boostCooldownTimer_ <= 0.0f;
        if (lastInputTriggered_) {
            if (lastCanBoost_) {
                StartBoost(false);
            } else {
                inputBlockedReason_ = "Boost cooldown or already boosting";
            }
        }
    }

    UpdateCloudFlowMultiplier(safeDeltaTime);
    UpdateBoostPostEffect();
    PushMultiplierToCloud();
}

void BoostController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(390.0f, 360.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("ブースト確認 (Boost Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Boostを使う (Enable Boost)", &enableBoost_);
    int keyIndex = static_cast<int>(boostInputKey_);
    const char* keyItems[] = { "Left Shift", "Right Shift", "B" };
    if (ImGui::Combo("Boost Key", &keyIndex, keyItems, IM_ARRAYSIZE(keyItems))) {
        boostInputKey_ = static_cast<InputKey>(keyIndex);
    }
    ImGui::Text("State: %s", GetStateName());
    ImGui::Text("Boost Key: %s", GetInputKeyName());
    ImGui::Text("Is Boosting: %s", IsBoosting() ? "true" : "false");
    ImGui::Text("Current Boost Power: %.2f", currentBoostPower_);
    ImGui::Text("Boost Timer: %.2f", boostTimer_);
    ImGui::Text("Boost Cooldown Timer: %.2f", boostCooldownTimer_);
    ImGui::DragFloat("Boost Duration", &boostDuration_, 0.01f, 0.05f, 10.0f, "%.2f");
    ImGui::DragFloat("Boost Cooldown", &boostCooldown_, 0.01f, 0.0f, 10.0f, "%.2f");
    ImGui::DragFloat("Ease In Time", &boostEaseInTime_, 0.01f, 0.0f, 3.0f, "%.2f");
    ImGui::DragFloat("Ease Out Time", &boostEaseOutTime_, 0.01f, 0.0f, 3.0f, "%.2f");
    ImGui::DragFloat("Cloud Flow Multiplier", &boostCloudFlowMultiplier_, 0.05f, 1.0f, 2.0f, "%.2f");
    ImGui::DragFloat("Cloud Boost Max Multiplier", &maxCloudBoostMultiplier_, 0.01f, 1.0f, 2.0f, "%.2f");
    ImGui::DragFloat("Cloud Boost Smooth Speed", &cloudBoostMultiplierSmoothSpeed_, 0.1f, 0.0f, 30.0f, "%.1f");
    ImGui::DragFloat("Visual Speed Multiplier", &boostVisualSpeedMultiplier_, 0.05f, 1.0f, 10.0f, "%.2f");
    ImGui::DragFloat("Future Radial Blur Intensity", &radialBlurIntensity_, 0.01f, 0.0f, 0.2f, "%.2f");
    ImGui::DragFloat("Future FOV Boost Amount", &fovBoostAmount_, 0.1f, 0.0f, 60.0f, "%.1f");
    ImGui::Text("Applied Cloud Flow Multiplier: %.2f", GetCloudFlowMultiplier());
    ImGui::Text("Target Cloud Flow Multiplier: %.2f", targetCloudFlowMultiplier_);
    ImGui::Text("Boost Effect Center: %.2f, %.2f (%s)", lastBoostEffectCenterX_, lastBoostEffectCenterY_, lastBoostEffectCenterValid_ ? "valid" : "invalid");
    ImGui::Text("Applied Visual Speed Multiplier: %.2f", GetVisualSpeedMultiplier());
    ImGui::Text("Game View Input Active: %s", gameViewInputActive_ ? "true" : "false");
    ImGui::Text("Input Triggered: %s", lastInputTriggered_ ? "true" : "false");
    ImGui::Text("Can Boost: %s", lastCanBoost_ ? "true" : "false");
    ImGui::Text("Boost Count: %llu", static_cast<unsigned long long>(boostCount_));
    ImGui::TextWrapped("Blocked Reason: %s", inputBlockedReason_.c_str());
    if (ImGui::Button("テストBoost発動 (Trigger Test Boost)")) {
        StartBoost(true);
    }
    ImGui::SameLine();
    if (ImGui::Button("Boostリセット (Reset Boost)")) {
        ResetBoost();
    }

    ImGui::End();
#endif
}

float BoostController::GetCloudFlowMultiplier() const {
    return currentCloudFlowMultiplier_;
}

float BoostController::GetVisualSpeedMultiplier() const {
    return 1.0f + (std::max)(0.0f, boostVisualSpeedMultiplier_ - 1.0f) * currentBoostPower_;
}

void BoostController::StartBoost(bool ignoreCooldown) {
    if (!enableBoost_ && !ignoreCooldown) {
        inputBlockedReason_ = "Boost disabled";
        return;
    }
    if (!ignoreCooldown && (state_ != State::Idle || boostCooldownTimer_ > 0.0f)) {
        inputBlockedReason_ = "Boost cooldown or already boosting";
        return;
    }

    state_ = State::Boosting;
    boostTimer_ = 0.0f;
    boostCooldownTimer_ = 0.0f;
    currentBoostPower_ = 0.0f;
    ++boostCount_;
    inputBlockedReason_ = "Boost started";
}

void BoostController::ResetBoost() {
    state_ = State::Idle;
    boostTimer_ = 0.0f;
    boostCooldownTimer_ = 0.0f;
    currentBoostPower_ = 0.0f;
    currentCloudFlowMultiplier_ = 1.0f;
    targetCloudFlowMultiplier_ = 1.0f;
    UpdateBoostPostEffect();
    inputBlockedReason_ = "Reset";
}

void BoostController::UpdateCloudFlowMultiplier(float deltaTime) {
    const float clampedMax = std::clamp(maxCloudBoostMultiplier_, 1.0f, 2.0f);
    const float requestedBoost = std::clamp(boostCloudFlowMultiplier_, 1.0f, clampedMax);
    targetCloudFlowMultiplier_ = 1.0f + (requestedBoost - 1.0f) * Saturate(currentBoostPower_);
    const float smoothSpeed = (std::max)(cloudBoostMultiplierSmoothSpeed_, 0.0f);
    const float t = smoothSpeed <= 0.0f ? 1.0f : 1.0f - std::exp(-std::clamp(deltaTime, 0.0f, 1.0f / 15.0f) * smoothSpeed);
    currentCloudFlowMultiplier_ += (targetCloudFlowMultiplier_ - currentCloudFlowMultiplier_) * std::clamp(t, 0.0f, 1.0f);
    if (!std::isfinite(currentCloudFlowMultiplier_)) {
        currentCloudFlowMultiplier_ = 1.0f;
    }
}

void BoostController::UpdateBoostPostEffect() {
    if (!postEffectController_) {
        return;
    }
    float centerX = 0.5f;
    float centerY = 0.5f;
    const bool centerValid = ProjectPlayerToScreen(centerX, centerY);
    lastBoostEffectCenterX_ = centerX;
    lastBoostEffectCenterY_ = centerY;
    lastBoostEffectCenterValid_ = centerValid;
    postEffectController_->SetBoostPostEffectTarget(currentBoostPower_, centerX, centerY, centerValid);
}

bool BoostController::ProjectPlayerToScreen(float& outX, float& outY) const {
    outX = 0.5f;
    outY = 0.5f;
    if (!player_ || !camera_) {
        return false;
    }
    const Vector3& p = player_->GetWorldPosition();
    const Matrix4x4& vp = camera_->GetViewProjectionMatrix();
    const float clipX = p.x * vp.m[0][0] + p.y * vp.m[1][0] + p.z * vp.m[2][0] + vp.m[3][0];
    const float clipY = p.x * vp.m[0][1] + p.y * vp.m[1][1] + p.z * vp.m[2][1] + vp.m[3][1];
    const float clipW = p.x * vp.m[0][3] + p.y * vp.m[1][3] + p.z * vp.m[2][3] + vp.m[3][3];
    if (clipW <= 0.0001f || !std::isfinite(clipW)) {
        return false;
    }
    const float ndcX = clipX / clipW;
    const float ndcY = clipY / clipW;
    if (!std::isfinite(ndcX) || !std::isfinite(ndcY)) {
        return false;
    }
    outX = std::clamp(ndcX * 0.5f + 0.5f, 0.0f, 1.0f);
    outY = std::clamp(-ndcY * 0.5f + 0.5f, 0.0f, 1.0f);
    return ndcX >= -1.25f && ndcX <= 1.25f && ndcY >= -1.25f && ndcY <= 1.25f;
}

void BoostController::PushMultiplierToCloud() {
    if (cloudPass_) {
        cloudPass_->SetExternalFlowMultiplier(GetCloudFlowMultiplier());
    }
}

unsigned char BoostController::ResolveInputKey() const {
    switch (boostInputKey_) {
    case InputKey::RightShift:
        return DIK_RSHIFT;
    case InputKey::B:
        return DIK_B;
    case InputKey::LeftShift:
    default:
        return DIK_LSHIFT;
    }
}

const char* BoostController::GetStateName() const {
    switch (state_) {
    case State::Boosting:
        return "Boosting";
    case State::Cooldown:
        return "Cooldown";
    case State::Idle:
    default:
        return "Idle";
    }
}

const char* BoostController::GetInputKeyName() const {
    switch (boostInputKey_) {
    case InputKey::RightShift:
        return "Right Shift";
    case InputKey::B:
        return "B";
    case InputKey::LeftShift:
    default:
        return "Left Shift";
    }
}

float BoostController::SmoothStep(float value) {
    const float t = Saturate(value);
    return t * t * (3.0f - 2.0f * t);
}

float BoostController::Saturate(float value) {
    if (!std::isfinite(value)) {
        return 0.0f;
    }
    return std::clamp(value, 0.0f, 1.0f);
}

