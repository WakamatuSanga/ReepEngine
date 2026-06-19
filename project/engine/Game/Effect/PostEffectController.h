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
    void SetDiagnosticSuppressed(bool isSuppressed);
    bool IsDiagnosticSuppressed() const { return diagnosticSuppressed_; }

private:
    void PlayFlash();
    void PlayGrayscale();
    void PlayFadeBlack();
    void RestoreGrayscale();
    float CalculateFlashAlpha() const;
    float CalculateFadeAlpha() const;

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
    std::string lastPostEffectType_ = "(none)";
    std::string lastResult_ = "(none)";
};
