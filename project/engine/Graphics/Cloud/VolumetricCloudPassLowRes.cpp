#include "VolumetricCloudPass.h"

#include "CloudVolume.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Core/SrvManager.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Utility/Logger.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>

using namespace Microsoft::WRL;

namespace {
    constexpr DXGI_FORMAT kCloudBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    constexpr float kCloudClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    constexpr size_t AlignConstantBufferSize(size_t size)
    {
        return (size + 0xff) & ~static_cast<size_t>(0xff);
    }

    ID3D12Resource* CreateCloudRenderTextureResource(ID3D12Device* device, uint32_t width, uint32_t height)
    {
        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resourceDesc.Width = width;
        resourceDesc.Height = height;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = kCloudBufferFormat;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_HEAP_PROPERTIES heapProperties{};
        heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_CLEAR_VALUE clearValue{};
        clearValue.Format = kCloudBufferFormat;
        clearValue.Color[0] = kCloudClearColor[0];
        clearValue.Color[1] = kCloudClearColor[1];
        clearValue.Color[2] = kCloudClearColor[2];
        clearValue.Color[3] = kCloudClearColor[3];

        ID3D12Resource* resource = nullptr;
        HRESULT hr = device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clearValue,
            IID_PPV_ARGS(&resource));
        assert(SUCCEEDED(hr));
        return resource;
    }

    void TransitionResource(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
    {
        if (!resource || before == after) {
            return;
        }

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);
    }
}

