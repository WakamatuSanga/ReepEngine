#include "PlayerJetExhaustBeamRenderer.h"

#include "Engine/Core/DirectXCommon.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Utility/Logger.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <d3dcompiler.h>

namespace {
    constexpr size_t kInitialVertexCapacity = 32;

    size_t AlignConstantBufferSize(size_t size) {
        return (size + 0xff) & ~static_cast<size_t>(0xff);
    }
}

bool PlayerJetExhaustBeamRenderer::Initialize(DirectXCommon* dxCommon) {
    assert(dxCommon);
    dxCommon_ = dxCommon;
    constantResource_ = dxCommon_->CreateBufferResource(AlignConstantBufferSize(sizeof(Constants)));
    constantResource_->Map(0, nullptr, reinterpret_cast<void**>(&constantData_));
    return EnsureVertexCapacity(kInitialVertexCapacity) && CreateRootSignature() && CreatePipelineState();
}

void PlayerJetExhaustBeamRenderer::Draw(
    const std::vector<Vertex>& vertices,
    const Camera* camera,
    float brightness,
    float flickerStrength,
    float time,
    uint32_t mode) {
    if (!dxCommon_ || !camera || vertices.empty() || !constantData_ || !EnsureVertexCapacity(vertices.size())) {
        return;
    }

    std::memcpy(vertexData_, vertices.data(), sizeof(Vertex) * vertices.size());
    vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(Vertex) * vertices.size());

    constantData_->viewProjection = camera->GetViewProjectionMatrix();
    constantData_->params = {
        std::clamp(brightness, 0.0f, 8.0f),
        std::clamp(flickerStrength, 0.0f, 1.0f),
        time,
        static_cast<float>(mode),
    };

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->SetGraphicsRootConstantBufferView(0, constantResource_->GetGPUVirtualAddress());
    commandList->DrawInstanced(static_cast<UINT>(vertices.size()), 1, 0, 0);
}

bool PlayerJetExhaustBeamRenderer::CreateRootSignature() {
    D3D12_ROOT_PARAMETER rootParameter{};
    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameter.Descriptor.ShaderRegister = 0;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    desc.pParameters = &rootParameter;
    desc.NumParameters = 1;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
        return false;
    }

    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
    return SUCCEEDED(hr);
}

bool PlayerJetExhaustBeamRenderer::CreatePipelineState() {
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/PlayerJetExhaustBeam.VS.hlsl", L"vs_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/PlayerJetExhaustBeam.PS.hlsl", L"ps_6_0");
    assert(vertexShaderBlob);
    assert(pixelShaderBlob);

    D3D12_INPUT_ELEMENT_DESC inputElements[2]{};
    inputElements[0].SemanticName = "POSITION";
    inputElements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElements[1].SemanticName = "TEXCOORD";
    inputElements[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature_.Get();
    desc.InputLayout = { inputElements, _countof(inputElements) };
    desc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    desc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.BlendState.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.DepthStencilState.DepthEnable = TRUE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    desc.NumRenderTargets = 2;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
    return SUCCEEDED(hr);
}

bool PlayerJetExhaustBeamRenderer::EnsureVertexCapacity(size_t vertexCount) {
    if (vertexCount == 0) {
        return true;
    }
    if (vertexCapacity_ >= vertexCount && vertexResource_) {
        return true;
    }

    vertexCapacity_ = (std::max)(vertexCount, kInitialVertexCapacity);
    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(Vertex) * vertexCapacity_);
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(Vertex) * vertexCapacity_);
    vertexBufferView_.StrideInBytes = sizeof(Vertex);
    return vertexData_ != nullptr;
}
