#include "ImpactDistortionController.h"

#include "Engine/Core/DirectXCommon.h"
#include "Engine/Core/SrvManager.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Utility/Logger.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <d3dcompiler.h>

using Microsoft::WRL::ComPtr;

namespace {
    constexpr float kClipMargin = 0.25f;

    size_t AlignConstantBufferSize(size_t size) {
        return (size + 0xff) & ~static_cast<size_t>(0xff);
    }

    Vector4 TransformPoint(const Vector3& value, const Matrix4x4& matrix) {
        return {
            value.x * matrix.m[0][0] + value.y * matrix.m[1][0] + value.z * matrix.m[2][0] + matrix.m[3][0],
            value.x * matrix.m[0][1] + value.y * matrix.m[1][1] + value.z * matrix.m[2][1] + matrix.m[3][1],
            value.x * matrix.m[0][2] + value.y * matrix.m[1][2] + value.z * matrix.m[2][2] + matrix.m[3][2],
            value.x * matrix.m[0][3] + value.y * matrix.m[1][3] + value.z * matrix.m[2][3] + matrix.m[3][3],
        };
    }

    Vector3 NormalizeOr(const Vector3& value, const Vector3& fallback) {
        const float lengthSq = value.x * value.x + value.y * value.y + value.z * value.z;
        if (lengthSq <= 0.000001f || !std::isfinite(lengthSq)) {
            return fallback;
        }
        const float invLength = 1.0f / std::sqrt(lengthSq);
        return { value.x * invLength, value.y * invLength, value.z * invLength };
    }

    Vector3 GetCameraForward(const Camera* camera) {
        if (!camera) {
            return { 0.0f, 0.0f, 1.0f };
        }
        const Matrix4x4& matrix = camera->GetWorldMatrix();
        return NormalizeOr({ matrix.m[2][0], matrix.m[2][1], matrix.m[2][2] }, { 0.0f, 0.0f, 1.0f });
    }

    Vector3 Add(const Vector3& a, const Vector3& b) {
        return { a.x + b.x, a.y + b.y, a.z + b.z };
    }

    Vector3 Scale(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    Vector3 Subtract(const Vector3& a, const Vector3& b) {
        return { a.x - b.x, a.y - b.y, a.z - b.z };
    }

    float Dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    float Lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }

    float ClampScale(float value, float minValue, float maxValue) {
        if (!std::isfinite(value)) {
            return 1.0f;
        }
        return std::clamp(value, minValue, maxValue);
    }

