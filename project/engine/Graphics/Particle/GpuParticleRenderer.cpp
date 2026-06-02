#include "GpuParticleRenderer.h"

#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Core/DirectXCommon.h"
#include "GpuParticleResources.h"
#include "GpuParticleTypes.h"
#include "Engine/Utility/Logger.h"
#include "Engine/Graphics/Texture/TextureManager.h"

#include <cassert>
#include <algorithm>
#include <cctype>
#include <d3dcompiler.h>
#include <filesystem>
#include "externals/DirectXTex/DirectXTex.h"

namespace {

bool CanLoadTextureFile(const std::string& texturePath) {
	if (texturePath.empty() || !std::filesystem::exists(texturePath)) {
		return false;
	}

	std::wstring widePath = std::filesystem::path(texturePath).wstring();
	std::string extension = std::filesystem::path(texturePath).extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(),
		[](unsigned char value) { return static_cast<char>(std::tolower(value)); });

	DirectX::TexMetadata metadata{};
	DirectX::ScratchImage image{};
	if (extension == ".dds") {
		return SUCCEEDED(DirectX::LoadFromDDSFile(widePath.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, image));
	}
	return SUCCEEDED(DirectX::LoadFromWICFile(widePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, &metadata, image));
}

}

bool GpuParticleRenderer::Initialize(DirectXCommon* dxCommon) {
	assert(dxCommon);
	dxCommon_ = dxCommon;
	perViewResource_ = dxCommon_->CreateBufferResource(GpuParticle::AlignConstantBufferSize(sizeof(GpuParticle::PerView)));
	perViewResource_->Map(0, nullptr, reinterpret_cast<void**>(&perViewData_));
	particleDebugResource_ = dxCommon_->CreateBufferResource(GpuParticle::AlignConstantBufferSize(sizeof(GpuParticle::ParticleDebugInfo)));
	particleDebugResource_->Map(0, nullptr, reinterpret_cast<void**>(&particleDebugData_));

	TextureManager::GetInstance()->LoadTexture(GpuParticle::kFallbackParticleTexturePath);
	particleTextureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(GpuParticle::kFallbackParticleTexturePath);
	fallbackTextureDescriptorIndex_ = GetDescriptorIndex(TextureManager::GetInstance()->GetSrvHandleGPU(particleTextureIndex_));
	particleTextureTableHandle_ = dxCommon_->GetSRVGPUDescriptorHandle(0);
	textureDescriptorIndexCache_[GpuParticle::kFallbackParticleTexturePath] = fallbackTextureDescriptorIndex_;
	return CreateRootSignature() && CreatePipelineState();
}

void GpuParticleRenderer::RefreshParticleTypeTextures(GpuParticle::State& state) {
	for (GpuParticle::ParticleType& particleType : state.particleTypes) {
		particleType.textureIndex = static_cast<int>(ResolveParticleTextureDescriptorIndex(particleType.texturePath));
	}
}

void GpuParticleRenderer::ReloadParticleTypeTextures(GpuParticle::State& state) {
	textureDescriptorIndexCache_.clear();
	textureDescriptorIndexCache_[GpuParticle::kFallbackParticleTexturePath] = fallbackTextureDescriptorIndex_;
	RefreshParticleTypeTextures(state);
}

bool GpuParticleRenderer::IsUsingFallbackTexture(const GpuParticle::ParticleType& particleType) const {
	return particleType.texturePath.empty() || static_cast<uint32_t>((std::max)(particleType.textureIndex, 0)) == fallbackTextureDescriptorIndex_;
}

void GpuParticleRenderer::UpdateView(const Camera* camera) {
	if (!camera || !perViewData_) {
		return;
	}

	Matrix4x4 billboardMatrix = camera->GetWorldMatrix();
	billboardMatrix.m[3][0] = 0.0f;
	billboardMatrix.m[3][1] = 0.0f;
	billboardMatrix.m[3][2] = 0.0f;
	perViewData_->billboardMatrix = billboardMatrix;
	perViewData_->viewProjection = camera->GetViewProjectionMatrix();
}

