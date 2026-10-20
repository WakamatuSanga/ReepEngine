#pragma once

#include "VolumetricCloudSettings.h"
#include "CloudResolutionComparison.h"

#include <array>
#include <memory>
#include <cstddef>
#include <d3d12.h>
#include <cstdint>
#include <wrl.h>

class Camera;
class CloudVolume;
class DirectXCommon;
class SrvManager;
class RailShooterCameraRig;
class VolumetricCloudPipelineResources;

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

    using AppearanceMode = VolumetricCloudSettings::AppearanceMode;
    using DebugViewMode = VolumetricCloudSettings::DebugViewMode;
    using ForceMode = VolumetricCloudSettings::ForceMode;
    using CloudFlowDirectionMode = VolumetricCloudSettings::CloudFlowDirectionMode;

public:
    ~VolumetricCloudPass();
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    ProjectedBounds BuildProjectedBounds(const Camera* camera, const CloudVolume* cloudVolume) const;
    ProjectedBounds BuildTitleBackgroundBounds(const Camera* camera, const CloudVolume* cloudVolume) const;
    void ConfigureStillTitleBackground();
    void ConfigureTitleForegroundCopy(const Camera* camera, const CloudVolume* source, CloudVolume* copy);
    void Render(const Camera* camera, const CloudVolume* cloudVolume, const ProjectedBounds& projectedBounds);
    void DrawImGui();
    // The scene only asks whether diagnostic presentation owns this update.
    bool UpdateResolutionComparison();
    bool BeginResolutionComparison(bool trial = false, AppearanceMode appearance = AppearanceMode::Legacy);
    void EndResolutionComparison();
    bool IsResolutionComparisonActive() const { return comparison_.IsActive(); }
    void RecordGpuSkipped(bool disabled);
    void MarkGpuScene();

    void SetDebugViewMode(DebugViewMode mode) { settings_.debugViewMode_ = mode; }
    DebugViewMode GetDebugViewMode() const { return settings_.debugViewMode_; }
    void SetEnabled(bool isEnabled) { settings_.isEnabled_ = isEnabled; }
    bool IsEnabled() const { return settings_.isEnabled_; }
    void SetForceMode(ForceMode mode) { settings_.forceMode_ = mode; }
    ForceMode GetForceMode() const { return settings_.forceMode_; }
    void SetExternalFlowMultiplier(float multiplier);
    void SetFlowReferenceRig(const RailShooterCameraRig* rig) { flowReferenceRig_ = rig; }
    void SetExternalBoostExtraFlowSpeed(float extraSpeed);
    void SetInfluenceFields(const Vector4* centersAndRadius, const Vector4* params, uint32_t count);
    void SetCloudInfluenceEnabled(bool enabled);
    void SetCameraForwardTunnelSettings(bool enabled, float length, float radius, float clearStrength);
    void ApplyGameModePerformancePreset();
    void ResetCloudDefaults();
    void SetDiagnosticDisableComposite(bool disabled) { diagnosticDisableCloudComposite_ = disabled; }
    void SetDiagnosticDisableDepthAwareUpsample(bool disabled) { diagnosticDisableDepthAwareUpsample_ = disabled; }
    float GetExternalFlowMultiplier() const { return externalFlowMultiplier_; }
    float GetExternalBoostExtraFlowSpeed() const { return externalBoostExtraFlowSpeed_; }
    float GetCloudBaseFlowSpeed() const;
    void SetGameplayFlowActive(bool active) { gameplayFlowActive_ = active; }
    float GetCurrentCloudFlowSpeed() const { return currentCloudFlowSpeed_; }
    float GetCloudFlowPhase() const { return cloudFlowPhase_; }
    float GetCloudResolutionScale() const { return settings_.cloudResolutionScale_; }
    bool IsLowResolutionCloudEnabled() const { return settings_.useLowResolutionCloud_; }
    bool IsCloudCompositeEnabled() const { return settings_.enableCloudComposite_ && !diagnosticDisableCloudComposite_; }
    bool IsDepthAwareUpsampleEnabled() const { return settings_.enableDepthAwareUpsample_ && !diagnosticDisableDepthAwareUpsample_; }

