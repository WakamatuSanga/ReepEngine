#pragma once

#include "Engine/math/Matrix4x4.h"

#include <array>
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
        FarCloudOnly = 4,
        VolumetricOnly = 5,
        NoiseDebug = 6,
        CloudSeaOnly = 7,
    };

    enum class ForceMode : uint32_t {
        None = 0,
        ForceSkip = 1,
        ForceFullscreen = 2,
        ForceScissor = 3,
        ForceMaxQuality = 4,
        ForceAggressiveLod = 5,
    };

    enum class CloudFlowDirectionMode : uint32_t {
        Fixed = 0,
        TowardCamera = 1,
        AwayFromCamera = 2,
        CameraForward = 3,
        NegativeCameraForward = 4,
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
    void SetInfluenceFields(const Vector4* centersAndRadius, const Vector4* params, uint32_t count);
    void SetCloudInfluenceEnabled(bool enabled);
    void SetCameraForwardTunnelSettings(bool enabled, float length, float radius, float clearStrength);
    void ApplyGameModePerformancePreset();
    void SetDiagnosticDisableComposite(bool disabled) { diagnosticDisableCloudComposite_ = disabled; }
    void SetDiagnosticDisableDepthAwareUpsample(bool disabled) { diagnosticDisableDepthAwareUpsample_ = disabled; }
    float GetExternalFlowMultiplier() const { return externalFlowMultiplier_; }
    float GetCloudResolutionScale() const { return cloudResolutionScale_; }
    bool IsLowResolutionCloudEnabled() const { return useLowResolutionCloud_; }
    bool IsCloudCompositeEnabled() const { return enableCloudComposite_ && !diagnosticDisableCloudComposite_; }
    bool IsDepthAwareUpsampleEnabled() const { return enableDepthAwareUpsample_ && !diagnosticDisableDepthAwareUpsample_; }

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
        Vector4 nearCameraFade;
        Vector4 cloudLayerFade;
        Vector4 cloudBottomShaping;
        Vector4 cloudBottomShapingExtra;
        Vector4 cloudBottomUndulation;
        Vector4 volumeEdgeFade;
        Vector4 farCloudLayer;
        Vector4 farCloudLayerExtra;
        Vector4 farCloudColor;
        Vector4 cloudSeaLayer;
        Vector4 cloudSeaShape;
        Vector4 cloudSeaFlow;
        Vector4 cloudSeaColor;
        std::array<Vector4, 16> influenceCentersAndRadius{};
        std::array<Vector4, 16> influenceParams{};
        Vector4 influenceSettings;
        Vector4 cameraTunnelStartLength;
        Vector4 cameraTunnelDirectionRadius;
    };

    struct CloudCompositeConstants {
        Vector2 cloudTextureSize;
        Vector2 outputTextureSize;
        uint32_t enableDepthAwareUpsample = 0;
        uint32_t enableGameplayObjectPreserve = 1;
        uint32_t enableCloudDepthTest = 1;
        uint32_t enableGameplayObjectMask = 0;
        float depthThreshold = 0.005f;
        float cloudOverGameplayObjectStrength = 0.2f;
        float foregroundCloudAlphaReduction = 0.8f;
        uint32_t compositeDebugMode = 0;
    };

    struct ResolvedCloudVolume {
        Vector3 center{};
        Vector3 halfExtents{};
    };

