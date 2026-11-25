#include "AimCorridorVisualController.h"

#include "AimCorridorVisualRenderer.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Core/GameViewport.h"
#include "Engine/Graphics/Sprite/Sprite.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kMouseReticleScale = 1.15f;
    constexpr float kOrangeRingDelay = 0.18f;
    constexpr std::array<float, 3> kRingDegreesPerSecond{-60.0f, 100.0f, 150.0f};
    constexpr std::array<float, 3> kRingSize{1.0f, 0.73f, 0.49f};
    constexpr std::array<Vector4, 3> kRingColors{{{1, 1, 1, 1}, {1, 0.27f, 0.015f, 1}, {1, 0.025f, 0.025f, 1}}};

    Vector3 Subtract(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    }

    float Dot(const Vector3& lhs, const Vector3& rhs) {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
    }

    float ClampFinite(float value, float minimum, float maximum, float fallback) {
        if (!std::isfinite(value)) {
            return std::clamp(fallback, minimum, maximum);
        }
        return std::clamp(value, minimum, maximum);
    }
}

void AimCorridorVisualController::SetTargetingVisualState(
    AimReticleVisualState state,
    float lockProgress) {
    reticleVisualState_ = state;
    targetingLockProgress_ = ClampFinite(lockProgress, 0.0f, 1.0f, 0.0f);
}

void AimCorridorVisualController::UpdateTargetingAppearance(float unscaledDeltaTime) {
    const Vector3 candidateColor{ 1.0f, 0.847059f, 0.290196f };
    const Vector3 acquiringColor{ 1.0f, 0.603922f, 0.196078f };
    const Vector3 lockedColor{ 1.0f, 0.290196f, 0.164706f };
    Vector3 targetColor = candidateColor;
    const float targetTintAmount = reticleVisualState_ == AimReticleVisualState::Normal ? 0.0f : 1.0f;
    if (reticleVisualState_ == AimReticleVisualState::AcquiringPreview) {
        const float t = std::clamp(targetingLockProgress_, 0.0f, 1.0f);
        targetColor = {
            candidateColor.x + (acquiringColor.x - candidateColor.x) * t,
            candidateColor.y + (acquiringColor.y - candidateColor.y) * t,
            candidateColor.z + (acquiringColor.z - candidateColor.z) * t,
        };
    } else if (reticleVisualState_ == AimReticleVisualState::LockedPreview) {
        targetColor = lockedColor;
    }
    const float alpha = 1.0f - std::exp(-unscaledDeltaTime / 0.08f);
    currentReticleTint_ = {
        currentReticleTint_.x + (targetColor.x - currentReticleTint_.x) * alpha,
        currentReticleTint_.y + (targetColor.y - currentReticleTint_.y) * alpha,
        currentReticleTint_.z + (targetColor.z - currentReticleTint_.z) * alpha,
    };
    reticleTintAmount_ += (targetTintAmount - reticleTintAmount_) * alpha;
}

