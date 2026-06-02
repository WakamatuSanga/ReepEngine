#pragma once

#include "Engine/Graphics/Camera/Camera.h"
#include "CloudVolume.h"
#include "Engine/Core/DirectXCommon.h"

#include <array>
#include <cstdint>
#include <wrl.h>

class VolumetricCloudPass {
public:
    struct ProjectedBounds {
        bool isVisible = false;
        bool isPassSkipped = true;
        bool useFullScreenScissor = true;
        bool isFullScreenFallback = false;
        bool isCameraInsideCloud = false;
        bool isNearPlaneCrossing = false;
        float scissorAreaRatio = 1.0f;
        float currentViewStepScale = 1.0f;
        float currentLightStepScale = 1.0f;
        uint32_t estimatedViewSteps = 0;
        uint32_t estimatedLightSteps = 0;
        D3D12_RECT scissorRect{};
    };

    enum class DebugViewMode : uint32_t {
        Final = 0,
        AlphaOnly = 1,
        DensityOnly = 2,
        LightOnly = 3,
    };

    enum class ForceMode : uint32_t {
        None = 0,
        ForceSkip = 1,
        ForceFullscreen = 2,
        ForceScissor = 3,
        ForceMaxQuality = 4,
        ForceAggressiveLod = 5,
    };

public:
    void Initialize(DirectXCommon* dxCommon);
    ProjectedBounds BuildProjectedBounds(const Camera* camera, const CloudVolume* cloudVolume) const;
    void Render(const Camera* camera, const CloudVolume* cloudVolume, const ProjectedBounds& projectedBounds);

    void SetDebugViewMode(DebugViewMode mode) { debugViewMode_ = mode; }
    DebugViewMode GetDebugViewMode() const { return debugViewMode_; }
    void SetEnabled(bool isEnabled) { isEnabled_ = isEnabled; }
    bool IsEnabled() const { return isEnabled_; }
    void SetForceMode(ForceMode mode) { forceMode_ = mode; }
    ForceMode GetForceMode() const { return forceMode_; }

private:
    struct CloudPassConstants {
        Matrix4x4 inverseViewProjection;

        Vector3 cameraPosition;
        float padding0 = 0.0f;

        Vector3 volumeCenter;
        float density = 0.0f;

        Vector3 volumeHalfExtents;
        float absorption = 0.0f;

        Vector3 windOffset;
        float noiseScale = 0.0f;

        Vector3 sunDirection;
        float detailNoiseScale = 0.0f;

        Vector4 cloudColor;

        float lightAbsorption = 0.0f;
        float detailWeight = 0.0f;
        float edgeFade = 0.0f;
        float ambientLighting = 0.0f;

        float sunIntensity = 0.0f;
        uint32_t viewStepCount = 0;
        uint32_t lightStepCount = 0;
        uint32_t debugViewMode = 0;
        float lodFactorScale = 1.0f;
        uint32_t disableDistanceLod = 0;
        float padding1 = 0.0f;
        float padding2 = 0.0f;
    };

private:
    void CreateRootSignature();
    void CreatePipelineState();
    void CreateConstantBuffer();
    void UpdateConstantBuffer(const Camera* camera, const CloudVolume* cloudVolume);

    static Vector3 Normalize(const Vector3& value);
    static Vector4 TransformPoint(const Vector3& value, const Matrix4x4& matrix);
    static D3D12_RECT MakeFullScreenScissor();
    static float ComputeDistanceToAabb(const Vector3& point, const CloudVolume* cloudVolume);
    static void ComputeDistanceLodScales(float entryDistance, const CloudVolume* cloudVolume, float lodFactorScale, bool disableDistanceLod, float& viewStepScale, float& densityScale);

private:
    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
    CloudPassConstants* constantData_ = nullptr;
    DebugViewMode debugViewMode_ = DebugViewMode::Final;
    ForceMode forceMode_ = ForceMode::None;
    bool isEnabled_ = true;
};
