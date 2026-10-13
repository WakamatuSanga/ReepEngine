#include "PlayerHudController.h"

#include "Engine/Core/WinApp.h"
#include "Engine/Game/Player/BoostController.h"
#include "Engine/Game/Player/PlayerBulletManager.h"
#include "Engine/Game/Player/PlayerDamageFeedbackController.h"
#include "Engine/Graphics/Sprite/Sprite.h"

#include <algorithm>
#include <cmath>

namespace {
    float Clamp01(float value) {
        if (!std::isfinite(value)) {
            return 0.0f;
        }
        return std::clamp(value, 0.0f, 1.0f);
    }

    float Approach(float current, float target, float deltaTime, float duration) {
        if (duration <= 0.0f) {
            return target;
        }
        const float step = std::clamp(deltaTime / duration, 0.0f, 1.0f);
        if (current < target) {
            return (std::min)(target, current + step);
        }
        return (std::max)(target, current - step);
    }

    float Pulse01(float time, float rate) {
        return 0.5f + 0.5f * std::sin(time * rate * 6.2831853f);
    }

    Vector4 WithAlpha(Vector4 color, float alpha) {
        color.w *= Clamp01(alpha);
        return color;
    }
}

PlayerHudController::PlayerHudController() = default;
PlayerHudController::~PlayerHudController() = default;

void PlayerHudController::Initialize(
    SpriteCommon* spriteCommon,
    PlayerDamageFeedbackController* damageFeedbackController,
    PlayerBulletManager* bulletManager,
    BoostController* boostController) {
    spriteCommon_ = spriteCommon;
    damageFeedbackController_ = damageFeedbackController;
    bulletManager_ = bulletManager;
    boostController_ = boostController;
    if (spriteCommon_) {
        rectSprite_ = std::make_unique<Sprite>();
        rectSprite_->Initialize(spriteCommon_);
        rectSprite_->SetTexture("resources/human/white.png");
        rectSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
        rectSprite_->Update();
    }
    Reset();
}

void PlayerHudController::Finalize() {
    rectSprite_.reset();
    spriteCommon_ = nullptr;
    damageFeedbackController_ = nullptr;
    bulletManager_ = nullptr;
    boostController_ = nullptr;
}

void PlayerHudController::Reset() {
    mode_ = HudMode::Hidden;
    requestedMode_ = HudMode::Hidden;
    finalMode_ = HudMode::Hidden;
    contextType_ = ContextType::None;
    requestedContextType_ = ContextType::None;
    activeContextType_ = ContextType::None;
    previousContextType_ = ContextType::None;
    lastHudEvent_ = "Reset";
    idleTimer_ = 0.0f;
    hpAlpha_ = 0.0f;
    contextAlpha_ = 0.0f;
    targetHpAlpha_ = 0.0f;
    targetContextAlpha_ = 0.0f;
    shotContextTimer_ = 0.0f;
    shotPulseTimer_ = 0.0f;
    debugPulseTimer_ = 0.0f;
    boostReadyPulseTimer_ = 0.0f;
    contextSwitchPulseTimer_ = 0.0f;
    hpDamageAnimTimer_ = 0.0f;
    damagedSegmentIndex_ = -1;
    debugHpOverride_ = -1;
    lowHpPulseValue_ = 0.0f;
    boostActivePulseValue_ = 0.0f;
    shotEventThisFrame_ = false;
    contextFillRate_ = 0.0f;
    bothContextDrawnSameFrame_ = false;
    previousHp_ = damageFeedbackController_ ? damageFeedbackController_->GetHp() : 3;
    SyncEventCounters();
    contextFillRate_ = 0.0f;
}

void PlayerHudController::SetGameModeActive(bool isGameMode) {
    if (!hasRuntimeModeState_) {
        hasRuntimeModeState_ = true;
        lastGameModeActive_ = isGameMode;
        return;
    }
    if (lastGameModeActive_ != isGameMode) {
        Reset();
        lastGameModeActive_ = isGameMode;
    }
}

