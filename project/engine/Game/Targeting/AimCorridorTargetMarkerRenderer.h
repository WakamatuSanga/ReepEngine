#pragma once

#include "Engine/math/Matrix4x4.h"

#include <cstdint>
#include <memory>

class AimCorridorVisualRenderer;
class Camera;
class DirectXCommon;

class AimCorridorTargetMarkerRenderer {
public:
    AimCorridorTargetMarkerRenderer();
    ~AimCorridorTargetMarkerRenderer();

    bool Initialize(DirectXCommon* dxCommon, Camera* camera);
    void Finalize();
    void Reset();
    void Update(
        float unscaledDeltaTime,
        bool targetVisible,
        const Vector3& targetWorldPosition,
        const Vector2& targetScreenRadius,
        float cameraDepth);
    void Draw();
    void DrawImGuiSection();

    bool IsEnabled() const { return enabled_; }
    float GetCurrentPixelSize() const { return currentPixelSize_; }
    float GetCurrentAlpha() const { return currentAlpha_; }
    uint32_t GetLastDrawCount() const { return lastDrawCount_; }

private:
    DirectXCommon* dxCommon_ = nullptr;
    Camera* camera_ = nullptr;
    std::unique_ptr<AimCorridorVisualRenderer> renderer_;

    Vector3 centerWorld_{};
    Vector3 cameraRight_{ 1.0f, 0.0f, 0.0f };
    Vector3 cameraUp_{ 0.0f, 1.0f, 0.0f };
    float worldWidth_ = 0.0f;
    float worldHeight_ = 0.0f;
    float currentPixelSize_ = 0.0f;
    float currentAlpha_ = 0.0f;
    float pulsePhase_ = 0.0f;

    float markerScale_ = 1.30f;
    float minimumPixelSize_ = 80.0f;
    float maximumPixelSize_ = 220.0f;
    float fallbackPixelSize_ = 110.0f;
    float baseAlpha_ = 0.82f;
    float coreIntensity_ = 1.05f;
    float glowIntensity_ = 0.45f;
    float glowAlpha_ = 0.24f;
    float pulseRate_ = 2.0f;
    float pulseAmount_ = 0.04f;
    float fadeInTime_ = 0.10f;

    bool initialized_ = false;
    bool enabled_ = true;
    bool drawable_ = false;
    uint32_t lastDrawCount_ = 0;
};
