#include "VolumetricCloudPass.h"
#include "Engine/Core/GpuTimestampProfiler.h"
#include "Engine/Core/RuntimeModeController.h"
#include "VolumetricCloudPipelineResources.h"

#include "CloudVolume.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Graphics/Camera/Camera.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cmath>

using namespace MatrixMath;

namespace {
    struct FrustumPlane {
        Vector4 coefficients;
    };

    float EvaluatePlane(const FrustumPlane& plane, const Vector3& point)
    {
        return
            plane.coefficients.x * point.x +
            plane.coefficients.y * point.y +
            plane.coefficients.z * point.z +
            plane.coefficients.w;
    }

    FrustumPlane NormalizePlane(const Vector4& coefficients)
    {
        const float lengthSquared =
            coefficients.x * coefficients.x +
            coefficients.y * coefficients.y +
            coefficients.z * coefficients.z;

        if (lengthSquared <= 0.000001f) {
            return { coefficients };
        }

        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        return {
            {
                coefficients.x * inverseLength,
                coefficients.y * inverseLength,
                coefficients.z * inverseLength,
                coefficients.w * inverseLength
            }
        };
    }
}

void VolumetricCloudPass::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    assert(dxCommon);
    assert(srvManager);
    EndResolutionComparison();
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    auto pipelines = VolumetricCloudPipelineResources::Acquire(*dxCommon_);
    if (pipelineResources_ != pipelines) {
        // Descriptor heaps/indices belong to their device. Same-device reinit
        // can reuse them; a different pipeline bundle identifies a new device.
        cloudColorResource_.Reset();
        cloudRtvHeap_.Reset();
        cloudColorSrvAllocated_ = false;
    }
    pipelineResources_ = std::move(pipelines);
    CreateConstantBuffer();
    CreateCompositeConstantBuffer();
    passKind_ = PassKind::Game;
    settings_.ApplyStartupDefaults();
    externalFlowMultiplier_ = 1.0f;
    currentCloudFlowSpeed_ = settings_.cloudBaseFlowSpeed_;
    // A new device/initialization is an explicit resource invalidation reason.
    hasValidCloudBuffer_ = false;
    recreateCloudBufferRequested_ = true;
}

