#include "AimCorridorTargetMarkerRenderer.h"

#include "Engine/Core/DirectXCommon.h"
#include "Engine/Game/UI/AimCorridorVisualRenderer.h"
#include "Engine/Graphics/Camera/Camera.h"

#include <algorithm>
#include <cmath>
#include <string>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kTwoPi = 6.28318530717958647692f;

    Vector3 NormalizeOr(const Vector3& value, const Vector3& fallback) {
        const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
        if (!std::isfinite(lengthSquared) || lengthSquared <= 0.000001f) {
            return fallback;
        }
        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        return { value.x * inverseLength, value.y * inverseLength, value.z * inverseLength };
    }

    float MoveToward(float current, float target, float maximumDelta) {
        if (current < target) {
            return (std::min)(current + maximumDelta, target);
        }
        return (std::max)(current - maximumDelta, target);
    }
}

AimCorridorTargetMarkerRenderer::AimCorridorTargetMarkerRenderer() = default;
AimCorridorTargetMarkerRenderer::~AimCorridorTargetMarkerRenderer() = default;

bool AimCorridorTargetMarkerRenderer::Initialize(DirectXCommon* dxCommon, Camera* camera) {
    Finalize();
    dxCommon_ = dxCommon;
    camera_ = camera;
    if (!dxCommon_ || !camera_) {
        return false;
    }
    renderer_ = std::make_unique<AimCorridorVisualRenderer>();
    const std::string markerTexture = "resources/ui/aim_corridor/Aiming_2.png";
    initialized_ = renderer_->Initialize(dxCommon_, markerTexture, markerTexture);
    Reset();
    return initialized_;
}

void AimCorridorTargetMarkerRenderer::Finalize() {
    if (renderer_) {
        renderer_->Finalize();
    }
    renderer_.reset();
    dxCommon_ = nullptr;
    camera_ = nullptr;
    initialized_ = false;
    Reset();
}

void AimCorridorTargetMarkerRenderer::Reset() {
    centerWorld_ = {};
    worldWidth_ = 0.0f;
    worldHeight_ = 0.0f;
    currentPixelSize_ = 0.0f;
    currentAlpha_ = 0.0f;
    pulsePhase_ = 0.0f;
    drawable_ = false;
    lastDrawCount_ = 0;
}

void AimCorridorTargetMarkerRenderer::Update(
    float unscaledDeltaTime,
    bool targetVisible,
    const Vector3& targetWorldPosition,
    const Vector2& targetScreenRadius,
    float cameraDepth) {
    lastDrawCount_ = 0;
    const float deltaTime = std::clamp(
        std::isfinite(unscaledDeltaTime) ? unscaledDeltaTime : 0.0f, 0.0f, 0.1f);
    if (!initialized_ || !enabled_ || !targetVisible || !dxCommon_ || !camera_
        || !std::isfinite(cameraDepth) || cameraDepth <= 0.0f) {
        currentAlpha_ = 0.0f;
        drawable_ = false;
        return;
    }

    markerScale_ = std::clamp(markerScale_, 1.0f, 2.5f);
    minimumPixelSize_ = std::clamp(minimumPixelSize_, 16.0f, 512.0f);
    maximumPixelSize_ = std::clamp(maximumPixelSize_, minimumPixelSize_, 512.0f);
    fallbackPixelSize_ = std::clamp(fallbackPixelSize_, minimumPixelSize_, maximumPixelSize_);
    baseAlpha_ = std::clamp(baseAlpha_, 0.0f, 1.0f);
    glowIntensity_ = std::clamp(glowIntensity_, 0.0f, 2.0f);
    pulseRate_ = std::clamp(pulseRate_, 0.0f, 10.0f);

    const D3D12_VIEWPORT viewport = dxCommon_->GetBackBufferViewport();
    if (!std::isfinite(viewport.Width) || !std::isfinite(viewport.Height)
        || viewport.Width <= 0.0f || viewport.Height <= 0.0f) {
        currentAlpha_ = 0.0f;
        drawable_ = false;
        return;
    }

    float boundsPixelSize = fallbackPixelSize_;
    if (std::isfinite(targetScreenRadius.x) && std::isfinite(targetScreenRadius.y)
        && targetScreenRadius.x > 0.0f && targetScreenRadius.y > 0.0f) {
        boundsPixelSize = (std::max)(
            targetScreenRadius.x * viewport.Width * 2.0f,
            targetScreenRadius.y * viewport.Height * 2.0f) * markerScale_;
    }
    currentPixelSize_ = std::clamp(boundsPixelSize, minimumPixelSize_, maximumPixelSize_);

    const float fovY = camera_->GetFovY();
    if (!std::isfinite(fovY) || fovY <= 0.0f || fovY >= 3.14159265f) {
        currentAlpha_ = 0.0f;
        drawable_ = false;
        return;
    }
    const float visibleWorldHeight = 2.0f * std::tan(fovY * 0.5f) * cameraDepth;
    worldHeight_ = visibleWorldHeight * (currentPixelSize_ / viewport.Height);
    const float markerAspect = renderer_ ? renderer_->GetNearAspectRatio() : 1.0f;
    worldWidth_ = worldHeight_ * ((std::isfinite(markerAspect) && markerAspect > 0.0f) ? markerAspect : 1.0f);
    const Matrix4x4& cameraWorld = camera_->GetWorldMatrix();
    cameraRight_ = NormalizeOr(
        { cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2] },
        { 1.0f, 0.0f, 0.0f });
    cameraUp_ = NormalizeOr(
        { cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2] },
        { 0.0f, 1.0f, 0.0f });
    centerWorld_ = targetWorldPosition;
    currentAlpha_ = MoveToward(currentAlpha_, baseAlpha_, deltaTime * baseAlpha_ / (std::max)(fadeInTime_, 0.001f));
    pulsePhase_ = std::fmod(pulsePhase_ + deltaTime * pulseRate_ * kTwoPi, kTwoPi);
    drawable_ = std::isfinite(worldWidth_) && std::isfinite(worldHeight_)
        && worldWidth_ > 0.001f && worldHeight_ > 0.001f;
}