void VolumetricCloudPass::RenderLowResolution(const Camera* camera, const CloudVolume* cloudVolume, const ProjectedBounds& projectedBounds)
{
    EnsureCloudBuffer();
    if (!cloudColorResource_) {
        RenderDirect(camera, cloudVolume, projectedBounds);
        return;
    }

    const int interval = (std::max)(1, cloudRenderInterval_);
    const bool shouldRenderCloud = !hasValidCloudBuffer_ || (frameCounter_ % static_cast<uint32_t>(interval) == 0u);
    ++frameCounter_;

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    if (shouldRenderCloud) {
        TransitionResource(commandList, cloudColorResource_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        commandList->ClearRenderTargetView(cloudColorRTV_, kCloudClearColor, 0, nullptr);
        commandList->OMSetRenderTargets(1, &cloudColorRTV_, FALSE, nullptr);

        const D3D12_VIEWPORT viewport = MakeViewport(cloudBufferWidth_, cloudBufferHeight_);
        const D3D12_RECT scissorRect = projectedBounds.useFullScreenScissor ?
            MakeScissor(cloudBufferWidth_, cloudBufferHeight_) :
            ScaleScissor(projectedBounds.scissorRect, cloudBufferWidth_, cloudBufferHeight_);
        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissorRect);

        UpdateConstantBuffer(camera, cloudVolume, cloudBufferWidth_, cloudBufferHeight_);
        ID3D12DescriptorHeap* descriptorHeaps[] = { dxCommon_->GetSrvDescriptorHeap() };
        commandList->SetDescriptorHeaps(1, descriptorHeaps);
        commandList->SetGraphicsRootSignature(rootSignature_.Get());
        commandList->SetPipelineState(pipelineState_.Get());
        commandList->SetGraphicsRootDescriptorTable(0, dxCommon_->GetDepthTextureSRVGPUHandle());
        commandList->SetGraphicsRootConstantBufferView(1, constantBuffer_->GetGPUVirtualAddress());
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->DrawInstanced(3, 1, 0, 0);

        TransitionResource(commandList, cloudColorResource_.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        hasValidCloudBuffer_ = true;
        ++renderedFrameCount_;
    } else {
        ++skippedFrameCount_;
    }

    if (IsCloudCompositeEnabled()) {
        CompositeCloudBuffer();
    }

    const uint32_t renderWidth = dxCommon_ ? dxCommon_->GetRenderTextureWidth() : WinApp::kClientWidth;
    const uint32_t renderHeight = dxCommon_ ? dxCommon_->GetRenderTextureHeight() : WinApp::kClientHeight;
    const D3D12_VIEWPORT fullScreenViewport = MakeViewport(renderWidth, renderHeight);
    const D3D12_RECT fullScreenScissor = MakeScissor(renderWidth, renderHeight);
    commandList->RSSetViewports(1, &fullScreenViewport);
    commandList->RSSetScissorRects(1, &fullScreenScissor);
}

void VolumetricCloudPass::CompositeCloudBuffer()
{
    if (!cloudColorResource_ || !hasValidCloudBuffer_) {
        return;
    }

    UpdateCompositeConstantBuffer();

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    D3D12_CPU_DESCRIPTOR_HANDLE sceneRTV = dxCommon_->GetRenderTextureRTV();
    commandList->OMSetRenderTargets(1, &sceneRTV, FALSE, nullptr);

    const uint32_t renderWidth = dxCommon_ ? dxCommon_->GetRenderTextureWidth() : WinApp::kClientWidth;
    const uint32_t renderHeight = dxCommon_ ? dxCommon_->GetRenderTextureHeight() : WinApp::kClientHeight;
    const D3D12_VIEWPORT viewport = MakeViewport(renderWidth, renderHeight);
    const D3D12_RECT scissorRect = MakeScissor(renderWidth, renderHeight);
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    ID3D12DescriptorHeap* descriptorHeaps[] = { dxCommon_->GetSrvDescriptorHeap() };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
    commandList->SetGraphicsRootSignature(compositeRootSignature_.Get());
    commandList->SetPipelineState(compositePipelineState_.Get());
    commandList->SetGraphicsRootDescriptorTable(0, cloudColorSRVGPU_);
    commandList->SetGraphicsRootDescriptorTable(1, dxCommon_->GetDepthTextureSRVGPUHandle());
    commandList->SetGraphicsRootConstantBufferView(2, compositeConstantBuffer_->GetGPUVirtualAddress());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
}

void VolumetricCloudPass::EnsureCloudBuffer()
{
    cloudResolutionScale_ = std::clamp(cloudResolutionScale_, 0.125f, 1.0f);
    const uint32_t renderWidth = dxCommon_ ? dxCommon_->GetRenderTextureWidth() : WinApp::kClientWidth;
    const uint32_t renderHeight = dxCommon_ ? dxCommon_->GetRenderTextureHeight() : WinApp::kClientHeight;
    const uint32_t requestedWidth = (std::max)(1u, static_cast<uint32_t>(std::ceil(static_cast<float>(renderWidth) * cloudResolutionScale_)));
    const uint32_t requestedHeight = (std::max)(1u, static_cast<uint32_t>(std::ceil(static_cast<float>(renderHeight) * cloudResolutionScale_)));
    if (!recreateCloudBufferRequested_ && cloudColorResource_ && cloudBufferWidth_ == requestedWidth && cloudBufferHeight_ == requestedHeight) {
        return;
    }

    recreateCloudBufferRequested_ = false;
    CreateCloudBuffer(requestedWidth, requestedHeight);
}

void VolumetricCloudPass::CreateCloudBuffer(uint32_t width, uint32_t height)
{
    assert(srvManager_);
    ID3D12Device* device = dxCommon_->GetDevice();
    assert(device);

    if (!cloudRtvHeap_) {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heapDesc.NumDescriptors = 1;
        HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&cloudRtvHeap_));
        assert(SUCCEEDED(hr));
    }

    cloudColorResource_.Reset();
    cloudColorResource_.Attach(CreateCloudRenderTextureResource(device, width, height));
    cloudColorRTV_ = cloudRtvHeap_->GetCPUDescriptorHandleForHeapStart();

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = kCloudBufferFormat;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    device->CreateRenderTargetView(cloudColorResource_.Get(), &rtvDesc, cloudColorRTV_);

    if (!cloudColorSrvAllocated_) {
        assert(srvManager_->CanAllocate());
        cloudColorSRVIndex_ = srvManager_->Allocate();
        cloudColorSrvAllocated_ = true;
    }
    cloudColorSRVCPU_ = srvManager_->GetCPUDescriptorHandle(cloudColorSRVIndex_);
    cloudColorSRVGPU_ = srvManager_->GetGPUDescriptorHandle(cloudColorSRVIndex_);
    srvManager_->CreateSRVforTexture2D(cloudColorSRVIndex_, cloudColorResource_.Get(), kCloudBufferFormat, 1);

    cloudBufferWidth_ = width;
    cloudBufferHeight_ = height;
    hasValidCloudBuffer_ = false;
}

