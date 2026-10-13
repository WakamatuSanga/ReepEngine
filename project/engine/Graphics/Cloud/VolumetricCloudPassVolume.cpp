#include "VolumetricCloudPass.h"

#include "CloudVolume.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Game/Camera/RailShooterCameraRig.h"

#include <algorithm>
#include <cmath>

void VolumetricCloudPass::ConfigureStillTitleBackground() {
    // Only called on the title's own pass. Keep gameplay presets untouched.
    cloudResolutionScale_ = 0.5f;
    enableCloudFlow_ = false;
    cloudBaseFlowSpeed_ = 0.0f;
    currentCloudFlowSpeed_ = 0.0f;
    useBoostFlowMultiplier_ = false;
    externalBoostExtraFlowSpeed_ = 0.0f;
    cloudFlowPhase_ = previousCloudFlowPhase_ = 0.0f;
    accumulatedFlowOffset_ = {};
    accumulatedLayerFlowOffsets_ = {};
    farCloudFlowSpeed_ = 0.0f;
    cloudSeaFlowSpeed_ = 0.0f;
    // Static world noise, rather than noise coordinates locked to the turning camera.
    cloudSeaUseCameraRelative_ = false;
    // Restore the background bank from before the foreground fill adjustment.
    cloudHeightOffset_ = -47.3f;
    SetCameraForwardTunnelSettings(true, 188.0f, 40.0f, 1.0f);
}

void VolumetricCloudPass::ConfigureTitleForegroundCopy(
    const Camera* camera, const CloudVolume* source, CloudVolume* copy) {
    if (!source || !copy) return;
    ConfigureStillTitleBackground();
    const auto bounds = ResolveCloudVolume(camera, source);
    copy->GetParameters() = source->GetParameters();
    copy->GetParameters().center = bounds.center;
    copy->GetParameters().halfExtents = bounds.halfExtents;
    // Keep the source noise and physical dimensions. The lower-screen copy is
    // viewed at a grazing angle: the background's 10.6-unit boundary fade erased
    // nearly the entire visible slice. Use a shorter, still smooth edge here.
    copy->GetParameters().edgeFade = 0.02f;
    copy->GetParameters().density *= 2.0f;
    volumeEdgeFadeDistance_ = 1.0f;
    SetCameraForwardTunnelSettings(false, 188.0f, 40.0f, 1.0f);
    // Only the original instance draws the distant sky layers.
    useCameraRelativeCloudVolume_ = false;
    enableFarCloudLayer_ = false;
    enableCloudSeaLayer_ = false;
    enableGameplayObjectPreserve_ = false;
    SetCloudInfluenceEnabled(false);
}

VolumetricCloudPass::ProjectedBounds VolumetricCloudPass::BuildTitleBackgroundBounds(
    const Camera* camera, const CloudVolume* cloudVolume) const {
    ProjectedBounds result = BuildProjectedBounds(camera, cloudVolume);
    // The same shader also draws independent sky planes ABOVE the volume AABB.
    // Its box-only scissor/cull is not a valid bound for those background layers.
    if (camera && cloudVolume && (enableFarCloudLayer_ || enableCloudSeaLayer_)) {
        result.isVisible = true;
        result.isPassSkipped = !isEnabled_ || forceMode_ == ForceMode::ForceSkip;
        result.useFullScreenScissor = true;
        result.scissorRect = MakeFullScreenScissor();
        result.scissorAreaRatio = 1.0f;
        result.isFullScreenFallback = true;
    }
    return result;
}

