#include "PostEffectController.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Core/WinApp.h"
#include "Engine/Graphics/Sprite/Sprite.h"
#include "Engine/Graphics/Sprite/SpriteCommon.h"
#include <algorithm>
#include <cctype>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
    std::string ToLowerString(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
            });
        return text;
    }

    float SmoothStep(float value) {
        const float t = std::clamp(value, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    void InitializeOverlaySprite(std::unique_ptr<Sprite>& sprite, SpriteCommon* spriteCommon) {
        sprite = std::make_unique<Sprite>();
        sprite->Initialize(spriteCommon);
        sprite->SetTexture("resources/human/white.png");
        sprite->SetPosition({ 0.0f, 0.0f });
        sprite->SetSize({ static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) });
        sprite->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
        sprite->Update();
    }
}

PostEffectController::PostEffectController() = default;

PostEffectController::~PostEffectController() = default;

void PostEffectController::Initialize(DirectXCommon* dxCommon, SpriteCommon* spriteCommon) {
    dxCommon_ = dxCommon;
    spriteCommon_ = spriteCommon;
    if (spriteCommon_) {
        InitializeOverlaySprite(flashSprite_, spriteCommon_);
        InitializeOverlaySprite(fadeSprite_, spriteCommon_);
    }
}

void PostEffectController::Finalize() {
    RestoreGrayscale();
    RestoreBoostRadialBlur();
    flashSprite_.reset();
    fadeSprite_.reset();
    dxCommon_ = nullptr;
    spriteCommon_ = nullptr;
}

void PostEffectController::Update(float deltaTime) {
    if (diagnosticSuppressed_ || !enabled_) {
        flashActive_ = false;
        fadeActive_ = false;
        grayscaleActive_ = false;
        RestoreGrayscale();
        RestoreBoostRadialBlur();
        currentBoostEffectIntensity_ = 0.0f;
        return;
    }

    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    UpdateBoostPostEffect(safeDeltaTime);
    if (flashActive_) {
        flashElapsed_ += safeDeltaTime;
        flashActive_ = flashElapsed_ < flashDuration_;
    }
    if (fadeActive_) {
        fadeElapsed_ += safeDeltaTime;
        fadeActive_ = fadeElapsed_ < fadeDuration_;
    }
    if (grayscaleActive_) {
        grayscaleElapsed_ += safeDeltaTime;
        if (dxCommon_) {
            DirectXCommon::PostEffectParameters& params = dxCommon_->GetPostEffectParameters();
            params.grayscaleEnabled = 1;
            params.grayscaleIntensity = grayscaleIntensity_;
        }
        if (grayscaleElapsed_ >= grayscaleDuration_) {
            grayscaleActive_ = false;
            RestoreGrayscale();
        }
    }
}

void PostEffectController::Draw() {
    if (diagnosticSuppressed_ || !enabled_ || !spriteCommon_) {
        return;
    }

    const float fadeAlpha = CalculateFadeAlpha();
    const float flashAlpha = CalculateFlashAlpha();
    if (fadeAlpha <= 0.001f && flashAlpha <= 0.001f) {
        return;
    }

    spriteCommon_->CommonDrawSetting();
    if (fadeSprite_ && fadeAlpha > 0.001f) {
        fadeSprite_->SetColor({ 0.0f, 0.0f, 0.0f, fadeAlpha });
        fadeSprite_->SetSize({ static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) });
        fadeSprite_->Update();
        fadeSprite_->Draw();
    }
    if (flashSprite_ && flashAlpha > 0.001f) {
        flashSprite_->SetColor({ 1.0f, 1.0f, 1.0f, flashAlpha });
        flashSprite_->SetSize({ static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) });
        flashSprite_->Update();
        flashSprite_->Draw();
    }
}

