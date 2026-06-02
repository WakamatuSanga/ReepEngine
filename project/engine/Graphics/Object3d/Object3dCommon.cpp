#include "Object3dCommon.h"
#include <cassert>
#include "Engine/Utility/Logger.h"

using namespace Microsoft::WRL;

void Object3dCommon::Initialize(DirectXCommon* dxCommon)
{
    assert(dxCommon);
    dxCommon_ = dxCommon;

    // パイプライン生成
    CreateGraphicsPipelineStates();
}

void Object3dCommon::CreateRootSignature()
{
    HRESULT hr = S_OK;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    D3D12_STATIC_SAMPLER_DESC staticSamplers[1]{};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_PARAMETER rootParameters[10]{};

    // [0] Pixel CBV : Material(b0)
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // [1] Vertex CBV : TransformationMatrix(b0)
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].Descriptor.ShaderRegister = 0;

    // [2] Pixel SRV : Texture(t0)
    D3D12_DESCRIPTOR_RANGE descriptorRange[3]{};
    descriptorRange[0].BaseShaderRegister = 0;
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

    // [3] Pixel CBV : DirectionalLight(b1)
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister = 1;

    // [4] Pixel CBV : EnvironmentMapData(b2)
    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[4].Descriptor.ShaderRegister = 2;

    // [5] Pixel SRV : EnvironmentTexture(t1)
    descriptorRange[1].BaseShaderRegister = 1;
    descriptorRange[1].NumDescriptors = 1;
    descriptorRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[5].DescriptorTable.pDescriptorRanges = &descriptorRange[1];
    rootParameters[5].DescriptorTable.NumDescriptorRanges = 1;

    // [6] Pixel CBV : DissolveData(b3)
    rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[6].Descriptor.ShaderRegister = 3;

    // [7] Pixel SRV : DissolveMaskTexture(t2)
    descriptorRange[2].BaseShaderRegister = 2;
    descriptorRange[2].NumDescriptors = 1;
    descriptorRange[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[7].DescriptorTable.pDescriptorRanges = &descriptorRange[2];
    rootParameters[7].DescriptorTable.NumDescriptorRanges = 1;

    // [8] Pixel CBV : RandomNoiseData(b4)
    rootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[8].Descriptor.ShaderRegister = 4;

    // [9] Pixel CBV : RingAppearanceData(b5)
    rootParameters[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[9].Descriptor.ShaderRegister = 5;

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeRootSignature(
        &descriptionRootSignature,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);
    if (FAILED(hr)) {
        Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        assert(false);
    }

    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

void Object3dCommon::CreateGraphicsPipelineStates()
{
    HRESULT hr = S_OK;

    // RootSignature生成
    CreateRootSignature();

    // シェーダーコンパイル
    auto vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Object3d.VS.hlsl", L"vs_6_0");
    auto pixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Object3d.PS.hlsl", L"ps_6_0");

    // InputLayout
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3]{};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    // Rasterizer (★ここを NONE にしないと、裏面や板ポリが消えることがあります)
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // カリングなしに変更
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    // DepthStencil
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // PSO ベース設定
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
    graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    graphicsPipelineStateDesc.NumRenderTargets = 2;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    graphicsPipelineStateDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // --- 各ブレンドモードの生成 ---
    graphicsPipelineStates_.resize((size_t)BlendMode::kCountOf);

    // Helper lambda
    auto CreatePSO = [&](BlendMode mode, const D3D12_BLEND_DESC& blendDesc) {
        graphicsPipelineStateDesc.BlendState = blendDesc;
        graphicsPipelineStateDesc.BlendState.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        graphicsPipelineStateDesc.BlendState.RenderTarget[1].BlendEnable = FALSE;
        hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
            &graphicsPipelineStateDesc,
            IID_PPV_ARGS(&graphicsPipelineStates_[(size_t)mode]));
        assert(SUCCEEDED(hr));
        };

    // kNormal
    {
        D3D12_BLEND_DESC b{};
        b.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        b.RenderTarget[0].BlendEnable = TRUE;
        b.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        b.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        b.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        b.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        b.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        b.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        CreatePSO(BlendMode::kNormal, b);
    }
    // kAdd
    {
        D3D12_BLEND_DESC b{};
        b.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        b.RenderTarget[0].BlendEnable = TRUE;
        b.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        b.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
        b.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        b.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        b.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        b.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        CreatePSO(BlendMode::kAdd, b);
    }
    // kSubtract
    {
        D3D12_BLEND_DESC b{};
        b.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        b.RenderTarget[0].BlendEnable = TRUE;
        b.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        b.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
        b.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
        b.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        b.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        b.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        CreatePSO(BlendMode::kSubtract, b);
    }
    // kMultiply
    {
        D3D12_BLEND_DESC b{};
        b.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        b.RenderTarget[0].BlendEnable = TRUE;
        b.RenderTarget[0].SrcBlend = D3D12_BLEND_ZERO;
        b.RenderTarget[0].DestBlend = D3D12_BLEND_SRC_COLOR;
        b.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        b.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
        b.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        b.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        CreatePSO(BlendMode::kMultiply, b);
    }
    // kScreen
    {
        D3D12_BLEND_DESC b{};
        b.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        b.RenderTarget[0].BlendEnable = TRUE;
        b.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
        b.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
        b.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        b.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        b.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        b.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        CreatePSO(BlendMode::kScreen, b);
    }
    // kNone
    {
        D3D12_BLEND_DESC b{};
        b.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        b.RenderTarget[0].BlendEnable = FALSE;
        CreatePSO(BlendMode::kNone, b);
    }
}

void Object3dCommon::CommonDrawSetting(BlendMode blendMode)
{
    // ルートシグネチャをセット
    dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
    // 指定されたブレンドモードのPSOをセット
    dxCommon_->GetCommandList()->SetPipelineState(graphicsPipelineStates_[(size_t)blendMode].Get());
    // プリミティブトポロジーをセット
    dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
