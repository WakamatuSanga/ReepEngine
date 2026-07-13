#include "PlayerDamageFeedbackController.h"

#include "Engine/Game/Effect/CombatEffectController.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Core/WinApp.h"
#include "Engine/Graphics/Sprite/Sprite.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <cmath>

namespace {
    constexpr float kTwoPi = 6.28318530718f;

    float Clamp01(float value) {
        return std::clamp(value, 0.0f, 1.0f);
    }
}

PlayerDamageFeedbackController::PlayerDamageFeedbackController() = default;

PlayerDamageFeedbackController::~PlayerDamageFeedbackController() = default;

void PlayerDamageFeedbackController::Initialize(
    SpriteCommon* spriteCommon,
    Player* player,
    CombatEffectController* combatEffectController) {
    spriteCommon_ = spriteCommon;
    player_ = player;
    combatEffectController_ = combatEffectController;
    hp_ = maxHp_;

    if (spriteCommon_) {
        flashSprite_ = std::make_unique<Sprite>();
        flashSprite_->Initialize(spriteCommon_);
        flashSprite_->SetTexture("resources/human/white.png");
        flashSprite_->SetPosition({ 0.0f, 0.0f });
        flashSprite_->SetSize({ static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) });
        flashSprite_->SetColor({ 1.0f, 0.08f, 0.03f, 0.0f });
        flashSprite_->Update();
    }
    ApplyBlinkAlpha(1.0f);
    lastResult_ = "Initialized";
}

void PlayerDamageFeedbackController::Finalize() {
    ApplyBlinkAlpha(1.0f);
    flashSprite_.reset();
    spriteCommon_ = nullptr;
    player_ = nullptr;
    combatEffectController_ = nullptr;
}

void PlayerDamageFeedbackController::Update(float unscaledDeltaTime) {
    const float dt = std::clamp(unscaledDeltaTime, 0.0f, 1.0f / 10.0f);
    blinkTimer_ += dt;

    if (invincibleTimer_ > 0.0f) {
        invincibleTimer_ = (std::max)(0.0f, invincibleTimer_ - dt);
    }
    if (damageFlashTimer_ > 0.0f) {
        damageFlashTimer_ = (std::max)(0.0f, damageFlashTimer_ - dt);
    }

    float blinkAlpha = 1.0f;
    if (enableDamageFeedback_ && IsInvincible() && !IsDead()) {
        const float wave = std::sin(blinkTimer_ * blinkRate_ * kTwoPi) * 0.5f + 0.5f;
        blinkAlpha = 0.55f + 0.45f * wave;
    }
    ApplyBlinkAlpha(blinkAlpha);
}

void PlayerDamageFeedbackController::Draw() {
    if (!enableDamageFeedback_ || !flashSprite_ || damageFlashTimer_ <= 0.0f || damageFlashDuration_ <= 0.0f) {
        return;
    }

    const float t = Clamp01(damageFlashTimer_ / damageFlashDuration_);
    const float alpha = damageFlashAlpha_ * t * t;
    if (alpha <= 0.001f) {
        return;
    }

    flashSprite_->SetPosition({ 0.0f, 0.0f });
    flashSprite_->SetSize({ static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) });
    flashSprite_->SetColor({ 1.0f, 0.08f, 0.03f, alpha });
    flashSprite_->Update();
    flashSprite_->Draw();
}

void PlayerDamageFeedbackController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(360.0f, 390.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Player Damage Feedback Debug")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Enable Damage Feedback", &enableDamageFeedback_);
    ImGui::Checkbox("Damage Spark Enabled", &damageSparkEnabled_);
    ImGui::Text("Player HP: %d / %d", hp_, maxHp_);
    ImGui::Text("Is Invincible: %s", IsInvincible() ? "true" : "false");
    ImGui::Text("Invincible Timer: %.2f", invincibleTimer_);
    ImGui::DragFloat("Invincible Duration", &invincibleDuration_, 0.01f, 0.0f, 5.0f, "%.2f");
    ImGui::DragFloat("Blink Rate", &blinkRate_, 0.1f, 0.0f, 30.0f, "%.1f");
    ImGui::DragFloat("Damage Flash Duration", &damageFlashDuration_, 0.01f, 0.0f, 2.0f, "%.2f");
    ImGui::DragFloat("Damage Flash Alpha", &damageFlashAlpha_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::Text("Last Damage Position: %.2f, %.2f, %.2f", lastDamagePosition_.x, lastDamagePosition_.y, lastDamagePosition_.z);
    ImGui::Text("Damage Feedback Count: %llu", static_cast<unsigned long long>(damageFeedbackCount_));
    ImGui::TextWrapped("Last Result: %s", lastResult_);

    if (ImGui::Button("Test Damage Feedback")) {
        const Vector3 position = player_ ? player_->GetWorldPosition() : Vector3{};
        ApplyDamage(position, 1);
    }
    ImGui::SameLine();
    if (ImGui::Button("Force Damage 1")) {
        const Vector3 position = player_ ? player_->GetWorldPosition() : Vector3{};
        ApplyDamage(position, 1, true);
    }
    if (ImGui::Button("Reset HP")) {
        ResetHp();
    }
    ImGui::Checkbox("Toggle Invincible", &debugForceInvincible_);

    ImGui::End();
#endif
}

bool PlayerDamageFeedbackController::ApplyDamage(const Vector3& position, int damage, bool ignoreInvincible) {
    if (!ignoreInvincible && IsInvincible()) {
        lastDamagePosition_ = position;
        lastResult_ = "Ignored: invincible";
        return false;
    }

    const int damageAmount = (std::max)(1, damage);
    hp_ = (std::max)(0, hp_ - damageAmount);
    invincibleTimer_ = (std::max)(0.0f, invincibleDuration_);
    lastDamagePosition_ = position;
    ++damageFeedbackCount_;
    TriggerDamageVisuals(position);
    lastResult_ = IsDead() ? "Damage applied: dead" : "Damage applied";
    return true;
}

bool PlayerDamageFeedbackController::IsInvincible() const {
    return debugForceInvincible_ || invincibleTimer_ > 0.0f;
}

void PlayerDamageFeedbackController::ResetHp() {
    hp_ = (std::max)(1, maxHp_);
    invincibleTimer_ = 0.0f;
    damageFlashTimer_ = 0.0f;
    debugForceInvincible_ = false;
    ApplyBlinkAlpha(1.0f);
    lastResult_ = "HP reset";
}

void PlayerDamageFeedbackController::ApplyBlinkAlpha(float alpha) {
    if (player_) {
        player_->SetDamageFeedbackAlpha(Clamp01(alpha));
    }
}

void PlayerDamageFeedbackController::TriggerDamageVisuals(const Vector3& position) {
    if (!enableDamageFeedback_) {
        return;
    }

    damageFlashTimer_ = (std::max)(0.0f, damageFlashDuration_);
    blinkTimer_ = 0.0f;
    ApplyBlinkAlpha(0.55f);
    if (damageSparkEnabled_ && combatEffectController_) {
        combatEffectController_->PlayEnemyBulletHitPlayer(position);
    }
}
