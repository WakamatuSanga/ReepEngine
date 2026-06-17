#include "PostEffectController.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Core/WinApp.h"
#include "Engine/Graphics/Sprite/Sprite.h"
#include "Engine/Graphics/Sprite/SpriteCommon.h"
#include <algorithm>
#include <cctype>
#include <cmath>

#ifdef _DEBUG
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
    flashSprite_.reset();
    fadeSprite_.reset();
    dxCommon_ = nullptr;
    spriteCommon_ = nullptr;
}

void PostEffectController::Update(float deltaTime) {
    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
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
    if (!spriteCommon_) {
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
#ifdef _DEBUG
    ImGui::SetNextWindowSize(ImVec2(380.0f, 340.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("ポストエフェクト確認 (PostEffect Controller Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("ポストエフェクト有効 (Enable PostEffects)", &enabled_);
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
    ImGui::End();
#endif
}

bool PostEffectController::PlayPostEffect(const std::string& postEffectType, std::string& resultMessage) {
    if (!enabled_) {
        resultMessage = "PostEffectController is disabled.";
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

void PostEffectController::RestoreGrayscale() {
    if (!dxCommon_ || !grayscaleSaved_) {
        return;
    }

    DirectXCommon::PostEffectParameters& params = dxCommon_->GetPostEffectParameters();
    params.grayscaleEnabled = previousGrayscaleEnabled_;
    params.grayscaleIntensity = previousGrayscaleIntensity_;
    grayscaleSaved_ = false;
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
