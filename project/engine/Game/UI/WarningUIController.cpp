#include "WarningUIController.h"
#include "Engine/Core/WinApp.h"
#include "Engine/Graphics/Sprite/Sprite.h"
#include "Engine/Graphics/Sprite/SpriteCommon.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Level/LevelEventRuntime.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kTwoPi = 6.28318530717958647692f;
    constexpr const char* kWarningTexturePath = "resources/ui/warning_text.png";
    constexpr const char* kDebugSolidTexturePath = "resources/human/white.png";

    float SafeDuration(float duration) {
        return (std::max)(0.05f, duration);
    }

    void LogWarningUi(const std::string& message) {
        OutputDebugStringA(("[WarningUI] " + message + "\n").c_str());
    }

    const char* GlowModeName(WarningUIController::GlowMode mode) {
        switch (mode) {
        case WarningUIController::GlowMode::None:
            return "None";
        case WarningUIController::GlowMode::SimpleSpriteGlow:
            return "Simple Sprite Glow";
        default:
            return "Unknown";
        }
    }

    std::string CurrentWorkingDirectoryString() {
        std::error_code error;
        const std::filesystem::path currentPath = std::filesystem::current_path(error);
        return error ? std::string("<unknown>") : currentPath.generic_string();
    }

    std::string ResolveTexturePath(const std::string& requestedPath, bool& exists, std::string& status) {
        const std::array<std::string, 5> candidates = {
            requestedPath,
            "project/" + requestedPath,
            "../project/" + requestedPath,
            "../../project/" + requestedPath,
            "../../../project/" + requestedPath,
        };

        std::ostringstream tried;
        std::error_code error;
        for (const std::string& candidate : candidates) {
            const std::filesystem::path candidatePath(candidate);
            tried << candidate << "; ";
            if (std::filesystem::exists(candidatePath, error)) {
                exists = true;
                status = "Found texture. cwd=" + CurrentWorkingDirectoryString();
                return candidatePath.generic_string();
            }
            error.clear();
        }

        exists = false;
        status = "Texture file missing. cwd=" + CurrentWorkingDirectoryString() + " tried=" + tried.str();
        return requestedPath;
    }
}

WarningUIController::WarningUIController() = default;

WarningUIController::~WarningUIController() = default;

void WarningUIController::Initialize(SpriteCommon* spriteCommon) {
    spriteCommon_ = spriteCommon;
    if (!spriteCommon_) {
        textureStatus_ = "SpriteCommon missing";
        drawSkippedReason_ = "Draw Path Not Ready: SpriteCommon missing";
        LogWarningUi(textureStatus_);
        return;
    }

    textSprite_ = std::make_unique<Sprite>();
    textSprite_->Initialize(spriteCommon_);

    for (std::unique_ptr<Sprite>& glowSprite : glowSprites_) {
        glowSprite = std::make_unique<Sprite>();
        glowSprite->Initialize(spriteCommon_);
    }

    debugSolidSprite_ = std::make_unique<Sprite>();
    debugSolidSprite_->Initialize(spriteCommon_);

    LoadWarningTexture();
    LoadDebugSolidTexture();
}

void WarningUIController::Finalize() {
    textSprite_.reset();
    for (std::unique_ptr<Sprite>& glowSprite : glowSprites_) {
        glowSprite.reset();
    }
    debugSolidSprite_.reset();
    spriteCommon_ = nullptr;
    isActive_ = false;
    elapsedTime_ = 0.0f;
    textureLoaded_ = false;
    textureHandleValid_ = false;
    textureSrvHandlePtr_ = 0ull;
    debugSolidTextureLoaded_ = false;
}

void WarningUIController::Update(float deltaTime) {
    if (!isActive_) {
        return;
    }

    elapsedTime_ += std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    if (elapsedTime_ >= duration_) {
        isActive_ = false;
        elapsedTime_ = duration_;
    }
}

