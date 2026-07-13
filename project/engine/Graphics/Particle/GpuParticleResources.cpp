#include "GpuParticleResources.h"

#include "Engine/Core/DirectXCommon.h"
#include "GpuParticleTypes.h"
#include "Engine/Core/SrvManager.h"

#include <algorithm>
#include <cassert>

namespace {

Microsoft::WRL::ComPtr<ID3D12Resource> CreateUavBufferResource(ID3D12Device* device, size_t sizeInBytes) {
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));
	return resource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateReadbackBufferResource(ID3D12Device* device, size_t sizeInBytes) {
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_READBACK;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));
	return resource;
}

}

bool GpuParticleResources::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const GpuParticle::State& state) {
	assert(dxCommon);
	assert(srvManager);
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	return CreateParticleResources(state) && CreateFreeListResources() && CreateDeadListResources() && CreateCounterReadbackResources();
}

bool GpuParticleResources::CreateParticleResources(const GpuParticle::State& state) {
	ID3D12Device* device = dxCommon_->GetDevice();
	particleResource_ = CreateUavBufferResource(device, sizeof(GpuParticle::Particle) * GpuParticle::kParticleCount);
	particleResourceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	particleUavIndex_ = srvManager_->Allocate();
	srvManager_->CreateUAVforStructuredBuffer(particleUavIndex_, particleResource_.Get(), GpuParticle::kParticleCount, sizeof(GpuParticle::Particle));
	particleUavHandle_ = srvManager_->GetGPUDescriptorHandle(particleUavIndex_);

	particleSrvIndex_ = srvManager_->Allocate();
	srvManager_->CreateSRVforStructuredBuffer(particleSrvIndex_, particleResource_.Get(), GpuParticle::kParticleCount, sizeof(GpuParticle::Particle));
	particleSrvHandle_ = srvManager_->GetGPUDescriptorHandle(particleSrvIndex_);

	particleTypeResource_ = dxCommon_->CreateBufferResource(sizeof(GpuParticle::ParticleTypeForGPU) * GpuParticle::kMaxParticleTypes);
	particleTypeResource_->Map(0, nullptr, reinterpret_cast<void**>(&particleTypeData_));
	particleTypeSrvIndex_ = srvManager_->Allocate();
	srvManager_->CreateSRVforStructuredBuffer(
		particleTypeSrvIndex_,
		particleTypeResource_.Get(),
		GpuParticle::kMaxParticleTypes,
		sizeof(GpuParticle::ParticleTypeForGPU));
	particleTypeSrvHandle_ = srvManager_->GetGPUDescriptorHandle(particleTypeSrvIndex_);
	UploadParticleTypes(state);
	return true;
}

bool GpuParticleResources::CreateFreeListResources() {
	ID3D12Device* device = dxCommon_->GetDevice();
	freeListResource_ = CreateUavBufferResource(device, sizeof(uint32_t) * GpuParticle::kParticleCount);
	freeListCounterResource_ = CreateUavBufferResource(device, D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT);
	freeListCounterResourceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	freeListCounterResetResource_ = dxCommon_->CreateBufferResource(sizeof(uint32_t));
	uint32_t* resetData = nullptr;
	freeListCounterResetResource_->Map(0, nullptr, reinterpret_cast<void**>(&resetData));
	*resetData = 0u;

	freeListUavIndex_ = srvManager_->Allocate();
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.NumElements = GpuParticle::kParticleCount;
	uavDesc.Buffer.StructureByteStride = sizeof(uint32_t);
	device->CreateUnorderedAccessView(
		freeListResource_.Get(),
		freeListCounterResource_.Get(),
		&uavDesc,
		srvManager_->GetCPUDescriptorHandle(freeListUavIndex_));
	freeListUavHandle_ = srvManager_->GetGPUDescriptorHandle(freeListUavIndex_);
	return true;
}