VolumetricCloudPass::ProjectedBounds VolumetricCloudPass::BuildProjectedBounds(const Camera* camera, const CloudVolume* cloudVolume) const
{
    ProjectedBounds result{};
    result.scissorRect = MakeFullScreenScissor();
    result.scissorAreaRatio = 0.0f;

    if (!camera || !cloudVolume) {
        return result;
    }

    const CloudVolume::Parameters& parameters = cloudVolume->GetParameters();
    const ResolvedCloudVolume resolvedVolume = ResolveCloudVolume(camera, cloudVolume);
    const bool disableDistanceLod = (settings_.forceMode_ == ForceMode::ForceMaxQuality);
    const float lodFactorScale = (settings_.forceMode_ == ForceMode::ForceAggressiveLod) ? 1.75f : 1.0f;
    const float entryDistance = ComputeDistanceToAabb(camera->GetViewTranslate(), resolvedVolume);
    float densityScale = 1.0f;
    ComputeDistanceLodScales(entryDistance, resolvedVolume, lodFactorScale, disableDistanceLod, result.currentViewStepScale, densityScale);
    result.currentLightStepScale = result.currentViewStepScale;
    result.estimatedViewSteps = (std::max)(1u, static_cast<uint32_t>(static_cast<float>((std::max)(parameters.viewStepCount, 1u)) * result.currentViewStepScale + 0.5f));
    result.estimatedLightSteps = (std::max)(1u, static_cast<uint32_t>(static_cast<float>((std::max)(parameters.lightStepCount, 1u)) * result.currentLightStepScale + 0.5f));

    result.isCameraInsideCloud = ContainsPoint(resolvedVolume, camera->GetViewTranslate());
    if (result.isCameraInsideCloud) {
        result.isVisible = true;
        result.useFullScreenScissor = true;
        result.isFullScreenFallback = true;
        result.scissorAreaRatio = 1.0f;
        result.isPassSkipped = !settings_.isEnabled_ || settings_.forceMode_ == ForceMode::ForceSkip;
        return result;
    }

    const std::array<Vector3, 8> corners = GetCorners(resolvedVolume);
    const Matrix4x4& viewProjection = camera->GetViewProjectionMatrix();

    const FrustumPlane planes[6] = {
        // Row-vector convention uses the matrix columns for frustum plane extraction.
        NormalizePlane({ viewProjection.m[0][3] + viewProjection.m[0][0], viewProjection.m[1][3] + viewProjection.m[1][0], viewProjection.m[2][3] + viewProjection.m[2][0], viewProjection.m[3][3] + viewProjection.m[3][0] }),
        NormalizePlane({ viewProjection.m[0][3] - viewProjection.m[0][0], viewProjection.m[1][3] - viewProjection.m[1][0], viewProjection.m[2][3] - viewProjection.m[2][0], viewProjection.m[3][3] - viewProjection.m[3][0] }),
        NormalizePlane({ viewProjection.m[0][3] + viewProjection.m[0][1], viewProjection.m[1][3] + viewProjection.m[1][1], viewProjection.m[2][3] + viewProjection.m[2][1], viewProjection.m[3][3] + viewProjection.m[3][1] }),
        NormalizePlane({ viewProjection.m[0][3] - viewProjection.m[0][1], viewProjection.m[1][3] - viewProjection.m[1][1], viewProjection.m[2][3] - viewProjection.m[2][1], viewProjection.m[3][3] - viewProjection.m[3][1] }),
        NormalizePlane({ viewProjection.m[0][2], viewProjection.m[1][2], viewProjection.m[2][2], viewProjection.m[3][2] }),
        NormalizePlane({ viewProjection.m[0][3] - viewProjection.m[0][2], viewProjection.m[1][3] - viewProjection.m[1][2], viewProjection.m[2][3] - viewProjection.m[2][2], viewProjection.m[3][3] - viewProjection.m[3][2] }),
    };

    for (const FrustumPlane& plane : planes) {
        bool isOutside = true;
        for (const Vector3& corner : corners) {
            if (EvaluatePlane(plane, corner) >= 0.0f) {
                isOutside = false;
                break;
            }
        }

        if (isOutside) {
            return result;
        }
    }

    result.isVisible = true;

    std::array<Vector4, 8> clipCorners{};
    for (size_t i = 0; i < corners.size(); ++i) {
        clipCorners[i] = TransformPoint(corners[i], viewProjection);

        // TODO: If we later add edge clipping against the frustum, this fallback can become tighter.
        // For now, any vertex behind the near plane uses full-screen scissor to avoid underestimating the rect.
        if (clipCorners[i].w <= 0.0001f || clipCorners[i].z < 0.0f) {
            result.isNearPlaneCrossing = true;
        }
    }

    if (result.isNearPlaneCrossing) {
        result.useFullScreenScissor = true;
        result.isFullScreenFallback = true;
        result.scissorAreaRatio = 1.0f;
        result.isPassSkipped = !settings_.isEnabled_ || settings_.forceMode_ == ForceMode::ForceSkip;
        return result;
    }

    float minNdcX = 1.0f;
    float maxNdcX = -1.0f;
    float minNdcY = 1.0f;
    float maxNdcY = -1.0f;

    for (const Vector4& clipCorner : clipCorners) {
        const float inverseW = 1.0f / clipCorner.w;
        const float ndcX = clipCorner.x * inverseW;
        const float ndcY = clipCorner.y * inverseW;

        minNdcX = (std::min)(minNdcX, ndcX);
        maxNdcX = (std::max)(maxNdcX, ndcX);
        minNdcY = (std::min)(minNdcY, ndcY);
        maxNdcY = (std::max)(maxNdcY, ndcY);
    }

    minNdcX = std::clamp(minNdcX, -1.0f, 1.0f);
    maxNdcX = std::clamp(maxNdcX, -1.0f, 1.0f);
    minNdcY = std::clamp(minNdcY, -1.0f, 1.0f);
    maxNdcY = std::clamp(maxNdcY, -1.0f, 1.0f);

    const float width = static_cast<float>(WinApp::kClientWidth);
    const float height = static_cast<float>(WinApp::kClientHeight);

    const LONG left = static_cast<LONG>(std::floor((minNdcX * 0.5f + 0.5f) * width));
    const LONG right = static_cast<LONG>(std::ceil((maxNdcX * 0.5f + 0.5f) * width));
    const LONG top = static_cast<LONG>(std::floor((1.0f - (maxNdcY * 0.5f + 0.5f)) * height));
    const LONG bottom = static_cast<LONG>(std::ceil((1.0f - (minNdcY * 0.5f + 0.5f)) * height));

    result.scissorRect.left = std::clamp<LONG>(left, 0, WinApp::kClientWidth);
    result.scissorRect.right = std::clamp<LONG>(right, 0, WinApp::kClientWidth);
    result.scissorRect.top = std::clamp<LONG>(top, 0, WinApp::kClientHeight);
    result.scissorRect.bottom = std::clamp<LONG>(bottom, 0, WinApp::kClientHeight);

    if (result.scissorRect.right <= result.scissorRect.left || result.scissorRect.bottom <= result.scissorRect.top) {
        result.isVisible = false;
        result.scissorRect = MakeFullScreenScissor();
        result.isPassSkipped = true;
        return result;
    }

    const float screenArea = static_cast<float>(WinApp::kClientWidth * WinApp::kClientHeight);
    const float scissorWidth = static_cast<float>(result.scissorRect.right - result.scissorRect.left);
    const float scissorHeight = static_cast<float>(result.scissorRect.bottom - result.scissorRect.top);
    result.scissorAreaRatio = (screenArea > 0.0f) ? ((scissorWidth * scissorHeight) / screenArea) : 1.0f;

    result.useFullScreenScissor =
        result.scissorRect.left == 0 &&
        result.scissorRect.top == 0 &&
        result.scissorRect.right == WinApp::kClientWidth &&
        result.scissorRect.bottom == WinApp::kClientHeight;

    if (settings_.forceMode_ == ForceMode::ForceFullscreen) {
        result.useFullScreenScissor = true;
        result.scissorRect = MakeFullScreenScissor();
        result.scissorAreaRatio = 1.0f;
    } else if (settings_.forceMode_ == ForceMode::ForceScissor) {
        // TODO: For camera-inside / near-plane-crossing cases we intentionally keep the safe fullscreen fallback.
        result.useFullScreenScissor = false;
    }

    result.isPassSkipped = !settings_.isEnabled_ || settings_.forceMode_ == ForceMode::ForceSkip;

    return result;
}