void WarningUIController::Draw() {
    ++drawCalledCount_;
    lastDrawAlpha_ = 0.0f;

    if (!enabled_) {
        drawSkippedReason_ = "Disabled";
        return;
    }
    if (!spriteCommon_) {
        drawSkippedReason_ = "Draw Path Not Ready: SpriteCommon missing";
        return;
    }

    bool commonDrawSettingApplied = false;
    const auto ensureCommonDrawSetting = [&]() {
        if (!commonDrawSettingApplied) {
            spriteCommon_->CommonDrawSetting();
            commonDrawSettingApplied = true;
        }
    };

    bool drewDebugSolidRect = false;
    if (drawDebugSolidRect_ && !isActive_) {
        if (debugSolidSprite_ && debugSolidTextureLoaded_) {
            ensureCommonDrawSetting();
            const float savedCenterX = centerX_;
            const float savedCenterY = centerY_;
            centerX_ = debugRectCenterX_;
            centerY_ = debugRectCenterY_;
            const float alpha = std::clamp(debugRectAlpha_, 0.0f, 1.0f);
            ApplySpriteLayout(
                *debugSolidSprite_,
                debugRectWidth_,
                debugRectHeight_,
                alpha,
                { debugRectColor_[0], debugRectColor_[1], debugRectColor_[2], alpha });
            debugSolidSprite_->Draw();
            centerX_ = savedCenterX;
            centerY_ = savedCenterY;
            drewDebugSolidRect = true;
            ++drawSubmittedCount_;
        } else {
            drawSkippedReason_ = "Debug Solid Texture Not Loaded";
        }
    }

    if (!isActive_) {
        drawSkippedReason_ = drewDebugSolidRect ? "Not Active (Debug Solid Rect drawn)" : "Not Active";
        return;
    }
    if (!textSprite_) {
        drawSkippedReason_ = "Sprite Null";
        return;
    }
    if (!textureLoaded_) {
        drawSkippedReason_ = "Texture Not Loaded";
        return;
    }
    if (!textureHandleValid_) {
        drawSkippedReason_ = "Invalid Texture Handle";
        return;
    }

    const float fadeAlpha = CalculateFadeAlpha();
    const float blinkAlpha = disableBlinkForDebug_ ? 1.0f : CalculateBlinkAlpha();
    const float textAlpha = forceFullAlpha_
        ? 1.0f
        : std::clamp(baseAlpha_ * fadeAlpha * blinkAlpha, 0.0f, 1.0f);
    if (textAlpha <= 0.001f) {
        drawSkippedReason_ = "Alpha Zero";
        return;
    }

    const float scale = disablePulseForDebug_ ? (std::max)(0.01f, scaleBase_) : CalculateScalePulse();
    const float width = displayWidth_ * scale;
    const float height = displayHeight_ * scale;
    if (width <= 1.0f || height <= 1.0f) {
        drawSkippedReason_ = "Size Zero";
        return;
    }

    ensureCommonDrawSetting();

    if (!drawTextOnly_ && glowMode_ == GlowMode::SimpleSpriteGlow) {
        const float glowPulse = CalculateGlowPulse();
        const std::array<float, 3> scaleSteps = {
            glowScale_,
            glowScale_ + 0.12f,
            glowScale_ + 0.26f,
        };
        const std::array<float, 3> alphaSteps = {
            0.30f,
            0.16f,
            0.08f,
        };

        for (size_t index = 0; index < glowSprites_.size(); ++index) {
            Sprite* glowSprite = glowSprites_[index].get();
            if (!glowSprite) {
                continue;
            }

            const float glowAlpha = std::clamp(textAlpha * glowPulse * alphaSteps[index], 0.0f, 1.0f);
            ApplySpriteLayout(
                *glowSprite,
                width * scaleSteps[index],
                height * scaleSteps[index],
                glowAlpha,
                { 1.0f, 0.08f, 0.02f, glowAlpha });
            glowSprite->Draw();
            ++drawSubmittedCount_;
        }
    }

    ApplySpriteLayout(*textSprite_, width, height, textAlpha, { 1.0f, 1.0f, 1.0f, textAlpha });
    textSprite_->Draw();
    ++drawSubmittedCount_;
    lastDrawFrame_ = drawCalledCount_;
    drawSkippedReason_ = drewDebugSolidRect ? "Drawn (with Debug Solid Rect)" : "Drawn";
}

