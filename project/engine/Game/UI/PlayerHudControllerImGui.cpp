#include "PlayerHudController.h"

#include "Engine/Game/Player/BoostController.h"
#include "Engine/Game/Player/PlayerBulletManager.h"
#include "Engine/Game/Player/PlayerDamageFeedbackController.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void PlayerHudController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(500.0f, 760.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Player HUD Debug")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Enable Player HUD", &enablePlayerHud_);
    ImGui::Checkbox("Force Visible", &forceVisible_);
    ImGui::Checkbox("Enable Visual Polish", &enableVisualPolish_);
    ImGui::Checkbox("Show Outer Shadow", &showOuterShadow_);
    ImGui::Checkbox("Show Inner Highlight", &showInnerHighlight_);
    ImGui::Checkbox("Show Corner Accent", &showCornerAccent_);
    ImGui::Text("Current HUD Mode: %s", GetModeName());
    ImGui::Text("Current Context Type: %s", GetContextName());
    ImGui::Text("Requested HUD Mode: %s", GetModeName(requestedMode_));
    ImGui::Text("Final HUD Mode: %s", GetModeName(finalMode_));
    ImGui::Text("Requested Context Type: %s", GetContextName(requestedContextType_));
    ImGui::Text("Active Context Type: %s", GetContextName(activeContextType_));
    ImGui::Text("Previous Context Type: %s", GetContextName(previousContextType_));
    ImGui::Text("HP Alpha / Target: %.2f / %.2f", hpAlpha_, targetHpAlpha_);
    ImGui::Text("Context Alpha / Target: %.2f / %.2f", contextAlpha_, targetContextAlpha_);
    ImGui::Text("Idle Timer: %.2f / %.2f", idleTimer_, visibleHoldDuration_);
    ImGui::Text("Current HP / Max HP: %d / %d",
        damageFeedbackController_ ? damageFeedbackController_->GetHp() : 0,
        damageFeedbackController_ ? damageFeedbackController_->GetMaxHp() : 0);
    ImGui::TextWrapped("Last HUD Event: %s", lastHudEvent_);

    ImGui::SeparatorText("判定確認 (Event / Priority)");
    ImGui::Text("Raw Charge Input Held: %s", rawChargeInputHeld_ ? "true" : "false");
    ImGui::Text("Raw Charge Time: %.3f", rawChargeTime_);
    ImGui::Text("Raw Charge Rate: %.3f", rawChargeRate_);
    ImGui::Text("Visual Charge Rate: %.3f", visualChargeRate_);
    ImGui::Text("Context Fill Rate: %.3f", contextFillRate_);
    ImGui::DragFloat("HUD Charge Start Delay", &hudChargeStartDelay_, 0.01f, 0.0f, 2.0f, "%.2f");
    ImGui::Text("Is Charge UI Active: %s", isChargeUiActive_ ? "true" : "false");
    ImGui::Text("Charge Dead Zone Active: %s", chargeDeadZoneActive_ ? "true" : "false");
    ImGui::Text("Using Raw Charge Rate For Draw: %s", usingRawChargeRateForDraw_ ? "true" : "false");
    ImGui::Text("Fired Bullet Count: %zu", bulletManager_ ? bulletManager_->GetFiredBulletCount() : 0);
    ImGui::Text("Previous Fired Bullet Count: %zu", previousShotCount_);
    ImGui::Text("Shot Event This Frame: %s", shotEventThisFrame_ ? "true" : "false");
    ImGui::Text("Shot Context Timer: %.2f", shotContextTimer_);
    ImGui::Text("Shot Pulse Timer: %.2f", shotPulseTimer_);
    ImGui::Text("Boost State: %d", previousBoostState_);
    ImGui::Text("Boost Gauge Rate: %.2f", debugBoostGaugeRate_);
    ImGui::Text("Damage Event Count: %llu", static_cast<unsigned long long>(previousDamageCount_));
    ImGui::Text("Boost Event Count: %llu", static_cast<unsigned long long>(previousBoostCount_));

    ImGui::SeparatorText("HP Visual");
    ImGui::Text("Previous HP: %d", previousHp_);
    ImGui::Text("Damaged Segment Index: %d", damagedSegmentIndex_);
    ImGui::Text("HP Damage Anim Timer: %.2f", hpDamageAnimTimer_);
    ImGui::DragFloat("HP Damage Anim Duration", &hpDamageAnimDuration_, 0.01f, 0.05f, 2.0f, "%.2f");
    ImGui::DragFloat("HP Damage Flash Duration", &hpDamageFlashDuration_, 0.01f, 0.01f, 1.0f, "%.2f");
    ImGui::Checkbox("Low HP Pulse Enabled", &lowHpPulseEnabled_);
    ImGui::DragFloat("Low HP Pulse Rate", &lowHpPulseRate_, 0.05f, 0.1f, 8.0f, "%.2f");
    ImGui::Text("Low HP Pulse Value: %.2f", lowHpPulseValue_);
    ImGui::ColorEdit4("Healthy Color", &hpGoodColor_.x);
    ImGui::ColorEdit4("Warning Color", &hpMidColor_.x);
    ImGui::ColorEdit4("Critical Color", &hpLowColor_.x);
    ImGui::ColorEdit4("Empty Segment Color", &hpEmptyColor_.x);
    ImGui::DragFloat("HP Shadow Alpha", &shadowAlpha_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("HP Frame Alpha", &hpFrameAlpha_, 0.01f, 0.0f, 1.5f, "%.2f");
    ImGui::DragFloat("HP Highlight Alpha", &hpHighlightAlpha_, 0.01f, 0.0f, 1.0f, "%.2f");

    ImGui::SeparatorText("Charge Visual");
    ImGui::Text("Charge Max: %s", (bulletManager_ && bulletManager_->IsChargeMax()) ? "true" : "false");
    ImGui::Text("Charge Max Pulse Value: %.2f", chargeMaxPulseValue_);
    ImGui::DragFloat("Charge Max Pulse Rate", &chargeMaxPulseRate_, 0.05f, 0.1f, 10.0f, "%.2f");
    ImGui::DragFloat("Charge Fill Alpha", &chargeFillAlpha_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Charge Frame Alpha", &chargeFrameAlpha_, 0.01f, 0.0f, 1.5f, "%.2f");
    ImGui::DragFloat("Leading Edge Width", &leadingEdgeWidth_, 0.25f, 1.0f, 12.0f, "%.1f");
    ImGui::DragFloat("Leading Edge Alpha", &leadingEdgeAlpha_, 0.01f, 0.0f, 1.0f, "%.2f");
    if (ImGui::Button("Test Charge Max Visual")) {
        visualChargeRate_ = 1.0f;
        contextFillRate_ = 1.0f;
        ForceHud(HudMode::HpAndCharge, ContextType::Charge, "Debug Charge Max Visual");
    }

    ImGui::SeparatorText("Boost Visual");
    ImGui::Text("Boost Active Pulse Value: %.2f", boostActivePulseValue_);
    ImGui::DragFloat("Cooldown Fill Alpha", &cooldownFillAlpha_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::Text("Boost Ready Pulse Timer: %.2f", boostReadyPulseTimer_);
    if (ImGui::Button("Test Boost Active Visual")) {
        ForceHud(HudMode::HpAndBoost, ContextType::Boost, "Debug Boost Active Visual");
    }
    ImGui::SameLine();
    if (ImGui::Button("Test Boost Cooldown Visual")) {
        ForceHud(HudMode::HpAndBoost, ContextType::Boost, "Debug Boost Cooldown Visual");
    }
    if (ImGui::Button("Test Boost Ready Pulse")) {
        boostReadyPulseTimer_ = boostReadyPulseDuration_;
        ForceHud(HudMode::HpAndBoost, ContextType::Boost, "Debug Boost Ready Pulse");
    }

    ImGui::SeparatorText("Context / Draw Diagnostics");
    ImGui::Text("Context Switch Pulse Timer: %.2f", contextSwitchPulseTimer_);
    if (ImGui::Button("Test Context Switch")) {
        const ContextType next = activeContextType_ == ContextType::Charge ? ContextType::Boost : ContextType::Charge;
        ForceHud(next == ContextType::Charge ? HudMode::HpAndCharge : HudMode::HpAndBoost, next, "Debug Context Switch");
    }
    ImGui::Text("HP Draw Count: %u", hpDrawCount_);
    ImGui::Text("Charge Context Draw Count: %u", chargeContextDrawCount_);
    ImGui::Text("Boost Context Draw Count: %u", boostContextDrawCount_);
    ImGui::Text("Both Context Drawn Same Frame: %s", bothContextDrawnSameFrame_ ? "true" : "false");

    ImGui::SeparatorText("Timing");
    ImGui::DragFloat("Hold Duration", &visibleHoldDuration_, 0.05f, 0.1f, 10.0f, "%.2f");
    ImGui::DragFloat("Fade In Duration", &fadeInDuration_, 0.01f, 0.0f, 2.0f, "%.2f");
    ImGui::DragFloat("Fade Out Duration", &fadeOutDuration_, 0.01f, 0.0f, 2.0f, "%.2f");
    ImGui::DragFloat("Shot Context Minimum Duration", &shotContextMinimumDuration_, 0.01f, 0.0f, 3.0f, "%.2f");
    ImGui::DragFloat("Shot Pulse Duration", &shotPulseDuration_, 0.01f, 0.0f, 1.0f, "%.2f");

    ImGui::SeparatorText("Test");
    if (ImGui::Button("Test HP Damage 3 to 2")) {
        debugHpOverride_ = 2;
        damagedSegmentIndex_ = 2;
        hpDamageAnimTimer_ = hpDamageAnimDuration_;
        ForceHud(HudMode::HpOnly, ContextType::None, "Debug HP Damage 3 to 2");
    }
    ImGui::SameLine();
    if (ImGui::Button("Test HP Damage 2 to 1")) {
        debugHpOverride_ = 1;
        damagedSegmentIndex_ = 1;
        hpDamageAnimTimer_ = hpDamageAnimDuration_;
        ForceHud(HudMode::HpOnly, ContextType::None, "Debug HP Damage 2 to 1");
    }
    if (ImGui::Button("Test Low HP Visual")) {
        debugHpOverride_ = 1;
        ForceHud(HudMode::HpOnly, ContextType::None, "Debug Low HP Visual");
    }
    ImGui::SameLine();
    if (ImGui::Button("Test Normal Shot Pulse")) {
        shotContextTimer_ = shotContextMinimumDuration_;
        shotPulseTimer_ = shotPulseDuration_;
        visualChargeRate_ = 0.0f;
        contextFillRate_ = 0.0f;
        ForceHud(HudMode::HpAndCharge, ContextType::Charge, "Debug Normal Shot Pulse");
    }
    if (ImGui::Button("Clear Visual Test State")) {
        debugHpOverride_ = -1;
        damagedSegmentIndex_ = -1;
        hpDamageAnimTimer_ = 0.0f;
        boostReadyPulseTimer_ = 0.0f;
        contextSwitchPulseTimer_ = 0.0f;
        visualChargeRate_ = 0.0f;
        contextFillRate_ = 0.0f;
    }
    if (ImGui::Button("Reset HUD")) {
        Reset();
    }

    ImGui::SeparatorText("Layout / Colors");
    ImGui::DragFloat2("HP Position", &hpX_, 1.0f, 0.0f, 4000.0f, "%.0f");
    ImGui::DragFloat2("HP Size", &hpWidth_, 1.0f, 1.0f, 1000.0f, "%.0f");
    ImGui::DragFloat("Segment Gap", &hpSegmentGap_, 0.5f, 0.0f, 40.0f, "%.1f");
    ImGui::DragFloat("Frame Thickness", &frameThickness_, 0.25f, 1.0f, 12.0f, "%.1f");
    ImGui::DragFloat("Charge Bottom Margin", &chargeContextBottomMargin_, 1.0f, 0.0f, 400.0f, "%.0f");
    ImGui::DragFloat("Boost Bottom Margin", &boostContextBottomMargin_, 1.0f, 0.0f, 400.0f, "%.0f");
    ImGui::DragFloat2("Charge Context Size", &chargeContextWidth_, 1.0f, 2.0f, 1000.0f, "%.0f");
    ImGui::DragFloat2("Boost Context Size", &boostContextWidth_, 1.0f, 2.0f, 1000.0f, "%.0f");
    ImGui::DragFloat("Context Background Alpha", &contextBackgroundAlpha_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::ColorEdit4("Charge Color", &chargeColor_.x);
    ImGui::ColorEdit4("Boost Color", &boostColor_.x);
    ImGui::ColorEdit4("Boost Cooldown Color", &boostCooldownColor_.x);

    ImGui::End();
#endif
}