private:
    void CreateRootSignature();
    void CreatePipelineState();
    void CreateCompositeRootSignature();
    void CreateCompositePipelineState();
    void CreateConstantBuffer();
    void CreateCompositeConstantBuffer();
    void UpdateConstantBuffer(const Camera* camera, const CloudVolume* cloudVolume, uint32_t renderWidth, uint32_t renderHeight);
    void UpdateQualityConstantBuffer();
    void ApplyUserPreferredCloudPreset();
    void DrawQualityImGuiControls();
    void UpdateCompositeConstantBuffer();
    void RenderDirect(const Camera* camera, const CloudVolume* cloudVolume, const ProjectedBounds& projectedBounds);
    void RenderLowResolution(const Camera* camera, const CloudVolume* cloudVolume, const ProjectedBounds& projectedBounds);
    void CompositeCloudBuffer();
    void EnsureCloudBuffer();
    void CreateCloudBuffer(uint32_t width, uint32_t height);
    ResolvedCloudVolume ResolveCloudVolume(const Camera* camera, const CloudVolume* cloudVolume) const;
    Vector3 ResolveCloudFlowDirection(const Camera* camera) const;

    static Vector3 Normalize(const Vector3& value);
    static Vector4 TransformPoint(const Vector3& value, const Matrix4x4& matrix);
    static Vector3 GetCameraForward(const Camera* camera);
    static Vector3 GetCameraUp(const Camera* camera);
    static bool ContainsPoint(const ResolvedCloudVolume& volume, const Vector3& point);
    static std::array<Vector3, 8> GetCorners(const ResolvedCloudVolume& volume);
    static D3D12_RECT MakeFullScreenScissor();
    static D3D12_RECT MakeScissor(uint32_t width, uint32_t height);
    static D3D12_RECT ScaleScissor(const D3D12_RECT& source, uint32_t width, uint32_t height);
    static D3D12_VIEWPORT MakeViewport(uint32_t width, uint32_t height);
    static float ComputeDistanceToAabb(const Vector3& point, const ResolvedCloudVolume& volume);
    static void ComputeDistanceLodScales(float entryDistance, const ResolvedCloudVolume& volume, float lodFactorScale, bool disableDistanceLod, float& viewStepScale, float& densityScale);

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
    bool enableGameplayObjectPreserve_ = true;
    bool preservePlayerFromLowResCloud_ = true;
    bool preserveEnemyFromLowResCloud_ = true;
    bool preserveBulletFromLowResCloud_ = true;
    bool enableCloudDepthTest_ = true;
    bool enableGameplayObjectMask_ = false;
    bool diagnosticDisableCloudComposite_ = false;
    bool diagnosticDisableDepthAwareUpsample_ = false;
    bool showCloudBufferPreview_ = false;
    bool enableCloudFlow_ = true;
    CloudFlowDirectionMode cloudFlowDirectionMode_ = CloudFlowDirectionMode::TowardCamera;
    bool invertCloudFlowDirection_ = false;
    bool useBoostFlowMultiplier_ = true;
    bool useCameraRelativeCloudVolume_ = true;
    bool enableNearCameraCloudFade_ = true;
    bool keepCameraBelowClouds_ = true;
    bool enableCloudBottomShaping_ = true;
    bool enableVolumeEdgeFade_ = true;
    bool enableFarCloudLayer_ = true;
    bool farCloudUseProceduralNoise_ = true;
    bool enableCloudSeaLayer_ = true;
    bool cloudSeaUseCameraRelative_ = true;
    bool recreateCloudBufferRequested_ = false;
    bool hasValidCloudBuffer_ = false;
    bool cloudColorSrvAllocated_ = false;
    Vector3 fixedCloudFlowDirection_{ 1.0f, 0.0f, 0.2f };
    Vector3 currentCloudFlowDirection_{ 0.0f, 0.0f, -1.0f };
    float cloudResolutionScale_ = 0.5f;
    float depthThreshold_ = 0.005f;
    float cloudOverGameplayObjectStrength_ = 0.2f;
    float foregroundCloudAlphaReduction_ = 0.8f;
    int cloudCompositeDebugMode_ = 0;
    float viewStepScale_ = 1.0f;
    float lightStepScale_ = 1.0f;
    float cloudBaseFlowSpeed_ = 10.0f;
    float externalFlowMultiplier_ = 1.0f;
    float currentCloudFlowSpeed_ = 0.35f;
    float cloudNearDistance_ = -5.0f;
    float cloudFarDistance_ = 200.0f;
    float cloudBehindCameraDistance_ = 5.0f;
    float cloudHeightOffset_ = 0.0f;
    float cloudVolumeWidth_ = 200.0f;
    float cloudVolumeHeight_ = 80.0f;
    float cloudVolumeDepth_ = 205.0f;
    float cameraToCloudBottom_ = 16.0f;
    float cloudLayerThickness_ = 90.0f;
    float cloudBottomFade_ = 40.0f;
    float cloudTopFade_ = 20.0f;
    float nearFadeStart_ = 0.0f;
    float nearFadeEnd_ = 20.0f;
    float nearDensityScale_ = 0.3f;
    float cloudBottomFlattenStrength_ = 0.15f;
    float cloudBottomSmoothness_ = 0.5f;
    float cloudBottomNoiseSuppression_ = 0.15f;
    float cloudBottomDensity_ = 0.8f;
    float cloudBottomUndulationStrength_ = 8.0f;
    float cloudBottomUndulationScale_ = 0.02f;
    float cloudBoundarySoftness_ = 0.5f;
    float cloudDetailNoiseNearBottom_ = 0.4f;
    float volumeEdgeFadeDistance_ = 20.0f;
    float farCloudDistance_ = 250.0f;
    float farCloudHeight_ = 40.0f;
    float farCloudScale_ = 0.012f;
    float farCloudAlpha_ = 0.45f;
    float farCloudFlowSpeed_ = 0.35f;
    float cloudSeaDistance_ = 180.0f;
    float cloudSeaHeight_ = 25.0f;
    float cloudSeaWidth_ = 360.0f;
    float cloudSeaDepth_ = 260.0f;
    float cloudSeaAlpha_ = 0.35f;
    float cloudSeaFlowSpeed_ = 10.0f;
    float cloudSeaNoiseScale_ = 0.02f;
    float cloudSeaSoftness_ = 0.5f;
    Vector4 cloudSeaColor_{ 0.90f, 0.95f, 1.0f, 1.0f };
    std::array<Vector4, 16> influenceCentersAndRadius_{};
    std::array<Vector4, 16> influenceParams_{};
    uint32_t influenceFieldCount_ = 0;
    bool enableCloudInfluenceClear_ = true;
    bool enableCameraForwardTunnel_ = true;
    float cameraForwardTunnelLength_ = 18.0f;
    float cameraForwardTunnelRadius_ = 3.0f;
    float cameraForwardTunnelClearStrength_ = 0.45f;
    char farCloudTexturePath_[128] = "procedural";
    int cloudRenderInterval_ = 1;
    uint32_t frameCounter_ = 0;
    uint32_t renderedFrameCount_ = 0;
    uint32_t skippedFrameCount_ = 0;
};