void VolumetricCloudPass::CreateCompositeRootSignature()
{
    D3D12_DESCRIPTOR_RANGE descriptorRanges[2]{};
    for (uint32_t i = 0; i < 2; ++i) {
        descriptorRanges[i].BaseShaderRegister = i;
        descriptorRanges[i].NumDescriptors = 1;
        descriptorRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        descriptorRanges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    }

    D3D12_ROOT_PARAMETER rootParameters[3]{};
    for (uint32_t i = 0; i < 2; ++i) {
        rootParameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[i].DescriptorTable.pDescriptorRanges = &descriptorRanges[i];
        rootParameters[i].DescriptorTable.NumDescriptorRanges = 1;
    }
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].Descriptor.ShaderRegister = 0;

    D3D12_STATIC_SAMPLER_DESC staticSampler{};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
    staticSampler.ShaderRegister = 0;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    desc.pParameters = rootParameters;
    desc.NumParameters = _countof(rootParameters);
    desc.pStaticSamplers = &staticSampler;
    desc.NumStaticSamplers = 1;

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&compositeRootSignature_));
    assert(SUCCEEDED(hr));
}

void VolumetricCloudPass::CreateCompositePipelineState()
{
    ComPtr<IDxcBlob> vs = dxCommon_->CompileShader(L"resources/shaders/CopyImage.VS.hlsl", L"vs_6_0");
    ComPtr<IDxcBlob> ps = dxCommon_->CompileShader(L"resources/shaders/VolumetricCloudComposite.PS.hlsl", L"ps_6_0");

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

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = compositeRootSignature_.Get();
    desc.InputLayout = { nullptr, 0 };
    desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    desc.BlendState = blendDesc;
    desc.RasterizerState = rasterizerDesc;
    desc.DepthStencilState = depthStencilDesc;
    desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&compositePipelineState_));
    assert(SUCCEEDED(hr));
}

void VolumetricCloudPass::CreateCompositeConstantBuffer()
{
    compositeConstantBuffer_ = dxCommon_->CreateBufferResource(AlignConstantBufferSize(sizeof(CloudCompositeConstants)));
    HRESULT hr = compositeConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&compositeConstantData_));
    assert(SUCCEEDED(hr));
}

void VolumetricCloudPass::UpdateCompositeConstantBuffer()
{
    compositeConstantData_->cloudTextureSize = { static_cast<float>((std::max)(cloudBufferWidth_, 1u)), static_cast<float>((std::max)(cloudBufferHeight_, 1u)) };
    const uint32_t renderWidth = dxCommon_ ? dxCommon_->GetRenderTextureWidth() : WinApp::kClientWidth;
    const uint32_t renderHeight = dxCommon_ ? dxCommon_->GetRenderTextureHeight() : WinApp::kClientHeight;
    compositeConstantData_->outputTextureSize = { static_cast<float>(renderWidth), static_cast<float>(renderHeight) };
    const bool preserveAnyGameplayObject = preservePlayerFromLowResCloud_ || preserveEnemyFromLowResCloud_ || preserveBulletFromLowResCloud_;
    compositeConstantData_->enableDepthAwareUpsample = IsDepthAwareUpsampleEnabled() ? 1u : 0u;
    compositeConstantData_->enableGameplayObjectPreserve = (enableGameplayObjectPreserve_ && preserveAnyGameplayObject) ? 1u : 0u;
    compositeConstantData_->enableCloudDepthTest = enableCloudDepthTest_ ? 1u : 0u;
    compositeConstantData_->enableGameplayObjectMask = enableGameplayObjectMask_ ? 1u : 0u;
    compositeConstantData_->depthThreshold = depthThreshold_;
    compositeConstantData_->cloudOverGameplayObjectStrength = std::clamp(cloudOverGameplayObjectStrength_, 0.0f, 1.0f);
    compositeConstantData_->foregroundCloudAlphaReduction = std::clamp(foregroundCloudAlphaReduction_, 0.0f, 1.0f);
    compositeConstantData_->compositeDebugMode = static_cast<uint32_t>((std::max)(cloudCompositeDebugMode_, 0));
}

