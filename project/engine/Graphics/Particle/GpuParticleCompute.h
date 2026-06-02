#pragma once

#include <d3d12.h>
#include <wrl.h>

class DirectXCommon;
class GpuParticleResources;
namespace GpuParticle {
struct EmitterInfo;
struct InitializeInfo;
struct RecycleInfo;
struct State;
struct UpdateInfo;
}

class GpuParticleCompute {
public:
	bool Initialize(DirectXCommon* dxCommon, const GpuParticle::State& state);
	void Dispatch(ID3D12GraphicsCommandList* commandList, GpuParticleResources& resources, GpuParticle::State& state);

private:
	bool CreateConstantBuffers(const GpuParticle::State& state);
	bool CreateInitializeFreeListRootSignature();
	bool CreateInitializeRootSignature();
	bool CreateEmitRootSignature();
	bool CreateUpdateRootSignature();
	bool CreateRecycleRootSignature();
	bool CreatePipelineStates();
	void DispatchInitializeFreeListIfNeeded(ID3D12GraphicsCommandList* commandList, GpuParticleResources& resources, GpuParticle::State& state);
	void ResetDeadListCounterIfNeeded(ID3D12GraphicsCommandList* commandList, GpuParticleResources& resources, GpuParticle::State& state);
	void DispatchInitializeIfNeeded(ID3D12GraphicsCommandList* commandList, GpuParticleResources& resources, GpuParticle::State& state);
	void DispatchEmitIfNeeded(ID3D12GraphicsCommandList* commandList, GpuParticleResources& resources, GpuParticle::State& state);
	void DispatchUpdate(ID3D12GraphicsCommandList* commandList, GpuParticleResources& resources, GpuParticle::State& state);
	void DispatchRecycleIfNeeded(ID3D12GraphicsCommandList* commandList, GpuParticleResources& resources, GpuParticle::State& state);
	void UpdateFreeListEstimate(GpuParticle::State& state, float deltaTime);

	DirectXCommon* dxCommon_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> initializeInfoResource_;
	GpuParticle::InitializeInfo* initializeInfoData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> updateInfoResource_;
	GpuParticle::UpdateInfo* updateInfoData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> emitterInfoResource_;
	GpuParticle::EmitterInfo* emitterInfoData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> recycleInfoResource_;
	GpuParticle::RecycleInfo* recycleInfoData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> initializeFreeListRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> initializeFreeListPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> initializeRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> initializePipelineState_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> emitRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> emitPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> updateRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> updatePipelineState_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> recycleRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> recyclePipelineState_;
};
