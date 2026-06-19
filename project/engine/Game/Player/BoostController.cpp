#include "BoostController.h"

#include "Engine/Graphics/Cloud/VolumetricCloudPass.h"
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
    PushMultiplierToCloud();
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
    ImGui::DragFloat("Cloud Flow Multiplier", &boostCloudFlowMultiplier_, 0.05f, 1.0f, 20.0f, "%.2f");
    ImGui::DragFloat("Visual Speed Multiplier", &boostVisualSpeedMultiplier_, 0.05f, 1.0f, 10.0f, "%.2f");
    ImGui::DragFloat("Future Radial Blur Intensity", &radialBlurIntensity_, 0.01f, 0.0f, 5.0f, "%.2f");
    ImGui::DragFloat("Future FOV Boost Amount", &fovBoostAmount_, 0.1f, 0.0f, 60.0f, "%.1f");
    ImGui::Text("Applied Cloud Flow Multiplier: %.2f", GetCloudFlowMultiplier());
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
    return 1.0f + (std::max)(0.0f, boostCloudFlowMultiplier_ - 1.0f) * currentBoostPower_;
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
    inputBlockedReason_ = "Reset";
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

