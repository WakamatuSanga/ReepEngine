#pragma once

#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

class DirectXCommon;
class SrvManager;
namespace GpuParticle {
struct ParticleTypeForGPU;
struct State;
}

class GpuParticleResources {
public:
	bool Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const GpuParticle::State& state);
	void UploadParticleTypes(const GpuParticle::State& state);

	void TransitionParticleResource(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES afterState);
	void ResetFreeListCounter(ID3D12GraphicsCommandList* commandList);
	void ResetDeadListCounter(ID3D12GraphicsCommandList* commandList);
	void CopyCounterResourcesForReadback(ID3D12GraphicsCommandList* commandList);
	void RefreshCounterReadbackValues(GpuParticle::State& state) const;

	D3D12_GPU_DESCRIPTOR_HANDLE GetParticleSrvHandle() const { return particleSrvHandle_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetParticleUavHandle() const { return particleUavHandle_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetParticleTypeSrvHandle() const { return particleTypeSrvHandle_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetFreeListUavHandle() const { return freeListUavHandle_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetDeadListUavHandle() const { return deadListUavHandle_; }
	uint32_t GetParticleSrvIndex() const { return particleSrvIndex_; }
	uint32_t GetParticleUavIndex() const { return particleUavIndex_; }
	uint32_t GetParticleTypeSrvIndex() const { return particleTypeSrvIndex_; }
	uint32_t GetFreeListUavIndex() const { return freeListUavIndex_; }
	uint32_t GetDeadListUavIndex() const { return deadListUavIndex_; }
	bool HasFreeListCounter() const { return freeListCounterResource_ != nullptr; }
	bool HasDeadListCounter() const { return deadListCounterResource_ != nullptr; }

private:
	bool CreateParticleResources(const GpuParticle::State& state);
	bool CreateFreeListResources();
	bool CreateDeadListResources();
	bool CreateCounterReadbackResources();
	void TransitionCounterResource(
		ID3D12GraphicsCommandList* commandList,
		ID3D12Resource* resource,
		D3D12_RESOURCE_STATES& currentState,
		D3D12_RESOURCE_STATES afterState);

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> particleResource_;
	D3D12_RESOURCE_STATES particleResourceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	uint32_t particleSrvIndex_ = 0;
	uint32_t particleUavIndex_ = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE particleSrvHandle_{};
	D3D12_GPU_DESCRIPTOR_HANDLE particleUavHandle_{};

	Microsoft::WRL::ComPtr<ID3D12Resource> particleTypeResource_;
	GpuParticle::ParticleTypeForGPU* particleTypeData_ = nullptr;
	uint32_t particleTypeSrvIndex_ = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE particleTypeSrvHandle_{};

	Microsoft::WRL::ComPtr<ID3D12Resource> freeListResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> freeListCounterResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> freeListCounterResetResource_;
	D3D12_RESOURCE_STATES freeListCounterResourceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	uint32_t freeListUavIndex_ = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE freeListUavHandle_{};

	Microsoft::WRL::ComPtr<ID3D12Resource> deadListResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> deadListCounterResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> deadListCounterResetResource_;
	D3D12_RESOURCE_STATES deadListCounterResourceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	uint32_t deadListUavIndex_ = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE deadListUavHandle_{};

	Microsoft::WRL::ComPtr<ID3D12Resource> freeListCounterReadbackResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> deadListCounterReadbackResource_;
	uint32_t* freeListCounterReadbackData_ = nullptr;
	uint32_t* deadListCounterReadbackData_ = nullptr;
};