void PostEffectController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(380.0f, 340.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("ポストエフェクト確認 (PostEffect Controller Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("ポストエフェクト有効 (Enable PostEffects)", &enabled_);
    if (diagnosticSuppressed_) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "影診断でPostEffectが一時無効です。");
    }
    ImGui::Text("Active Effects: Flash=%s Grayscale=%s FadeBlack=%s",
        flashActive_ ? "ON" : "OFF",
        grayscaleActive_ ? "ON" : "OFF",
        fadeActive_ ? "ON" : "OFF");
    ImGui::TextWrapped("Last PostEffect Type: %s", lastPostEffectType_.c_str());
    ImGui::TextWrapped("Last Result: %s", lastResult_.c_str());
    ImGui::SliderFloat("Flash Intensity", &flashIntensity_, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Grayscale Intensity", &grayscaleIntensity_, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Fade Alpha", &fadeIntensity_, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Flash Duration", &flashDuration_, 0.01f, 0.01f, 5.0f, "%.2f");
    ImGui::DragFloat("Grayscale Duration", &grayscaleDuration_, 0.05f, 0.1f, 10.0f, "%.2f");
    ImGui::DragFloat("FadeBlack Duration", &fadeDuration_, 0.05f, 0.1f, 10.0f, "%.2f");
    if (ImGui::Button("Test Flash")) {
        std::string result;
        PlayPostEffect("Flash", result);
    }
    ImGui::SameLine();
    if (ImGui::Button("Test Grayscale")) {
        std::string result;
        PlayPostEffect("Grayscale", result);
    }
    ImGui::SameLine();
    if (ImGui::Button("Test FadeBlack")) {
        std::string result;
        PlayPostEffect("FadeBlack", result);
    }

    ImGui::SeparatorText("Boost Post Effect Debug");
    ImGui::Checkbox("Boost Post Effectを使う (Enable Boost Post Effect)", &enableBoostPostEffect_);
    ImGui::Checkbox("Player位置を中心にする (Boost Effect Center Follow Player)", &boostEffectCenterFollowPlayer_);
    ImGui::DragFloat("Max Boost Effect Intensity", &maxBoostEffectIntensity_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Boost Effect Start Threshold", &boostEffectStartThreshold_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Boost Effect Smooth Speed", &boostEffectSmoothSpeed_, 0.1f, 0.0f, 30.0f, "%.1f");
    ImGui::DragFloat("Radial Blur Strength", &maxBoostRadialBlurStrength_, 0.005f, 0.0f, 0.5f, "%.3f");
    ImGui::DragFloat("Center Clear Radius", &boostEffectCenterClearRadius_, 0.005f, 0.0f, 1.0f, "%.3f");
    ImGui::DragFloat("Outer Effect Radius", &boostEffectOuterRadius_, 0.005f, 0.01f, 2.0f, "%.3f");
    if (ImGui::Button("安全なBoost演出プリセット (Use Safe Boost Effect Preset)")) {
        maxBoostEffectIntensity_ = 0.08f;
        boostEffectStartThreshold_ = 0.08f;
        boostEffectSmoothSpeed_ = 7.0f;
        maxBoostRadialBlurStrength_ = 0.08f;
        boostEffectCenterClearRadius_ = 0.14f;
        boostEffectOuterRadius_ = 0.80f;
    }
    ImGui::SameLine();
    if (ImGui::Button("標準Boost演出プリセット (Use Medium Boost Effect Preset)")) {
        maxBoostEffectIntensity_ = 0.14f;
        boostEffectStartThreshold_ = 0.06f;
        boostEffectSmoothSpeed_ = 8.0f;
        maxBoostRadialBlurStrength_ = 0.14f;
        boostEffectCenterClearRadius_ = 0.16f;
        boostEffectOuterRadius_ = 0.75f;
    }
    ImGui::SameLine();
    if (ImGui::Button("標準+Boost演出プリセット (Use Medium Plus Boost Effect Preset)")) {
        maxBoostEffectIntensity_ = 0.17f;
        boostEffectStartThreshold_ = 0.06f;
        boostEffectSmoothSpeed_ = 8.5f;
        maxBoostRadialBlurStrength_ = 0.17f;
        boostEffectCenterClearRadius_ = 0.18f;
        boostEffectOuterRadius_ = 0.72f;
    }
    ImGui::SameLine();
    if (ImGui::Button("強めBoost演出プリセット (Use Strong Boost Effect Preset)")) {
        maxBoostEffectIntensity_ = 0.20f;
        boostEffectStartThreshold_ = 0.05f;
        boostEffectSmoothSpeed_ = 9.0f;
        maxBoostRadialBlurStrength_ = 0.18f;
        boostEffectCenterClearRadius_ = 0.18f;
        boostEffectOuterRadius_ = 0.70f;
    }
    ImGui::Text("Current Boost Power: %.3f", requestedBoostPower_);
    ImGui::Text("Current Boost Effect Intensity: %.3f", currentBoostEffectIntensity_);
    ImGui::Text("Current Boost Effect Center: %.2f, %.2f", currentBoostCenterX_, currentBoostCenterY_);
    ImGui::TextWrapped("Boost PostEffect Reason: %s", boostPostEffectReason_.c_str());
    ImGui::End();
#endif
}