void PlayerHudController::Update(float unscaledDeltaTime) {
    const float dt = std::clamp(unscaledDeltaTime, 0.0f, 1.0f / 10.0f);
    debugPulseTimer_ += dt;
    shotContextTimer_ = (std::max)(0.0f, shotContextTimer_ - dt);
    shotPulseTimer_ = (std::max)(0.0f, shotPulseTimer_ - dt);
    boostReadyPulseTimer_ = (std::max)(0.0f, boostReadyPulseTimer_ - dt);
    contextSwitchPulseTimer_ = (std::max)(0.0f, contextSwitchPulseTimer_ - dt);
    hpDamageAnimTimer_ = (std::max)(0.0f, hpDamageAnimTimer_ - dt);
    lowHpPulseValue_ = Pulse01(debugPulseTimer_, lowHpPulseRate_);
    boostActivePulseValue_ = Pulse01(debugPulseTimer_, boostActivePulseRate_);

    if (enablePlayerHud_) {
        UpdateEventState(dt);
    } else {
        mode_ = HudMode::Hidden;
        contextType_ = ContextType::None;
        finalMode_ = mode_;
        activeContextType_ = contextType_;
        shotEventThisFrame_ = false;
        contextFillRate_ = 0.0f;
    }
    UpdateFade(dt);
}

void PlayerHudController::Draw() {
    hpDrawCount_ = 0;
    shotContextDrawCount_ = 0;
    boostContextDrawCount_ = 0;
    bothContextDrawnSameFrame_ = false;
    if (!rectSprite_) {
        return;
    }
    if (hpAlpha_ > 0.001f) {
        DrawHpGauge(hpAlpha_);
    }
    if (contextAlpha_ > 0.001f && activeContextType_ != ContextType::None) {
        DrawContextGauge(contextAlpha_);
    }
    bothContextDrawnSameFrame_ = shotContextDrawCount_ > 0 && boostContextDrawCount_ > 0;
}

void PlayerHudController::SyncEventCounters() {
    previousDamageCount_ = damageFeedbackController_ ? damageFeedbackController_->GetDamageFeedbackCount() : 0;
    previousShotCount_ = bulletManager_ ? bulletManager_->GetFiredBulletCount() : 0;
    previousBoostCount_ = boostController_ ? boostController_->GetBoostCount() : 0;
    contextFillRate_ = 0.0f;
    previousBoostState_ = static_cast<int>(boostController_ ? boostController_->GetState() : BoostController::State::Idle);
}

void PlayerHudController::ShowHud(HudMode mode, ContextType context, const char* eventName) {
    requestedMode_ = mode;
    requestedContextType_ = context;
    if (contextType_ != context) {
        previousContextType_ = contextType_;
        if (context != ContextType::None) {
            contextSwitchPulseTimer_ = contextSwitchPulseDuration_;
        }
    }
    mode_ = mode;
    contextType_ = context;
    finalMode_ = mode_;
    activeContextType_ = contextType_;
    idleTimer_ = 0.0f;
    lastHudEvent_ = eventName;
}

void PlayerHudController::ForceHud(HudMode mode, ContextType context, const char* eventName) {
    ShowHud(mode, context, eventName);
    hpAlpha_ = (mode == HudMode::Hidden) ? 0.0f : 1.0f;
    contextAlpha_ = (context == ContextType::None) ? 0.0f : 1.0f;
}