void VolumetricCloudPass::Render(const Camera* camera, const CloudVolume* cloudVolume, const ProjectedBounds& projectedBounds)
{
    assert(dxCommon_);
    assert(camera);
    assert(cloudVolume);
    assert(pipelineResources_ && pipelineResources_->GetRootSignature());
    assert(pipelineResources_ && pipelineResources_->GetPipelineState());
    assert(constantBuffer_);
    assert(constantData_);

    MarkGpuScene();
    if (comparison_.IsActive() && !comparison_.Maintain(settings_, dxCommon_->GetRenderTextureWidth(), dxCommon_->GetRenderTextureHeight())) {
        if (auto* runtime = RuntimeModeController::GetActiveController()) runtime->SetCloudComparisonActive(false);
        dxCommon_->GetGpuProfiler().SetEnabled(comparisonSavedProfilerEnabled_);
        hasValidCloudBuffer_ = false;
    }
    if (projectedBounds.isPassSkipped || !projectedBounds.isVisible) {
        RecordGpuSkipped(!settings_.isEnabled_ || settings_.forceMode_ == ForceMode::ForceSkip);
        return;
    }
    ++diagnosticInvocation_;
    diagnosticReallocated_ = false;

    if (settings_.useLowResolutionCloud_) {
        RenderLowResolution(camera, cloudVolume, projectedBounds);
    } else {
        RenderDirect(camera, cloudVolume, projectedBounds);
    }
}

