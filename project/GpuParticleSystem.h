#pragma once

#include "Matrix4x4.h"

#include <cstdint>
#include <d3d12.h>
#include <vector>
#include <wrl.h>

class Camera;
class DirectXCommon;
class SrvManager;

class GpuParticleSystem {
public:
	bool Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void Update(const Camera* camera);
	void Draw();
	void DrawImGui();

private:
	static constexpr uint32_t kParticleCount = 1024;

	struct Particle {
		Vector3 translate;
		float scale;
		Vector3 velocity;
		float currentTime;
		Vector4 color;
		float lifeTime;
		uint32_t alive;
		float padding[2];
	};

	struct InitializeInfo {
		uint32_t particleCount;
		uint32_t randomEnabled;
		uint32_t randomSeed;
		uint32_t emitterEnabled;
		Vector3 emitterPosition;
		float emitterRadius;
		uint32_t emitCount;
		float padding[3];
	};

	struct UpdateInfo {
		uint32_t particleCount;
		float deltaTime;
		uint32_t freeListEnabled;
		uint32_t deadListEnabled;
	};

	struct EmitterInfo {
		uint32_t emitCount;
		uint32_t randomSeed;
		uint32_t randomEnabled;
		uint32_t padding0;
		Vector3 emitterPosition;
		float emitterRadius;
	};

	struct RecycleInfo {
		uint32_t recycleCount;
		float padding[3];
	};

	struct PerView {
		Matrix4x4 viewProjection;
		Matrix4x4 billboardMatrix;
	};

	struct EmitBatchEstimate {
		uint32_t particleCount = 0;
		float elapsedTime = 0.0f;
		float returnDelay = 0.0f;
	};

	bool CreateResources();
	bool CreateFreeListResources();
	bool CreateDeadListResources();
	bool CreateInitializeFreeListRootSignature();
	bool CreateInitializeFreeListPipelineState();
	bool CreateInitializeRootSignature();
	bool CreateInitializePipelineState();
	bool CreateEmitRootSignature();
	bool CreateEmitPipelineState();
	bool CreateUpdateRootSignature();
	bool CreateUpdatePipelineState();
	bool CreateRecycleRootSignature();
	bool CreateRecyclePipelineState();
	bool CreateDrawRootSignature();
	bool CreateDrawPipelineState();
	void DispatchInitializeFreeListIfNeeded(ID3D12GraphicsCommandList* commandList);
	void DispatchInitializeIfNeeded(ID3D12GraphicsCommandList* commandList);
	void DispatchEmitIfNeeded(ID3D12GraphicsCommandList* commandList);
	void DispatchUpdate(ID3D12GraphicsCommandList* commandList);
	void DispatchRecycleIfNeeded(ID3D12GraphicsCommandList* commandList);
	void TransitionParticleResource(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES afterState);
	void TransitionFreeListCounterResource(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES afterState);
	void TransitionDeadListCounterResource(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES afterState);
	void ResetFreeListCounter(ID3D12GraphicsCommandList* commandList);
	void ResetDeadListCounter(ID3D12GraphicsCommandList* commandList);
	void ResetDeadListCounterIfNeeded(ID3D12GraphicsCommandList* commandList);
	void RequestInitialize();
	void RequestEmit();
	void RequestRecycle();
	void UpdateFreeListEstimate(float deltaTime);
	uint32_t EstimateActiveParticleCount() const;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	bool isEnabled_ = true;
	bool isUpdateEnabled_ = true;
	bool isRandomInitializeEnabled_ = true;
	bool isEmitterEnabled_ = true;
	bool useFreeListEmit_ = false;
	bool useDeadList_ = false;
	bool autoRecycleDeadList_ = false;
	bool isInitialized_ = false;
	bool needsInitializeDispatch_ = true;
	bool needsInitializeFreeListDispatch_ = true;
	bool needsDeadListCounterReset_ = true;
	bool needsEmitDispatch_ = false;
	bool needsRecycleDispatch_ = false;
	bool isFreeListInitialized_ = false;
	bool isDeadListReady_ = false;
	float deltaTime_ = 1.0f / 60.0f;
	float elapsedTimeSinceInitialize_ = 0.0f;
	float emitInterval_ = 1.5f;
	float emitTimer_ = 0.0f;
	uint32_t activeCountEstimate_ = kParticleCount;
	uint32_t freeListRemainingEstimate_ = 0;
	uint32_t deadListCountEstimate_ = 0;
	uint32_t lastEmitDispatchCount_ = 0;
	uint32_t recycleCount_ = kParticleCount;
	uint32_t lastRecycleDispatchCount_ = 0;
	uint32_t randomSeed_ = 1;
	uint32_t emitCount_ = 256;
	Vector3 emitterPosition_ = { 0.0f, 1.5f, 3.0f };
	float emitterRadius_ = 0.8f;
	std::vector<EmitBatchEstimate> emitBatchEstimates_;

	Microsoft::WRL::ComPtr<ID3D12Resource> particleResource_;
	D3D12_RESOURCE_STATES particleResourceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	uint32_t particleSrvIndex_ = 0;
	uint32_t particleUavIndex_ = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE particleSrvHandle_{};
	D3D12_GPU_DESCRIPTOR_HANDLE particleUavHandle_{};

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

	Microsoft::WRL::ComPtr<ID3D12Resource> initializeInfoResource_;
	InitializeInfo* initializeInfoData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> updateInfoResource_;
	UpdateInfo* updateInfoData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> emitterInfoResource_;
	EmitterInfo* emitterInfoData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> recycleInfoResource_;
	RecycleInfo* recycleInfoData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;
	PerView* perViewData_ = nullptr;

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
	Microsoft::WRL::ComPtr<ID3D12RootSignature> drawRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> drawPipelineState_;
};