void PlayerHudController::UpdateEventState(float deltaTime) {
    const uint64_t damageCount = damageFeedbackController_ ? damageFeedbackController_->GetDamageFeedbackCount() : 0;
    const size_t shotCount = bulletManager_ ? bulletManager_->GetFiredBulletCount() : 0;
    const uint64_t boostCount = boostController_ ? boostController_->GetBoostCount() : 0;
    const int currentHp = damageFeedbackController_ ? damageFeedbackController_->GetHp() : previousHp_;
    debugBoostGaugeRate_ = GetBoostRate();
    contextFillRate_ = 0.0f;
    const BoostController::State boostState = boostController_ ? boostController_->GetState() : BoostController::State::Idle;

    const bool damageEvent = damageCount > previousDamageCount_;
    shotEventThisFrame_ = shotCount > previousShotCount_;
    const bool boostStartEvent = boostCount > previousBoostCount_;
    const bool boostEndedEvent = previousBoostState_ == static_cast<int>(BoostController::State::Boosting) && boostState == BoostController::State::Cooldown;
    const bool boostReadyEvent = previousBoostState_ == static_cast<int>(BoostController::State::Cooldown) && boostState == BoostController::State::Idle;

    if (currentHp < previousHp_) {
        damagedSegmentIndex_ = (std::max)(0, previousHp_ - 1);
        hpDamageAnimTimer_ = hpDamageAnimDuration_;
    }
    previousHp_ = currentHp;
    if (boostReadyEvent) {
        boostReadyPulseTimer_ = boostReadyPulseDuration_;
    }

    if (damageCount < previousDamageCount_) {
        previousDamageCount_ = damageCount;
    }
    if (shotCount < previousShotCount_) {
        previousShotCount_ = shotCount;
    }
    if (boostCount < previousBoostCount_) {
        previousBoostCount_ = boostCount;
    }

    if (shotEventThisFrame_) {
        shotContextTimer_ = (std::max)(shotContextTimer_, shotContextMinimumDuration_);
        shotPulseTimer_ = shotPulseDuration_;
        contextFillRate_ = 0.0f;
    }

    if (damageEvent) {
        ShowHud(HudMode::HpOnly, ContextType::None, "Damage");
    } else if (shotContextTimer_ > 0.0f) {
        ShowHud(HudMode::HpAndShot, ContextType::Shot, shotEventThisFrame_ ? "Shot Fired" : "Shot Context");
    } else if (boostStartEvent || boostState == BoostController::State::Boosting) {
        ShowHud(HudMode::HpAndBoost, ContextType::Boost, boostStartEvent ? "Boost Start" : "Boosting");
    } else if (boostEndedEvent) {
        ShowHud(HudMode::HpAndBoost, ContextType::Boost, "Boost Cooldown");
    } else {
        requestedMode_ = mode_;
        requestedContextType_ = contextType_;
        finalMode_ = mode_;
        activeContextType_ = contextType_;
    }

    contextFillRate_ = GetContextFillRate();

    previousDamageCount_ = damageCount;
    previousShotCount_ = shotCount;
    previousBoostCount_ = boostCount;
    previousBoostState_ = static_cast<int>(boostState);

    const bool keepAlive = boostState == BoostController::State::Boosting;
    if (mode_ != HudMode::Hidden && !keepAlive) {
        idleTimer_ += deltaTime;
        if (idleTimer_ >= visibleHoldDuration_) {
            ShowHud(HudMode::Hidden, ContextType::None, "Auto Hidden");
        }
    }
    contextFillRate_ = GetContextFillRate();
}

void PlayerHudController::UpdateFade(float deltaTime) {
    HudMode displayMode = mode_;
    ContextType displayContext = contextType_;
    if (forceVisible_ && displayMode == HudMode::Hidden) {
        displayMode = HudMode::HpAndShot;
        displayContext = ContextType::Shot;
    }

    targetHpAlpha_ = displayMode == HudMode::Hidden ? 0.0f : 1.0f;
    targetContextAlpha_ = displayContext == ContextType::None ? 0.0f : 1.0f;
    const float hpDuration = targetHpAlpha_ > hpAlpha_ ? fadeInDuration_ : fadeOutDuration_;
    const float contextDuration = targetContextAlpha_ > contextAlpha_ ? fadeInDuration_ : fadeOutDuration_;
    hpAlpha_ = Approach(hpAlpha_, targetHpAlpha_, deltaTime, hpDuration);
    contextAlpha_ = Approach(contextAlpha_, targetContextAlpha_, deltaTime, contextDuration);
}

void PlayerHudController::DrawRect(const Vector2& position, const Vector2& size, const Vector4& color, float alphaScale) {
    if (!rectSprite_ || size.x <= 0.0f || size.y <= 0.0f || alphaScale <= 0.001f) {
        return;
    }
    rectSprite_->SetPosition(position);
    rectSprite_->SetSize(size);
    rectSprite_->SetColor(WithAlpha(color, alphaScale));
    rectSprite_->Update();
    rectSprite_->Draw();
}

void PlayerHudController::DrawFrame(const Vector2& position, const Vector2& size, float thickness, const Vector4& color, float alphaScale) {
    const float t = (std::max)(1.0f, thickness);
    DrawRect(position, { size.x, t }, color, alphaScale);
    DrawRect({ position.x, position.y + size.y - t }, { size.x, t }, color, alphaScale);
    DrawRect(position, { t, size.y }, color, alphaScale);
    DrawRect({ position.x + size.x - t, position.y }, { t, size.y }, color, alphaScale);
}

void PlayerHudController::DrawCornerAccents(const Vector2& position, const Vector2& size, const Vector4& color, float alphaScale) {
    if (!enableVisualPolish_ || !showCornerAccent_) {
        return;
    }
    DrawRect({ position.x - accentOffset_, position.y - accentOffset_ }, { accentLength_, accentThickness_ }, color, alphaScale * accentAlpha_);
    DrawRect({ position.x + size.x - accentLength_ + accentOffset_, position.y + size.y + accentOffset_ - accentThickness_ }, { accentLength_, accentThickness_ }, color, alphaScale * accentAlpha_);
}