void WarningUIController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(470.0f, 700.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("WARNING UI?? (Warning UI Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("WARNING UI??? (Enable Warning UI)", &enabled_);
    if (ImGui::Button("Test Show Warning")) {
        ShowWarning(debugDuration_);
        lastShowSource_ = "Debug";
    }
    ImGui::DragFloat("Duration", &debugDuration_, 0.05f, 0.1f, 10.0f, "%.2f");
    ImGui::DragFloat("Position X", &centerX_, 0.005f, 0.0f, 1.0f, "%.3f");
    ImGui::DragFloat("Position Y", &centerY_, 0.005f, 0.0f, 1.0f, "%.3f");
    ImGui::DragFloat("Display Width", &displayWidth_, 5.0f, 100.0f, 2000.0f, "%.1f");
    ImGui::DragFloat("Display Height", &displayHeight_, 5.0f, 50.0f, 1000.0f, "%.1f");
    ImGui::DragFloat("Base Alpha", &baseAlpha_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Blink Rate", &blinkRate_, 0.1f, 0.0f, 30.0f, "%.2f");

    const char* glowModes[] = { "None", "Simple Sprite Glow" };
    int glowModeIndex = static_cast<int>(glowMode_);
    if (ImGui::Combo("Glow Mode", &glowModeIndex, glowModes, IM_ARRAYSIZE(glowModes))) {
        glowMode_ = static_cast<GlowMode>(std::clamp(glowModeIndex, 0, 1));
    }
    ImGui::Text("Glow: %s", GlowModeName(glowMode_));
    ImGui::DragFloat("Glow Intensity", &glowIntensity_, 0.02f, 0.0f, 5.0f, "%.2f");
    ImGui::DragFloat("Glow Pulse Rate", &glowPulseRate_, 0.1f, 0.0f, 30.0f, "%.2f");
    ImGui::DragFloat("Glow Scale", &glowScale_, 0.01f, 1.0f, 2.0f, "%.2f");
    ImGui::DragFloat("Fade In Time", &fadeInTime_, 0.01f, 0.0f, 2.0f, "%.2f");
    ImGui::DragFloat("Fade Out Time", &fadeOutTime_, 0.01f, 0.0f, 2.0f, "%.2f");
    ImGui::DragFloat("Scale Base", &scaleBase_, 0.005f, 0.5f, 2.0f, "%.3f");
    ImGui::DragFloat("Scale Pulse", &scalePulse_, 0.005f, 0.0f, 0.3f, "%.3f");

    ImGui::SeparatorText("Visibility Isolation Debug");
    ImGui::Checkbox("Force Full Alpha", &forceFullAlpha_);
    ImGui::Checkbox("Disable Blink For Debug", &disableBlinkForDebug_);
    ImGui::Checkbox("Disable Pulse For Debug", &disablePulseForDebug_);
    ImGui::Checkbox("Draw Text Only", &drawTextOnly_);
    if (ImGui::Checkbox("Use Fallback Texture", &useFallbackTexture_)) {
        LoadWarningTexture();
    }
    ImGui::TextWrapped("Fallback Texture Path: %s", fallbackTexturePath_.c_str());
    if (ImGui::Button("Reload Warning Texture")) {
        LoadWarningTexture();
    }

    ImGui::SeparatorText("Debug Solid Rect");
    ImGui::Checkbox("Draw Debug Solid Rect", &drawDebugSolidRect_);
    ImGui::DragFloat("Debug Rect Alpha", &debugRectAlpha_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Debug Rect Position X", &debugRectCenterX_, 0.005f, 0.0f, 1.0f, "%.3f");
    ImGui::DragFloat("Debug Rect Position Y", &debugRectCenterY_, 0.005f, 0.0f, 1.0f, "%.3f");
    ImGui::DragFloat("Debug Rect Width", &debugRectWidth_, 5.0f, 10.0f, 2000.0f, "%.1f");
    ImGui::DragFloat("Debug Rect Height", &debugRectHeight_, 5.0f, 10.0f, 1000.0f, "%.1f");
    ImGui::ColorEdit4("Debug Rect Color", debugRectColor_.data());

    ImGui::SeparatorText("Draw Path Debug");
    ImGui::Text("Draw Called Count: %llu", static_cast<unsigned long long>(drawCalledCount_));
    ImGui::Text("Draw Submitted Count: %llu", static_cast<unsigned long long>(drawSubmittedCount_));
    ImGui::Text("Last Draw Frame: %llu", static_cast<unsigned long long>(lastDrawFrame_));
    ImGui::TextWrapped("Draw Skipped Reason: %s", drawSkippedReason_.c_str());
    ImGui::Text("Is Active: %s", isActive_ ? "true" : "false");
    ImGui::Text("Enable Warning UI: %s", enabled_ ? "true" : "false");

    ImGui::SeparatorText("Texture Visibility Debug");
    ImGui::Text("Texture Loaded: %s", textureLoaded_ ? "true" : "false");
    ImGui::Text("Texture File Exists: %s", textureFileExists_ ? "true" : "false");
    ImGui::TextWrapped("Texture Path: %s", texturePath_.c_str());
    ImGui::Text("Texture Index: %u", textureIndex_);
    ImGui::Text("SRV Handle Valid: %s", textureHandleValid_ ? "true" : "false");
    ImGui::Text("SRV Handle: 0x%llX", textureSrvHandlePtr_);
    ImGui::TextWrapped("Texture Status: %s", textureStatus_.c_str());
    ImGui::Text("Debug Solid Texture Loaded: %s", debugSolidTextureLoaded_ ? "true" : "false");
    ImGui::TextWrapped("Debug Solid Texture Status: %s", debugSolidTextureStatus_.c_str());
    ImGui::Text("UV Range: 0,0 -> 1,1");
    ImGui::Text("Last Draw Alpha: %.3f", lastDrawAlpha_);
    ImGui::Text("Last Tint: %.2f, %.2f, %.2f, %.2f", lastTintColor_[0], lastTintColor_[1], lastTintColor_[2], lastTintColor_[3]);
    ImGui::Text("Last Draw Position px: %.1f, %.1f", lastDrawPositionPx_[0], lastDrawPositionPx_[1]);
    ImGui::Text("Last Draw Size px: %.1f, %.1f", lastDrawSizePx_[0], lastDrawSizePx_[1]);
    ImGui::Text("Last Draw Position NDC: %.3f, %.3f", lastDrawPositionNdc_[0], lastDrawPositionNdc_[1]);
    ImGui::Text("Last Draw Size NDC: %.3f, %.3f", lastDrawSizeNdc_[0], lastDrawSizeNdc_[1]);

    ImGui::Separator();
    ImGui::Text("Current Time: %.2f / %.2f", elapsedTime_, duration_);
    ImGui::TextWrapped("Text: %s", currentText_.c_str());
    ImGui::TextWrapped("Last Source: %s", lastShowSource_.c_str());

    ImGui::End();
#endif
}

void WarningUIController::ShowWarning(float duration) {
    ShowWarning("WARNING", duration);
}

void WarningUIController::ShowWarning(const std::string& text, float duration) {
    currentText_ = text.empty() ? "WARNING" : text;
    duration_ = SafeDuration(duration);
    elapsedTime_ = 0.0f;
    isActive_ = true;
    drawDebugSolidRect_ = false;
}

bool WarningUIController::HandleAction(const FiredEventAction& action, std::string& resultMessage) {
    const float duration = action.warningDuration > 0.0f ? action.warningDuration : debugDuration_;
    ShowWarning(action.warningText.empty() ? "WARNING" : action.warningText, duration);
    lastShowSource_ = "EventAction";
    resultMessage =
        "ShowWarning text=" + currentText_ +
        " duration=" + std::to_string(duration_);
    return true;
}

void WarningUIController::LoadWarningTexture() {
    textureLoaded_ = false;
    textureHandleValid_ = false;
    textureSrvHandlePtr_ = 0ull;
    textureIndex_ = 0;

    const std::string requestedPath = useFallbackTexture_ ? fallbackTexturePath_ : kWarningTexturePath;
    texturePath_ = ResolveTexturePath(requestedPath, textureFileExists_, textureStatus_);
    LogWarningUi("Load texture: " + texturePath_);
    if (!textureFileExists_) {
        LogWarningUi(textureStatus_);
        return;
    }
    if (!textSprite_) {
        textureStatus_ = "Text sprite missing";
        LogWarningUi(textureStatus_);
        return;
    }

    textSprite_->SetTexture(texturePath_);
    textureIndex_ = textSprite_->GetTextureIndex();

    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_);
    textureSrvHandlePtr_ = static_cast<unsigned long long>(srvHandle.ptr);
    textureHandleValid_ = srvHandle.ptr != 0;
    textureLoaded_ = textureHandleValid_;

    for (std::unique_ptr<Sprite>& glowSprite : glowSprites_) {
        if (glowSprite) {
            glowSprite->SetTexture(texturePath_);
        }
    }

    textureStatus_ = textureLoaded_
        ? "Loaded. cwd=" + CurrentWorkingDirectoryString()
        : "Texture SRV handle invalid. cwd=" + CurrentWorkingDirectoryString();
    LogWarningUi(textureStatus_ + " index=" + std::to_string(textureIndex_));
}

void WarningUIController::LoadDebugSolidTexture() {
    bool exists = false;
    debugSolidTexturePath_ = ResolveTexturePath(kDebugSolidTexturePath, exists, debugSolidTextureStatus_);
    debugSolidTextureLoaded_ = false;
    if (!exists || !debugSolidSprite_) {
        LogWarningUi("Debug solid texture failed: " + debugSolidTextureStatus_);
        return;
    }

    debugSolidSprite_->SetTexture(debugSolidTexturePath_);
    debugSolidTextureLoaded_ = true;
    debugSolidTextureStatus_ = "Loaded. path=" + debugSolidTexturePath_;
}

float WarningUIController::CalculateFadeAlpha() const {
    const float fadeIn = fadeInTime_ > 0.001f ? std::clamp(elapsedTime_ / fadeInTime_, 0.0f, 1.0f) : 1.0f;
    const float remainingTime = duration_ - elapsedTime_;
    const float fadeOut = fadeOutTime_ > 0.001f ? std::clamp(remainingTime / fadeOutTime_, 0.0f, 1.0f) : 1.0f;
    return (std::min)(fadeIn, fadeOut);
}

float WarningUIController::CalculateBlinkAlpha() const {
    const float pulse = 0.5f + 0.5f * std::sin(elapsedTime_ * blinkRate_ * kTwoPi);
    return 0.55f + 0.45f * pulse;
}

float WarningUIController::CalculateGlowPulse() const {
    const float pulse = 0.5f + 0.5f * std::sin(elapsedTime_ * glowPulseRate_ * kTwoPi);
    return std::clamp(glowIntensity_ * (0.65f + 0.35f * pulse), 0.0f, 5.0f);
}

float WarningUIController::CalculateScalePulse() const {
    const float pulse = std::sin(elapsedTime_ * blinkRate_ * kTwoPi);
    return (std::max)(0.01f, scaleBase_ + scalePulse_ * pulse);
}

void WarningUIController::ApplySpriteLayout(
    Sprite& sprite,
    float width,
    float height,
    float alpha,
    const std::array<float, 4>& color) {
    const float screenWidth = static_cast<float>(WinApp::kClientWidth);
    const float screenHeight = static_cast<float>(WinApp::kClientHeight);
    const float clampedWidth = (std::max)(1.0f, width);
    const float clampedHeight = (std::max)(1.0f, height);
    const float leftPx = screenWidth * centerX_ - clampedWidth * 0.5f;
    const float topPx = screenHeight * centerY_ - clampedHeight * 0.5f;

    const float leftNdc = (leftPx / screenWidth) * 2.0f - 1.0f;
    const float topNdc = 1.0f - (topPx / screenHeight) * 2.0f;
    const float widthNdc = (clampedWidth / screenWidth) * 2.0f;
    const float heightNdc = -(clampedHeight / screenHeight) * 2.0f;

    lastDrawAlpha_ = std::clamp(alpha, 0.0f, 1.0f);
    lastTintColor_ = { color[0], color[1], color[2], lastDrawAlpha_ };
    lastDrawPositionPx_ = { leftPx, topPx };
    lastDrawSizePx_ = { clampedWidth, clampedHeight };
    lastDrawPositionNdc_ = { leftNdc, topNdc };
    lastDrawSizeNdc_ = { widthNdc, heightNdc };

    sprite.SetPosition({ leftPx, topPx });
    sprite.SetSize({ clampedWidth, clampedHeight });
    sprite.SetColor({
        color[0],
        color[1],
        color[2],
        lastDrawAlpha_,
        });
    sprite.Update();
}
