#include "AimCorridorVisualRenderer.h"

#include "Engine/Core/DirectXCommon.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Utility/Logger.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <d3dcompiler.h>
#include <filesystem>

namespace {
    uint32_t AlignConstantBufferSize(uint32_t size) {
        return (size + 0xffu) & ~0xffu;
    }

    Vector3 Add(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 Scale(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    bool IsDrawableFrame(const AimCorridorVisualRenderer::FrameDraw& frame) {
        return std::isfinite(frame.width) && std::isfinite(frame.height) &&
            frame.width > 0.001f && frame.height > 0.001f && frame.alpha > 0.001f;
    }
}

AimCorridorVisualRenderer::~AimCorridorVisualRenderer() {
    Finalize();
}

bool AimCorridorVisualRenderer::Initialize(
    DirectXCommon* dxCommon,
    const std::string& nearTexturePath,
    const std::string& farTexturePath) {
    Finalize();
    dxCommon_ = dxCommon;
    if (!dxCommon_) {
        return false;
    }

    // Draw order is fixed as far (slot 0), then near (slot 1).
    LoadTexture(0, farTexturePath);
    LoadTexture(1, nearTexturePath);
    initialized_ = CreateBuffers() && CreateRootSignature() && CreatePipelineState();
    if (!initialized_) {
        Logger::Log("[AimCorridor] Renderer initialization failed.\n");
    }
    return initialized_;
}

void AimCorridorVisualRenderer::Finalize() {
    initialized_ = false;
    vertexData_ = nullptr;
    constantData_ = nullptr;
    vertexBufferView_ = {};
    constantStride_ = 0;
    textures_ = {};
    constantResource_.Reset();
    vertexResource_.Reset();
    pipelineState_.Reset();
    rootSignature_.Reset();
    dxCommon_ = nullptr;
}

uint32_t AimCorridorVisualRenderer::Draw(
    const Camera* camera,
    const FrameDraw& farFrame,
    const FrameDraw& nearFrame,
    bool drawFar,
    bool drawNear,
    bool disableGlow,
    bool showCoreOnly) {
    if (!initialized_ || !dxCommon_ || !camera || !vertexData_ || !constantData_) {
        return 0;
    }

    const std::array<FrameDraw, kFrameCount> frames = { farFrame, nearFrame };
    const std::array<bool, kFrameCount> drawFrames = { drawFar, drawNear };
    for (uint32_t i = 0; i < kFrameCount; ++i) {
        BuildFrameVertices(i, frames[i]);
        WriteConstants(i, *camera, frames[i], disableGlow, showCoreOnly);
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    if (!commandList) {
        return 0;
    }

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    uint32_t drawCount = 0;
    for (uint32_t i = 0; i < kFrameCount; ++i) {
        if (!drawFrames[i] || !textures_[i].loaded || !IsDrawableFrame(frames[i])) {
            continue;
        }
        commandList->SetGraphicsRootConstantBufferView(
            0,
            constantResource_->GetGPUVirtualAddress() + static_cast<UINT64>(constantStride_) * i);
        commandList->SetGraphicsRootDescriptorTable(1, textures_[i].srvHandle);
        commandList->DrawInstanced(kVerticesPerFrame, 1, i * kVerticesPerFrame, 0);
        ++drawCount;
    }
    return drawCount;
}

bool AimCorridorVisualRenderer::LoadTexture(uint32_t frameIndex, const std::string& texturePath) {
    if (frameIndex >= textures_.size() || texturePath.empty() || !std::filesystem::exists(texturePath)) {
        Logger::Log("[AimCorridor] Texture not found: " + texturePath + "\n");
        return false;
    }

    TextureManager* textureManager = TextureManager::GetInstance();
    textureManager->LoadTexture(texturePath);
    const uint32_t textureIndex = textureManager->GetTextureIndexByFilePath(texturePath);
    const DirectX::TexMetadata& metadata = textureManager->GetMetaData(textureIndex);
    if (metadata.width == 0 || metadata.height == 0) {
        Logger::Log("[AimCorridor] Texture has invalid dimensions: " + texturePath + "\n");
        return false;
    }

    TextureState& state = textures_[frameIndex];
    state.srvHandle = textureManager->GetSrvHandleGPU(textureIndex);
    state.inverseWidth = 1.0f / static_cast<float>(metadata.width);
    state.inverseHeight = 1.0f / static_cast<float>(metadata.height);
    state.aspectRatio = static_cast<float>(metadata.width) / static_cast<float>(metadata.height);
    state.loaded = state.srvHandle.ptr != 0;
    return state.loaded;
}

bool AimCorridorVisualRenderer::CreateRootSignature() {
    D3D12_DESCRIPTOR_RANGE descriptorRange{};
    descriptorRange.BaseShaderRegister = 0;
    descriptorRange.NumDescriptors = 1;
    descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    std::array<D3D12_ROOT_PARAMETER, 2> rootParameters{};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &descriptorRange;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    desc.NumParameters = static_cast<UINT>(rootParameters.size());
    desc.pParameters = rootParameters.data();
    desc.NumStaticSamplers = 1;
    desc.pStaticSamplers = &sampler;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            Logger::Log(static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_));
    return SUCCEEDED(hr);
}

bool AimCorridorVisualRenderer::CreatePipelineState() {
    constexpr wchar_t kVertexShaderPath[] = L"resources/shaders/AimCorridor.VS.hlsl";
    constexpr wchar_t kPixelShaderPath[] = L"resources/shaders/AimCorridor.PS.hlsl";
    if (!std::filesystem::exists(kVertexShaderPath) || !std::filesystem::exists(kPixelShaderPath)) {
        Logger::Log("[AimCorridor] Shader file is missing.\n");
        return false;
    }

    Microsoft::WRL::ComPtr<IDxcBlob> vertexShader = dxCommon_->CompileShader(kVertexShaderPath, L"vs_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShader = dxCommon_->CompileShader(kPixelShaderPath, L"ps_6_0");
    if (!vertexShader || !pixelShader) {
        return false;
    }

    const D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature_.Get();
    desc.InputLayout = { inputElements, _countof(inputElements) };
    desc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
    desc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RasterizerState.DepthClipEnable = TRUE;
    D3D12_RENDER_TARGET_BLEND_DESC& blend = desc.BlendState.RenderTarget[0];
    blend.BlendEnable = TRUE;
    blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.DestBlend = D3D12_BLEND_ONE;
    blend.BlendOp = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleDesc.Count = 1;

    return SUCCEEDED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(&pipelineState_)));
}

bool AimCorridorVisualRenderer::CreateBuffers() {
    constexpr uint32_t kVertexCount = kFrameCount * kVerticesPerFrame;
    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(Vertex) * kVertexCount);
    if (!vertexResource_ || FAILED(vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_)))) {
        return false;
    }
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(Vertex) * kVertexCount;
    vertexBufferView_.StrideInBytes = sizeof(Vertex);

    static_assert(sizeof(Constants) % 16 == 0, "Aim Corridor constants must stay 16-byte aligned");
    constantStride_ = AlignConstantBufferSize(sizeof(Constants));
    constantResource_ = dxCommon_->CreateBufferResource(constantStride_ * kFrameCount);
    if (!constantResource_ || FAILED(constantResource_->Map(0, nullptr, reinterpret_cast<void**>(&constantData_)))) {
        return false;
    }
    return true;
}