void PostEffectController::SetBoostPostEffectTarget(float boostPower, float centerX, float centerY, bool centerValid) {
    requestedBoostPower_ = std::clamp(boostPower, 0.0f, 1.0f);
    requestedBoostCenterX_ = std::clamp(centerX, 0.0f, 1.0f);
    requestedBoostCenterY_ = std::clamp(centerY, 0.0f, 1.0f);
    requestedBoostCenterValid_ = centerValid && std::isfinite(centerX) && std::isfinite(centerY);
}

bool PostEffectController::PlayPostEffect(const std::string& postEffectType, std::string& resultMessage) {
    if (!enabled_ || diagnosticSuppressed_) {
        resultMessage = diagnosticSuppressed_
            ? "PostEffectController is suppressed by shadow-like debug."
            : "PostEffectController is disabled.";
        lastResult_ = resultMessage;
        return false;
    }

    const std::string type = postEffectType.empty() ? "Flash" : postEffectType;
    const std::string lowerType = ToLowerString(type);
    lastPostEffectType_ = type;
    if (lowerType == "flash") {
        PlayFlash();
    } else if (lowerType == "grayscale" || lowerType == "greyscale") {
        PlayGrayscale();
    } else if (lowerType == "fadeblack" || lowerType == "fade_black") {
        PlayFadeBlack();
    } else {
        resultMessage = "Unsupported postEffectType: " + type;
        lastResult_ = resultMessage;
        return false;
    }

    resultMessage = "Played post effect: " + type;
    lastResult_ = resultMessage;
    return true;
}

void PostEffectController::PlayFlash() {
    flashElapsed_ = 0.0f;
    flashActive_ = true;
}

void PostEffectController::PlayGrayscale() {
    if (dxCommon_ && !grayscaleSaved_) {
        const DirectXCommon::PostEffectParameters& params = dxCommon_->GetPostEffectParameters();
        previousGrayscaleEnabled_ = params.grayscaleEnabled;
        previousGrayscaleIntensity_ = params.grayscaleIntensity;
        grayscaleSaved_ = true;
    }
    grayscaleElapsed_ = 0.0f;
    grayscaleActive_ = true;
}

void PostEffectController::PlayFadeBlack() {
    fadeElapsed_ = 0.0f;
    fadeActive_ = true;
}

void PostEffectController::UpdateBoostPostEffect(float deltaTime) {
    const float targetIntensity = (enableBoostPostEffect_ && requestedBoostPower_ > boostEffectStartThreshold_)
        ? requestedBoostPower_ * std::clamp(maxBoostEffectIntensity_, 0.0f, 1.0f)
        : 0.0f;
    const float smoothSpeed = (std::max)(boostEffectSmoothSpeed_, 0.0f);
    const float t = smoothSpeed <= 0.0f ? 1.0f : 1.0f - std::exp(-std::clamp(deltaTime, 0.0f, 1.0f / 15.0f) * smoothSpeed);
    currentBoostEffectIntensity_ += (targetIntensity - currentBoostEffectIntensity_) * std::clamp(t, 0.0f, 1.0f);
    if (boostEffectCenterFollowPlayer_ && requestedBoostCenterValid_) {
        currentBoostCenterX_ = requestedBoostCenterX_;
        currentBoostCenterY_ = requestedBoostCenterY_;
    } else {
        currentBoostCenterX_ = 0.5f;
        currentBoostCenterY_ = 0.5f;
    }
    if (currentBoostEffectIntensity_ > 0.001f) {
        ApplyBoostRadialBlur(currentBoostEffectIntensity_ * std::clamp(maxBoostRadialBlurStrength_, 0.0f, 0.5f));
    } else {
        RestoreBoostRadialBlur();
        boostPostEffectReason_ = enableBoostPostEffect_ ? "Waiting for boost" : "Disabled";
    }
}