    float SmoothStep(float value) {
        const float t = std::clamp(value, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    void TransitionResource(
        ID3D12GraphicsCommandList* commandList,
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES before,
        D3D12_RESOURCE_STATES after) {
        if (!commandList || !resource || before == after) {
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

ImpactDistortionController::ImpactDistortionController() = default;

ImpactDistortionController::~ImpactDistortionController() = default;

void ImpactDistortionController::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const Camera* camera) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    camera_ = camera;
    instances_.resize(kMaxGpuInstances);

    enemyDefeatPreset_.lifetime = 0.30f;
    enemyDefeatPreset_.startRadius = 0.020f;
    enemyDefeatPreset_.endRadius = 0.150f;
    enemyDefeatPreset_.distortionStrength = 0.040f;
    enemyDefeatPreset_.ringThickness = 0.030f;
    enemyDefeatPreset_.ringStrength = 0.55f;
    enemyDefeatPreset_.chromaticStrength = 0.010f;
    enemyDefeatPreset_.flashStrength = 0.35f;
    enemyDefeatPreset_.maxSpawnPerFrame = 4;

    CreatePipeline();
    CreateConstantBuffer();
}

void ImpactDistortionController::Finalize() {
    if (constantBuffer_) {
        constantBuffer_->Unmap(0, nullptr);
    }
    constantData_ = nullptr;
    constantBuffer_.Reset();
    sourceCopyResource_.Reset();
    pipelineState_.Reset();
    rootSignature_.Reset();
    dxCommon_ = nullptr;
    srvManager_ = nullptr;
    camera_ = nullptr;
}

void ImpactDistortionController::BeginFrame() {
    bulletCancelSpawnedThisFrame_ = 0;
    enemyDefeatSpawnedThisFrame_ = 0;
}

void ImpactDistortionController::Update(float deltaTime) {
    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    activeInstanceCount_ = 0;
    for (Instance& instance : instances_) {
        if (!instance.active) {
            continue;
        }
        instance.elapsed += safeDeltaTime;
        if (instance.elapsed >= (std::max)(instance.preset.lifetime, 0.001f)) {
            instance.active = false;
            continue;
        }
        ++activeInstanceCount_;
    }
}

void ImpactDistortionController::Draw() {
    if (!enabled_ || activeInstanceCount_ <= 0 || !dxCommon_ || !rootSignature_ || !pipelineState_) {
        return;
    }

    EnsureResources();
    if (!sourceCopyResource_ || !constantBuffer_ || !constantData_) {
        return;
    }

    CopySceneToSource();
    UpdateConstantBuffer();

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    D3D12_CPU_DESCRIPTOR_HANDLE renderRTV = dxCommon_->GetRenderTextureRTV();
    const uint32_t renderWidth = (std::max)(1u, dxCommon_->GetRenderTextureWidth());
    const uint32_t renderHeight = (std::max)(1u, dxCommon_->GetRenderTextureHeight());

    D3D12_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(renderWidth);
    viewport.Height = static_cast<float>(renderHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissor{};
    scissor.right = static_cast<LONG>(renderWidth);
    scissor.bottom = static_cast<LONG>(renderHeight);

    ID3D12DescriptorHeap* descriptorHeaps[] = { dxCommon_->GetSrvDescriptorHeap() };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
    commandList->OMSetRenderTargets(1, &renderRTV, FALSE, nullptr);
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->SetGraphicsRootDescriptorTable(0, srvManager_->GetGPUDescriptorHandle(sourceCopySrvIndex_));
    commandList->SetGraphicsRootConstantBufferView(1, constantBuffer_->GetGPUVirtualAddress());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);

    TransitionResource(commandList, sourceCopyResource_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    sourceCopyState_ = D3D12_RESOURCE_STATE_COPY_DEST;
}

void ImpactDistortionController::TriggerBulletCancel(const Vector3& worldPosition) {
    Trigger(TriggerType::BulletCancel, worldPosition);
}

void ImpactDistortionController::TriggerEnemyDefeat(const Vector3& worldPosition) {
    Trigger(TriggerType::EnemyDefeat, worldPosition);
}

bool ImpactDistortionController::ProjectWorldToScreenUv(const Vector3& worldPosition, Vector2& screenUv) const {
    if (!camera_) {
        return false;
    }

    const Vector4 clip = TransformPoint(worldPosition, camera_->GetViewProjectionMatrix());
    if (clip.w <= 0.0001f || !std::isfinite(clip.w)) {
        return false;
    }

    const float invW = 1.0f / clip.w;
    const float ndcX = clip.x * invW;
    const float ndcY = clip.y * invW;
    if (!std::isfinite(ndcX) || !std::isfinite(ndcY)) {
        return false;
    }

    screenUv.x = ndcX * 0.5f + 0.5f;
    screenUv.y = -ndcY * 0.5f + 0.5f;
    return screenUv.x >= -kClipMargin && screenUv.x <= 1.0f + kClipMargin &&
        screenUv.y >= -kClipMargin && screenUv.y <= 1.0f + kClipMargin;
}

bool ImpactDistortionController::CalculateCameraDepth(const Vector3& worldPosition, float& cameraDepth) const {
    if (!camera_) {
        return false;
    }

    const Vector3 toImpact = Subtract(worldPosition, camera_->GetTranslate());
    cameraDepth = Dot(toImpact, GetCameraForward(camera_));
    return std::isfinite(cameraDepth);
}

ImpactDistortionController::ComputedDistanceScale ImpactDistortionController::CalculateDistanceScale(TriggerType type, float cameraDepth) const {
    ComputedDistanceScale scale{};
    if (!enableDistanceBasedScale_) {
        return scale;
    }
    if (type == TriggerType::BulletCancel && !applyDistanceScaleToBulletCancel_) {
        return scale;
    }

    const float nearDepth = distanceScaleSettings_.nearDepth;
    const float farDepth = (std::max)(distanceScaleSettings_.farDepth, nearDepth + 0.001f);
    const float t = std::clamp((cameraDepth - nearDepth) / (farDepth - nearDepth), 0.0f, 1.0f);
    const float bulletBlend = (type == TriggerType::BulletCancel)
        ? std::clamp(bulletCancelDistanceScaleStrength_, 0.0f, 1.0f)
        : 1.0f;

    auto blendForType = [bulletBlend](float value) {
        return Lerp(1.0f, value, bulletBlend);
    };

    scale.radius = ClampScale(blendForType(Lerp(distanceScaleSettings_.nearRadiusScale, distanceScaleSettings_.farRadiusScale, t)), 0.60f, 1.30f);
    scale.strength = ClampScale(blendForType(Lerp(distanceScaleSettings_.nearStrengthScale, distanceScaleSettings_.farStrengthScale, t)), 0.60f, 1.20f);
    scale.chromatic = ClampScale(blendForType(Lerp(distanceScaleSettings_.nearChromaticScale, distanceScaleSettings_.farChromaticScale, t)), 0.50f, 1.20f);
    scale.ring = ClampScale(blendForType(Lerp(distanceScaleSettings_.nearRingScale, distanceScaleSettings_.farRingScale, t)), 0.60f, 1.20f);
    return scale;
}

void ImpactDistortionController::ApplyDistanceScale(EffectPreset& preset, const ComputedDistanceScale& scale) const {
    preset.startRadius *= scale.radius;
    preset.endRadius *= scale.radius;
    preset.ringThickness *= scale.radius;
    preset.distortionStrength *= scale.strength;
    preset.flashStrength *= scale.strength;
    preset.chromaticStrength *= scale.chromatic;
    preset.ringStrength *= scale.ring;
}

void ImpactDistortionController::Trigger(TriggerType type, const Vector3& worldPosition) {
    lastTriggerType_ = type;
    lastWorldPosition_ = worldPosition;
    lastComputedRadiusScale_ = 1.0f;
    lastComputedStrengthScale_ = 1.0f;
    lastComputedChromaticScale_ = 1.0f;
    lastComputedRingScale_ = 1.0f;
    if (!enabled_) {
        lastResult_ = "Skipped: disabled";
        return;
    }

    EffectPreset preset = (type == TriggerType::EnemyDefeat) ? enemyDefeatPreset_ : bulletCancelPreset_;
    int& spawnedThisFrame = (type == TriggerType::EnemyDefeat) ? enemyDefeatSpawnedThisFrame_ : bulletCancelSpawnedThisFrame_;
    if (spawnedThisFrame >= (std::max)(0, preset.maxSpawnPerFrame)) {
        ++droppedInstanceCount_;
        lastResult_ = "Dropped: max spawn per frame";
        return;
    }

    float cameraDepth = 0.0f;
    if (!CalculateCameraDepth(worldPosition, cameraDepth) || cameraDepth <= 0.0001f) {
        ++droppedInstanceCount_;
        lastImpactDepth_ = cameraDepth;
        lastResult_ = "Dropped: impact is behind camera";
        return;
    }
    lastImpactDepth_ = cameraDepth;

    Vector2 screenUv{};
    if (!ProjectWorldToScreenUv(worldPosition, screenUv)) {
        ++droppedInstanceCount_;
        lastScreenUv_ = screenUv;
        lastResult_ = "Dropped: world position is outside screen";
        return;
    }

    const ComputedDistanceScale distanceScale = CalculateDistanceScale(type, cameraDepth);
    ApplyDistanceScale(preset, distanceScale);
    lastComputedRadiusScale_ = distanceScale.radius;
    lastComputedStrengthScale_ = distanceScale.strength;
    lastComputedChromaticScale_ = distanceScale.chromatic;
    lastComputedRingScale_ = distanceScale.ring;

    const int instanceLimit = std::clamp(maxActiveInstances_, 1, static_cast<int>(kMaxGpuInstances));
    Instance* freeSlot = nullptr;
    for (int i = 0; i < instanceLimit; ++i) {
        if (!instances_[static_cast<size_t>(i)].active) {
            freeSlot = &instances_[static_cast<size_t>(i)];
            break;
        }
    }
    if (!freeSlot) {
        ++droppedInstanceCount_;
        lastScreenUv_ = screenUv;
        lastResult_ = "Dropped: active instance slots are full";
        return;
    }

    freeSlot->type = type;
    freeSlot->worldPosition = worldPosition;
    freeSlot->screenUv = screenUv;
    freeSlot->cameraDepth = cameraDepth;
    freeSlot->elapsed = 0.0f;
    freeSlot->preset = preset;
    freeSlot->active = true;
    ++spawnedThisFrame;
    lastScreenUv_ = screenUv;
    lastResult_ = std::string("Spawned ") + GetTriggerTypeName(type);
}

void ImpactDistortionController::EnsureResources() {
    if (!dxCommon_) {
        return;
    }
    const uint32_t width = (std::max)(1u, dxCommon_->GetRenderTextureWidth());
    const uint32_t height = (std::max)(1u, dxCommon_->GetRenderTextureHeight());
    if (sourceCopyResource_ && sourceCopyWidth_ == width && sourceCopyHeight_ == height) {
        return;
    }
    CreateSourceCopy(width, height);
}

void ImpactDistortionController::CreatePipeline() {
    if (!dxCommon_ || !dxCommon_->GetDevice()) {
        lastResult_ = "Pipeline skipped: DirectXCommon missing";
        return;
    }

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

    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    rootDesc.pParameters = rootParameters;
    rootDesc.NumParameters = _countof(rootParameters);
    rootDesc.pStaticSamplers = &sampler;
    rootDesc.NumStaticSamplers = 1;

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            Logger::Log(reinterpret_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        lastResult_ = "Failed to serialize root signature";
        return;
    }
    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_));
    if (FAILED(hr)) {
        lastResult_ = "Failed to create root signature";
        return;
    }

    ComPtr<IDxcBlob> vs = dxCommon_->CompileShader(L"resources/shaders/CopyImage.VS.hlsl", L"vs_6_0");
    ComPtr<IDxcBlob> ps = dxCommon_->CompileShader(L"resources/shaders/ImpactDistortion.PS.hlsl", L"ps_6_0");
    if (!vs || !ps) {
        lastResult_ = "Failed to compile impact distortion shader";
        return;
    }

    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = FALSE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
    pipelineDesc.pRootSignature = rootSignature_.Get();
    pipelineDesc.InputLayout = { nullptr, 0 };
    pipelineDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    pipelineDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    pipelineDesc.BlendState = blendDesc;
    pipelineDesc.RasterizerState = rasterizerDesc;
    pipelineDesc.DepthStencilState = depthStencilDesc;
    pipelineDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    pipelineDesc.NumRenderTargets = 1;
    pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineDesc.SampleDesc.Count = 1;
    pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&pipelineState_));
    if (FAILED(hr)) {
        lastResult_ = "Failed to create graphics pipeline state";
    }
}