void VolumetricCloudPass::SetExternalFlowMultiplier(float multiplier)
{
    if (!std::isfinite(multiplier)) {
        externalFlowMultiplier_ = 1.0f;
        return;
    }
    externalFlowMultiplier_ = std::clamp(multiplier, 0.0f, 2.0f);
}

void VolumetricCloudPass::SetExternalBoostExtraFlowSpeed(float extraSpeed)
{
    if (!std::isfinite(extraSpeed)) {
        externalBoostExtraFlowSpeed_ = 0.0f;
        return;
    }
    externalBoostExtraFlowSpeed_ = std::clamp(extraSpeed, 0.0f, 30.0f);
}

void VolumetricCloudPass::SetInfluenceFields(const Vector4* centersAndRadius, const Vector4* params, uint32_t count)
{
    const uint32_t clampedCount = std::clamp(count, 0u, 16u);
    influenceFieldCount_ = clampedCount;
    influenceCentersAndRadius_.fill({ 0.0f, 0.0f, 0.0f, 0.0f });
    influenceParams_.fill({ 0.0f, 0.0f, 1.0f, 0.0f });
    if (!centersAndRadius || !params) {
        influenceFieldCount_ = 0;
        return;
    }
    for (uint32_t index = 0; index < clampedCount; ++index) {
        influenceCentersAndRadius_[index] = centersAndRadius[index];
        influenceParams_[index] = params[index];
    }
}

void VolumetricCloudPass::SetCloudInfluenceEnabled(bool enabled)
{
    settings_.enableCloudInfluenceClear_ = enabled;
}

void VolumetricCloudPass::SetCameraForwardTunnelSettings(bool enabled, float length, float radius, float clearStrength)
{
    settings_.enableCameraForwardTunnel_ = enabled;
    settings_.cameraForwardTunnelLength_ = (std::max)(length, 0.0f);
    settings_.cameraForwardTunnelRadius_ = (std::max)(radius, 0.0f);
    settings_.cameraForwardTunnelClearStrength_ = std::clamp(clearStrength, 0.0f, 1.0f);
}
void VolumetricCloudPass::ApplyGameModePerformancePreset() {
    if (comparison_.IsActive()) return;
    ApplyQualityPreset(VolumetricCloudSettings::QualityPreset::Light);
}
void VolumetricCloudPass::ApplyQualityPreset(VolumetricCloudSettings::QualityPreset preset) {
    if (settings_.ApplyQualityPreset(preset)) hasValidCloudBuffer_ = false;
}

