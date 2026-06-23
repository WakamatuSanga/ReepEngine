#include "VolumetricCloudPass.h"

#include "CloudVolume.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Utility/Logger.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cmath>

using namespace MatrixMath;
using namespace Microsoft::WRL;

namespace {
    constexpr size_t AlignConstantBufferSize(size_t size)
    {
        return (size + 0xff) & ~static_cast<size_t>(0xff);
    }

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
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    CreateRootSignature();
    CreatePipelineState();
    CreateCompositeRootSignature();
    CreateCompositePipelineState();
    CreateConstantBuffer();
    CreateCompositeConstantBuffer();
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
    const bool disableDistanceLod = (forceMode_ == ForceMode::ForceMaxQuality);
    const float lodFactorScale = (forceMode_ == ForceMode::ForceAggressiveLod) ? 1.75f : 1.0f;
    const float entryDistance = ComputeDistanceToAabb(camera->GetTranslate(), resolvedVolume);
    float densityScale = 1.0f;
    ComputeDistanceLodScales(entryDistance, resolvedVolume, lodFactorScale, disableDistanceLod, result.currentViewStepScale, densityScale);
    result.currentLightStepScale = result.currentViewStepScale;
    result.estimatedViewSteps = (std::max)(1u, static_cast<uint32_t>(static_cast<float>((std::max)(parameters.viewStepCount, 1u)) * result.currentViewStepScale + 0.5f));
    result.estimatedLightSteps = (std::max)(1u, static_cast<uint32_t>(static_cast<float>((std::max)(parameters.lightStepCount, 1u)) * result.currentLightStepScale + 0.5f));

