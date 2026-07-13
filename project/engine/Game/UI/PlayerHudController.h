#pragma once

#include "Engine/math/Matrix4x4.h"
#include <cstddef>
#include <cstdint>
#include <memory>

class BoostController;
class PlayerBulletManager;
class PlayerDamageFeedbackController;
class Sprite;
class SpriteCommon;

class PlayerHudController {
public:
    enum class HudMode {
        Hidden,
        HpOnly,
        HpAndCharge,
        HpAndBoost,
    };

    enum class ContextType {
        None,
        Charge,
        Boost,
    };

    PlayerHudController();
    ~PlayerHudController();

    void Initialize(
        SpriteCommon* spriteCommon,
        PlayerDamageFeedbackController* damageFeedbackController,
        PlayerBulletManager* bulletManager,
        BoostController* boostController);
    void Finalize();
    void Reset();
    void SetGameModeActive(bool isGameMode);
    void Update(float unscaledDeltaTime);
    void Draw();
    void DrawImGui();

private:
    void SyncEventCounters();
    void ShowHud(HudMode mode, ContextType context, const char* eventName);
    void ForceHud(HudMode mode, ContextType context, const char* eventName);
    void UpdateEventState(float deltaTime);
    void UpdateFade(float deltaTime);
    void DrawRect(const Vector2& position, const Vector2& size, const Vector4& color, float alphaScale);
    void DrawFrame(const Vector2& position, const Vector2& size, float thickness, const Vector4& color, float alphaScale);
    void DrawCornerAccents(const Vector2& position, const Vector2& size, const Vector4& color, float alphaScale);
    void DrawHpGauge(float alpha);
    void DrawContextGauge(float alpha);
    float GetRawChargeRate() const;
    float GetVisualChargeRate() const;
    float GetContextFillRate() const;
    float GetBoostRate() const;
    const char* GetModeName() const;
    const char* GetContextName() const;
    const char* GetModeName(HudMode mode) const;
    const char* GetContextName(ContextType context) const;

    SpriteCommon* spriteCommon_ = nullptr;
    PlayerDamageFeedbackController* damageFeedbackController_ = nullptr;
    PlayerBulletManager* bulletManager_ = nullptr;
    BoostController* boostController_ = nullptr;
    std::unique_ptr<Sprite> rectSprite_;

    bool enablePlayerHud_ = true;
    bool forceVisible_ = false;
    bool lastGameModeActive_ = false;
    bool hasRuntimeModeState_ = false;
    bool enableVisualPolish_ = true;
    bool showOuterShadow_ = true;
    bool showInnerHighlight_ = true;
    bool showCornerAccent_ = true;
    HudMode mode_ = HudMode::Hidden;
    HudMode requestedMode_ = HudMode::Hidden;
    HudMode finalMode_ = HudMode::Hidden;
    ContextType contextType_ = ContextType::None;
    ContextType requestedContextType_ = ContextType::None;
    ContextType activeContextType_ = ContextType::None;
    ContextType previousContextType_ = ContextType::None;
    const char* lastHudEvent_ = "None";

    float idleTimer_ = 0.0f;
    float visibleHoldDuration_ = 3.0f;
    float fadeInDuration_ = 0.12f;
    float fadeOutDuration_ = 0.35f;
    float hpAlpha_ = 0.0f;
    float contextAlpha_ = 0.0f;
    float targetHpAlpha_ = 0.0f;
    float targetContextAlpha_ = 0.0f;
    float hudChargeStartDelay_ = 0.22f;
    float shotContextTimer_ = 0.0f;
    float shotContextMinimumDuration_ = 0.45f;
    float shotPulseTimer_ = 0.0f;
    float shotPulseDuration_ = 0.18f;
    float debugPulseTimer_ = 0.0f;

