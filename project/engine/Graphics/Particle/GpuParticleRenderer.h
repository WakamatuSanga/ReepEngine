#pragma once

#include <cstdint>
#include <d3d12.h>
#include <string>
#include <unordered_map>
#include <wrl.h>

class Camera;
class DirectXCommon;
class GpuParticleResources;
namespace GpuParticle {
struct ParticleDebugInfo;
struct ParticleType;
struct PerView;
struct State;
}

class GpuParticleRenderer {
public:
	bool Initialize(DirectXCommon* dxCommon);
	void RefreshParticleTypeTextures(GpuParticle::State& state);
	void ReloadParticleTypeTextures(GpuParticle::State& state);
	void UpdateView(const Camera* camera);
	void Draw(ID3D12GraphicsCommandList* commandList, GpuParticleResources& resources, const GpuParticle::State& state);
	uint32_t GetTextureIndex() const { return particleTextureIndex_; }
	uint32_t GetFallbackTextureDescriptorIndex() const { return fallbackTextureDescriptorIndex_; }
	bool IsUsingFallbackTexture(const GpuParticle::ParticleType& particleType) const;
	bool IsParticleBlendEnabled() const { return particleBlendEnabled_; }
	D3D12_BLEND GetParticleSrcBlend() const { return particleSrcBlend_; }
	D3D12_BLEND GetParticleDestBlend() const { return particleDestBlend_; }
	D3D12_BLEND GetParticleSrcBlendAlpha() const { return particleSrcBlendAlpha_; }
	D3D12_BLEND GetParticleDestBlendAlpha() const { return particleDestBlendAlpha_; }

private:
	bool CreateRootSignature();
	bool CreatePipelineState();
	uint32_t ResolveParticleTextureDescriptorIndex(const std::string& texturePath);
	uint32_t GetDescriptorIndex(D3D12_GPU_DESCRIPTOR_HANDLE handle) const;

	DirectXCommon* dxCommon_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;
	GpuParticle::PerView* perViewData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> particleDebugResource_;
	GpuParticle::ParticleDebugInfo* particleDebugData_ = nullptr;
	uint32_t particleTextureIndex_ = 0;
	uint32_t fallbackTextureDescriptorIndex_ = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE particleTextureTableHandle_{};
	std::unordered_map<std::string, uint32_t> textureDescriptorIndexCache_;
	bool particleBlendEnabled_ = true;
	D3D12_BLEND particleSrcBlend_ = D3D12_BLEND_SRC_ALPHA;
	D3D12_BLEND particleDestBlend_ = D3D12_BLEND_INV_SRC_ALPHA;
	D3D12_BLEND particleSrcBlendAlpha_ = D3D12_BLEND_ONE;
	D3D12_BLEND particleDestBlendAlpha_ = D3D12_BLEND_ZERO;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> drawRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> drawPipelineState_;
};