void AimCorridorVisualController::UpdatePresentation() {
    mainReticleCenterWorld_ = farCenter_;
    mainReticleProjectionValid_ = ProjectWorldToScreenUv(
        mainReticleCenterWorld_, mainReticleCenterScreenUv_);
    mainReticleDepth_ = Dot(Subtract(mainReticleCenterWorld_, cameraPosition_), cameraForward_);

    // The world-space Aiming_1 footprint remains the lock-on and corridor-shot reference.
    const float farAspect = renderer_ ? renderer_->GetFarAspectRatio() : 0.0f;
    const bool aspectValid = std::isfinite(farAspect) && farAspect > 0.0f;
    const bool depthValid = std::isfinite(mainReticleDepth_) && mainReticleDepth_ > 0.0f;
    const bool fovValid = std::isfinite(leadFovY_) && leadFovY_ > 0.0f && leadFovY_ < kPi;
    const float visibleWorldHeightAtDepth = depthValid && fovValid
        ? 2.0f * std::tan(leadFovY_ * 0.5f) * mainReticleDepth_
        : 0.0f;

    effectiveMainReticleWorldHeight_ = manualMainReticleWorldHeight_;
    if (autoMainReticleSize_ && std::isfinite(visibleWorldHeightAtDepth) && visibleWorldHeightAtDepth > 0.0f) {
        effectiveMainReticleWorldHeight_ = ClampFinite(
            visibleWorldHeightAtDepth * mainReticleViewportHeightRatio_,
            0.01f,
            100.0f,
            manualMainReticleWorldHeight_);
    }
    effectiveMainReticleWorldWidth_ = aspectValid
        ? effectiveMainReticleWorldHeight_ * farAspect
        : 0.0f;
    mainReticleSizeValid_ = depthValid
        && aspectValid
        && std::isfinite(effectiveMainReticleWorldHeight_)
        && effectiveMainReticleWorldHeight_ > 0.0f
        && std::isfinite(effectiveMainReticleWorldWidth_)
        && effectiveMainReticleWorldWidth_ > 0.0f;

    estimatedMainReticlePixelHeight_ = 0.0f;
    estimatedMainReticlePixelWidth_ = 0.0f;
    mainReticleScreenRect_ = {};
    const D3D12_VIEWPORT viewport = dxCommon_ ? dxCommon_->GetBackBufferViewport() : D3D12_VIEWPORT{};
    if (mainReticleSizeValid_
        && std::isfinite(visibleWorldHeightAtDepth)
        && visibleWorldHeightAtDepth > 0.0f
        && std::isfinite(viewport.Height)
        && viewport.Height > 0.0f) {
        const float projectedHeightRatio = effectiveMainReticleWorldHeight_ / visibleWorldHeightAtDepth;
        estimatedMainReticlePixelHeight_ = viewport.Height * projectedHeightRatio;
        estimatedMainReticlePixelWidth_ = estimatedMainReticlePixelHeight_ * farAspect;
        if (mainReticleProjectionValid_ && std::isfinite(viewport.Width) && viewport.Width > 0.0f
            && estimatedMainReticlePixelWidth_ > 0.0f && estimatedMainReticlePixelHeight_ > 0.0f) {
            mainReticleScreenRect_.centerUV = mainReticleCenterScreenUv_;
            mainReticleScreenRect_.halfSizeUV = {
                estimatedMainReticlePixelWidth_ / viewport.Width * 0.5f,
                estimatedMainReticlePixelHeight_ / viewport.Height * 0.5f,
            };
            mainReticleScreenRect_.minUV = {
                mainReticleScreenRect_.centerUV.x - mainReticleScreenRect_.halfSizeUV.x,
                mainReticleScreenRect_.centerUV.y - mainReticleScreenRect_.halfSizeUV.y,
            };
            mainReticleScreenRect_.maxUV = {
                mainReticleScreenRect_.centerUV.x + mainReticleScreenRect_.halfSizeUV.x,
                mainReticleScreenRect_.centerUV.y + mainReticleScreenRect_.halfSizeUV.y,
            };
            mainReticleScreenRect_.valid = true;
        }
    }
}

AimReticleScreenRect AimCorridorVisualController::GetMouseReticleScreenRect(const GameViewport& gameViewport) const {
    const auto& rect = gameViewport.GetGameViewportRect();
    const Vector2 client = gameViewport.GetClientSize();
    if (!mouseReticleRings_[0] || rect.width <= 1.0f || rect.height <= 1.0f || client.x <= 1.0f || client.y <= 1.0f) return {};
    const Vector2 cursor = gameViewport.GetMouseNormalizedInGameViewport();
    // World projection covers the back buffer, including the area under the menu strip.
    const Vector2 center{(rect.left + cursor.x * rect.width) / client.x,
        (rect.top + cursor.y * rect.height) / client.y};
    const float radius = rect.height * mainReticleViewportHeightRatio_ * kMouseReticleScale * 0.5f;
    const Vector2 half{radius / client.x, radius / client.y};
    return {center, half, {center.x - half.x, center.y - half.y}, {center.x + half.x, center.y + half.y}, true};
}

void AimCorridorVisualController::UpdateMouseLockRings(std::string_view targetId, bool locked, float lockElapsed, float deltaTime) {
    if (targetId != mouseRingTargetId_) {
        mouseRingTargetId_ = targetId;
        mouseRingAngles_.fill(0.0f);
        mouseRingCount_ = 1;
    }
    if (targetId.empty()) return;
    const size_t count = locked ? 3u : (lockElapsed >= kOrangeRingDelay ? 2u : 1u);
    for (size_t i = 0; i < (std::min)(count, mouseRingCount_); ++i) {
        mouseRingAngles_[i] = std::fmod(mouseRingAngles_[i] + deltaTime * kRingDegreesPerSecond[i] * kPi / 180.0f, 2.0f * kPi);
    }
    mouseRingCount_ = count;
}