void VolumetricCloudPass::RenderDirect(const Camera* camera, const CloudVolume* cloudVolume, const ProjectedBounds& projectedBounds)
{
    const uint32_t renderWidth = dxCommon_ ? dxCommon_->GetRenderTextureWidth() : WinApp::kClientWidth;
    const uint32_t renderHeight = dxCommon_ ? dxCommon_->GetRenderTextureHeight() : WinApp::kClientHeight;
    UpdateConstantBuffer(camera, cloudVolume, renderWidth, renderHeight);

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    D3D12_CPU_DESCRIPTOR_HANDLE sceneRTV = dxCommon_->GetRenderTextureRTV();
    commandList->OMSetRenderTargets(1, &sceneRTV, FALSE, nullptr);

    const D3D12_VIEWPORT viewport = MakeViewport(renderWidth, renderHeight);
    commandList->RSSetViewports(1, &viewport);

    const D3D12_RECT scissorRect =
        projectedBounds.useFullScreenScissor ?
        MakeScissor(renderWidth, renderHeight) :
        ScaleScissor(projectedBounds.scissorRect, renderWidth, renderHeight);
    commandList->RSSetScissorRects(1, &scissorRect);

    ID3D12DescriptorHeap* descriptorHeaps[] = { dxCommon_->GetSrvDescriptorHeap() };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);

    const auto gpuToken = BeginGpuMeasurement(false, 0, true);
    commandList->SetGraphicsRootSignature(pipelineResources_->GetRootSignature());
    commandList->SetPipelineState(pipelineResources_->GetPipelineState());
    commandList->SetGraphicsRootDescriptorTable(0, dxCommon_->GetDepthTextureSRVGPUHandle());
    commandList->SetGraphicsRootConstantBufferView(1, constantBuffer_->GetGPUVirtualAddress());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
    EndGpuMeasurement(gpuToken);
    BeginGpuMeasurement(true, 1, true);

    const D3D12_RECT fullScreenScissor = MakeScissor(renderWidth, renderHeight);
    const D3D12_VIEWPORT fullScreenViewport = MakeViewport(renderWidth, renderHeight);
    commandList->RSSetViewports(1, &fullScreenViewport);
    commandList->RSSetScissorRects(1, &fullScreenScissor);
}

void VolumetricCloudPass::CreateConstantBuffer()
{
    size_t bufferSize = AlignConstantBufferSize(sizeof(CloudPassConstants));
    constantBuffer_ = dxCommon_->CreateBufferResource(bufferSize);

    HRESULT hr = constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantData_));
    assert(SUCCEEDED(hr));
}

