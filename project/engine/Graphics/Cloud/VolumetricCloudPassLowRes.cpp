#include "VolumetricCloudPass.h"
#include "VolumetricCloudPipelineResources.h"

#include "CloudVolume.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Core/SrvManager.h"
#include "Engine/Graphics/Camera/Camera.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>

namespace {
    constexpr DXGI_FORMAT kCloudBufferFormat = VolumetricCloudPipelineResources::kColorFormat;
    constexpr float kCloudClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

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

    const int interval = (std::max)(1, settings_.cloudRenderInterval_);
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
        const auto gpuToken = BeginGpuMeasurement(false);
        commandList->SetGraphicsRootSignature(pipelineResources_->GetRootSignature());
        commandList->SetPipelineState(pipelineResources_->GetPipelineState());
        commandList->SetGraphicsRootDescriptorTable(0, dxCommon_->GetDepthTextureSRVGPUHandle());
        commandList->SetGraphicsRootConstantBufferView(1, constantBuffer_->GetGPUVirtualAddress());
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->DrawInstanced(3, 1, 0, 0);
        EndGpuMeasurement(gpuToken);

        TransitionResource(commandList, cloudColorResource_.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        hasValidCloudBuffer_ = true;
        ++renderedFrameCount_;
    } else {
        ++skippedFrameCount_;
        RecordGpuReuse();
    }