bool AimCorridorVisualController::DrawMouseReticle(const GameViewport& gameViewport) {
    if (!gameModeActive_ || !playerAlive_ || !enabled_ || forceHide_ ||
        displayMode_ != DisplayMode::MainReticleOnly || debugDiamondPreview_ ||
        gameViewport.GetMode() != GameViewport::Mode::FullscreenGame ||
        !gameViewport.IsMouseInGameViewport() ||
        (!gameViewport.HasAutomatedAim() && !gameViewport.IsGameWindowActive())) {
        return false;
    }

    const auto& rect = gameViewport.GetGameViewportRect();
    const auto reticle = GetMouseReticleScreenRect(gameViewport);
    if (!reticle.valid) return false;
    const Vector2 clientSize = gameViewport.GetClientSize();
    const auto viewport = dxCommon_->GetBackBufferViewport();
    if (clientSize.x <= 0.0f || clientSize.y <= 0.0f || viewport.Width <= 0.0f || viewport.Height <= 0.0f) {
        return false;
    }
    const float scaleX = viewport.Width / clientSize.x;
    const float scaleY = viewport.Height / clientSize.y;
    const auto scissor = dxCommon_->GetBackBufferScissorRect();
    const D3D12_RECT clip{
        (std::max)(scissor.left, static_cast<LONG>(std::ceil(viewport.TopLeftX + rect.left * scaleX))),
        (std::max)(scissor.top, static_cast<LONG>(std::ceil(viewport.TopLeftY + rect.top * scaleY))),
        (std::min)(scissor.right, static_cast<LONG>(std::ceil(viewport.TopLeftX + (rect.left + rect.width) * scaleX))),
        (std::min)(scissor.bottom, static_cast<LONG>(std::ceil(viewport.TopLeftY + (rect.top + rect.height) * scaleY))),
    };
    if (clip.left >= clip.right || clip.top >= clip.bottom) {
        return false;
    }

    auto* commandList = dxCommon_->GetCommandList();
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &clip);
    spriteCommon_->CommonDrawSetting(true);
    const Vector2 center{reticle.centerUV.x * clientSize.x, reticle.centerUV.y * clientSize.y};
    for (size_t i = 0; i < mouseRingCount_; ++i) {
        auto& ring = *mouseReticleRings_[i];
        const float half = reticle.halfSizeUV.y * clientSize.y * kRingSize[i];
        const float angle = mouseRingAngles_[i];
        const float c = std::cos(angle), s = std::sin(angle);
        ring.SetCanvasSize(clientSize);
        ring.SetPosition({center.x - half * (c - s), center.y - half * (s + c)});
        ring.SetSize({2.0f * half, 2.0f * half});
        ring.SetRotation(angle);
        ring.SetColor(kRingColors[i]);
        ring.Update();
        ring.Draw();
    }
    commandList->RSSetScissorRects(1, &scissor);
    lastDrawCount_ = static_cast<uint32_t>(mouseRingCount_);
    return true;
}

void AimCorridorVisualController::DrawPresentation(float pulse) {
    const bool mainReticleOnly = displayMode_ == DisplayMode::MainReticleOnly;
    const AppearanceParameters& farAppearance = mainReticleOnly ? mainReticleAppearance_ : farAppearance_;

    AimCorridorVisualRenderer::FrameDraw farFrame{};
    farFrame.center = mainReticleOnly ? mainReticleCenterWorld_ : farCenter_;
    farFrame.right = cameraRight_;
    farFrame.up = cameraUp_;
    farFrame.height = mainReticleOnly ? effectiveMainReticleWorldHeight_ : effectiveFarWorldHeight_;
    const bool useAiming2ForMain = mainReticleOnly && !debugDiamondPreview_ && renderer_->IsNearTextureLoaded();
    farFrame.width = useAiming2ForMain ? farFrame.height * renderer_->GetNearAspectRatio()
        : (mainReticleOnly ? effectiveMainReticleWorldWidth_ : farWorldWidth_);
    farFrame.alpha = currentAlpha_ * (forceFullAlpha_ ? 1.0f : farAppearance.alpha);
    farFrame.coreIntensity = farAppearance.coreIntensity;
    farFrame.glowIntensity = farAppearance.glowIntensity;
    farFrame.glowAlpha = farAppearance.glowAlpha;
    farFrame.glowRadiusTexels = farAppearance.glowRadiusTexels;
    farFrame.pulseScale = 1.0f + pulse * farAppearance.pulseAmount;
    farFrame.coreTint = currentReticleTint_;
    farFrame.glowTint = currentReticleTint_;
    farFrame.tintAmount = mainReticleOnly ? reticleTintAmount_ : 0.0f;

    const float nearAspect = renderer_ ? renderer_->GetNearAspectRatio() : 0.0f;
    const bool previewAspectValid = std::isfinite(nearAspect) && nearAspect > 0.0f;
    AimCorridorVisualRenderer::FrameDraw nearFrame{};
    nearFrame.center = mainReticleOnly ? mainReticleCenterWorld_ : nearCenter_;
    nearFrame.right = cameraRight_;
    nearFrame.up = cameraUp_;
    nearFrame.height = mainReticleOnly
        ? effectiveMainReticleWorldHeight_ * debugDiamondPreviewScale_
        : nearWorldHeight_;
    nearFrame.width = mainReticleOnly && previewAspectValid
        ? nearFrame.height * nearAspect
        : nearWorldWidth_;
    nearFrame.alpha = currentAlpha_ * (forceFullAlpha_ ? 1.0f : nearAppearance_.alpha);
    nearFrame.coreIntensity = nearAppearance_.coreIntensity;
    nearFrame.glowIntensity = nearAppearance_.glowIntensity;
    nearFrame.glowAlpha = nearAppearance_.glowAlpha;
    nearFrame.glowRadiusTexels = nearAppearance_.glowRadiusTexels;
    nearFrame.pulseScale = 1.0f + pulse * nearAppearance_.pulseAmount;

    const bool drawFar = !mainReticleOnly || mainReticleSizeValid_;
    const bool drawNear = mainReticleOnly
        ? debugDiamondPreview_ && mainReticleSizeValid_ && previewAspectValid
        : true;
    lastDrawCount_ = renderer_->Draw(
        camera_, farFrame, nearFrame, drawFar, drawNear, disableGlow_, showCoreOnly_, useAiming2ForMain);
}