VolumetricCloudPass::ResolvedCloudVolume VolumetricCloudPass::ResolveCloudVolume(
    const Camera* camera,
    const CloudVolume* cloudVolume) const
{
    ResolvedCloudVolume resolved{};
    if (!cloudVolume) {
        return resolved;
    }

    const CloudVolume::Parameters& parameters = cloudVolume->GetParameters();
    resolved.center = parameters.center;
    resolved.halfExtents = parameters.halfExtents;

    if (!useCameraRelativeCloudVolume_ || !camera) {
        return resolved;
    }

    const Vector3 cameraPosition = camera->GetViewTranslate();
    const Vector3 cameraForward = GetCameraForward(camera);
    const float behindDistance = (std::max)(0.0f, cloudBehindCameraDistance_);
    const float nearDistance = (std::min)(cloudNearDistance_, -behindDistance);
    const float farDistance = (std::max)(cloudFarDistance_, nearDistance + 1.0f);
    const float distanceDepth = farDistance - nearDistance;
    const float effectiveDepth = (std::max)((std::max)(cloudVolumeDepth_, distanceDepth), 1.0f);
    const float centerDistance = (farDistance + nearDistance) * 0.5f;

    resolved.center = {
        cameraPosition.x + cameraForward.x * centerDistance,
        cameraPosition.y + cameraForward.y * centerDistance,
        cameraPosition.z + cameraForward.z * centerDistance
    };
    resolved.halfExtents = {
        (std::max)(cloudVolumeWidth_ * 0.5f, 1.0f),
        (std::max)(cloudVolumeHeight_ * 0.5f, 1.0f),
        (std::max)(effectiveDepth * 0.5f, 1.0f)
    };

    if (keepCameraBelowClouds_) {
        const float layerThickness = (std::max)(cloudLayerThickness_, 1.0f);
        const float cloudBottomY = cameraPosition.y + (std::max)(cameraToCloudBottom_, 0.0f) + cloudHeightOffset_;
        const float cloudTopY = cloudBottomY + layerThickness;
        resolved.center.y = (cloudBottomY + cloudTopY) * 0.5f;
        resolved.halfExtents.y = layerThickness * 0.5f;
    } else {
        resolved.center.y = cameraPosition.y + cloudHeightOffset_;
        resolved.halfExtents.y = (std::max)(cloudVolumeHeight_ * 0.5f, 1.0f);
    }

    return resolved;
}

Vector3 VolumetricCloudPass::ResolveCloudFlowDirection(const Camera* camera) const
{
    // Flow follows progression, not the cosmetic follow/look/bank camera.
    // The render camera remains authoritative for volume placement and depth.
    const Vector3 cameraForward = flowReferenceRig_ && flowReferenceRig_->UsesGameplayComposition()
        ? flowReferenceRig_->GetBaseForward() : GetCameraForward(camera);
    Vector3 direction = fixedCloudFlowDirection_;

    switch (cloudFlowDirectionMode_) {
    case CloudFlowDirectionMode::Fixed:
        direction = fixedCloudFlowDirection_;
        break;
    case CloudFlowDirectionMode::TowardCamera:
    case CloudFlowDirectionMode::NegativeCameraForward:
        direction = { -cameraForward.x, -cameraForward.y, -cameraForward.z };
        break;
    case CloudFlowDirectionMode::AwayFromCamera:
    case CloudFlowDirectionMode::CameraForward:
        direction = cameraForward;
        break;
    default:
        direction = { -cameraForward.x, -cameraForward.y, -cameraForward.z };
        break;
    }

    if (invertCloudFlowDirection_) {
        direction = { -direction.x, -direction.y, -direction.z };
    }

    return Normalize(direction);
}

Vector3 VolumetricCloudPass::GetCameraForward(const Camera* camera)
{
    if (!camera) {
        return { 0.0f, 0.0f, 1.0f };
    }

    const Matrix4x4& matrix = camera->GetWorldMatrix();
    return Normalize({ matrix.m[2][0], matrix.m[2][1], matrix.m[2][2] });
}

Vector3 VolumetricCloudPass::GetCameraUp(const Camera* camera)
{
    if (!camera) {
        return { 0.0f, 1.0f, 0.0f };
    }

    const Matrix4x4& matrix = camera->GetWorldMatrix();
    return Normalize({ matrix.m[1][0], matrix.m[1][1], matrix.m[1][2] });
}

bool VolumetricCloudPass::ContainsPoint(const ResolvedCloudVolume& volume, const Vector3& point)
{
    const Vector3 min = {
        volume.center.x - volume.halfExtents.x,
        volume.center.y - volume.halfExtents.y,
        volume.center.z - volume.halfExtents.z
    };
    const Vector3 max = {
        volume.center.x + volume.halfExtents.x,
        volume.center.y + volume.halfExtents.y,
        volume.center.z + volume.halfExtents.z
    };

    return
        point.x >= min.x && point.x <= max.x &&
        point.y >= min.y && point.y <= max.y &&
        point.z >= min.z && point.z <= max.z;
}

std::array<Vector3, 8> VolumetricCloudPass::GetCorners(const ResolvedCloudVolume& volume)
{
    const Vector3 min = {
        volume.center.x - volume.halfExtents.x,
        volume.center.y - volume.halfExtents.y,
        volume.center.z - volume.halfExtents.z
    };
    const Vector3 max = {
        volume.center.x + volume.halfExtents.x,
        volume.center.y + volume.halfExtents.y,
        volume.center.z + volume.halfExtents.z
    };

    return {
        Vector3{ min.x, min.y, min.z },
        Vector3{ max.x, min.y, min.z },
        Vector3{ min.x, max.y, min.z },
        Vector3{ max.x, max.y, min.z },
        Vector3{ min.x, min.y, max.z },
        Vector3{ max.x, min.y, max.z },
        Vector3{ min.x, max.y, max.z },
        Vector3{ max.x, max.y, max.z }
    };
}