void VolumetricCloudPass::UpdateConstantBuffer(const Camera* camera, const CloudVolume* cloudVolume, uint32_t renderWidth, uint32_t renderHeight)
{
    const bool appearance = passKind_ == PassKind::Game && settings_.appearanceMode_ != AppearanceMode::Legacy;
    const auto parameters = appearance ? cloudVolume->GetAppearanceCandidateParameters(
        settings_.appearanceMode_ == AppearanceMode::ShapeAndLighting) : cloudVolume->GetParameters();

    constantData_->inverseViewProjection = Inverse(camera->GetViewProjectionMatrix());
    // Ray origin and inverse projection must include the same framing/shake offset.
    constantData_->cameraPosition = camera->GetViewTranslate();
    const ResolvedCloudVolume resolvedVolume = ResolveCloudVolume(camera, cloudVolume);

    constantData_->volumeCenter = resolvedVolume.center;
    constantData_->density = parameters.density;
    constantData_->volumeHalfExtents = resolvedVolume.halfExtents;
    constantData_->absorption = parameters.absorption;
    constantData_->windOffset = settings_.enableCloudFlow_ ? Vector3{ 0.0f, 0.0f, 0.0f } : cloudVolume->GetWindOffset();
    constantData_->noiseScale = parameters.noiseScale;
    constantData_->sunDirection = Normalize(parameters.sunDirection);
    constantData_->detailNoiseScale = parameters.detailNoiseScale;
    constantData_->cloudColor = parameters.color;
    constantData_->lightAbsorption = parameters.lightAbsorption;
    constantData_->detailWeight = parameters.detailWeight;
    constantData_->edgeFade = parameters.edgeFade;
    constantData_->ambientLighting = parameters.ambientLighting;
    constantData_->sunIntensity = parameters.sunIntensity;
    constantData_->viewStepCount = (std::max)(1u, static_cast<uint32_t>(static_cast<float>((std::max)(parameters.viewStepCount, 1u)) * settings_.viewStepScale_ + 0.5f));
    constantData_->lightStepCount = (std::max)(1u, static_cast<uint32_t>(static_cast<float>((std::max)(parameters.lightStepCount, 1u)) * settings_.lightStepScale_ + 0.5f));
    constantData_->debugViewMode = static_cast<uint32_t>(settings_.debugViewMode_);
    constantData_->lodFactorScale = (settings_.forceMode_ == ForceMode::ForceAggressiveLod) ? 1.75f : 1.0f;
    constantData_->disableDistanceLod = (settings_.forceMode_ == ForceMode::ForceMaxQuality) ? 1u : 0u;
    static_assert(offsetof(CloudPassConstants, volumeShapeMode) == 76);
    static_assert(offsetof(CloudPassConstants, renderInfo) == 208);
    static_assert(sizeof(CloudPassConstants) % 16 == 0);
    constantData_->volumeShapeMode = appearance ? 1u : 0u;
    constantData_->padding1 = 0.0f;
    constantData_->padding2 = 0.0f;
    const uint32_t depthWidth = dxCommon_ ? dxCommon_->GetRenderTextureWidth() : WinApp::kClientWidth;
    const uint32_t depthHeight = dxCommon_ ? dxCommon_->GetRenderTextureHeight() : WinApp::kClientHeight;
    constantData_->renderInfo = {
        static_cast<float>(depthWidth),
        static_cast<float>(depthHeight),
        static_cast<float>(renderWidth),
        static_cast<float>(renderHeight)
    };

    UpdateCloudFlow(camera, cloudVolume);
    constantData_->nearCameraFade = {
        settings_.nearFadeStart_,
        (std::max)(settings_.nearFadeStart_ + 0.001f, settings_.nearFadeEnd_),
        std::clamp(settings_.nearDensityScale_, 0.0f, 1.0f),
        settings_.enableNearCameraCloudFade_ ? 1.0f : 0.0f
    };
    constantData_->cloudLayerFade = {
        (std::max)(settings_.cloudBottomFade_, 0.001f),
        (std::max)(settings_.cloudTopFade_, 0.001f),
        (std::max)(settings_.cloudLayerThickness_, 1.0f),
        settings_.keepCameraBelowClouds_ ? 1.0f : 0.0f
    };
    constantData_->cloudBottomShaping = {
        settings_.enableCloudBottomShaping_ ? 1.0f : 0.0f,
        std::clamp(settings_.cloudBottomFlattenStrength_, 0.0f, 1.0f),
        std::clamp(settings_.cloudBottomSmoothness_, 0.0f, 1.0f),
        std::clamp(settings_.cloudBottomNoiseSuppression_, 0.0f, 1.0f)
    };
    constantData_->influenceCentersAndRadius = influenceCentersAndRadius_;
    constantData_->influenceParams = influenceParams_;
    const Vector3 tunnelForward = Normalize(GetCameraForward(camera));
    constantData_->influenceSettings = {
        static_cast<float>(std::clamp(influenceFieldCount_, 0u, 16u)),
        settings_.enableCloudInfluenceClear_ ? 1.0f : 0.0f,
        settings_.enableCameraForwardTunnel_ ? 1.0f : 0.0f,
        settings_.cameraForwardTunnelClearStrength_
    };
    constantData_->cameraTunnelStartLength = {
        constantData_->cameraPosition.x,
        constantData_->cameraPosition.y,
        constantData_->cameraPosition.z,
        settings_.cameraForwardTunnelLength_
    };
    constantData_->cameraTunnelDirectionRadius = {
        tunnelForward.x,
        tunnelForward.y,
        tunnelForward.z,
        settings_.cameraForwardTunnelRadius_
    };
    UpdateQualityConstantBuffer();
}

Vector3 VolumetricCloudPass::Normalize(const Vector3& value)
{
    float lengthSquared =
        value.x * value.x +
        value.y * value.y +
        value.z * value.z;

    if (lengthSquared <= 0.000001f) {
        return { 0.0f, -1.0f, 0.0f };
    }

    float inverseLength = 1.0f / std::sqrt(lengthSquared);
    return {
        value.x * inverseLength,
        value.y * inverseLength,
        value.z * inverseLength
    };
}

