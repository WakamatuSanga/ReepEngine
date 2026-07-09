#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>

class Sprite;
class SpriteCommon;
struct FiredEventAction;

class WarningUIController {
public:
    enum class GlowMode {
        None,
        SimpleSpriteGlow,
    };

    WarningUIController();
    ~WarningUIController();

    void Initialize(SpriteCommon* spriteCommon);
    void Finalize();
    void Update(float deltaTime);
    void Draw();
    void DrawImGui();

    void ShowWarning(float duration);
    void ShowWarning(const std::string& text, float duration);
    void ShowWarning(const std::string& text, float duration, const std::string& source);
    void HideWarning();
    bool HandleAction(const FiredEventAction& action, std::string& resultMessage);

    bool IsActive() const { return isActive_; }

private:
    void LoadWarningTexture();
    void LoadDebugSolidTexture();
    float CalculateFadeAlpha() const;
    float CalculateBlinkAlpha() const;
    float CalculateGlowPulse() const;
    float CalculateScalePulse() const;
    void ApplySpriteLayout(Sprite& sprite, float width, float height, float alpha, const std::array<float, 4>& color);

    SpriteCommon* spriteCommon_ = nullptr;
    std::unique_ptr<Sprite> textSprite_;
    std::array<std::unique_ptr<Sprite>, 3> glowSprites_;
    std::unique_ptr<Sprite> debugSolidSprite_;

    bool enabled_ = true;
    bool isActive_ = false;
    float elapsedTime_ = 0.0f;
    float duration_ = 1.5f;
    float debugDuration_ = 1.5f;
    float lastShowDuration_ = 0.0f;
    float centerX_ = 0.5f;
    float centerY_ = 0.38f;
    float displayWidth_ = 980.0f;
    float displayHeight_ = 245.0f;
    float baseAlpha_ = 1.0f;
    float blinkRate_ = 8.0f;
    float glowIntensity_ = 1.2f;
    float glowPulseRate_ = 6.0f;
    float glowScale_ = 1.22f;
    float fadeInTime_ = 0.10f;
    float fadeOutTime_ = 0.25f;
    float scaleBase_ = 1.0f;
    float scalePulse_ = 0.035f;
    GlowMode glowMode_ = GlowMode::SimpleSpriteGlow;
    std::string currentText_ = "WARNING";
    std::string lastShowSource_ = "Debug";
    uint64_t showCount_ = 0;

    bool forceFullAlpha_ = false;
    bool disableBlinkForDebug_ = false;
    bool disablePulseForDebug_ = false;
    bool drawTextOnly_ = false;
    bool useFallbackTexture_ = false;
    std::string fallbackTexturePath_ = "resources/human/white.png";

    bool drawDebugSolidRect_ = false;
    float debugRectAlpha_ = 0.65f;
    float debugRectCenterX_ = 0.5f;
    float debugRectCenterY_ = 0.38f;
    float debugRectWidth_ = 1000.0f;
    float debugRectHeight_ = 250.0f;
    std::array<float, 4> debugRectColor_{ 1.0f, 0.0f, 0.0f, 0.65f };

    std::string texturePath_ = "resources/ui/warning_text.png";
    std::string textureStatus_ = "Not loaded";
    bool textureFileExists_ = false;
    bool textureLoaded_ = false;
    bool textureHandleValid_ = false;
    uint32_t textureIndex_ = 0;
    unsigned long long textureSrvHandlePtr_ = 0ull;

    std::string debugSolidTexturePath_ = "resources/human/white.png";
    std::string debugSolidTextureStatus_ = "Not loaded";
    bool debugSolidTextureLoaded_ = false;

    uint64_t drawCalledCount_ = 0;
    uint64_t drawSubmittedCount_ = 0;
    uint64_t lastDrawFrame_ = 0;
    std::string drawSkippedReason_ = "Not drawn yet";
    float lastDrawAlpha_ = 0.0f;
    std::array<float, 4> lastTintColor_{ 1.0f, 1.0f, 1.0f, 0.0f };
    std::array<float, 2> lastDrawPositionPx_{ 0.0f, 0.0f };
    std::array<float, 2> lastDrawSizePx_{ 0.0f, 0.0f };
    std::array<float, 2> lastDrawPositionNdc_{ 0.0f, 0.0f };
    std::array<float, 2> lastDrawSizeNdc_{ 0.0f, 0.0f };
};