void ImpactDistortionController::CreateSourceCopy(uint32_t width, uint32_t height) {
    if (!dxCommon_ || !srvManager_ || !dxCommon_->GetRenderTextureResource()) {
        return;
    }

    ID3D12Device* device = dxCommon_->GetDevice();
    D3D12_RESOURCE_DESC desc = dxCommon_->GetRenderTextureResource()->GetDesc();
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    sourceCopyResource_.Reset();
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&sourceCopyResource_));
    if (FAILED(hr) || !sourceCopyResource_) {
        lastResult_ = "Failed to create source copy texture";
        return;
    }

    if (!sourceCopySrvAllocated_) {
        if (!srvManager_->CanAllocate()) {
            sourceCopyResource_.Reset();
            lastResult_ = "Failed to allocate source copy SRV";
            return;
        }
        sourceCopySrvIndex_ = srvManager_->Allocate();
        sourceCopySrvAllocated_ = true;
    }
    srvManager_->CreateSRVforTexture2D(sourceCopySrvIndex_, sourceCopyResource_.Get(), desc.Format, desc.MipLevels);
    sourceCopyState_ = D3D12_RESOURCE_STATE_COPY_DEST;
    sourceCopyWidth_ = width;
    sourceCopyHeight_ = height;
}

void ImpactDistortionController::CreateConstantBuffer() {
    if (!dxCommon_) {
        return;
    }
    constantBuffer_ = dxCommon_->CreateBufferResource(AlignConstantBufferSize(sizeof(GpuConstants)));
    if (!constantBuffer_) {
        lastResult_ = "Failed to create constant buffer";
        return;
    }
    HRESULT hr = constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantData_));
    if (FAILED(hr)) {
        constantData_ = nullptr;
        lastResult_ = "Failed to map constant buffer";
    }
}