void VolumetricCloudPass::UpdateCloudFlow(const Camera* camera, const CloudVolume* cloudVolume)
{
    Vector3 flowDirection = ResolveCloudFlowDirection(camera);
    currentCloudFlowDirection_ = flowDirection;

    const float boostExtraSpeed = useBoostFlowMultiplier_ ? std::clamp(externalBoostExtraFlowSpeed_, 0.0f, 30.0f) : 0.0f;
    const float previousSpeed = currentCloudFlowSpeed_;
    currentCloudFlowSpeed_ = enableCloudFlow_ ? cloudBaseFlowSpeed_ + boostExtraSpeed : 0.0f;

    const float elapsedTime = cloudVolume->GetElapsedTime();
    float deltaTime = 0.0f;
    if (std::isfinite(elapsedTime)) {
        if (hasPreviousCloudElapsedTime_) {
            deltaTime = elapsedTime - previousCloudElapsedTime_;
            if (deltaTime < 0.0f) {
                accumulatedFlowOffset_ = {};
                accumulatedLayerFlowOffsets_ = {};
                cloudFlowPhase_ = 0.0f;
                previousCloudFlowPhase_ = 0.0f;
                cloudFlowPhaseIncreasing_ = true;
                cloudFlowPhaseDecreasedWarning_ = false;
                deltaTime = 0.0f;
            }
        }
        previousCloudElapsedTime_ = elapsedTime;
        hasPreviousCloudElapsedTime_ = true;
    }
    const float safeDeltaTime = std::clamp(std::isfinite(deltaTime) ? deltaTime : 0.0f, 0.0f, 1.0f / 30.0f);
    // Rebase the scalar speed segment, retaining the integrated noise position.
    // Neither a speed edit nor a turn may rotate/re-scale past travel.
    if (currentCloudFlowSpeed_ != previousSpeed) cloudFlowPhase_ = 0.0f;
    previousCloudFlowPhase_ = cloudFlowPhase_;
    cloudFlowPhase_ += currentCloudFlowSpeed_ * safeDeltaTime;
    if (!std::isfinite(cloudFlowPhase_)) {
        cloudFlowPhase_ = previousCloudFlowPhase_;
    }
    cloudFlowPhaseIncreasing_ = cloudFlowPhase_ + 0.0001f >= previousCloudFlowPhase_;
    if (!cloudFlowPhaseIncreasing_) {
        cloudFlowPhaseDecreasedWarning_ = true;
    }

    const float travel = currentCloudFlowSpeed_ * safeDeltaTime;
    accumulatedFlowOffset_.x += flowDirection.x * travel;
    accumulatedFlowOffset_.y += flowDirection.y * travel;
    accumulatedFlowOffset_.z += flowDirection.z * travel;
    const float horizontalLength = std::hypot(flowDirection.x, flowDirection.z);
    const Vector2 horizontal = horizontalLength > 0.0001f
        ? Vector2{ flowDirection.x / horizontalLength, flowDirection.z / horizontalLength }
        : Vector2{ 0.0f, -1.0f };
    accumulatedLayerFlowOffsets_.x += horizontal.x * farCloudFlowSpeed_ * safeDeltaTime;
    accumulatedLayerFlowOffsets_.y += horizontal.y * farCloudFlowSpeed_ * safeDeltaTime;
    accumulatedLayerFlowOffsets_.z += horizontal.x * cloudSeaFlowSpeed_ * safeDeltaTime;
    accumulatedLayerFlowOffsets_.w += horizontal.y * cloudSeaFlowSpeed_ * safeDeltaTime;
    constantData_->accumulatedFlowOffset = {
        accumulatedFlowOffset_.x, accumulatedFlowOffset_.y, accumulatedFlowOffset_.z, 0.0f };
    constantData_->accumulatedLayerFlowOffsets = accumulatedLayerFlowOffsets_;
    constantData_->cloudFlowDirectionSpeed = {
        flowDirection.x,
        flowDirection.y,
        flowDirection.z,
        currentCloudFlowSpeed_
    };
    constantData_->cloudTime = cloudFlowPhase_;
    constantData_->enableCloudFlow = enableCloudFlow_ ? 1u : 0u;
    constantData_->cloudRawTime = std::isfinite(elapsedTime) ? elapsedTime : 0.0f;
    constantData_->padding4 = 0.0f;
}