    float hpX_ = 32.0f;
    float hpY_ = 28.0f;
    float hpWidth_ = 240.0f;
    float hpHeight_ = 18.0f;
    float hpSegmentGap_ = 4.0f;
    float chargeContextWidth_ = 280.0f;
    float chargeContextHeight_ = 10.0f;
    float chargeContextBottomMargin_ = 64.0f;
    float boostContextWidth_ = 240.0f;
    float boostContextHeight_ = 8.0f;
    float boostContextBottomMargin_ = 40.0f;
    float frameThickness_ = 2.0f;
    float hpBackgroundAlpha_ = 0.42f;
    float contextBackgroundAlpha_ = 0.42f;
    float shadowOffset_ = 3.0f;
    float shadowAlpha_ = 0.25f;
    float hpEmptySegmentAlpha_ = 0.18f;
    float hpFrameAlpha_ = 0.60f;
    float hpHighlightAlpha_ = 0.18f;
    float contextHighlightAlpha_ = 0.18f;
    float accentLength_ = 10.0f;
    float accentThickness_ = 2.0f;
    float accentOffset_ = 3.0f;
    float accentAlpha_ = 0.55f;
    float leadingEdgeWidth_ = 3.0f;
    float leadingEdgeAlpha_ = 0.90f;
    float chargeFillAlpha_ = 0.85f;
    float chargeFrameAlpha_ = 0.62f;
    float chargeMaxPulseRate_ = 4.0f;
    float chargeMaxGlowAlpha_ = 0.18f;
    float chargeMaxGlowExpand_ = 3.0f;
    float boostFrameAlpha_ = 0.62f;
    float boostActivePulseRate_ = 5.0f;
    float boostActivePulseStrength_ = 0.10f;
    float cooldownFillAlpha_ = 0.55f;
    float cooldownFrameAlpha_ = 0.42f;
    float cooldownLeadingEdgeAlpha_ = 0.40f;
    float boostReadyPulseDuration_ = 0.22f;
    float boostReadyPulseAlpha_ = 0.25f;
    float contextSwitchPulseDuration_ = 0.12f;

    Vector4 hpGoodColor_{ 0.16f, 0.92f, 0.58f, 1.0f };
    Vector4 hpMidColor_{ 0.95f, 0.74f, 0.20f, 1.0f };
    Vector4 hpLowColor_{ 1.0f, 0.30f, 0.12f, 1.0f };
    Vector4 hpEmptyColor_{ 0.16f, 0.04f, 0.04f, 1.0f };
    Vector4 chargeColor_{ 0.35f, 0.95f, 1.0f, 1.0f };
    Vector4 boostColor_{ 0.20f, 0.58f, 1.0f, 1.0f };
    Vector4 boostCooldownColor_{ 0.24f, 0.42f, 0.62f, 1.0f };
    Vector4 frameColor_{ 0.80f, 0.96f, 1.0f, 1.0f };
    Vector4 backgroundColor_{ 0.02f, 0.03f, 0.04f, 1.0f };

    uint64_t previousDamageCount_ = 0;
    size_t previousShotCount_ = 0;
    uint64_t previousBoostCount_ = 0;
    bool previousChargeHeld_ = false;
    bool previousChargeUiActive_ = false;
    int previousBoostState_ = 0;
    int previousHp_ = 3;
    int damagedSegmentIndex_ = -1;
    int debugHpOverride_ = -1;

    bool rawChargeInputHeld_ = false;
    bool isChargeUiActive_ = false;
    bool chargeDeadZoneActive_ = false;
    bool shotEventThisFrame_ = false;
    bool bothContextDrawnSameFrame_ = false;
    bool usingRawChargeRateForDraw_ = false;
    bool lowHpPulseEnabled_ = true;
    float rawChargeTime_ = 0.0f;
    float rawChargeRate_ = 0.0f;
    float visualChargeRate_ = 0.0f;
    float contextFillRate_ = 0.0f;
    float debugBoostGaugeRate_ = 0.0f;
    float hpDamageAnimTimer_ = 0.0f;
    float hpDamageAnimDuration_ = 0.35f;
    float hpDamageFlashDuration_ = 0.12f;
    float lowHpPulseRate_ = 3.0f;
    float lowHpMinAlpha_ = 0.75f;
    float lowHpMaxAlpha_ = 1.0f;
    float lowHpFramePulseAlpha_ = 0.20f;
    float lowHpPulseValue_ = 0.0f;
    float chargeMaxPulseValue_ = 0.0f;
    float boostActivePulseValue_ = 0.0f;
    float boostReadyPulseTimer_ = 0.0f;
    float contextSwitchPulseTimer_ = 0.0f;
    uint32_t hpDrawCount_ = 0;
    uint32_t chargeContextDrawCount_ = 0;
    uint32_t boostContextDrawCount_ = 0;
};