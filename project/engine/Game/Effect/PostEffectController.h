#pragma once
#include <memory>
#include <string>

class DirectXCommon;
class Sprite;
class SpriteCommon;

class PostEffectController {
public:
    PostEffectController();
    ~PostEffectController();

    void Initialize(DirectXCommon* dxCommon, SpriteCommon* spriteCommon);
    void Finalize();
    void Update(float deltaTime);
    void Draw();
    void DrawImGui();

    bool PlayPostEffect(const std::string& postEffectType, std::string& resultMessage);
    void SetBoostPostEffectTarget(float boostPower, float centerX, float centerY, bool centerValid);
    void SetDiagnosticSuppressed(bool isSuppressed);
    bool IsDiagnosticSuppressed() const { return diagnosticSuppressed_; }

private:
    void PlayFlash();
    void PlayGrayscale();
    void PlayFadeBlack();
    void RestoreGrayscale();
    float CalculateFlashAlpha() const;
    float CalculateFadeAlpha() const;
    void UpdateBoostPostEffect(float deltaTime);
    void ApplyBoostRadialBlur(float strength);
    void RestoreBoostRadialBlur();

    DirectXCommon* dxCommon_ = nullptr;
    SpriteCommon* spriteCommon_ = nullptr;
    std::unique_ptr<Sprite> flashSprite_;
    std::unique_ptr<Sprite> fadeSprite_;

    bool enabled_ = true;
    bool diagnosticSuppressed_ = false;
    bool grayscaleSaved_ = false;
    unsigned int previousGrayscaleEnabled_ = 0;
    float previousGrayscaleIntensity_ = 1.0f;
    float flashElapsed_ = 0.0f;
    float grayscaleElapsed_ = 0.0f;
    float fadeElapsed_ = 0.0f;
    float flashDuration_ = 0.2f;
    float grayscaleDuration_ = 1.5f;
    float fadeDuration_ = 1.0f;
    float flashIntensity_ = 0.85f;
    float grayscaleIntensity_ = 1.0f;
    float fadeIntensity_ = 0.85f;
    bool flashActive_ = false;
    bool grayscaleActive_ = false;
    bool fadeActive_ = false;
    bool enableBoostPostEffect_ = true;
    bool boostEffectCenterFollowPlayer_ = true;
    bool boostRadialSaved_ = false;
    unsigned int previousRadialBlurEnabled_ = 0;
    float previousRadialBlurStrength_ = 0.0f;
    float previousRadialBlurCenterX_ = 0.5f;
    float previousRadialBlurCenterY_ = 0.5f;
    unsigned int previousRadialBlurSampleCount_ = 8;
    float previousRadialBlurCenterClearRadius_ = 0.0f;
    float previousRadialBlurOuterEffectRadius_ = 1.0f;
    float requestedBoostPower_ = 0.0f;
    float requestedBoostCenterX_ = 0.5f;
    float requestedBoostCenterY_ = 0.5f;
    bool requestedBoostCenterValid_ = false;
    float currentBoostEffectIntensity_ = 0.0f;
    float maxBoostEffectIntensity_ = 0.17f;
    float boostEffectStartThreshold_ = 0.06f;
    float boostEffectSmoothSpeed_ = 8.5f;
    float maxBoostRadialBlurStrength_ = 0.17f;
    float boostEffectCenterClearRadius_ = 0.18f;
    float boostEffectOuterRadius_ = 0.72f;
    float currentBoostCenterX_ = 0.5f;
    float currentBoostCenterY_ = 0.5f;
    std::string boostPostEffectReason_ = "Inactive";
    std::string lastPostEffectType_ = "(none)";
    std::string lastResult_ = "(none)";
};
