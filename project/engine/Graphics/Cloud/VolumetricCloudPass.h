#pragma once

#include "Engine/math/Matrix4x4.h"

#include <d3d12.h>
#include <cstdint>
#include <wrl.h>

class Camera;
class CloudVolume;
class DirectXCommon;
class SrvManager;

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
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    ProjectedBounds BuildProjectedBounds(const Camera* camera, const CloudVolume* cloudVolume) const;
    void Render(const Camera* camera, const CloudVolume* cloudVolume, const ProjectedBounds& projectedBounds);
    void DrawImGui();

    void SetDebugViewMode(DebugViewMode mode) { debugViewMode_ = mode; }
    DebugViewMode GetDebugViewMode() const { return debugViewMode_; }
    void SetEnabled(bool isEnabled) { isEnabled_ = isEnabled; }
    bool IsEnabled() const { return isEnabled_; }
    void SetForceMode(ForceMode mode) { forceMode_ = mode; }
    ForceMode GetForceMode() const { return forceMode_; }
    void SetExternalFlowMultiplier(float multiplier);
    float GetExternalFlowMultiplier() const { return externalFlowMultiplier_; }

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
        Vector4 renderInfo;
        Vector4 cloudFlowDirectionSpeed;
        float cloudTime = 0.0f;
        uint32_t enableCloudFlow = 0;
        float padding3 = 0.0f;
        float padding4 = 0.0f;
    };

    struct CloudCompositeConstants {
        Vector2 cloudTextureSize;
        Vector2 outputTextureSize;
        uint32_t enableDepthAwareUpsample = 0;
        float depthThreshold = 0.005f;
        Vector2 padding{};
    };

private:
    void CreateRootSignature();
    void CreatePipelineState();
    void CreateCompositeRootSignature();
    void CreateCompositePipelineState();
    void CreateConstantBuffer();
    void CreateCompositeConstantBuffer();
    void UpdateConstantBuffer(const Camera* camera, const CloudVolume* cloudVolume, uint32_t renderWidth, uint32_t renderHeight);
    void UpdateCompositeConstantBuffer();
    void RenderDirect(const Camera* camera, const CloudVolume* cloudVolume, const ProjectedBounds& projectedBounds);
    void RenderLowResolution(const Camera* camera, const CloudVolume* cloudVolume, const ProjectedBounds& projectedBounds);
    void CompositeCloudBuffer();
    void EnsureCloudBuffer();
    void CreateCloudBuffer(uint32_t width, uint32_t height);

    static Vector3 Normalize(const Vector3& value);
    static Vector4 TransformPoint(const Vector3& value, const Matrix4x4& matrix);
    static D3D12_RECT MakeFullScreenScissor();
    static D3D12_RECT MakeScissor(uint32_t width, uint32_t height);
    static D3D12_RECT ScaleScissor(const D3D12_RECT& source, uint32_t width, uint32_t height);
    static D3D12_VIEWPORT MakeViewport(uint32_t width, uint32_t height);
    static float ComputeDistanceToAabb(const Vector3& point, const CloudVolume* cloudVolume);
    static void ComputeDistanceLodScales(float entryDistance, const CloudVolume* cloudVolume, float lodFactorScale, bool disableDistanceLod, float& viewStepScale, float& densityScale);

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> compositeRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> compositePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> compositeConstantBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> cloudColorResource_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> cloudRtvHeap_;
    D3D12_CPU_DESCRIPTOR_HANDLE cloudColorRTV_{};
    D3D12_CPU_DESCRIPTOR_HANDLE cloudColorSRVCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE cloudColorSRVGPU_{};
    uint32_t cloudColorSRVIndex_ = 0;
    uint32_t cloudBufferWidth_ = 0;
    uint32_t cloudBufferHeight_ = 0;
    CloudPassConstants* constantData_ = nullptr;
    CloudCompositeConstants* compositeConstantData_ = nullptr;
    DebugViewMode debugViewMode_ = DebugViewMode::Final;
    ForceMode forceMode_ = ForceMode::None;
    bool isEnabled_ = true;
    bool useLowResolutionCloud_ = true;
    bool enableCloudComposite_ = true;
    bool enableDepthAwareUpsample_ = false;
    bool showCloudBufferPreview_ = false;
    bool enableCloudFlow_ = true;
    bool useCameraForwardFlow_ = true;
    bool useBoostFlowMultiplier_ = true;
    bool recreateCloudBufferRequested_ = false;
    bool hasValidCloudBuffer_ = false;
    bool cloudColorSrvAllocated_ = false;
    Vector3 fixedCloudFlowDirection_{ 1.0f, 0.0f, 0.2f };
    float cloudResolutionScale_ = 0.5f;
    float depthThreshold_ = 0.005f;
    float viewStepScale_ = 1.0f;
    float lightStepScale_ = 1.0f;
    float cloudBaseFlowSpeed_ = 0.35f;
    float externalFlowMultiplier_ = 1.0f;
    float currentCloudFlowSpeed_ = 0.35f;
    int cloudRenderInterval_ = 1;
    uint32_t frameCounter_ = 0;
    uint32_t renderedFrameCount_ = 0;
    uint32_t skippedFrameCount_ = 0;
};