void GpuParticleRenderer::Draw(ID3D12GraphicsCommandList* commandList, GpuParticleResources& resources, const GpuParticle::State& state) {
	if (particleDebugData_) {
		particleDebugData_->debugViewMode = state.particleDebugViewMode;
	}

	resources.TransitionParticleResource(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->SetGraphicsRootSignature(drawRootSignature_.Get());
	commandList->SetPipelineState(drawPipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(0, perViewResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(1, resources.GetParticleSrvHandle());
	commandList->SetGraphicsRootDescriptorTable(2, resources.GetParticleTypeSrvHandle());
	commandList->SetGraphicsRootDescriptorTable(3, particleTextureTableHandle_);
	commandList->SetGraphicsRootConstantBufferView(4, particleDebugResource_->GetGPUVirtualAddress());
	commandList->DrawInstanced(6, GpuParticle::kParticleCount, 0, 0);
}

bool GpuParticleRenderer::CreateRootSignature() {
	D3D12_DESCRIPTOR_RANGE srvRanges[3]{};
	for (uint32_t index = 0; index < _countof(srvRanges); ++index) {
		srvRanges[index].BaseShaderRegister = index;
		srvRanges[index].NumDescriptors = 1;
		srvRanges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRanges[index].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	}
	srvRanges[2].BaseShaderRegister = 2;
	srvRanges[2].NumDescriptors = GpuParticle::kMaxParticleTextureDescriptors;

	D3D12_ROOT_PARAMETER rootParameters[5]{};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	for (uint32_t parameterIndex = 1; parameterIndex <= 3; ++parameterIndex) {
		rootParameters[parameterIndex].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[parameterIndex].ShaderVisibility = parameterIndex == 3 ? D3D12_SHADER_VISIBILITY_PIXEL : D3D12_SHADER_VISIBILITY_VERTEX;
		rootParameters[parameterIndex].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[parameterIndex].DescriptorTable.pDescriptorRanges = &srvRanges[parameterIndex - 1];
	}
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[4].Descriptor.ShaderRegister = 1;

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
		IID_PPV_ARGS(&drawRootSignature_));
	assert(SUCCEEDED(hr));
	return SUCCEEDED(hr);
}

uint32_t GpuParticleRenderer::ResolveParticleTextureDescriptorIndex(const std::string& texturePath) {
	const std::string resolvedPath = texturePath.empty() ? GpuParticle::kFallbackParticleTexturePath : texturePath;
	const auto cachedIt = textureDescriptorIndexCache_.find(resolvedPath);
	if (cachedIt != textureDescriptorIndexCache_.end()) {
		return cachedIt->second;
	}

	if (resolvedPath != GpuParticle::kFallbackParticleTexturePath && !CanLoadTextureFile(resolvedPath)) {
		return fallbackTextureDescriptorIndex_;
	}

	TextureManager::GetInstance()->LoadTexture(resolvedPath);
	const uint32_t textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(resolvedPath);
	const uint32_t descriptorIndex = GetDescriptorIndex(TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex));
	textureDescriptorIndexCache_[resolvedPath] = descriptorIndex;
	return descriptorIndex;
}

uint32_t GpuParticleRenderer::GetDescriptorIndex(D3D12_GPU_DESCRIPTOR_HANDLE handle) const {
	const D3D12_GPU_DESCRIPTOR_HANDLE baseHandle = dxCommon_->GetSRVGPUDescriptorHandle(0);
	const UINT descriptorSize = dxCommon_->GetSrvDescriptorSize();
	if (handle.ptr < baseHandle.ptr || descriptorSize == 0) {
		return 0;
	}

	const uint64_t offset = handle.ptr - baseHandle.ptr;
	const uint32_t descriptorIndex = static_cast<uint32_t>(offset / descriptorSize);
	return descriptorIndex < GpuParticle::kMaxParticleTextureDescriptors ? descriptorIndex : fallbackTextureDescriptorIndex_;
}

bool GpuParticleRenderer::CreatePipelineState() {
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/GpuParticle.VS.hlsl", L"vs_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/GpuParticle.PS.hlsl", L"ps_6_0");
	assert(vertexShaderBlob);
	assert(pixelShaderBlob);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = drawRootSignature_.Get();
	desc.InputLayout = { nullptr, 0 };
	desc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	desc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
	desc.BlendState.RenderTarget[0].BlendEnable = particleBlendEnabled_ ? TRUE : FALSE;
	desc.BlendState.RenderTarget[0].SrcBlend = particleSrcBlend_;
	desc.BlendState.RenderTarget[0].DestBlend = particleDestBlend_;
	desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	desc.BlendState.RenderTarget[0].SrcBlendAlpha = particleSrcBlendAlpha_;
	desc.BlendState.RenderTarget[0].DestBlendAlpha = particleDestBlendAlpha_;
	desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
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

	HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&drawPipelineState_));
	assert(SUCCEEDED(hr));
	return SUCCEEDED(hr);
}
