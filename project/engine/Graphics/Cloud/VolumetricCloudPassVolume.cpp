#include "VolumetricCloudPass.h"

#include "CloudVolume.h"
#include "Engine/Graphics/Camera/Camera.h"

#include <algorithm>

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

    const Vector3 cameraPosition = camera->GetTranslate();
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
    const Vector3 cameraForward = GetCameraForward(camera);
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