Vector4 VolumetricCloudPass::TransformPoint(const Vector3& value, const Matrix4x4& matrix)
{
    return {
        value.x * matrix.m[0][0] + value.y * matrix.m[1][0] + value.z * matrix.m[2][0] + matrix.m[3][0],
        value.x * matrix.m[0][1] + value.y * matrix.m[1][1] + value.z * matrix.m[2][1] + matrix.m[3][1],
        value.x * matrix.m[0][2] + value.y * matrix.m[1][2] + value.z * matrix.m[2][2] + matrix.m[3][2],
        value.x * matrix.m[0][3] + value.y * matrix.m[1][3] + value.z * matrix.m[2][3] + matrix.m[3][3]
    };
}

D3D12_RECT VolumetricCloudPass::MakeFullScreenScissor()
{
    return MakeScissor(WinApp::kClientWidth, WinApp::kClientHeight);
}

D3D12_RECT VolumetricCloudPass::MakeScissor(uint32_t width, uint32_t height)
{
    return { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
}

D3D12_RECT VolumetricCloudPass::ScaleScissor(const D3D12_RECT& source, uint32_t width, uint32_t height)
{
    const float scaleX = static_cast<float>(width) / static_cast<float>((std::max)(WinApp::kClientWidth, 1));
    const float scaleY = static_cast<float>(height) / static_cast<float>((std::max)(WinApp::kClientHeight, 1));

    D3D12_RECT result{};
    result.left = std::clamp<LONG>(static_cast<LONG>(std::floor(static_cast<float>(source.left) * scaleX)), 0, static_cast<LONG>(width));
    result.top = std::clamp<LONG>(static_cast<LONG>(std::floor(static_cast<float>(source.top) * scaleY)), 0, static_cast<LONG>(height));
    result.right = std::clamp<LONG>(static_cast<LONG>(std::ceil(static_cast<float>(source.right) * scaleX)), 0, static_cast<LONG>(width));
    result.bottom = std::clamp<LONG>(static_cast<LONG>(std::ceil(static_cast<float>(source.bottom) * scaleY)), 0, static_cast<LONG>(height));
    if (result.right <= result.left || result.bottom <= result.top) {
        return MakeScissor(width, height);
    }
    return result;
}

D3D12_VIEWPORT VolumetricCloudPass::MakeViewport(uint32_t width, uint32_t height)
{
    D3D12_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    return viewport;
}

float VolumetricCloudPass::ComputeDistanceToAabb(const Vector3& point, const ResolvedCloudVolume& volume)
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

    const float dx = (std::max)((std::max)(min.x - point.x, 0.0f), point.x - max.x);
    const float dy = (std::max)((std::max)(min.y - point.y, 0.0f), point.y - max.y);
    const float dz = (std::max)((std::max)(min.z - point.z, 0.0f), point.z - max.z);

    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void VolumetricCloudPass::ComputeDistanceLodScales(float entryDistance, const ResolvedCloudVolume& volume, float lodFactorScale, bool disableDistanceLod, float& viewStepScale, float& densityScale)
{
    const float volumeRadius =
        (std::max)(
            std::sqrt(
                volume.halfExtents.x * volume.halfExtents.x +
                volume.halfExtents.y * volume.halfExtents.y +
                volume.halfExtents.z * volume.halfExtents.z),
            0.0001f);

    const float lodStart = (std::max)(volumeRadius * 0.75f, 8.0f);
    const float lodEnd = lodStart + (std::max)(volumeRadius * 2.50f, 16.0f);
    float lodFactor = 0.0f;

    if (!disableDistanceLod) {
        lodFactor = std::clamp((entryDistance - lodStart) / (std::max)(lodEnd - lodStart, 0.0001f), 0.0f, 1.0f);
        lodFactor = std::clamp(lodFactor * lodFactorScale, 0.0f, 1.0f);
    }

    viewStepScale = 1.0f + (0.45f - 1.0f) * lodFactor;
    densityScale = 1.0f + (0.65f - 1.0f) * lodFactor;
}