void AimCorridorTargetMarkerRenderer::Draw() {
    lastDrawCount_ = 0;
    if (!drawable_ || !renderer_ || !camera_) {
        return;
    }
    AimCorridorVisualRenderer::FrameDraw unused{};
    AimCorridorVisualRenderer::FrameDraw marker{};
    marker.center = centerWorld_;
    marker.right = cameraRight_;
    marker.up = cameraUp_;
    marker.width = worldWidth_;
    marker.height = worldHeight_;
    marker.alpha = currentAlpha_;
    marker.coreIntensity = coreIntensity_;
    marker.glowIntensity = glowIntensity_;
    marker.glowAlpha = glowAlpha_;
    marker.glowRadiusTexels = 1.15f;
    marker.pulseScale = 1.0f + std::sin(pulsePhase_) * pulseAmount_;
    marker.coreTint = { 1.0f, 0.290196f, 0.164706f };
    marker.glowTint = { 1.0f, 0.40f, 0.12f };
    marker.tintAmount = 1.0f;
    lastDrawCount_ = renderer_->Draw(camera_, unused, marker, false, true, false, false);
}

void AimCorridorTargetMarkerRenderer::DrawImGuiSection() {
#ifdef USE_IMGUI
    ImGui::Checkbox("ひし形マーカーを有効化##MarkerEnabled", &enabled_);
    ImGui::DragFloat("マーカー透明度##MarkerAlpha", &baseAlpha_, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("マーカー倍率##MarkerScale", &markerScale_, 0.01f, 1.0f, 2.5f);
    ImGui::DragFloat("最小マーカーサイズ##MarkerMinSize", &minimumPixelSize_, 1.0f, 16.0f, 512.0f, "%.0f px");
    ImGui::DragFloat("最大マーカーサイズ##MarkerMaxSize", &maximumPixelSize_, 1.0f, 16.0f, 512.0f, "%.0f px");
    ImGui::DragFloat("既定マーカーサイズ##MarkerFallbackSize", &fallbackPixelSize_, 1.0f, 16.0f, 512.0f, "%.0f px");
    ImGui::DragFloat("マーカー発光強度##MarkerGlow", &glowIntensity_, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("マーカー脈動速度##MarkerPulseRate", &pulseRate_, 0.05f, 0.0f, 10.0f);
    ImGui::Text("マーカー現在サイズ: %.1f px", currentPixelSize_);
    ImGui::Text("マーカー現在透明度: %.3f", currentAlpha_);
    ImGui::Text("マーカー描画回数: %u", lastDrawCount_);
#endif
}