void PlayerHudController::DrawHpGauge(float alpha) {
    ++hpDrawCount_;
    const int actualHp = damageFeedbackController_ ? damageFeedbackController_->GetHp() : 0;
    const int maxHp = (std::max)(1, damageFeedbackController_ ? damageFeedbackController_->GetMaxHp() : 3);
    const int hp = std::clamp(debugHpOverride_ >= 0 ? debugHpOverride_ : actualHp, 0, maxHp);
    Vector4 activeColor = hpGoodColor_;
    if (hp <= 1) {
        activeColor = hpLowColor_;
    } else if (hp <= 2) {
        activeColor = hpMidColor_;
    }

    const Vector2 origin{ hpX_, hpY_ };
    const Vector2 framePos{ origin.x - 4.0f, origin.y - 4.0f };
    const Vector2 frameSize{ hpWidth_ + 8.0f, hpHeight_ + 8.0f };
    if (enableVisualPolish_ && showOuterShadow_) {
        DrawRect({ framePos.x + shadowOffset_, framePos.y + shadowOffset_ }, frameSize, { 0.0f, 0.0f, 0.0f, 1.0f }, alpha * shadowAlpha_);
    }
    DrawRect(framePos, frameSize, backgroundColor_, alpha * hpBackgroundAlpha_);

    const float totalGap = hpSegmentGap_ * static_cast<float>(maxHp - 1);
    const float segmentWidth = (hpWidth_ - totalGap) / static_cast<float>(maxHp);
    const float lowPulse = lowHpPulseEnabled_ && hp == 1 ? lowHpMinAlpha_ + (lowHpMaxAlpha_ - lowHpMinAlpha_) * lowHpPulseValue_ : 1.0f;
    for (int i = 0; i < maxHp; ++i) {
        const Vector2 pos{ origin.x + static_cast<float>(i) * (segmentWidth + hpSegmentGap_), origin.y };
        DrawRect(pos, { segmentWidth, hpHeight_ }, hpEmptyColor_, alpha * hpEmptySegmentAlpha_);
        if (i < hp) {
            DrawRect({ pos.x + 1.0f, pos.y + 1.0f }, { segmentWidth - 2.0f, hpHeight_ - 2.0f }, activeColor, alpha * lowPulse);
            if (enableVisualPolish_ && showInnerHighlight_) {
                DrawRect({ pos.x + 2.0f, pos.y + 2.0f }, { segmentWidth - 4.0f, 2.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, alpha * hpHighlightAlpha_ * lowPulse);
            }
        }
        if (i == damagedSegmentIndex_ && hpDamageAnimTimer_ > 0.0f) {
            const float flash = Clamp01(hpDamageAnimTimer_ / (std::max)(hpDamageAnimDuration_, 0.001f));
            DrawRect({ pos.x + 1.0f, pos.y + 1.0f }, { segmentWidth - 2.0f, hpHeight_ - 2.0f }, { 1.0f, 0.42f, 0.18f, 1.0f }, alpha * flash * 0.85f);
        }
    }

    float frameAlpha = hpFrameAlpha_;
    if (hp == 1 && lowHpPulseEnabled_) {
        frameAlpha += lowHpFramePulseAlpha_ * lowHpPulseValue_;
    }
    Vector4 frame = hp <= 0 ? Vector4{ 0.55f, 0.06f, 0.04f, 1.0f } : frameColor_;
    DrawFrame(framePos, frameSize, frameThickness_, frame, alpha * frameAlpha);
    DrawCornerAccents(framePos, frameSize, activeColor, alpha);
}

void PlayerHudController::DrawContextGauge(float alpha) {
    const bool boost = activeContextType_ == ContextType::Boost;
    const BoostController::State boostState = boostController_ ? boostController_->GetState() : BoostController::State::Idle;
    const bool boosting = boost && boostState == BoostController::State::Boosting;
    const bool cooldown = boost && boostState == BoostController::State::Cooldown;
    const float width = boost ? boostContextWidth_ : shotContextWidth_;
    const float height = boost ? boostContextHeight_ : shotContextHeight_;
    const float bottomMargin = boost ? boostContextBottomMargin_ : shotContextBottomMargin_;
    const float screenWidth = static_cast<float>(WinApp::kClientWidth);
    const float screenHeight = static_cast<float>(WinApp::kClientHeight);
    const Vector2 origin{ screenWidth * 0.5f - width * 0.5f, screenHeight - bottomMargin - height };
    const Vector2 framePos{ origin.x - 4.0f, origin.y - 4.0f };
    const Vector2 frameSize{ width + 8.0f, height + 8.0f };
    Vector4 color = boost ? (cooldown ? boostCooldownColor_ : boostColor_) : shotColor_;
    const float rate = GetContextFillRate();

    if (boost) {
        ++boostContextDrawCount_;
    } else {
        ++shotContextDrawCount_;
    }
    if (enableVisualPolish_ && showOuterShadow_) {
        DrawRect({ framePos.x + shadowOffset_, framePos.y + shadowOffset_ }, frameSize, { 0.0f, 0.0f, 0.0f, 1.0f }, alpha * shadowAlpha_);
    }
    DrawRect(framePos, frameSize, backgroundColor_, alpha * contextBackgroundAlpha_);

    float frameAlpha = boost ? (cooldown ? cooldownFrameAlpha_ : boostFrameAlpha_) : shotFrameAlpha_;
    if (!boost && shotPulseTimer_ > 0.0f) {
        frameAlpha += 0.22f * Clamp01(shotPulseTimer_ / (std::max)(shotPulseDuration_, 0.001f));
    }
    if (boosting) {
        frameAlpha += boostActivePulseStrength_ * boostActivePulseValue_;
    }
    if (boost && boostReadyPulseTimer_ > 0.0f) {
        frameAlpha += boostReadyPulseAlpha_ * Clamp01(boostReadyPulseTimer_ / (std::max)(boostReadyPulseDuration_, 0.001f));
    }
    if (contextSwitchPulseTimer_ > 0.0f) {
        frameAlpha += 0.25f * Clamp01(contextSwitchPulseTimer_ / (std::max)(contextSwitchPulseDuration_, 0.001f));
    }

    const float fillWidth = width * Clamp01(rate);
    const float fillAlpha = boost ? (cooldown ? cooldownFillAlpha_ : 0.82f + boostActivePulseStrength_ * boostActivePulseValue_) : shotFillAlpha_;
    if (fillWidth > 0.5f) {
        DrawRect(origin, { fillWidth, height }, color, alpha * fillAlpha);
        if (enableVisualPolish_ && showInnerHighlight_) {
            const float highlightAlpha = contextHighlightAlpha_;
            DrawRect({ origin.x + 1.0f, origin.y + 1.0f }, { (std::max)(0.0f, fillWidth - 2.0f), 2.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, alpha * highlightAlpha);
        }
        const float edgeAlpha = boost && cooldown ? cooldownLeadingEdgeAlpha_ : leadingEdgeAlpha_;
        const float edgeWidth = (std::min)(leadingEdgeWidth_, fillWidth);
        DrawRect({ origin.x + fillWidth - edgeWidth, origin.y }, { edgeWidth, height }, { 1.0f, 1.0f, 1.0f, 1.0f }, alpha * edgeAlpha);
    }
    if (!boost && shotPulseTimer_ > 0.0f && rate <= 0.001f) {
        const float pulse = Clamp01(shotPulseTimer_ / (std::max)(shotPulseDuration_, 0.001f));
        DrawRect(origin, { (std::min)(3.0f, width), height }, shotColor_, alpha * pulse * 0.85f);
        DrawRect({ origin.x, origin.y + 1.0f }, { width, 2.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, alpha * pulse * 0.12f);
    }

    DrawFrame(framePos, frameSize, frameThickness_, frameColor_, alpha * frameAlpha);
    DrawCornerAccents(framePos, frameSize, color, alpha);
}

float PlayerHudController::GetContextFillRate() const {
    if (activeContextType_ == ContextType::Boost) {
        return GetBoostRate();
    }
    return 0.0f;
}

float PlayerHudController::GetBoostRate() const {
    return boostController_ ? Clamp01(boostController_->GetBoostGaugeRate()) : 0.0f;
}

const char* PlayerHudController::GetModeName() const {
    return GetModeName(mode_);
}

const char* PlayerHudController::GetContextName() const {
    return GetContextName(contextType_);
}

const char* PlayerHudController::GetModeName(HudMode mode) const {
    switch (mode) {
    case HudMode::HpOnly:
        return "HpOnly";
    case HudMode::HpAndShot:
        return "HpAndShot";
    case HudMode::HpAndBoost:
        return "HpAndBoost";
    case HudMode::Hidden:
    default:
        return "Hidden";
    }
}

const char* PlayerHudController::GetContextName(ContextType context) const {
    switch (context) {
    case ContextType::Shot:
        return "Shot";
    case ContextType::Boost:
        return "Boost";
    case ContextType::None:
    default:
        return "None";
    }
}