void AimCorridorVisualController::ClampPresentationParameters() {
    mainReticleViewportHeightRatio_ = ClampFinite(mainReticleViewportHeightRatio_, 0.05f, 0.16f, 0.095f);
    manualMainReticleWorldHeight_ = ClampFinite(manualMainReticleWorldHeight_, 0.01f, 100.0f, 5.0f);
    debugDiamondPreviewScale_ = ClampFinite(debugDiamondPreviewScale_, 1.0f, 2.0f, 1.30f);
    mainReticlePulseRate_ = ClampFinite(mainReticlePulseRate_, 0.0f, 10.0f, 1.2f);
    mainReticleAppearance_.alpha = ClampFinite(mainReticleAppearance_.alpha, 0.0f, 1.0f, 0.68f);
    mainReticleAppearance_.coreIntensity = ClampFinite(mainReticleAppearance_.coreIntensity, 0.0f, 2.0f, 1.00f);
    mainReticleAppearance_.glowIntensity = ClampFinite(mainReticleAppearance_.glowIntensity, 0.0f, 2.0f, 0.35f);
    mainReticleAppearance_.glowAlpha = ClampFinite(mainReticleAppearance_.glowAlpha, 0.0f, 1.0f, 0.20f);
    mainReticleAppearance_.glowRadiusTexels = ClampFinite(
        mainReticleAppearance_.glowRadiusTexels, 0.0f, 4.0f, 1.15f);
    mainReticleAppearance_.pulseAmount = ClampFinite(mainReticleAppearance_.pulseAmount, 0.0f, 0.25f, 0.015f);
}

void AimCorridorVisualController::ResetPresentationParameters() {
    displayMode_ = DisplayMode::MainReticleOnly;
    autoMainReticleSize_ = true;
    mainReticleViewportHeightRatio_ = 0.095f;
    manualMainReticleWorldHeight_ = 5.0f;
    mainReticleAppearance_ = { 0.68f, 1.00f, 0.35f, 0.20f, 1.15f, 0.015f };
    mainReticlePulseRate_ = 1.2f;
    debugDiamondPreview_ = false;
    debugDiamondPreviewScale_ = 1.30f;
    reticleVisualState_ = AimReticleVisualState::Normal;
    ClampPresentationParameters();
    ResetPresentationRuntimeState();
}

void AimCorridorVisualController::ResetPresentationRuntimeState() {
    mainReticleCenterWorld_ = {};
    mainReticleCenterScreenUv_ = {};
    mainReticleScreenRect_ = {};
    reticleTintAmount_ = 0.0f;
    targetingLockProgress_ = 0.0f;
    effectiveMainReticleWorldHeight_ = manualMainReticleWorldHeight_;
    effectiveMainReticleWorldWidth_ = 0.0f;
    estimatedMainReticlePixelWidth_ = 0.0f;
    estimatedMainReticlePixelHeight_ = 0.0f;
    mainReticleDepth_ = 0.0f;
    mainReticleSizeValid_ = false;
    mainReticleProjectionValid_ = false;
}