void ImpactDistortionController::UpdateConstantBuffer() {
    if (!constantData_ || !dxCommon_) {
        return;
    }

    std::memset(constantData_, 0, sizeof(GpuConstants));
    constantData_->screenSizeAndOptions = {
        static_cast<float>((std::max)(1u, dxCommon_->GetRenderTextureWidth())),
        static_cast<float>((std::max)(1u, dxCommon_->GetRenderTextureHeight())),
        enabled_ ? 1.0f : 0.0f,
        showScreenPositionMarker_ ? 1.0f : 0.0f
    };

    float disableFlags = 0.0f;
    disableFlags += disableChromatic_ ? 1.0f : 0.0f;
    disableFlags += disableRingHighlight_ ? 2.0f : 0.0f;
    disableFlags += disableFlash_ ? 4.0f : 0.0f;
    constantData_->commonParams = {
        0.0f,
        forceStrongDistortion_ ? 1.0f : 0.0f,
        disableFlags,
        static_cast<float>(qualityMode_)
    };

    uint32_t gpuIndex = 0;
    const uint32_t limit = static_cast<uint32_t>(std::clamp(maxActiveInstances_, 1, static_cast<int>(kMaxGpuInstances)));
    for (uint32_t i = 0; i < limit && gpuIndex < kMaxGpuInstances; ++i) {
        const Instance& instance = instances_[i];
        if (!instance.active) {
            continue;
        }

        const float lifetime = (std::max)(instance.preset.lifetime, 0.001f);
        const float normalizedAge = std::clamp(instance.elapsed / lifetime, 0.0f, 1.0f);
        const float easedAge = SmoothStep(normalizedAge);
        const float radius = instance.preset.startRadius + (instance.preset.endRadius - instance.preset.startRadius) * easedAge;
        GpuInstance& gpu = constantData_->instances[gpuIndex];
        gpu.data0 = { instance.screenUv.x, instance.screenUv.y, radius, normalizedAge };
        gpu.data1 = {
            instance.preset.distortionStrength,
            instance.preset.ringThickness,
            instance.preset.ringStrength,
            instance.preset.chromaticStrength
        };
        gpu.data2 = {
            instance.preset.flashStrength,
            instance.type == TriggerType::EnemyDefeat ? 1.0f : 0.0f,
            0.0f,
            0.0f
        };
        ++gpuIndex;
    }
    constantData_->commonParams[0] = static_cast<float>(gpuIndex);
}

void ImpactDistortionController::CopySceneToSource() {
    if (!dxCommon_ || !sourceCopyResource_ || !dxCommon_->GetRenderTextureResource()) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    ID3D12Resource* sceneResource = dxCommon_->GetRenderTextureResource();
    TransitionResource(commandList, sceneResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
    TransitionResource(commandList, sourceCopyResource_.Get(), sourceCopyState_, D3D12_RESOURCE_STATE_COPY_DEST);
    sourceCopyState_ = D3D12_RESOURCE_STATE_COPY_DEST;
    commandList->CopyResource(sourceCopyResource_.Get(), sceneResource);
    TransitionResource(commandList, sceneResource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    TransitionResource(commandList, sourceCopyResource_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    sourceCopyState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

const char* ImpactDistortionController::GetTriggerTypeName(TriggerType type) const {
    switch (type) {
    case TriggerType::EnemyDefeat:
        return "EnemyDefeat";
    case TriggerType::BulletCancel:
    default:
        return "BulletCancel";
    }
}