private:
    struct CloudPassConstants {
        Matrix4x4 inverseViewProjection;

        Vector3 cameraPosition;
        uint32_t volumeShapeMode = 0; // Reuses the original 4-byte padding at offset 76.

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
        float cloudRawTime = 0.0f;
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
        Vector4 accumulatedFlowOffset;
        Vector4 accumulatedLayerFlowOffsets; // far x/z, sea x/z
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
    // omission: 0=draw, 1=reuse/N/A, 2=disabled, 3=cull.
    uint32_t BeginGpuMeasurement(bool composite, int omission = 0, bool direct = false);
    void EndGpuMeasurement(uint32_t token);
    void RecordGpuReuse();
    void DrawGpuDiagnosticsImGui();
    void DrawComparisonControlsImGui();
    void CreateConstantBuffer();
    void CreateCompositeConstantBuffer();
    void UpdateConstantBuffer(const Camera* camera, const CloudVolume* cloudVolume, uint32_t renderWidth, uint32_t renderHeight);
    void UpdateQualityConstantBuffer();
    void ApplyUserPreferredCloudPreset();
    void ApplyQualityPreset(VolumetricCloudSettings::QualityPreset preset);
    static constexpr size_t AlignConstantBufferSize(size_t size) { return (size + 0xff) & ~size_t{0xff}; }
    void DrawQualityImGuiControls();
    void UpdateCompositeConstantBuffer();
    void RenderDirect(const Camera* camera, const CloudVolume* cloudVolume, const ProjectedBounds& projectedBounds);
    void RenderLowResolution(const Camera* camera, const CloudVolume* cloudVolume, const ProjectedBounds& projectedBounds);
    void CompositeCloudBuffer();
    void EnsureCloudBuffer();
    void CreateCloudBuffer(uint32_t width, uint32_t height);
    ResolvedCloudVolume ResolveCloudVolume(const Camera* camera, const CloudVolume* cloudVolume) const;
    Vector3 ResolveCloudFlowDirection(const Camera* camera) const;
    void UpdateCloudFlow(const Camera* camera, const CloudVolume* cloudVolume);

    static Vector3 Normalize(const Vector3& value);
    static Vector4 TransformPoint(const Vector3& value, const Matrix4x4& matrix);
    static Vector3 GetCameraForward(const Camera* camera);
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

    std::shared_ptr<VolumetricCloudPipelineResources> pipelineResources_;
    VolumetricCloudSettings settings_;
    enum class PassKind { Game, TitleBackground, TitleForeground };
    PassKind passKind_ = PassKind::Game;
    uint64_t diagnosticInvocation_ = 0;
    bool diagnosticReallocated_ = false;
    bool comparisonSavedProfilerEnabled_ = false;
    CloudResolutionComparison comparison_;
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
    bool diagnosticDisableCloudComposite_ = false;
    bool diagnosticDisableDepthAwareUpsample_ = false;
    bool recreateCloudBufferRequested_ = false;
    bool hasValidCloudBuffer_ = false;
    bool cloudColorSrvAllocated_ = false;
    Vector3 currentCloudFlowDirection_{ 0.0f, 0.0f, -1.0f };
    bool gameplayFlowActive_ = false;
    float externalFlowMultiplier_ = 1.0f;
    float externalBoostExtraFlowSpeed_ = 0.0f;
    float currentCloudFlowSpeed_ = 0.35f;
    float cloudFlowPhase_ = 0.0f;
    const RailShooterCameraRig* flowReferenceRig_ = nullptr;
    Vector3 accumulatedFlowOffset_{};
    Vector4 accumulatedLayerFlowOffsets_{};
    float previousCloudElapsedTime_ = 0.0f;
    float previousCloudFlowPhase_ = 0.0f;
    bool hasPreviousCloudElapsedTime_ = false;
    bool cloudFlowPhaseIncreasing_ = true;
    bool cloudFlowPhaseDecreasedWarning_ = false;
    std::array<Vector4, 16> influenceCentersAndRadius_{};
    std::array<Vector4, 16> influenceParams_{};
    uint32_t influenceFieldCount_ = 0;
    uint32_t frameCounter_ = 0;
    uint32_t renderedFrameCount_ = 0;
    uint32_t skippedFrameCount_ = 0;
};