void PostEffectController::ApplyBoostRadialBlur(float strength) {
    if (!dxCommon_) {
        boostPostEffectReason_ = "DirectXCommon missing";
        return;
    }
    DirectXCommon::PostEffectParameters& params = dxCommon_->GetPostEffectParameters();
    if (!boostRadialSaved_) {
        previousRadialBlurEnabled_ = params.radialBlurEnabled;
        previousRadialBlurStrength_ = params.radialBlurStrength;
        previousRadialBlurCenterX_ = params.radialBlurCenter[0];
        previousRadialBlurCenterY_ = params.radialBlurCenter[1];
        previousRadialBlurSampleCount_ = params.radialBlurSampleCount;
        previousRadialBlurCenterClearRadius_ = params.radialBlurCenterClearRadius;
        previousRadialBlurOuterEffectRadius_ = params.radialBlurOuterEffectRadius;
        boostRadialSaved_ = true;
    }
    params.radialBlurEnabled = 1u;
    params.radialBlurStrength = (std::max)(strength, previousRadialBlurEnabled_ ? previousRadialBlurStrength_ : 0.0f);
    params.radialBlurCenter = { currentBoostCenterX_, currentBoostCenterY_ };
    params.radialBlurSampleCount = previousRadialBlurEnabled_ ? previousRadialBlurSampleCount_ : 6u;
    params.radialBlurCenterClearRadius = std::clamp(boostEffectCenterClearRadius_, 0.0f, 1.0f);
    params.radialBlurOuterEffectRadius = std::clamp((std::max)(boostEffectOuterRadius_, params.radialBlurCenterClearRadius + 0.001f), 0.001f, 2.0f);
    boostPostEffectReason_ = requestedBoostCenterValid_ ? "Boost radial blur active" : "Boost radial blur active with center fallback";
}

void PostEffectController::RestoreBoostRadialBlur() {
    if (!dxCommon_ || !boostRadialSaved_) {
        boostRadialSaved_ = false;
        return;
    }
    DirectXCommon::PostEffectParameters& params = dxCommon_->GetPostEffectParameters();
    params.radialBlurEnabled = previousRadialBlurEnabled_;
    params.radialBlurStrength = previousRadialBlurStrength_;
    params.radialBlurCenter = { previousRadialBlurCenterX_, previousRadialBlurCenterY_ };
    params.radialBlurSampleCount = previousRadialBlurSampleCount_;
    params.radialBlurCenterClearRadius = previousRadialBlurCenterClearRadius_;
    params.radialBlurOuterEffectRadius = previousRadialBlurOuterEffectRadius_;
    boostRadialSaved_ = false;
}

void PostEffectController::RestoreGrayscale() {
    if (!dxCommon_ || !grayscaleSaved_) {
        return;
    }

    DirectXCommon::PostEffectParameters& params = dxCommon_->GetPostEffectParameters();
    params.grayscaleEnabled = previousGrayscaleEnabled_;
    params.grayscaleIntensity = previousGrayscaleIntensity_;
    grayscaleSaved_ = false;
}

void PostEffectController::SetDiagnosticSuppressed(bool isSuppressed) {
    if (diagnosticSuppressed_ == isSuppressed) {
        return;
    }

    diagnosticSuppressed_ = isSuppressed;
    if (diagnosticSuppressed_) {
        flashActive_ = false;
        fadeActive_ = false;
        grayscaleActive_ = false;
        RestoreGrayscale();
        RestoreBoostRadialBlur();
        currentBoostEffectIntensity_ = 0.0f;
    }
}

float PostEffectController::CalculateFlashAlpha() const {
    if (!flashActive_ || flashDuration_ <= 0.0001f) {
        return 0.0f;
    }
    return flashIntensity_ * (1.0f - SmoothStep(flashElapsed_ / flashDuration_));
}

float PostEffectController::CalculateFadeAlpha() const {
    if (!fadeActive_ || fadeDuration_ <= 0.0001f) {
        return 0.0f;
    }
    const float t = std::clamp(fadeElapsed_ / fadeDuration_, 0.0f, 1.0f);
    return fadeIntensity_ * std::sin(t * 3.14159265358979323846f);
}