    if (IsCloudCompositeEnabled()) {
        CompositeCloudBuffer();
    } else {
        BeginGpuMeasurement(true, 2);
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
    const auto gpuToken = BeginGpuMeasurement(true);
    commandList->SetGraphicsRootSignature(pipelineResources_->GetCompositeRootSignature());
    commandList->SetPipelineState(pipelineResources_->GetCompositePipelineState());
    commandList->SetGraphicsRootDescriptorTable(0, cloudColorSRVGPU_);
    commandList->SetGraphicsRootDescriptorTable(1, dxCommon_->GetDepthTextureSRVGPUHandle());
    commandList->SetGraphicsRootConstantBufferView(2, compositeConstantBuffer_->GetGPUVirtualAddress());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
    EndGpuMeasurement(gpuToken);
}

void VolumetricCloudPass::EnsureCloudBuffer()
{
    settings_.cloudResolutionScale_ = std::clamp(settings_.cloudResolutionScale_, 0.125f, 1.0f);
    const uint32_t renderWidth = dxCommon_ ? dxCommon_->GetRenderTextureWidth() : WinApp::kClientWidth;
    const uint32_t renderHeight = dxCommon_ ? dxCommon_->GetRenderTextureHeight() : WinApp::kClientHeight;
    const uint32_t requestedWidth = (std::max)(1u, static_cast<uint32_t>(std::ceil(static_cast<float>(renderWidth) * settings_.cloudResolutionScale_)));
    const uint32_t requestedHeight = (std::max)(1u, static_cast<uint32_t>(std::ceil(static_cast<float>(renderHeight) * settings_.cloudResolutionScale_)));
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

    diagnosticReallocated_ = true;
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
    const bool preserveAnyGameplayObject = settings_.preservePlayerFromLowResCloud_ || settings_.preserveEnemyFromLowResCloud_ || settings_.preserveBulletFromLowResCloud_;
    compositeConstantData_->enableDepthAwareUpsample = IsDepthAwareUpsampleEnabled() ? 1u : 0u;
    compositeConstantData_->enableGameplayObjectPreserve = (settings_.enableGameplayObjectPreserve_ && preserveAnyGameplayObject) ? 1u : 0u;
    compositeConstantData_->enableCloudDepthTest = settings_.enableCloudDepthTest_ ? 1u : 0u;
    compositeConstantData_->enableGameplayObjectMask = settings_.enableGameplayObjectMask_ ? 1u : 0u;
    compositeConstantData_->depthThreshold = settings_.depthThreshold_;
    compositeConstantData_->cloudOverGameplayObjectStrength = std::clamp(settings_.cloudOverGameplayObjectStrength_, 0.0f, 1.0f);
    compositeConstantData_->foregroundCloudAlphaReduction = std::clamp(settings_.foregroundCloudAlphaReduction_, 0.0f, 1.0f);
    compositeConstantData_->compositeDebugMode = static_cast<uint32_t>((std::max)(settings_.cloudCompositeDebugMode_, 0));
}

void VolumetricCloudPass::DrawImGui()
{
#ifdef USE_IMGUI
    if (comparison_.IsActive()) {
        DrawComparisonControlsImGui();
        if (ImGui::CollapsingHeader("雲GPUの計測・診断")) DrawGpuDiagnosticsImGui();
        return;
    }
    if (ImGui::Button("雲の形状改善候補（形＋光）を試用")) BeginResolutionComparison(true, AppearanceMode::ShapeAndLighting);
    if (ImGui::Button("形だけの切り分けを試用")) BeginResolutionComparison(true, AppearanceMode::ShapeOnly);
    if (ImGui::Button("調整候補（0.25）を試用")) BeginResolutionComparison(true);
    if (ImGui::Button("解像度だけの固定比較を開始 (0.125 -> 1.0)")) BeginResolutionComparison();
    if (comparison_.IsActive()) return;
    ImGui::TextWrapped("固定比較には雲の有効化・合成ON・Force Skip解除・Timestamp対応が必要です。開始時に雲が見える視点を選んでください。");
    if (ImGui::CollapsingHeader("雲GPUの計測・診断")) DrawGpuDiagnosticsImGui();
    if (!ImGui::CollapsingHeader("雲の品質・合成・Flow設定")) return;
    ImGui::SeparatorText("雲の軽量化設定 (GPU Optimization)");
    ImGui::TextWrapped("まずは「バランス」を使ってください。PlayerやEnemyを低解像度にしたくない場合はGame Mode Render Scaleを1.0に保ち、Cloud Resolution Scaleだけを下げます。");

    if (ImGui::Button("画質優先")) {
        ApplyQualityPreset(VolumetricCloudSettings::QualityPreset::High);
    }
    ImGui::SameLine();
    if (ImGui::Button("バランス")) {
        ApplyQualityPreset(VolumetricCloudSettings::QualityPreset::Balanced);
    }
    ImGui::SameLine();
    if (ImGui::Button("軽量")) {
        ApplyQualityPreset(VolumetricCloudSettings::QualityPreset::Light);
    }
    if (ImGui::Button("超軽量")) {
        ApplyQualityPreset(VolumetricCloudSettings::QualityPreset::UltraLight);
    }
    ImGui::SameLine();
    if (ImGui::Button("比較用Direct描画")) {
        ApplyQualityPreset(VolumetricCloudSettings::QualityPreset::Direct);
    }

    ImGui::Spacing();
    ImGui::TextWrapped("境界がにじむ場合は Depth-aware Upsample をONにしてください。雲がぼやけすぎる場合は Resolution Scale を0.5に戻してください。カメラ移動中にカクつく場合は Render Interval を1にしてください。");

    ImGui::Checkbox("低解像度雲を使う (Enable Low Resolution Cloud)", &settings_.useLowResolutionCloud_);
    ImGui::TextWrapped("ONにすると雲を小さい解像度で描画してから拡大合成します。軽くなりますが、少しぼやけます。");
    ImGui::Checkbox("雲をシーンに合成する (Enable Cloud Composite)", &settings_.enableCloudComposite_);
    ImGui::TextWrapped("低解像度Cloud Bufferの色と透明度を、現在のシーン描画へ合成します。通常はONのまま使います。");

    const char* scaleNames[] = { "1.0", "0.5", "0.25", "0.125" };
    const float scales[] = { 1.0f, 0.5f, 0.25f, 0.125f };
    int scaleIndex = 1;
    for (int i = 0; i < 4; ++i) {
        if (std::abs(settings_.cloudResolutionScale_ - scales[i]) < 0.001f) {
            scaleIndex = i;
        }
    }
    if (ImGui::Combo("雲の解像度倍率 (Cloud Resolution Scale)", &scaleIndex, scaleNames, IM_ARRAYSIZE(scaleNames))) {
        settings_.cloudResolutionScale_ = scales[scaleIndex];
    }
    ImGui::TextWrapped("Cloud Resolution Scaleは雲だけを低解像度で描画します。Player / Enemy / Bulletは低解像度になりません。通常はこちらを下げて軽量化してください。");
    if (ImGui::Button("雲バッファを再作成 (Recreate Cloud Buffer)")) {
        recreateCloudBufferRequested_ = true;
    }
    ImGui::Text("雲バッファサイズ (Cloud Buffer Size): %u x %u", cloudBufferWidth_, cloudBufferHeight_);
    ImGui::Text("Rendered / Reused Frames: %u / %u", renderedFrameCount_, skippedFrameCount_);

    ImGui::Checkbox("深度を使って境界のにじみを抑える (Depth-aware Upsample)", &settings_.enableDepthAwareUpsample_);
    ImGui::TextWrapped("ONにすると、地形や機体の境界で雲がにじみにくくなります。");
    if (diagnosticDisableCloudComposite_ || diagnosticDisableDepthAwareUpsample_) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "影診断で雲合成またはDepth-aware Upsampleが一時無効です。");
    }
    ImGui::SliderFloat("深度差しきい値 (Depth Threshold)", &settings_.depthThreshold_, 0.0001f, 0.05f, "%.4f");
    ImGui::TextWrapped("小さいほど境界を厳しく判定します。にじむ場合は下げ、ガタつく場合は上げます。");
    ImGui::SliderFloat("視線方向ステップ倍率 (View Step Scale)", &settings_.viewStepScale_, 0.25f, 1.5f, "%.2f");
    ImGui::SliderFloat("光方向ステップ倍率 (Light Step Scale)", &settings_.lightStepScale_, 0.25f, 1.5f, "%.2f");
    ImGui::SliderInt("雲の更新間隔 (Cloud Render Interval)", &settings_.cloudRenderInterval_, 1, 6);
    ImGui::TextWrapped("1は毎フレーム更新、2は2フレームに1回更新です。大きいほど軽くなりますが、カメラ移動時にカクつきやすくなります。");

    const char* debugViewNames[] = { "Final", "Alpha only", "Density only", "Light only", "Far Cloud only", "Volumetric only", "Noise / UV", "Cloud Sea only" };
    int debugView = static_cast<int>(settings_.debugViewMode_);
    if (ImGui::Combo("雲のデバッグ表示 (Cloud Debug Mode)", &debugView, debugViewNames, IM_ARRAYSIZE(debugViewNames))) {
        settings_.debugViewMode_ = static_cast<DebugViewMode>(debugView);
    }

    if (settings_.cloudResolutionScale_ <= 0.125f) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "注意: 0.125はかなり軽いですが、雲がぼやけやすいです。");
    }
    if (settings_.cloudRenderInterval_ >= 2) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "注意: Render Intervalが2以上だと、カメラ移動中に雲がカクつく可能性があります。");
    }
    if (!settings_.useLowResolutionCloud_) {
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.35f, 1.0f), "注意: Direct描画は画質比較用です。重くなる可能性があります。");
    }

    ImGui::SeparatorText("雲の流れ方向 (Cloud Flow Direction)");
    ImGui::TextWrapped("奥から手前へ流したい場合は Toward Camera を使ってください。見た目が逆に流れる場合は Invert Flow Direction をONにしてください。");
    ImGui::Checkbox("雲を流す (Enable Cloud Flow)", &settings_.enableCloudFlow_);
    const char* flowModeNames[] = {
        "固定方向 (Fixed)",
        "カメラへ向かう (Toward Camera)",
        "カメラから遠ざかる (Away From Camera)",
        "Camera Forward",
        "Negative Camera Forward",
    };
    int flowMode = static_cast<int>(settings_.cloudFlowDirectionMode_);
    if (ImGui::Combo("流れ方向モード (Flow Direction Mode)", &flowMode, flowModeNames, IM_ARRAYSIZE(flowModeNames))) {
        settings_.cloudFlowDirectionMode_ = static_cast<CloudFlowDirectionMode>(flowMode);
    }
    ImGui::Checkbox("流れ方向を反転 (Invert Flow Direction)", &settings_.invertCloudFlowDirection_);
    if (settings_.cloudFlowDirectionMode_ == CloudFlowDirectionMode::Fixed) {
        ImGui::DragFloat3("固定流れ方向 (Flow Direction)", &settings_.fixedCloudFlowDirection_.x, 0.01f, -10.0f, 10.0f, "%.2f");
    }
    if (gameplayFlowActive_) ImGui::Text("Gameplay基準流速: %.1f（飛行調整で変更、上限120）", GetCloudBaseFlowSpeed());
    else ImGui::DragFloat("基本流速 (Base Flow Speed)", &settings_.cloudBaseFlowSpeed_, .1f, 0, 120, "%.1f");
    ImGui::TextWrapped("Base Flow Speed は雲が奥から手前へ流れる基本速度です。Boost中は通常速度に追加速度が足されます。");
    ImGui::Checkbox("Boost連動を使う (Use Boost Flow Extra Speed)", &settings_.useBoostFlowMultiplier_);
    ImGui::Text("旧Boost倍率互換値 (Legacy External Boost Multiplier): %.2f", externalFlowMultiplier_);
    ImGui::Text("基準流速: %.2f / 倍率 %.2f / 追加速度 %.2f", GetCloudBaseFlowSpeed(), externalFlowMultiplier_, externalBoostExtraFlowSpeed_);
    ImGui::Text("Cloud Boost Extra Speed: %.2f", settings_.useBoostFlowMultiplier_ ? externalBoostExtraFlowSpeed_ : 0.0f);
    ImGui::Text("現在の流速 (Cloud Flow Current Speed): %.2f", currentCloudFlowSpeed_);
    ImGui::Text("Cloud Flow Phase / Current Speed Segment: %.3f", cloudFlowPhase_);
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
    ImGui::TextWrapped("Boost連動ONでは、基本流速にBoostControllerの追加速度を足します。通常時は+0、Boost中は最大追加速度へ近づきます。");

    ImGui::SeparatorText("カメラ相対の雲範囲 (Camera Relative Cloud Volume)");
    ImGui::TextWrapped("Cloud Near Distanceを負にすると、カメラの少し後ろまで雲を描けます。Y方向は雲レイヤー設定でカメラより上へ逃がし、初期状態では雲の下で戦えるようにします。");
    if (ImGui::Button("雲の下で戦う 改良版")) {
        settings_.ApplyUnderCloudPreset();
    }
    ImGui::SameLine();
    if (ImGui::Button("雲海強め")) {
        if (settings_.ApplyCloudSeaPreset()) hasValidCloudBuffer_ = false;
    }
    if (ImGui::Button("雲の中を突っ切る用")) {
        settings_.ApplyFlyThroughPreset();
    }
    ImGui::Checkbox("カメラ相対雲ボリュームを使う (Use Camera Relative Volume)", &settings_.useCameraRelativeCloudVolume_);
    ImGui::DragFloat("雲の開始距離 (Cloud Near Distance)", &settings_.cloudNearDistance_, 0.5f, -100.0f, 100.0f, "%.1f");
    ImGui::DragFloat("雲の奥行き距離 (Cloud Far Distance)", &settings_.cloudFarDistance_, 1.0f, 1.0f, 1000.0f, "%.1f");
    ImGui::DragFloat("カメラ後方まで描画する距離 (Behind Camera Distance)", &settings_.cloudBehindCameraDistance_, 0.5f, 0.0f, 100.0f, "%.1f");
    ImGui::DragFloat("雲の高さオフセット (Cloud Height Offset)", &settings_.cloudHeightOffset_, 0.25f, -100.0f, 100.0f, "%.1f");
    ImGui::DragFloat("雲の幅 (Cloud Volume Width)", &settings_.cloudVolumeWidth_, 1.0f, 1.0f, 1000.0f, "%.1f");
    ImGui::DragFloat("雲の高さ (Cloud Volume Height)", &settings_.cloudVolumeHeight_, 1.0f, 1.0f, 500.0f, "%.1f");
    ImGui::DragFloat("雲の奥行き (Cloud Volume Depth)", &settings_.cloudVolumeDepth_, 1.0f, 1.0f, 1000.0f, "%.1f");

    ImGui::SeparatorText("雲レイヤー設定 (Cloud Layer)");
    ImGui::TextWrapped("初期状態では「カメラを雲の下に置く」をONにしてください。雲の底面が画面上側や奥側に見え、プレイヤーや弾が雲に埋もれにくくなります。");
    ImGui::Checkbox("カメラを雲の下に置く (Keep Camera Below Clouds)", &settings_.keepCameraBelowClouds_);
    ImGui::DragFloat("カメラから雲底までの距離 (Camera To Cloud Bottom)", &settings_.cameraToCloudBottom_, 0.25f, 0.0f, 200.0f, "%.1f");
    ImGui::TextWrapped("Camera To Cloud Bottom を下げると、雲が画面内に入りやすくなります。雲の下で戦う場合は14〜18付近が目安です。下げすぎると開幕から雲の中になります。");
    ImGui::DragFloat("雲レイヤー厚み (Cloud Layer Thickness)", &settings_.cloudLayerThickness_, 1.0f, 1.0f, 300.0f, "%.1f");
    ImGui::DragFloat("雲の底面フェード (Cloud Bottom Fade)", &settings_.cloudBottomFade_, 0.5f, 0.1f, 100.0f, "%.1f");
    ImGui::DragFloat("雲の上面フェード (Cloud Top Fade)", &settings_.cloudTopFade_, 0.5f, 0.1f, 100.0f, "%.1f");
    const float cloudBottomOffset = settings_.keepCameraBelowClouds_ ? (settings_.cameraToCloudBottom_ + settings_.cloudHeightOffset_) : (settings_.cloudHeightOffset_ - settings_.cloudVolumeHeight_ * 0.5f);
    const float cloudTopOffset = settings_.keepCameraBelowClouds_ ? (settings_.cameraToCloudBottom_ + settings_.cloudHeightOffset_ + settings_.cloudLayerThickness_) : (settings_.cloudHeightOffset_ + settings_.cloudVolumeHeight_ * 0.5f);
    ImGui::Text("雲の底面高さ (Cloud Bottom Offset): %.1f", cloudBottomOffset);
    ImGui::Text("雲の上面高さ (Cloud Top Offset): %.1f", cloudTopOffset);

    ImGui::Checkbox("雲底の形を整える (Enable Cloud Bottom Shaping)", &settings_.enableCloudBottomShaping_);
    ImGui::SliderFloat("雲底の平らさ (Cloud Bottom Flatten Strength)", &settings_.cloudBottomFlattenStrength_, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("雲底のなめらかさ (Cloud Bottom Smoothness)", &settings_.cloudBottomSmoothness_, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("雲底のノイズ抑制 (Cloud Bottom Noise Suppression)", &settings_.cloudBottomNoiseSuppression_, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("雲底の濃さ (Cloud Bottom Density)", &settings_.cloudBottomDensity_, 0.0f, 1.5f, "%.2f");
    ImGui::SliderFloat("雲底のうねり (Cloud Bottom Undulation Strength)", &settings_.cloudBottomUndulationStrength_, 0.0f, 24.0f, "%.1f");
    ImGui::DragFloat("雲底のうねりスケール (Cloud Bottom Undulation Scale)", &settings_.cloudBottomUndulationScale_, 0.001f, 0.001f, 0.1f, "%.3f");
    ImGui::SliderFloat("雲境界の柔らかさ (Cloud Boundary Softness)", &settings_.cloudBoundarySoftness_, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("雲底付近のディテール (Cloud Detail Noise Near Bottom)", &settings_.cloudDetailNoiseNearBottom_, 0.0f, 1.0f, "%.2f");
    ImGui::TextWrapped("Bottom Undulationは雲底を少し上下させ、断面っぽさを減らします。Bottom Flattenを上げすぎると雲底が平らに見えます。");
    ImGui::Checkbox("Volume端フェードを使う (Enable Volume Edge Fade)", &settings_.enableVolumeEdgeFade_);
    ImGui::DragFloat("Volume端フェード距離 (Volume Edge Fade Distance)", &settings_.volumeEdgeFadeDistance_, 0.5f, 0.1f, 100.0f, "%.1f");

    ImGui::SeparatorText("近距離フェード (Near Camera Fade)");
    ImGui::TextWrapped("近くが白くなりすぎる場合はNear Fadeを強めてください。カメラすぐ近くを薄くし、少し先で通常濃度へ戻します。");
    ImGui::Checkbox("近距離フェードを使う (Enable Near Camera Fade)", &settings_.enableNearCameraCloudFade_);
    ImGui::DragFloat("近距離フェード開始 (Near Fade Start)", &settings_.nearFadeStart_, 0.1f, 0.0f, 100.0f, "%.1f");
    ImGui::DragFloat("近距離フェード終了 (Near Fade End)", &settings_.nearFadeEnd_, 0.1f, 0.1f, 200.0f, "%.1f");
    ImGui::SliderFloat("近距離濃度倍率 (Near Density Scale)", &settings_.nearDensityScale_, 0.0f, 1.0f, "%.2f");

    DrawQualityImGuiControls();
    ImGui::Checkbox("雲バッファのプレビュー表示 (Show Cloud Buffer Preview)", &settings_.showCloudBufferPreview_);
    if (settings_.showCloudBufferPreview_ && cloudColorResource_) {
        const float previewWidth = 240.0f;
        const float aspect = (cloudBufferWidth_ > 0) ? (static_cast<float>(cloudBufferHeight_) / static_cast<float>(cloudBufferWidth_)) : 0.5625f;
        ImGui::Image(static_cast<ImTextureID>(cloudColorSRVGPU_.ptr), ImVec2(previewWidth, previewWidth * aspect), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
    }
#endif
}
