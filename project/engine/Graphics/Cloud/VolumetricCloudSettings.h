#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstdint>

// User-editable cloud rendering configuration. No device, scene, or frame state.
struct VolumetricCloudSettings {
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

    enum class QualityPreset { High, Balanced, Light, UltraLight, Direct };
    void ApplyStartupDefaults();
    bool ResetDefaults();
    bool ApplyUserPreferredPreset();
    bool ApplyQualityPreset(QualityPreset preset);
    bool ApplyUnderCloudPreset();
    bool ApplyCloudSeaPreset();
    bool ApplyFlyThroughPreset();
    void ConfigureStillTitleBackground();
    void ConfigureTitleForegroundCopy();
    bool operator==(const VolumetricCloudSettings& rhs) const;

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
    Vector3 fixedCloudFlowDirection_{ 1.0f, 0.0f, 0.2f };
    float cloudResolutionScale_ = 0.5f;
    float depthThreshold_ = 0.005f;
    float cloudOverGameplayObjectStrength_ = 0.2f;
    float foregroundCloudAlphaReduction_ = 0.8f;
    int cloudCompositeDebugMode_ = 0;
    float viewStepScale_ = 1.0f;
    float lightStepScale_ = 1.0f;
    float cloudBaseFlowSpeed_ = 10.0f;
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
    bool enableCloudInfluenceClear_ = true;
    bool enableCameraForwardTunnel_ = true;
    float cameraForwardTunnelLength_ = 18.0f;
    float cameraForwardTunnelRadius_ = 3.0f;
    float cameraForwardTunnelClearStrength_ = 0.45f;
    int cloudRenderInterval_ = 1;
private:
    void SetSampling(float resolution, bool depthAware, int interval, float view, float light);
    void ApplyDefaultSampling(float resolution);
    void ApplyDefaultVolume(float farDistance, float width, float depth);
    void ApplyPreferredValues(bool startup);
};