bool GpuParticleResources::CreateDeadListResources() {
	ID3D12Device* device = dxCommon_->GetDevice();
	deadListResource_ = CreateUavBufferResource(device, sizeof(uint32_t) * GpuParticle::kParticleCount);
	deadListCounterResource_ = CreateUavBufferResource(device, D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT);
	deadListCounterResourceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	deadListCounterResetResource_ = dxCommon_->CreateBufferResource(sizeof(uint32_t));
	uint32_t* resetData = nullptr;
	deadListCounterResetResource_->Map(0, nullptr, reinterpret_cast<void**>(&resetData));
	*resetData = 0u;

	deadListUavIndex_ = srvManager_->Allocate();
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.NumElements = GpuParticle::kParticleCount;
	uavDesc.Buffer.StructureByteStride = sizeof(uint32_t);
	device->CreateUnorderedAccessView(
		deadListResource_.Get(),
		deadListCounterResource_.Get(),
		&uavDesc,
		srvManager_->GetCPUDescriptorHandle(deadListUavIndex_));
	deadListUavHandle_ = srvManager_->GetGPUDescriptorHandle(deadListUavIndex_);
	return true;
}

bool GpuParticleResources::CreateCounterReadbackResources() {
	ID3D12Device* device = dxCommon_->GetDevice();
	freeListCounterReadbackResource_ = CreateReadbackBufferResource(device, sizeof(uint32_t));
	deadListCounterReadbackResource_ = CreateReadbackBufferResource(device, sizeof(uint32_t));
	freeListCounterReadbackResource_->Map(0, nullptr, reinterpret_cast<void**>(&freeListCounterReadbackData_));
	deadListCounterReadbackResource_->Map(0, nullptr, reinterpret_cast<void**>(&deadListCounterReadbackData_));
	if (freeListCounterReadbackData_) {
		*freeListCounterReadbackData_ = 0u;
	}
	if (deadListCounterReadbackData_) {
		*deadListCounterReadbackData_ = 0u;
	}
	return true;
}

void GpuParticleResources::UploadParticleTypes(const GpuParticle::State& state) {
	if (!particleTypeData_ || state.particleTypes.empty()) {
		return;
	}

	for (uint32_t index = 0; index < GpuParticle::kMaxParticleTypes; ++index) {
		const GpuParticle::ParticleType& source = GpuParticle::GetParticleType(state, index);
		GpuParticle::ParticleTypeForGPU& destination = particleTypeData_[index];
		destination.baseColor = source.baseColor;
		destination.startColor = source.startColor;
		destination.endColor = source.endColor;
		destination.startScale = (std::max)(source.startScale, 0.001f);
		destination.endScale = (std::max)(source.endScale, 0.001f);
		destination.lifeTimeMin = (std::max)(source.lifeTimeMin, 0.01f);
		destination.lifeTimeMax = (std::max)(source.lifeTimeMax, destination.lifeTimeMin);
		destination.speedMin = (std::max)(source.speedMin, 0.0f);
		destination.speedMax = (std::max)(source.speedMax, destination.speedMin);
		destination.gravity = source.gravity;
		destination.drag = (std::max)(source.drag, 0.0f);
		destination.useAtlas = source.useAtlas ? 1u : 0u;
		destination.atlasRows = (std::max)(source.atlasRows, 1u);
		destination.atlasColumns = (std::max)(source.atlasColumns, 1u);
		const uint32_t atlasCapacity = destination.atlasRows * destination.atlasColumns;
		destination.frameCount = std::clamp(source.frameCount, 1u, atlasCapacity);
		destination.frameSpeed = (std::max)(source.frameSpeed, 0.0f);
		destination.loopAtlas = source.loopAtlas ? 1u : 0u;
		destination.textureIndex = static_cast<uint32_t>((std::max)(source.textureIndex, 0));
		destination.physicsFlags = (source.enablePhysics ? GpuParticle::kParticlePhysicsEnable : 0u) |
			(source.enablePlaneCollision ? GpuParticle::kParticlePhysicsPlaneCollision : 0u) |
			(source.killBelowPlane ? GpuParticle::kParticlePhysicsKillBelowPlane : 0u);
		destination.collisionPlaneY = source.collisionPlaneY;
		destination.restitution = std::clamp(source.restitution, 0.0f, 1.0f);
		destination.friction = std::clamp(source.friction, 0.0f, 1.0f);
		destination.bounceVelocityThreshold = (std::max)(source.bounceVelocityThreshold, 0.0f);
		destination.maxBounceCount = std::clamp(source.maxBounceCount, 0u, 255u);
		destination.collisionDamping = std::clamp(source.collisionDamping, 0.0f, 2.0f);
		destination.affectedByInfluenceField = source.affectedByInfluenceField ? 1u : 0u;
		destination.influenceResponseScale = std::clamp(source.influenceResponseScale, 0.0f, 10.0f);
		destination.affectedByRailFlow = source.affectedByRailFlow ? 1u : 0u;
		destination.railFlowScale = std::clamp(source.railFlowScale, 0.0f, 10.0f);
		destination.affectedByChargeGather = source.affectedByChargeGather ? 1u : 0u;
		destination.chargeGatherStrength = (std::max)(source.chargeGatherStrength, 0.0f);
		destination.chargeGatherKillRadius = (std::max)(source.chargeGatherKillRadius, 0.001f);
		destination.chargeGatherSwirlStrength = (std::max)(source.chargeGatherSwirlStrength, 0.0f);
		destination.chargeGatherResponseScale = std::clamp(source.chargeGatherResponseScale, 0.0f, 10.0f);
		destination.scaleByChargeRate = source.scaleByChargeRate ? 1u : 0u;
		destination.brightnessByChargeRate = source.brightnessByChargeRate ? 1u : 0u;
		destination.emissionByChargeRate = source.emissionByChargeRate ? 1u : 0u;
		destination.chargeGatherTargetMode = source.chargeGatherTargetMode;
		destination.chargeGatherTargetOffset = source.chargeGatherTargetOffset;
		destination.padding2[0] = 0.0f;
		destination.padding2[1] = 0.0f;
	}
}