void VolumetricCloudPass::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::SeparatorText("雲の軽量化設定 (GPU Optimization)");
    ImGui::TextWrapped("まずは「バランス」を使ってください。PlayerやEnemyを低解像度にしたくない場合はGame Mode Render Scaleを1.0に保ち、Cloud Resolution Scaleだけを下げます。");

    auto applyPreset = [&](bool lowResolution, float resolutionScale, bool depthAware, int renderInterval, float viewScale, float lightScale) {
        useLowResolutionCloud_ = lowResolution;
        enableCloudComposite_ = true;
        cloudResolutionScale_ = resolutionScale;
        enableDepthAwareUpsample_ = depthAware;
        cloudRenderInterval_ = renderInterval;
        viewStepScale_ = viewScale;
        lightStepScale_ = lightScale;
        recreateCloudBufferRequested_ = true;
        };

    if (ImGui::Button("画質優先")) {
        applyPreset(true, 0.5f, true, 1, 1.0f, 1.0f);
    }
    ImGui::SameLine();
    if (ImGui::Button("バランス")) {
        applyPreset(true, 0.5f, true, 1, 0.75f, 0.75f);
    }
    ImGui::SameLine();
    if (ImGui::Button("軽量")) {
        applyPreset(true, 0.25f, true, 1, 0.5f, 0.5f);
    }
    if (ImGui::Button("超軽量")) {
        applyPreset(true, 0.125f, false, 2, 0.35f, 0.35f);
    }
    ImGui::SameLine();
    if (ImGui::Button("比較用Direct描画")) {
        useLowResolutionCloud_ = false;
        cloudRenderInterval_ = 1;
        viewStepScale_ = 1.0f;
        lightStepScale_ = 1.0f;
    }

    ImGui::Spacing();
    ImGui::TextWrapped("境界がにじむ場合は Depth-aware Upsample をONにしてください。雲がぼやけすぎる場合は Resolution Scale を0.5に戻してください。カメラ移動中にカクつく場合は Render Interval を1にしてください。");

    ImGui::Checkbox("低解像度雲を使う (Enable Low Resolution Cloud)", &useLowResolutionCloud_);
    ImGui::TextWrapped("ONにすると雲を小さい解像度で描画してから拡大合成します。軽くなりますが、少しぼやけます。");
    ImGui::Checkbox("雲をシーンに合成する (Enable Cloud Composite)", &enableCloudComposite_);
    ImGui::TextWrapped("低解像度Cloud Bufferの色と透明度を、現在のシーン描画へ合成します。通常はONのまま使います。");

    const char* scaleNames[] = { "1.0", "0.5", "0.25", "0.125" };
    const float scales[] = { 1.0f, 0.5f, 0.25f, 0.125f };
    int scaleIndex = 1;
    for (int i = 0; i < 4; ++i) {
        if (std::abs(cloudResolutionScale_ - scales[i]) < 0.001f) {
            scaleIndex = i;
        }
    }
    if (ImGui::Combo("雲の解像度倍率 (Cloud Resolution Scale)", &scaleIndex, scaleNames, IM_ARRAYSIZE(scaleNames))) {
        cloudResolutionScale_ = scales[scaleIndex];
        recreateCloudBufferRequested_ = true;
    }
    ImGui::TextWrapped("Cloud Resolution Scaleは雲だけを低解像度で描画します。Player / Enemy / Bulletは低解像度になりません。通常はこちらを下げて軽量化してください。");
    if (ImGui::Button("雲バッファを再作成 (Recreate Cloud Buffer)")) {
        recreateCloudBufferRequested_ = true;
    }
    ImGui::Text("雲バッファサイズ (Cloud Buffer Size): %u x %u", cloudBufferWidth_, cloudBufferHeight_);
    ImGui::Text("Rendered / Reused Frames: %u / %u", renderedFrameCount_, skippedFrameCount_);

    ImGui::Checkbox("深度を使って境界のにじみを抑える (Depth-aware Upsample)", &enableDepthAwareUpsample_);
    ImGui::TextWrapped("ONにすると、地形や機体の境界で雲がにじみにくくなります。");
    if (diagnosticDisableCloudComposite_ || diagnosticDisableDepthAwareUpsample_) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "影診断で雲合成またはDepth-aware Upsampleが一時無効です。");
    }
    ImGui::SliderFloat("深度差しきい値 (Depth Threshold)", &depthThreshold_, 0.0001f, 0.05f, "%.4f");
    ImGui::TextWrapped("小さいほど境界を厳しく判定します。にじむ場合は下げ、ガタつく場合は上げます。");
    ImGui::SliderFloat("視線方向ステップ倍率 (View Step Scale)", &viewStepScale_, 0.25f, 1.5f, "%.2f");
    ImGui::SliderFloat("光方向ステップ倍率 (Light Step Scale)", &lightStepScale_, 0.25f, 1.5f, "%.2f");
    ImGui::SliderInt("雲の更新間隔 (Cloud Render Interval)", &cloudRenderInterval_, 1, 6);
    ImGui::TextWrapped("1は毎フレーム更新、2は2フレームに1回更新です。大きいほど軽くなりますが、カメラ移動時にカクつきやすくなります。");

    const char* debugViewNames[] = { "Final", "Alpha only", "Density only", "Light only", "Far Cloud only", "Volumetric only", "Noise / UV", "Cloud Sea only" };
    int debugView = static_cast<int>(debugViewMode_);
    if (ImGui::Combo("雲のデバッグ表示 (Cloud Debug Mode)", &debugView, debugViewNames, IM_ARRAYSIZE(debugViewNames))) {
        debugViewMode_ = static_cast<DebugViewMode>(debugView);
    }

    if (cloudResolutionScale_ <= 0.125f) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "注意: 0.125はかなり軽いですが、雲がぼやけやすいです。");
    }
    if (cloudRenderInterval_ >= 2) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "注意: Render Intervalが2以上だと、カメラ移動中に雲がカクつく可能性があります。");
    }
    if (!useLowResolutionCloud_) {
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.35f, 1.0f), "注意: Direct描画は画質比較用です。重くなる可能性があります。");
    }

    ImGui::SeparatorText("雲の流れ方向 (Cloud Flow Direction)");
    ImGui::TextWrapped("奥から手前へ流したい場合は Toward Camera を使ってください。見た目が逆に流れる場合は Invert Flow Direction をONにしてください。");
    ImGui::Checkbox("雲を流す (Enable Cloud Flow)", &enableCloudFlow_);
    const char* flowModeNames[] = {
        "固定方向 (Fixed)",
        "カメラへ向かう (Toward Camera)",
        "カメラから遠ざかる (Away From Camera)",
        "Camera Forward",
        "Negative Camera Forward",
    };
    int flowMode = static_cast<int>(cloudFlowDirectionMode_);
    if (ImGui::Combo("流れ方向モード (Flow Direction Mode)", &flowMode, flowModeNames, IM_ARRAYSIZE(flowModeNames))) {
        cloudFlowDirectionMode_ = static_cast<CloudFlowDirectionMode>(flowMode);
    }
    ImGui::Checkbox("流れ方向を反転 (Invert Flow Direction)", &invertCloudFlowDirection_);
    if (cloudFlowDirectionMode_ == CloudFlowDirectionMode::Fixed) {
        ImGui::DragFloat3("固定流れ方向 (Flow Direction)", &fixedCloudFlowDirection_.x, 0.01f, -10.0f, 10.0f, "%.2f");
    }
    ImGui::DragFloat("基本流速 (Base Flow Speed)", &cloudBaseFlowSpeed_, 0.01f, 0.0f, 20.0f, "%.2f");
    ImGui::TextWrapped("Base Flow Speed は雲が奥から手前へ流れる基本速度です。Boost中はここに倍率が掛かります。");
    ImGui::Checkbox("Boost連動を使う (Use Boost Flow Multiplier)", &useBoostFlowMultiplier_);
    ImGui::Text("外部Boost倍率 (External Boost Multiplier): %.2f", externalFlowMultiplier_);
    ImGui::Text("Cloud Flow Base Speed: %.2f", cloudBaseFlowSpeed_);
    ImGui::Text("Cloud Boost Target Multiplier: %.2f", useBoostFlowMultiplier_ ? externalFlowMultiplier_ : 1.0f);
    ImGui::Text("Cloud Boost Current Multiplier: %.2f", useBoostFlowMultiplier_ ? externalFlowMultiplier_ : 1.0f);
    ImGui::Text("Cloud Boost Max Multiplier: 2.00");
    ImGui::Text("現在の流速 (Cloud Flow Current Speed): %.2f", currentCloudFlowSpeed_);
    ImGui::Text("Cloud Flow Phase / Accumulated Distance: %.3f", cloudFlowPhase_);
    ImGui::Text("Cloud Flow Phase Increasing: %s", cloudFlowPhaseIncreasing_ ? "true" : "false");
    if (cloudFlowPhaseDecreasedWarning_) {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.15f, 1.0f), "Warning: Cloud flow phase decreased. This may cause rewind.");
        if (ImGui::Button("Cloud Flow警告をクリア (Clear Cloud Flow Warning)")) {
            cloudFlowPhaseDecreasedWarning_ = false;
        }
    }
    ImGui::Text("現在の流れ方向 (Cloud Flow Direction): %.2f, %.2f, %.2f",
        currentCloudFlowDirection_.x,
        currentCloudFlowDirection_.y,
        currentCloudFlowDirection_.z);
    ImGui::TextWrapped("Boost連動ONでは、基本流速にBoostControllerの倍率を掛けます。通常時は1.0倍、Boost中は設定倍率へ近づきます。");

    ImGui::SeparatorText("カメラ相対の雲範囲 (Camera Relative Cloud Volume)");
    ImGui::TextWrapped("Cloud Near Distanceを負にすると、カメラの少し後ろまで雲を描けます。Y方向は雲レイヤー設定でカメラより上へ逃がし、初期状態では雲の下で戦えるようにします。");
    auto applyUnderCloudPreset = [&]() {
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
    };
    if (ImGui::Button("雲の下で戦う 改良版")) {
        applyUnderCloudPreset();
    }
    ImGui::SameLine();
    if (ImGui::Button("雲海強め")) {
        applyUnderCloudPreset();
        enableFarCloudLayer_ = true;
        farCloudAlpha_ = 0.55f;
        farCloudDistance_ = 300.0f;
        enableCloudSeaLayer_ = true; cloudSeaAlpha_ = 0.55f; cloudSeaDistance_ = 220.0f; cloudSeaDepth_ = 320.0f;
        cloudFarDistance_ = 250.0f;
        useLowResolutionCloud_ = true;
        enableDepthAwareUpsample_ = true;
        cloudResolutionScale_ = 0.25f;
        recreateCloudBufferRequested_ = true;
    }
    if (ImGui::Button("雲の中を突っ切る用")) {
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
    }
    ImGui::Checkbox("カメラ相対雲ボリュームを使う (Use Camera Relative Volume)", &useCameraRelativeCloudVolume_);
    ImGui::DragFloat("雲の開始距離 (Cloud Near Distance)", &cloudNearDistance_, 0.5f, -100.0f, 100.0f, "%.1f");
    ImGui::DragFloat("雲の奥行き距離 (Cloud Far Distance)", &cloudFarDistance_, 1.0f, 1.0f, 1000.0f, "%.1f");
    ImGui::DragFloat("カメラ後方まで描画する距離 (Behind Camera Distance)", &cloudBehindCameraDistance_, 0.5f, 0.0f, 100.0f, "%.1f");
    ImGui::DragFloat("雲の高さオフセット (Cloud Height Offset)", &cloudHeightOffset_, 0.25f, -100.0f, 100.0f, "%.1f");
    ImGui::DragFloat("雲の幅 (Cloud Volume Width)", &cloudVolumeWidth_, 1.0f, 1.0f, 1000.0f, "%.1f");
    ImGui::DragFloat("雲の高さ (Cloud Volume Height)", &cloudVolumeHeight_, 1.0f, 1.0f, 500.0f, "%.1f");
    ImGui::DragFloat("雲の奥行き (Cloud Volume Depth)", &cloudVolumeDepth_, 1.0f, 1.0f, 1000.0f, "%.1f");

    ImGui::SeparatorText("雲レイヤー設定 (Cloud Layer)");
    ImGui::TextWrapped("初期状態では「カメラを雲の下に置く」をONにしてください。雲の底面が画面上側や奥側に見え、プレイヤーや弾が雲に埋もれにくくなります。");
    ImGui::Checkbox("カメラを雲の下に置く (Keep Camera Below Clouds)", &keepCameraBelowClouds_);
    ImGui::DragFloat("カメラから雲底までの距離 (Camera To Cloud Bottom)", &cameraToCloudBottom_, 0.25f, 0.0f, 200.0f, "%.1f");
    ImGui::TextWrapped("Camera To Cloud Bottom を下げると、雲が画面内に入りやすくなります。雲の下で戦う場合は14〜18付近が目安です。下げすぎると開幕から雲の中になります。");
    ImGui::DragFloat("雲レイヤー厚み (Cloud Layer Thickness)", &cloudLayerThickness_, 1.0f, 1.0f, 300.0f, "%.1f");
    ImGui::DragFloat("雲の底面フェード (Cloud Bottom Fade)", &cloudBottomFade_, 0.5f, 0.1f, 100.0f, "%.1f");
    ImGui::DragFloat("雲の上面フェード (Cloud Top Fade)", &cloudTopFade_, 0.5f, 0.1f, 100.0f, "%.1f");
    const float cloudBottomOffset = keepCameraBelowClouds_ ? (cameraToCloudBottom_ + cloudHeightOffset_) : (cloudHeightOffset_ - cloudVolumeHeight_ * 0.5f);
    const float cloudTopOffset = keepCameraBelowClouds_ ? (cameraToCloudBottom_ + cloudHeightOffset_ + cloudLayerThickness_) : (cloudHeightOffset_ + cloudVolumeHeight_ * 0.5f);
    ImGui::Text("雲の底面高さ (Cloud Bottom Offset): %.1f", cloudBottomOffset);
    ImGui::Text("雲の上面高さ (Cloud Top Offset): %.1f", cloudTopOffset);

    ImGui::Checkbox("雲底の形を整える (Enable Cloud Bottom Shaping)", &enableCloudBottomShaping_);
    ImGui::SliderFloat("雲底の平らさ (Cloud Bottom Flatten Strength)", &cloudBottomFlattenStrength_, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("雲底のなめらかさ (Cloud Bottom Smoothness)", &cloudBottomSmoothness_, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("雲底のノイズ抑制 (Cloud Bottom Noise Suppression)", &cloudBottomNoiseSuppression_, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("雲底の濃さ (Cloud Bottom Density)", &cloudBottomDensity_, 0.0f, 1.5f, "%.2f");
    ImGui::SliderFloat("雲底のうねり (Cloud Bottom Undulation Strength)", &cloudBottomUndulationStrength_, 0.0f, 24.0f, "%.1f");
    ImGui::DragFloat("雲底のうねりスケール (Cloud Bottom Undulation Scale)", &cloudBottomUndulationScale_, 0.001f, 0.001f, 0.1f, "%.3f");
    ImGui::SliderFloat("雲境界の柔らかさ (Cloud Boundary Softness)", &cloudBoundarySoftness_, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("雲底付近のディテール (Cloud Detail Noise Near Bottom)", &cloudDetailNoiseNearBottom_, 0.0f, 1.0f, "%.2f");
    ImGui::TextWrapped("Bottom Undulationは雲底を少し上下させ、断面っぽさを減らします。Bottom Flattenを上げすぎると雲底が平らに見えます。");
    ImGui::Checkbox("Volume端フェードを使う (Enable Volume Edge Fade)", &enableVolumeEdgeFade_);
    ImGui::DragFloat("Volume端フェード距離 (Volume Edge Fade Distance)", &volumeEdgeFadeDistance_, 0.5f, 0.1f, 100.0f, "%.1f");

    ImGui::SeparatorText("近距離フェード (Near Camera Fade)");
    ImGui::TextWrapped("近くが白くなりすぎる場合はNear Fadeを強めてください。カメラすぐ近くを薄くし、少し先で通常濃度へ戻します。");
    ImGui::Checkbox("近距離フェードを使う (Enable Near Camera Fade)", &enableNearCameraCloudFade_);
    ImGui::DragFloat("近距離フェード開始 (Near Fade Start)", &nearFadeStart_, 0.1f, 0.0f, 100.0f, "%.1f");
    ImGui::DragFloat("近距離フェード終了 (Near Fade End)", &nearFadeEnd_, 0.1f, 0.1f, 200.0f, "%.1f");
    ImGui::SliderFloat("近距離濃度倍率 (Near Density Scale)", &nearDensityScale_, 0.0f, 1.0f, "%.2f");

    DrawQualityImGuiControls();
    ImGui::Checkbox("雲バッファのプレビュー表示 (Show Cloud Buffer Preview)", &showCloudBufferPreview_);
    if (showCloudBufferPreview_ && cloudColorResource_) {
        const float previewWidth = 240.0f;
        const float aspect = (cloudBufferWidth_ > 0) ? (static_cast<float>(cloudBufferHeight_) / static_cast<float>(cloudBufferWidth_)) : 0.5625f;
        ImGui::Image(static_cast<ImTextureID>(cloudColorSRVGPU_.ptr), ImVec2(previewWidth, previewWidth * aspect), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
    }
#endif
}
