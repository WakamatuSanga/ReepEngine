#include "CombatSlowMotionController.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <cmath>

namespace {
    float Saturate(float value) {
        return std::clamp(value, 0.0f, 1.0f);
    }

    float SmoothStep(float value) {
        const float t = Saturate(value);
        return t * t * (3.0f - 2.0f * t);
    }
}

CombatSlowMotionController::CombatSlowMotionController() = default;
CombatSlowMotionController::~CombatSlowMotionController() = default;

void CombatSlowMotionController::Initialize() {
    initialized_ = true;
    Reset();
}

void CombatSlowMotionController::Finalize() {
    initialized_ = false;
    Reset();
}

void CombatSlowMotionController::Reset() {
    isActive_ = false;
    currentTimeScale_ = 1.0f;
    timer_ = 0.0f;
    activeDuration_ = 0.0f;
    cooldownTimer_ = 0.0f;
    activeSlowTimeScale_ = slowTimeScale_;
    lastCancelCount_ = 0;
}

void CombatSlowMotionController::Update(float unscaledDeltaTime) {
    ClampSettings();
    const float safeDeltaTime = std::clamp(unscaledDeltaTime, 0.0f, 1.0f / 15.0f);

    if (cooldownTimer_ > 0.0f) {
        cooldownTimer_ = (std::max)(0.0f, cooldownTimer_ - safeDeltaTime);
    }

    if (!isActive_) {
        currentTimeScale_ = 1.0f;
        return;
    }

    timer_ += safeDeltaTime;
    if (timer_ >= activeDuration_) {
        isActive_ = false;
        timer_ = 0.0f;
        activeDuration_ = 0.0f;
        currentTimeScale_ = 1.0f;
        return;
    }

    const float fadeIn = easeInTime_ <= 0.0f ? 1.0f : SmoothStep(timer_ / easeInTime_);
    const float fadeOutStart = (std::max)(0.0f, activeDuration_ - easeOutTime_);
    float fadeOut = 1.0f;
    if (easeOutTime_ > 0.0f && timer_ > fadeOutStart) {
        fadeOut = 1.0f - SmoothStep((timer_ - fadeOutStart) / easeOutTime_);
    }
    const float slowWeight = Saturate(fadeIn * fadeOut);
    currentTimeScale_ = 1.0f + (activeSlowTimeScale_ - 1.0f) * slowWeight;
}

void CombatSlowMotionController::TriggerBulletCancelSlowMotion(int cancelCount) {
    ClampSettings();
    lastCancelCount_ = cancelCount;
    if (!initialized_ || !enabled_ || cancelCount < minCancelCount_) {
        ++ignoredTriggerCount_;
        return;
    }

    const float requestedDuration = ResolveDurationForCancelCount(cancelCount);
    const float requestedScale = ResolveSlowScaleForCancelCount(cancelCount);
    if (cooldownTimer_ > 0.0f && isActive_) {
        activeDuration_ = (std::min)(maxExtendTime_, (std::max)(activeDuration_, timer_ + requestedDuration * 0.35f));
        activeSlowTimeScale_ = (std::min)(activeSlowTimeScale_, requestedScale);
        ++ignoredTriggerCount_;
        return;
    }
    if (cooldownTimer_ > 0.0f) {
        ++ignoredTriggerCount_;
        return;
    }

    isActive_ = true;
    timer_ = 0.0f;
    activeDuration_ = requestedDuration;
    activeSlowTimeScale_ = requestedScale;
    cooldownTimer_ = cooldown_;
    ++triggerCount_;
}

void CombatSlowMotionController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(360.0f, 320.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Combat Slow Motion Debug")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Enable Bullet Cancel Slow Motion", &enabled_);
    ImGui::Checkbox("Use Scaled Delta For Effects", &useScaledDeltaForEffects_);
    if (ImGui::Button("Test Trigger Slow Motion")) {
        TriggerBulletCancelSlowMotion(1);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Slow Motion")) {
        Reset();
    }

    ImGui::SeparatorText("Runtime");
    ImGui::Text("Current Time Scale: %.3f", currentTimeScale_);
    ImGui::Text("Is Active: %s", isActive_ ? "true" : "false");
    ImGui::Text("Current Timer: %.3f", timer_);
    ImGui::Text("Current Cooldown: %.3f", cooldownTimer_);
    ImGui::Text("Last Cancel Count: %d", lastCancelCount_);
    ImGui::Text("Trigger Count: %u", triggerCount_);
    ImGui::Text("Ignored Trigger Count: %u", ignoredTriggerCount_);

    ImGui::SeparatorText("Settings");
    ImGui::DragFloat("Slow Time Scale", &slowTimeScale_, 0.01f, 0.05f, 1.0f, "%.2f");
    ImGui::DragFloat("Duration", &duration_, 0.005f, 0.03f, 1.0f, "%.3f");
    ImGui::DragFloat("Ease In Time", &easeInTime_, 0.001f, 0.0f, 0.3f, "%.3f");
    ImGui::DragFloat("Ease Out Time", &easeOutTime_, 0.005f, 0.0f, 0.5f, "%.3f");
    ImGui::DragFloat("Cooldown", &cooldown_, 0.005f, 0.0f, 1.0f, "%.3f");
    ImGui::DragFloat("Max Extend Time", &maxExtendTime_, 0.005f, 0.05f, 1.0f, "%.3f");
    ImGui::DragInt("Min Cancel Count", &minCancelCount_, 1.0f, 1, 16);
    ImGui::End();
#endif
}

void CombatSlowMotionController::ClampSettings() {
    slowTimeScale_ = std::clamp(slowTimeScale_, 0.05f, 1.0f);
    duration_ = std::clamp(duration_, 0.03f, 1.0f);
    easeInTime_ = std::clamp(easeInTime_, 0.0f, 0.3f);
    easeOutTime_ = std::clamp(easeOutTime_, 0.0f, 0.5f);
    cooldown_ = std::clamp(cooldown_, 0.0f, 1.0f);
    maxExtendTime_ = std::clamp(maxExtendTime_, 0.05f, 1.0f);
    minCancelCount_ = std::clamp(minCancelCount_, 1, 16);
}

float CombatSlowMotionController::ResolveDurationForCancelCount(int cancelCount) const {
    float result = duration_;
    if (cancelCount >= 4) {
        result = 0.25f;
    } else if (cancelCount >= 2) {
        result = 0.22f;
    }
    return (std::min)(result, maxExtendTime_);
}

float CombatSlowMotionController::ResolveSlowScaleForCancelCount(int cancelCount) const {
    if (cancelCount >= 4) {
        return (std::max)(0.30f, slowTimeScale_ - 0.05f);
    }
    return slowTimeScale_;
}