void GpuParticleResources::TransitionParticleResource(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES afterState) {
	if (particleResourceState_ == afterState) {
		return;
	}

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = particleResource_.Get();
	barrier.Transition.StateBefore = particleResourceState_;
	barrier.Transition.StateAfter = afterState;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);
	particleResourceState_ = afterState;
}

void GpuParticleResources::TransitionCounterResource(
	ID3D12GraphicsCommandList* commandList,
	ID3D12Resource* resource,
	D3D12_RESOURCE_STATES& currentState,
	D3D12_RESOURCE_STATES afterState) {
	if (currentState == afterState) {
		return;
	}

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = resource;
	barrier.Transition.StateBefore = currentState;
	barrier.Transition.StateAfter = afterState;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);
	currentState = afterState;
}

void GpuParticleResources::ResetFreeListCounter(ID3D12GraphicsCommandList* commandList) {
	TransitionCounterResource(commandList, freeListCounterResource_.Get(), freeListCounterResourceState_, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->CopyBufferRegion(freeListCounterResource_.Get(), 0, freeListCounterResetResource_.Get(), 0, sizeof(uint32_t));
	TransitionCounterResource(commandList, freeListCounterResource_.Get(), freeListCounterResourceState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void GpuParticleResources::ResetDeadListCounter(ID3D12GraphicsCommandList* commandList) {
	TransitionCounterResource(commandList, deadListCounterResource_.Get(), deadListCounterResourceState_, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->CopyBufferRegion(deadListCounterResource_.Get(), 0, deadListCounterResetResource_.Get(), 0, sizeof(uint32_t));
	TransitionCounterResource(commandList, deadListCounterResource_.Get(), deadListCounterResourceState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void GpuParticleResources::CopyCounterResourcesForReadback(ID3D12GraphicsCommandList* commandList) {
	TransitionCounterResource(commandList, freeListCounterResource_.Get(), freeListCounterResourceState_, D3D12_RESOURCE_STATE_COPY_SOURCE);
	TransitionCounterResource(commandList, deadListCounterResource_.Get(), deadListCounterResourceState_, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->CopyBufferRegion(freeListCounterReadbackResource_.Get(), 0, freeListCounterResource_.Get(), 0, sizeof(uint32_t));
	commandList->CopyBufferRegion(deadListCounterReadbackResource_.Get(), 0, deadListCounterResource_.Get(), 0, sizeof(uint32_t));
	TransitionCounterResource(commandList, freeListCounterResource_.Get(), freeListCounterResourceState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	TransitionCounterResource(commandList, deadListCounterResource_.Get(), deadListCounterResourceState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void GpuParticleResources::RefreshCounterReadbackValues(GpuParticle::State& state) const {
	if (!state.isCounterReadbackPending) {
		return;
	}
	if (freeListCounterReadbackData_) {
		state.actualFreeListCount = *freeListCounterReadbackData_;
	}
	if (deadListCounterReadbackData_) {
		state.actualDeadListCount = *deadListCounterReadbackData_;
	}
	state.isCounterReadbackPending = false;
	state.isCounterReadbackValid = true;
}
