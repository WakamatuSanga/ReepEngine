#include "VolumetricCloudSettings.h"
#include <algorithm>
#include <tuple>

void VolumetricCloudSettings::SetSampling(float resolution, bool depthAware, int interval, float view, float light) {
    appearanceMode_ = AppearanceMode::Legacy;
    cloudResolutionScale_ = resolution;
    enableDepthAwareUpsample_ = depthAware;
    cloudRenderInterval_ = interval;
    viewStepScale_ = view;
    lightStepScale_ = light;
}
void VolumetricCloudSettings::ApplyDefaultSampling(float resolution) {
    SetSampling(resolution, true, 1, 1.50f, 0.25f);
    depthThreshold_ = 0.0500f;
    debugViewMode_ = DebugViewMode::Final;
}
bool VolumetricCloudSettings::ApplyTunedCandidate() {
    const auto before = *this;
    appearanceMode_ = AppearanceMode::Legacy;
    // Sampling-only candidate. Shape, density, lighting and layer settings are
    // intentionally inherited from the settings at trial entry.
    useLowResolutionCloud_ = true;
    cloudResolutionScale_ = 0.25f;
    cloudRenderInterval_ = 1;
    viewStepScale_ = 0.75f;
    lightStepScale_ = 0.50f;
    return !(*this == before);
}
bool VolumetricCloudSettings::ApplyAppearanceCandidate(AppearanceMode mode) {
    const auto before = *this;
    ApplyTunedCandidate();
    appearanceMode_ = mode;
    if (mode != AppearanceMode::Legacy) volumeEdgeFadeDistance_ = 4.5f;
    return !(*this == before);
}
void VolumetricCloudSettings::ApplyDefaultVolume(float farDistance, float width, float depth) {
    useCameraRelativeCloudVolume_ = true;
    cloudNearDistance_ = -5.0f;
    cloudFarDistance_ = farDistance;
    cloudBehindCameraDistance_ = 5.0f;
    cloudHeightOffset_ = -50.3f;
    cloudVolumeWidth_ = width;
    cloudVolumeHeight_ = 1.0f;
    cloudVolumeDepth_ = depth;
    keepCameraBelowClouds_ = true;
    cameraToCloudBottom_ = 0.0f;
}
void VolumetricCloudSettings::ApplyStartupDefaults() {
    // Initialization, like the former preferred + partial reset sequence, leaves
    // unmentioned options alone when the same pass is initialized again.
    isEnabled_ = true;
    forceMode_ = ForceMode::None;
    ApplyPreferredValues(true);
}
bool VolumetricCloudSettings::ResetDefaults() {
    const auto before = *this;
    isEnabled_ = true;
    forceMode_ = ForceMode::None;
    ApplyDefaultSampling(0.125f);
    ApplyDefaultVolume(188.0f, 150.0f, 1.0f);
    return !(*this == before);
}
void VolumetricCloudSettings::ApplyPreferredValues(bool startup) {
    ApplyDefaultSampling(startup ? 0.125f : 1.0f);
    ApplyDefaultVolume(startup ? 188.0f : 250.0f, startup ? 150.0f : 226.0f, startup ? 1.0f : 262.0f);

    useLowResolutionCloud_ = true;
    enableCloudComposite_ = true;
    enableGameplayObjectPreserve_ = true;
    preservePlayerFromLowResCloud_ = true;
    preserveEnemyFromLowResCloud_ = true;
    preserveBulletFromLowResCloud_ = true;
    enableCloudDepthTest_ = true;
    enableGameplayObjectMask_ = false;
    cloudOverGameplayObjectStrength_ = 0.2f;
    foregroundCloudAlphaReduction_ = 0.8f;
    cloudCompositeDebugMode_ = 0;
    enableCloudFlow_ = true;
    cloudFlowDirectionMode_ = CloudFlowDirectionMode::TowardCamera;
    invertCloudFlowDirection_ = false;
    cloudBaseFlowSpeed_ = 20.0f / 3.0f;
    farCloudFlowSpeed_ = 0.35f / 3.0f;
    useBoostFlowMultiplier_ = true;
    cloudLayerThickness_ = 36.0f;
    cloudBottomFade_ = 0.1f;
    cloudTopFade_ = 0.1f;
    enableNearCameraCloudFade_ = true;
    nearFadeStart_ = 0.0f;
    nearFadeEnd_ = 8.4f;
    nearDensityScale_ = 0.34f;
    enableCloudBottomShaping_ = true;
    cloudBottomFlattenStrength_ = 0.0f;
    cloudBottomSmoothness_ = 0.30f;
    cloudBottomNoiseSuppression_ = 0.40f;
    cloudBottomDensity_ = 0.64f;
    cloudBottomUndulationStrength_ = 1.0f;
    cloudBottomUndulationScale_ = 0.052f;
    cloudBoundarySoftness_ = 0.86f;
    cloudDetailNoiseNearBottom_ = 1.0f;
    enableVolumeEdgeFade_ = true;
    volumeEdgeFadeDistance_ = 10.6f;
    enableCloudSeaLayer_ = true;
    cloudSeaUseCameraRelative_ = true;
    cloudSeaDistance_ = 180.0f;
    cloudSeaHeight_ = 22.0f;
    cloudSeaWidth_ = 360.0f;
    cloudSeaDepth_ = 320.0f;
    cloudSeaAlpha_ = 0.42f;
    cloudSeaFlowSpeed_ = 8.9f / 3.0f;
    cloudSeaNoiseScale_ = 0.045f;
    cloudSeaSoftness_ = 0.38f;
    cloudSeaColor_ = { 230.0f / 255.0f, 242.0f / 255.0f, 1.0f, 1.0f };
}
bool VolumetricCloudSettings::ApplyUserPreferredPreset() {
    const auto before = *this;
    ApplyPreferredValues(false);
    return !(*this == before);
}
bool VolumetricCloudSettings::ApplyQualityPreset(QualityPreset preset) {
    const auto before = std::tuple{appearanceMode_, useLowResolutionCloud_, enableCloudComposite_, cloudResolutionScale_, enableDepthAwareUpsample_, cloudRenderInterval_, viewStepScale_, lightStepScale_};
    appearanceMode_ = AppearanceMode::Legacy;
    if (preset == QualityPreset::Direct) {
        useLowResolutionCloud_ = false;
        cloudRenderInterval_ = 1;
        viewStepScale_ = lightStepScale_ = 1.0f;
    } else {
        useLowResolutionCloud_ = true;
        enableCloudComposite_ = true;
        switch (preset) {
        case QualityPreset::High: SetSampling(0.5f, true, 1, 1.0f, 1.0f); break;
        case QualityPreset::Balanced: SetSampling(0.5f, true, 1, 0.75f, 0.75f); break;
        case QualityPreset::Light: SetSampling(0.25f, true, 1, 0.5f, 0.5f); break;
        case QualityPreset::UltraLight: SetSampling(0.125f, false, 2, 0.35f, 0.35f); break;
        default: break;
        }
    }
    return before != std::tuple{appearanceMode_, useLowResolutionCloud_, enableCloudComposite_, cloudResolutionScale_, enableDepthAwareUpsample_, cloudRenderInterval_, viewStepScale_, lightStepScale_};
}
bool VolumetricCloudSettings::ApplyUnderCloudPreset() {
    const auto before = *this;
    appearanceMode_ = AppearanceMode::Legacy;
    cloudFlowDirectionMode_ = CloudFlowDirectionMode::TowardCamera;
    invertCloudFlowDirection_ = false;
    useCameraRelativeCloudVolume_ = true;
    keepCameraBelowClouds_ = true;
    cloudBaseFlowSpeed_ = 10.0f;
    cloudNearDistance_ = -5.0f;
    cloudBehindCameraDistance_ = 5.0f;
    cloudFarDistance_ = 200.0f;
    cloudVolumeWidth_ = 200.0f;
    cloudVolumeHeight_ = 90.0f;
    cloudVolumeDepth_ = 205.0f;
    cloudHeightOffset_ = 0.0f;
    cameraToCloudBottom_ = 16.0f;
    cloudLayerThickness_ = 90.0f;
    cloudBottomFade_ = 40.0f;
    cloudTopFade_ = 20.0f;
    enableNearCameraCloudFade_ = true;
    nearFadeStart_ = 0.0f;
    nearFadeEnd_ = 20.0f;
    nearDensityScale_ = 0.3f;
    enableCloudBottomShaping_ = true;
    cloudBottomFlattenStrength_ = 0.15f;
    cloudBottomSmoothness_ = 0.5f;
    cloudBottomNoiseSuppression_ = 0.15f;
    cloudBottomDensity_ = 0.8f;
    cloudBottomUndulationStrength_ = 8.0f;
    cloudBottomUndulationScale_ = 0.02f;
    cloudBoundarySoftness_ = 0.5f;
    cloudDetailNoiseNearBottom_ = 0.4f;
    enableVolumeEdgeFade_ = true;
    volumeEdgeFadeDistance_ = 20.0f;
    enableFarCloudLayer_ = true;
    farCloudDistance_ = 250.0f;
    farCloudHeight_ = 40.0f;
    farCloudScale_ = 0.012f;
    farCloudAlpha_ = 0.45f;
    farCloudFlowSpeed_ = 0.35f;
    enableCloudSeaLayer_ = true; cloudSeaDistance_ = 180.0f; cloudSeaHeight_ = 25.0f; cloudSeaAlpha_ = 0.35f;
    cloudSeaFlowSpeed_ = 10.0f; cloudSeaNoiseScale_ = 0.02f; cloudSeaSoftness_ = 0.5f;
    return !(*this == before);
}
bool VolumetricCloudSettings::ApplyCloudSeaPreset() {
    const auto before = *this;
    ApplyUnderCloudPreset();
    enableFarCloudLayer_ = true;
    farCloudAlpha_ = 0.55f;
    farCloudDistance_ = 300.0f;
    enableCloudSeaLayer_ = true; cloudSeaAlpha_ = 0.55f; cloudSeaDistance_ = 220.0f; cloudSeaDepth_ = 320.0f;
    cloudFarDistance_ = 250.0f;
    useLowResolutionCloud_ = true;
    enableDepthAwareUpsample_ = true;
    cloudResolutionScale_ = 0.25f;
    return !(*this == before);
}
bool VolumetricCloudSettings::ApplyFlyThroughPreset() {
    const auto before = *this;
    appearanceMode_ = AppearanceMode::Legacy;
    enableCloudSeaLayer_ = true;
    cloudFlowDirectionMode_ = CloudFlowDirectionMode::TowardCamera;
    invertCloudFlowDirection_ = false;
    useCameraRelativeCloudVolume_ = true;
    keepCameraBelowClouds_ = false;
    cloudNearDistance_ = -10.0f;
    cloudBehindCameraDistance_ = 10.0f;
    cloudFarDistance_ = 150.0f;
    cloudVolumeWidth_ = 180.0f;
    cloudVolumeHeight_ = 90.0f;
    cloudVolumeDepth_ = 170.0f;
    cloudHeightOffset_ = 8.0f;
    cameraToCloudBottom_ = 0.0f;
    cloudLayerThickness_ = 90.0f;
    cloudBottomFade_ = 20.0f;
    cloudTopFade_ = 20.0f;
    cloudBaseFlowSpeed_ = (std::max)(cloudBaseFlowSpeed_, 10.0f);
    enableNearCameraCloudFade_ = true;
    nearFadeStart_ = 0.0f;
    nearFadeEnd_ = 12.0f;
    nearDensityScale_ = 0.6f;
    enableCloudBottomShaping_ = true;
    cloudBottomFlattenStrength_ = 0.2f;
    cloudBottomSmoothness_ = 0.25f;
    cloudBottomNoiseSuppression_ = 0.15f;
    cloudBottomDensity_ = 0.95f;
    enableFarCloudLayer_ = true;
    farCloudAlpha_ = 0.35f;
    return !(*this == before);
}
void VolumetricCloudSettings::ConfigureStillTitleBackground() {
    appearanceMode_ = AppearanceMode::Legacy;
    // Only called on the title's own pass. Keep gameplay presets untouched.
    cloudResolutionScale_ = 0.5f;
    enableCloudFlow_ = false;
    cloudBaseFlowSpeed_ = 0.0f;
    useBoostFlowMultiplier_ = false;
    farCloudFlowSpeed_ = 0.0f;
    cloudSeaFlowSpeed_ = 0.0f;
    // Static world noise, rather than noise coordinates locked to the turning camera.
    cloudSeaUseCameraRelative_ = false;
    // Restore the background bank from before the foreground fill adjustment.
    cloudHeightOffset_ = -47.3f;
    enableCameraForwardTunnel_ = true;
    cameraForwardTunnelLength_ = 188.0f;
    cameraForwardTunnelRadius_ = 40.0f;
    cameraForwardTunnelClearStrength_ = 1.0f;
}
void VolumetricCloudSettings::ConfigureTitleForegroundCopy() {
    appearanceMode_ = AppearanceMode::Legacy;
    volumeEdgeFadeDistance_ = 1.0f;
    enableCameraForwardTunnel_ = false;
    useCameraRelativeCloudVolume_ = false;
    enableFarCloudLayer_ = false;
    enableCloudSeaLayer_ = false;
    enableGameplayObjectPreserve_ = false;
    enableCloudInfluenceClear_ = false;
}
bool VolumetricCloudSettings::operator==(const VolumetricCloudSettings& rhs) const {
    return
        debugViewMode_ == rhs.debugViewMode_ &&
        forceMode_ == rhs.forceMode_ &&
        isEnabled_ == rhs.isEnabled_ &&
        useLowResolutionCloud_ == rhs.useLowResolutionCloud_ &&
        enableCloudComposite_ == rhs.enableCloudComposite_ &&
        enableDepthAwareUpsample_ == rhs.enableDepthAwareUpsample_ &&
        enableGameplayObjectPreserve_ == rhs.enableGameplayObjectPreserve_ &&
        preservePlayerFromLowResCloud_ == rhs.preservePlayerFromLowResCloud_ &&
        preserveEnemyFromLowResCloud_ == rhs.preserveEnemyFromLowResCloud_ &&
        preserveBulletFromLowResCloud_ == rhs.preserveBulletFromLowResCloud_ &&
        enableCloudDepthTest_ == rhs.enableCloudDepthTest_ &&
        enableGameplayObjectMask_ == rhs.enableGameplayObjectMask_ &&
        showCloudBufferPreview_ == rhs.showCloudBufferPreview_ &&
        enableCloudFlow_ == rhs.enableCloudFlow_ &&
        cloudFlowDirectionMode_ == rhs.cloudFlowDirectionMode_ &&
        invertCloudFlowDirection_ == rhs.invertCloudFlowDirection_ &&
        useBoostFlowMultiplier_ == rhs.useBoostFlowMultiplier_ &&
        useCameraRelativeCloudVolume_ == rhs.useCameraRelativeCloudVolume_ &&
        enableNearCameraCloudFade_ == rhs.enableNearCameraCloudFade_ &&
        keepCameraBelowClouds_ == rhs.keepCameraBelowClouds_ &&
        enableCloudBottomShaping_ == rhs.enableCloudBottomShaping_ &&
        enableVolumeEdgeFade_ == rhs.enableVolumeEdgeFade_ &&
        enableFarCloudLayer_ == rhs.enableFarCloudLayer_ &&
        farCloudUseProceduralNoise_ == rhs.farCloudUseProceduralNoise_ &&
        enableCloudSeaLayer_ == rhs.enableCloudSeaLayer_ &&
        cloudSeaUseCameraRelative_ == rhs.cloudSeaUseCameraRelative_ &&
        fixedCloudFlowDirection_.x == rhs.fixedCloudFlowDirection_.x &&
        fixedCloudFlowDirection_.y == rhs.fixedCloudFlowDirection_.y &&
        fixedCloudFlowDirection_.z == rhs.fixedCloudFlowDirection_.z &&
        cloudResolutionScale_ == rhs.cloudResolutionScale_ &&
        depthThreshold_ == rhs.depthThreshold_ &&
        cloudOverGameplayObjectStrength_ == rhs.cloudOverGameplayObjectStrength_ &&
        foregroundCloudAlphaReduction_ == rhs.foregroundCloudAlphaReduction_ &&
        cloudCompositeDebugMode_ == rhs.cloudCompositeDebugMode_ &&
        viewStepScale_ == rhs.viewStepScale_ &&
        lightStepScale_ == rhs.lightStepScale_ &&
        cloudBaseFlowSpeed_ == rhs.cloudBaseFlowSpeed_ &&
        cloudNearDistance_ == rhs.cloudNearDistance_ &&
        cloudFarDistance_ == rhs.cloudFarDistance_ &&
        cloudBehindCameraDistance_ == rhs.cloudBehindCameraDistance_ &&
        cloudHeightOffset_ == rhs.cloudHeightOffset_ &&
        cloudVolumeWidth_ == rhs.cloudVolumeWidth_ &&
        cloudVolumeHeight_ == rhs.cloudVolumeHeight_ &&
        cloudVolumeDepth_ == rhs.cloudVolumeDepth_ &&
        cameraToCloudBottom_ == rhs.cameraToCloudBottom_ &&
        cloudLayerThickness_ == rhs.cloudLayerThickness_ &&
        cloudBottomFade_ == rhs.cloudBottomFade_ &&
        cloudTopFade_ == rhs.cloudTopFade_ &&
        nearFadeStart_ == rhs.nearFadeStart_ &&
        nearFadeEnd_ == rhs.nearFadeEnd_ &&
        nearDensityScale_ == rhs.nearDensityScale_ &&
        cloudBottomFlattenStrength_ == rhs.cloudBottomFlattenStrength_ &&
        cloudBottomSmoothness_ == rhs.cloudBottomSmoothness_ &&
        cloudBottomNoiseSuppression_ == rhs.cloudBottomNoiseSuppression_ &&
        cloudBottomDensity_ == rhs.cloudBottomDensity_ &&
        cloudBottomUndulationStrength_ == rhs.cloudBottomUndulationStrength_ &&
        cloudBottomUndulationScale_ == rhs.cloudBottomUndulationScale_ &&
        cloudBoundarySoftness_ == rhs.cloudBoundarySoftness_ &&
        cloudDetailNoiseNearBottom_ == rhs.cloudDetailNoiseNearBottom_ &&
        volumeEdgeFadeDistance_ == rhs.volumeEdgeFadeDistance_ &&
        farCloudDistance_ == rhs.farCloudDistance_ &&
        farCloudHeight_ == rhs.farCloudHeight_ &&
        farCloudScale_ == rhs.farCloudScale_ &&
        farCloudAlpha_ == rhs.farCloudAlpha_ &&
        farCloudFlowSpeed_ == rhs.farCloudFlowSpeed_ &&
        cloudSeaDistance_ == rhs.cloudSeaDistance_ &&
        cloudSeaHeight_ == rhs.cloudSeaHeight_ &&
        cloudSeaWidth_ == rhs.cloudSeaWidth_ &&
        cloudSeaDepth_ == rhs.cloudSeaDepth_ &&
        cloudSeaAlpha_ == rhs.cloudSeaAlpha_ &&
        cloudSeaFlowSpeed_ == rhs.cloudSeaFlowSpeed_ &&
        cloudSeaNoiseScale_ == rhs.cloudSeaNoiseScale_ &&
        cloudSeaSoftness_ == rhs.cloudSeaSoftness_ &&
        cloudSeaColor_.x == rhs.cloudSeaColor_.x &&
        cloudSeaColor_.y == rhs.cloudSeaColor_.y &&
        cloudSeaColor_.z == rhs.cloudSeaColor_.z &&
        cloudSeaColor_.w == rhs.cloudSeaColor_.w &&
        enableCloudInfluenceClear_ == rhs.enableCloudInfluenceClear_ &&
        enableCameraForwardTunnel_ == rhs.enableCameraForwardTunnel_ &&
        cameraForwardTunnelLength_ == rhs.cameraForwardTunnelLength_ &&
        cameraForwardTunnelRadius_ == rhs.cameraForwardTunnelRadius_ &&
        cameraForwardTunnelClearStrength_ == rhs.cameraForwardTunnelClearStrength_ &&
        cloudRenderInterval_ == rhs.cloudRenderInterval_ &&
        appearanceMode_ == rhs.appearanceMode_;
}