void AimCorridorVisualRenderer::BuildFrameVertices(uint32_t frameIndex, const FrameDraw& frame) {
    Vertex* vertices = vertexData_ + frameIndex * kVerticesPerFrame;
    const Vector3 halfRight = Scale(frame.right, frame.width * 0.5f);
    const Vector3 halfUp = Scale(frame.up, frame.height * 0.5f);
    const Vector3 leftBottom = Add(Add(frame.center, Scale(halfRight, -1.0f)), Scale(halfUp, -1.0f));
    const Vector3 leftTop = Add(Add(frame.center, Scale(halfRight, -1.0f)), halfUp);
    const Vector3 rightTop = Add(Add(frame.center, halfRight), halfUp);
    const Vector3 rightBottom = Add(Add(frame.center, halfRight), Scale(halfUp, -1.0f));

    vertices[0] = { leftBottom, { 0.0f, 1.0f } };
    vertices[1] = { leftTop, { 0.0f, 0.0f } };
    vertices[2] = { rightTop, { 1.0f, 0.0f } };
    vertices[3] = { leftBottom, { 0.0f, 1.0f } };
    vertices[4] = { rightTop, { 1.0f, 0.0f } };
    vertices[5] = { rightBottom, { 1.0f, 1.0f } };
}

void AimCorridorVisualRenderer::WriteConstants(
    uint32_t frameIndex,
    const Camera& camera,
    const FrameDraw& frame,
    bool disableGlow,
    bool showCoreOnly) {
    Constants constants{};
    constants.viewProjection = camera.GetViewProjectionMatrix();
    constants.appearance = {
        std::clamp(frame.alpha, 0.0f, 1.0f),
        std::clamp(frame.coreIntensity, 0.0f, 2.0f),
        std::clamp(frame.glowIntensity, 0.0f, 2.0f),
        std::clamp(frame.glowAlpha, 0.0f, 1.0f),
    };
    constants.sampling = {
        textures_[frameIndex].inverseWidth,
        textures_[frameIndex].inverseHeight,
        std::clamp(frame.glowRadiusTexels, 0.0f, 4.0f),
        std::clamp(frame.pulseScale, 0.5f, 1.5f),
    };
    constants.flags = {
        disableGlow ? 1.0f : 0.0f,
        showCoreOnly ? 1.0f : 0.0f,
        0.0f,
        0.0f,
    };
    constants.coreTint = {
        std::clamp(frame.coreTint.x, 0.0f, 1.0f),
        std::clamp(frame.coreTint.y, 0.0f, 1.0f),
        std::clamp(frame.coreTint.z, 0.0f, 1.0f),
        std::clamp(frame.tintAmount, 0.0f, 1.0f),
    };
    constants.glowTint = {
        std::clamp(frame.glowTint.x, 0.0f, 1.0f),
        std::clamp(frame.glowTint.y, 0.0f, 1.0f),
        std::clamp(frame.glowTint.z, 0.0f, 1.0f),
        0.0f,
    };
    std::memcpy(constantData_ + static_cast<size_t>(constantStride_) * frameIndex, &constants, sizeof(constants));
}