    result.isCameraInsideCloud = ContainsPoint(resolvedVolume, camera->GetTranslate());
    if (result.isCameraInsideCloud) {
        result.isVisible = true;
        result.useFullScreenScissor = true;
        result.isFullScreenFallback = true;
        result.scissorAreaRatio = 1.0f;
        result.isPassSkipped = !isEnabled_ || forceMode_ == ForceMode::ForceSkip;
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
        result.isPassSkipped = !isEnabled_ || forceMode_ == ForceMode::ForceSkip;
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

    if (forceMode_ == ForceMode::ForceFullscreen) {
        result.useFullScreenScissor = true;
        result.scissorRect = MakeFullScreenScissor();
        result.scissorAreaRatio = 1.0f;
    } else if (forceMode_ == ForceMode::ForceScissor) {
        // TODO: For camera-inside / near-plane-crossing cases we intentionally keep the safe fullscreen fallback.
        result.useFullScreenScissor = false;
    }

    result.isPassSkipped = !isEnabled_ || forceMode_ == ForceMode::ForceSkip;

    return result;
}

void VolumetricCloudPass::Render(const Camera* camera, const CloudVolume* cloudVolume, const ProjectedBounds& projectedBounds)
{
    assert(dxCommon_);
    assert(camera);
    assert(cloudVolume);
    assert(rootSignature_);
    assert(pipelineState_);
    assert(constantBuffer_);
    assert(constantData_);

    if (projectedBounds.isPassSkipped || !projectedBounds.isVisible) {
        return;
    }

    if (useLowResolutionCloud_) {
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
    externalFlowMultiplier_ = (std::max)(0.0f, multiplier);
}

void VolumetricCloudPass::ApplyGameModePerformancePreset()
{
    const bool needsRecreate = std::fabs(cloudResolutionScale_ - 0.25f) > 0.001f;
    useLowResolutionCloud_ = true;
    enableCloudComposite_ = true;
    enableDepthAwareUpsample_ = true;
    cloudResolutionScale_ = 0.25f;
    cloudRenderInterval_ = 1;
    viewStepScale_ = 0.5f;
    lightStepScale_ = 0.5f;
    recreateCloudBufferRequested_ = recreateCloudBufferRequested_ || needsRecreate;
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

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->SetGraphicsRootDescriptorTable(0, dxCommon_->GetDepthTextureSRVGPUHandle());
    commandList->SetGraphicsRootConstantBufferView(1, constantBuffer_->GetGPUVirtualAddress());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);

    const D3D12_RECT fullScreenScissor = MakeScissor(renderWidth, renderHeight);
    const D3D12_VIEWPORT fullScreenViewport = MakeViewport(renderWidth, renderHeight);
    commandList->RSSetViewports(1, &fullScreenViewport);
    commandList->RSSetScissorRects(1, &fullScreenScissor);
}

void VolumetricCloudPass::CreateRootSignature()
{
    HRESULT hr = S_OK;

    D3D12_DESCRIPTOR_RANGE descriptorRange{};
    descriptorRange.BaseShaderRegister = 0;
    descriptorRange.NumDescriptors = 1;
    descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[2]{};

    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Descriptor.ShaderRegister = 0;
    rootParameters[1].Descriptor.RegisterSpace = 0;

    D3D12_STATIC_SAMPLER_DESC staticSampler{};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
    staticSampler.ShaderRegister = 0;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pStaticSamplers = &staticSampler;
    rootSignatureDesc.NumStaticSamplers = 1;

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }

    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

void VolumetricCloudPass::CreatePipelineState()
{
    ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/CopyImage.VS.hlsl", L"vs_6_0");
    ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/VolumetricCloud.PS.hlsl", L"ps_6_0");

    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = FALSE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc{};
    pipelineStateDesc.pRootSignature = rootSignature_.Get();
    pipelineStateDesc.InputLayout = { nullptr, 0 };
    pipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    pipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    pipelineStateDesc.BlendState = blendDesc;
    pipelineStateDesc.RasterizerState = rasterizerDesc;
    pipelineStateDesc.DepthStencilState = depthStencilDesc;
    pipelineStateDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    pipelineStateDesc.NumRenderTargets = 1;
    pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateDesc.SampleDesc.Count = 1;
    pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
        &pipelineStateDesc,
        IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
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
    const CloudVolume::Parameters& parameters = cloudVolume->GetParameters();

    constantData_->inverseViewProjection = Inverse(camera->GetViewProjectionMatrix());
    constantData_->cameraPosition = camera->GetTranslate();
    const ResolvedCloudVolume resolvedVolume = ResolveCloudVolume(camera, cloudVolume);

    constantData_->volumeCenter = resolvedVolume.center;
    constantData_->density = parameters.density;
    constantData_->volumeHalfExtents = resolvedVolume.halfExtents;
    constantData_->absorption = parameters.absorption;
    constantData_->windOffset = enableCloudFlow_ ? Vector3{ 0.0f, 0.0f, 0.0f } : cloudVolume->GetWindOffset();
    constantData_->noiseScale = parameters.noiseScale;
    constantData_->sunDirection = Normalize(parameters.sunDirection);
    constantData_->detailNoiseScale = parameters.detailNoiseScale;
    constantData_->cloudColor = parameters.color;
    constantData_->lightAbsorption = parameters.lightAbsorption;
    constantData_->detailWeight = parameters.detailWeight;
    constantData_->edgeFade = parameters.edgeFade;
    constantData_->ambientLighting = parameters.ambientLighting;
    constantData_->sunIntensity = parameters.sunIntensity;
    constantData_->viewStepCount = (std::max)(1u, static_cast<uint32_t>(static_cast<float>((std::max)(parameters.viewStepCount, 1u)) * viewStepScale_ + 0.5f));
    constantData_->lightStepCount = (std::max)(1u, static_cast<uint32_t>(static_cast<float>((std::max)(parameters.lightStepCount, 1u)) * lightStepScale_ + 0.5f));
    constantData_->debugViewMode = static_cast<uint32_t>(debugViewMode_);
    constantData_->lodFactorScale = (forceMode_ == ForceMode::ForceAggressiveLod) ? 1.75f : 1.0f;
    constantData_->disableDistanceLod = (forceMode_ == ForceMode::ForceMaxQuality) ? 1u : 0u;
    constantData_->padding0 = 0.0f;
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

    Vector3 flowDirection = ResolveCloudFlowDirection(camera);
    currentCloudFlowDirection_ = flowDirection;

    const float boostMultiplier = useBoostFlowMultiplier_ ? externalFlowMultiplier_ : 1.0f;
    currentCloudFlowSpeed_ = enableCloudFlow_ ? cloudBaseFlowSpeed_ * (std::max)(0.0f, boostMultiplier) : 0.0f;
    constantData_->cloudFlowDirectionSpeed = {
        flowDirection.x,
        flowDirection.y,
        flowDirection.z,
        currentCloudFlowSpeed_
    };
    constantData_->cloudTime = cloudVolume->GetElapsedTime();
    constantData_->enableCloudFlow = enableCloudFlow_ ? 1u : 0u;
    constantData_->padding3 = 0.0f;
    constantData_->padding4 = 0.0f;
    constantData_->nearCameraFade = {
        nearFadeStart_,
        (std::max)(nearFadeStart_ + 0.001f, nearFadeEnd_),
        std::clamp(nearDensityScale_, 0.0f, 1.0f),
        enableNearCameraCloudFade_ ? 1.0f : 0.0f
    };
    constantData_->cloudLayerFade = {
        (std::max)(cloudBottomFade_, 0.001f),
        (std::max)(cloudTopFade_, 0.001f),
        (std::max)(cloudLayerThickness_, 1.0f),
        keepCameraBelowClouds_ ? 1.0f : 0.0f
    };
    constantData_->cloudBottomShaping = {
        enableCloudBottomShaping_ ? 1.0f : 0.0f,
        std::clamp(cloudBottomFlattenStrength_, 0.0f, 1.0f),
        std::clamp(cloudBottomSmoothness_, 0.0f, 1.0f),
        std::clamp(cloudBottomNoiseSuppression_, 0.0f, 1.0f)
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
