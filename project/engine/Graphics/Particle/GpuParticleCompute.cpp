#include "GpuParticleCompute.h"

#include "Engine/Core/DirectXCommon.h"
#include "GpuParticleResources.h"
#include "GpuParticleTypes.h"
#include "Engine/Utility/Logger.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <d3dcompiler.h>

namespace {

D3D12_DESCRIPTOR_RANGE MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE type, uint32_t shaderRegister) {
	D3D12_DESCRIPTOR_RANGE range{};
	range.RangeType = type;
	range.NumDescriptors = 1;
	range.BaseShaderRegister = shaderRegister;
	range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	return range;
}

D3D12_ROOT_PARAMETER MakeTableParameter(D3D12_DESCRIPTOR_RANGE& range) {
	D3D12_ROOT_PARAMETER parameter{};
	parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	parameter.DescriptorTable.NumDescriptorRanges = 1;
	parameter.DescriptorTable.pDescriptorRanges = &range;
	return parameter;
}

D3D12_ROOT_PARAMETER MakeCbvParameter(uint32_t shaderRegister) {
	D3D12_ROOT_PARAMETER parameter{};
	parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	parameter.Descriptor.ShaderRegister = shaderRegister;
	return parameter;
}

bool CreateRootSignature(ID3D12Device* device, const D3D12_ROOT_PARAMETER* parameters, uint32_t parameterCount, ID3D12RootSignature** rootSignature) {
	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	desc.NumParameters = parameterCount;
	desc.pParameters = parameters;

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

	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature));
	assert(SUCCEEDED(hr));
	return SUCCEEDED(hr);
}

bool CreateComputePipeline(
	DirectXCommon* dxCommon,
	ID3D12RootSignature* rootSignature,
	const wchar_t* shaderPath,
	ID3D12PipelineState** pipelineState) {
	Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = dxCommon->CompileShader(shaderPath, L"cs_6_0");
	assert(shaderBlob);
	D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature;
	desc.CS = { shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize() };
	HRESULT hr = dxCommon->GetDevice()->CreateComputePipelineState(&desc, IID_PPV_ARGS(pipelineState));
	assert(SUCCEEDED(hr));
	return SUCCEEDED(hr);
}

void InsertUavBarrier(ID3D12GraphicsCommandList* commandList) {
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.UAV.pResource = nullptr;
	commandList->ResourceBarrier(1, &barrier);
}

float Length(const Vector3& value) {
	return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

Vector3 NormalizeOrUp(const Vector3& value) {
	const float length = Length(value);
	if (length <= 0.00001f) {
		return { 0.0f, 1.0f, 0.0f };
	}
	return { value.x / length, value.y / length, value.z / length };
}

Vector3 MakePositiveVector3(const Vector3& value) {
	return {
		(std::max)(value.x, 0.0f),
		(std::max)(value.y, 0.0f),
		(std::max)(value.z, 0.0f),
	};
}

void WriteEmitterShapeSettings(const GpuParticle::Emitter& emitter, GpuParticle::InitializeInfo& info) {
	info.emitterBoxSize = MakePositiveVector3(emitter.boxSize);
	info.emitterConeHeight = (std::max)(emitter.coneHeight, 0.001f);
	info.emitterShape = static_cast<uint32_t>(GpuParticle::ClampEmitterShape(emitter.shape));
	info.padding = 0.0f;
}

void WriteEmitterShapeSettings(const GpuParticle::Emitter& emitter, GpuParticle::EmitterInfo& info) {
	info.emitterBoxSize = MakePositiveVector3(emitter.boxSize);
	info.emitterConeHeight = (std::max)(emitter.coneHeight, 0.001f);
	info.emitterShape = static_cast<uint32_t>(GpuParticle::ClampEmitterShape(emitter.shape));
	info.emitterDirection = NormalizeOrUp(emitter.direction);
}

}

bool GpuParticleCompute::Initialize(DirectXCommon* dxCommon, const GpuParticle::State& state) {
	assert(dxCommon);
	dxCommon_ = dxCommon;
	return CreateConstantBuffers(state) &&
		CreateInitializeFreeListRootSignature() &&
		CreateInitializeRootSignature() &&
		CreateEmitRootSignature() &&
		CreateUpdateRootSignature() &&
		CreateRecycleRootSignature() &&
		CreatePipelineStates();
}

bool GpuParticleCompute::CreateConstantBuffers(const GpuParticle::State& state) {
	const GpuParticle::Emitter& primaryEmitter = GpuParticle::GetPrimaryEmitter(state);

	initializeInfoResource_ = dxCommon_->CreateBufferResource(GpuParticle::AlignConstantBufferSize(sizeof(GpuParticle::InitializeInfo)));
	initializeInfoResource_->Map(0, nullptr, reinterpret_cast<void**>(&initializeInfoData_));
	initializeInfoData_->particleCount = GpuParticle::kParticleCount;
	initializeInfoData_->randomEnabled = state.isRandomInitializeEnabled ? 1u : 0u;
	initializeInfoData_->randomSeed = primaryEmitter.randomSeed;
	initializeInfoData_->emitterEnabled = (state.isEmitterEnabled && primaryEmitter.enabled) ? 1u : 0u;
	initializeInfoData_->emitterPosition = primaryEmitter.position;
	initializeInfoData_->emitterRadius = primaryEmitter.radius;
	WriteEmitterShapeSettings(primaryEmitter, *initializeInfoData_);
	initializeInfoData_->emitCount = primaryEmitter.emitCount;
	initializeInfoData_->particleTypeIndex = primaryEmitter.particleTypeIndex;

	updateInfoResource_ = dxCommon_->CreateBufferResource(GpuParticle::AlignConstantBufferSize(sizeof(GpuParticle::UpdateInfo)));
	updateInfoResource_->Map(0, nullptr, reinterpret_cast<void**>(&updateInfoData_));
	updateInfoData_->particleCount = GpuParticle::kParticleCount;
	updateInfoData_->deltaTime = state.deltaTime;
	updateInfoData_->freeListEnabled = state.useFreeListEmit ? 1u : 0u;
	updateInfoData_->deadListEnabled = (state.useFreeListEmit && state.useDeadList) ? 1u : 0u;
	updateInfoData_->influenceCentersAndRadius = state.influenceCentersAndRadius;
	updateInfoData_->influenceParams = state.influenceParams;
	updateInfoData_->influenceFieldCount = std::clamp(state.influenceFieldCount, 0u, GpuParticle::kMaxInfluenceFields);
	updateInfoData_->enableParticleInfluence = state.enableParticleInfluence ? 1u : 0u;
	updateInfoData_->particleInfluenceResponseScale = std::clamp(state.particleInfluenceResponseScale, 0.0f, 10.0f);
	updateInfoData_->padding = 0.0f;
	updateInfoData_->railFlowDirectionSpeed = { state.railFlowDirection.x, state.railFlowDirection.y, state.railFlowDirection.z, (std::max)(state.railFlowSpeed, 0.0f) };
	updateInfoData_->railFlowSettings = { state.enableRailParticleFlow ? 1.0f : 0.0f, std::clamp(state.railFlowScale, 0.0f, 10.0f), (std::max)(state.railSpawnAheadDistance, 0.0f), (std::max)(state.railDespawnBehindDistance, 0.0f) };
	updateInfoData_->railFlowCameraPosition = { state.railFlowCameraPosition.x, state.railFlowCameraPosition.y, state.railFlowCameraPosition.z, 1.0f };
	updateInfoData_->chargeGatherTargetAndRate = { state.chargeGatherTargetPosition.x, state.chargeGatherTargetPosition.y, state.chargeGatherTargetPosition.z, std::clamp(state.chargeGatherRate, 0.0f, 1.0f) };
	updateInfoData_->chargeGatherSettings = { state.enableChargeGather ? 1.0f : 0.0f, std::clamp(state.chargeGatherStrengthScale, 0.0f, 10.0f), std::clamp(state.chargeGatherSwirlScale, 0.0f, 10.0f), std::clamp(state.chargeGatherBrightnessScale, 0.0f, 10.0f) };

	emitterInfoResource_ = dxCommon_->CreateBufferResource(GpuParticle::AlignConstantBufferSize(sizeof(GpuParticle::EmitterInfo)));
	emitterInfoResource_->Map(0, nullptr, reinterpret_cast<void**>(&emitterInfoData_));
	emitterInfoData_->emitCount = primaryEmitter.emitCount;
	emitterInfoData_->randomSeed = primaryEmitter.randomSeed;
	emitterInfoData_->randomEnabled = state.isRandomInitializeEnabled ? 1u : 0u;
	emitterInfoData_->particleTypeIndex = primaryEmitter.particleTypeIndex;
	emitterInfoData_->emitterPosition = primaryEmitter.position;
	emitterInfoData_->emitterRadius = primaryEmitter.radius;
	WriteEmitterShapeSettings(primaryEmitter, *emitterInfoData_);

	recycleInfoResource_ = dxCommon_->CreateBufferResource(GpuParticle::AlignConstantBufferSize(sizeof(GpuParticle::RecycleInfo)));
	recycleInfoResource_->Map(0, nullptr, reinterpret_cast<void**>(&recycleInfoData_));
	recycleInfoData_->recycleCount = 0;
	return true;
}

bool GpuParticleCompute::CreateInitializeFreeListRootSignature() {
	D3D12_DESCRIPTOR_RANGE freeListRange = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0);
	D3D12_ROOT_PARAMETER parameter = MakeTableParameter(freeListRange);
	return CreateRootSignature(dxCommon_->GetDevice(), &parameter, 1, initializeFreeListRootSignature_.GetAddressOf());
}

bool GpuParticleCompute::CreateInitializeRootSignature() {
	D3D12_DESCRIPTOR_RANGE particleRange = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0);
	D3D12_DESCRIPTOR_RANGE typeRange = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0);
	D3D12_ROOT_PARAMETER parameters[] = {
		MakeTableParameter(particleRange),
		MakeCbvParameter(0),
		MakeTableParameter(typeRange),
	};
	return CreateRootSignature(dxCommon_->GetDevice(), parameters, _countof(parameters), initializeRootSignature_.GetAddressOf());
}

bool GpuParticleCompute::CreateEmitRootSignature() {
	D3D12_DESCRIPTOR_RANGE particleRange = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0);
	D3D12_DESCRIPTOR_RANGE freeListRange = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1);
	D3D12_DESCRIPTOR_RANGE typeRange = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0);
	D3D12_ROOT_PARAMETER parameters[] = {
		MakeTableParameter(particleRange),
		MakeTableParameter(freeListRange),
		MakeCbvParameter(0),
		MakeTableParameter(typeRange),
	};
	return CreateRootSignature(dxCommon_->GetDevice(), parameters, _countof(parameters), emitRootSignature_.GetAddressOf());
}

bool GpuParticleCompute::CreateUpdateRootSignature() {
	D3D12_DESCRIPTOR_RANGE particleRange = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0);
	D3D12_DESCRIPTOR_RANGE freeListRange = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1);
	D3D12_DESCRIPTOR_RANGE deadListRange = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2);
	D3D12_DESCRIPTOR_RANGE typeRange = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0);
	D3D12_ROOT_PARAMETER parameters[] = {
		MakeTableParameter(particleRange),
		MakeTableParameter(freeListRange),
		MakeTableParameter(deadListRange),
		MakeCbvParameter(0),
		MakeTableParameter(typeRange),
	};
	return CreateRootSignature(dxCommon_->GetDevice(), parameters, _countof(parameters), updateRootSignature_.GetAddressOf());
}

bool GpuParticleCompute::CreateRecycleRootSignature() {
	D3D12_DESCRIPTOR_RANGE deadListRange = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0);
	D3D12_DESCRIPTOR_RANGE freeListRange = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1);
	D3D12_ROOT_PARAMETER parameters[] = {
		MakeTableParameter(deadListRange),
		MakeTableParameter(freeListRange),
		MakeCbvParameter(0),
	};
	return CreateRootSignature(dxCommon_->GetDevice(), parameters, _countof(parameters), recycleRootSignature_.GetAddressOf());
}

bool GpuParticleCompute::CreatePipelineStates() {
	return CreateComputePipeline(dxCommon_, initializeFreeListRootSignature_.Get(), L"resources/shaders/InitializeFreeList.CS.hlsl", initializeFreeListPipelineState_.GetAddressOf()) &&
		CreateComputePipeline(dxCommon_, initializeRootSignature_.Get(), L"resources/shaders/InitializeParticle.CS.hlsl", initializePipelineState_.GetAddressOf()) &&
		CreateComputePipeline(dxCommon_, emitRootSignature_.Get(), L"resources/shaders/EmitParticle.CS.hlsl", emitPipelineState_.GetAddressOf()) &&
		CreateComputePipeline(dxCommon_, updateRootSignature_.Get(), L"resources/shaders/UpdateParticle.CS.hlsl", updatePipelineState_.GetAddressOf()) &&
		CreateComputePipeline(dxCommon_, recycleRootSignature_.Get(), L"resources/shaders/RecycleDeadParticle.CS.hlsl", recyclePipelineState_.GetAddressOf());
}

void GpuParticleCompute::Dispatch(ID3D12GraphicsCommandList* commandList, GpuParticleResources& resources, GpuParticle::State& state) {
	DispatchInitializeFreeListIfNeeded(commandList, resources, state);
	ResetDeadListCounterIfNeeded(commandList, resources, state);
	DispatchInitializeIfNeeded(commandList, resources, state);
	DispatchEmitIfNeeded(commandList, resources, state);
	DispatchUpdate(commandList, resources, state);
	DispatchRecycleIfNeeded(commandList, resources, state);
}

void GpuParticleCompute::DispatchInitializeFreeListIfNeeded(
	ID3D12GraphicsCommandList* commandList,
	GpuParticleResources& resources,
	GpuParticle::State& state) {
	if (!state.needsInitializeFreeListDispatch) {
		return;
	}

	resources.ResetFreeListCounter(commandList);
	commandList->SetComputeRootSignature(initializeFreeListRootSignature_.Get());
	commandList->SetPipelineState(initializeFreeListPipelineState_.Get());
	commandList->SetComputeRootDescriptorTable(0, resources.GetFreeListUavHandle());
	commandList->Dispatch((GpuParticle::kParticleCount + 1023) / 1024, 1, 1);
	InsertUavBarrier(commandList);

	state.needsInitializeFreeListDispatch = false;
	state.isFreeListInitialized = true;
	state.freeListRemainingEstimate = GpuParticle::kParticleCount;
	state.emitBatchEstimates.clear();
}

void GpuParticleCompute::ResetDeadListCounterIfNeeded(
	ID3D12GraphicsCommandList* commandList,
	GpuParticleResources& resources,
	GpuParticle::State& state) {
	if (!state.needsDeadListCounterReset) {
		return;
	}
	resources.ResetDeadListCounter(commandList);
	InsertUavBarrier(commandList);
	state.needsDeadListCounterReset = false;
	state.isDeadListReady = true;
	state.deadListCountEstimate = 0;
}

void GpuParticleCompute::DispatchInitializeIfNeeded(
	ID3D12GraphicsCommandList* commandList,
	GpuParticleResources& resources,
	GpuParticle::State& state) {
	if (!state.needsInitializeDispatch) {
		return;
	}

	resources.TransitionParticleResource(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	const GpuParticle::Emitter& emitter = GpuParticle::GetPrimaryEmitter(state);
	const float emissionAccumulator = emitter.emissionAccumulator;
	const bool shouldRestoreEmissionAccumulator = emitter.emissionRate > 0.0f;
	initializeInfoData_->particleCount = GpuParticle::kParticleCount;
	initializeInfoData_->randomEnabled = state.isRandomInitializeEnabled ? 1u : 0u;
	initializeInfoData_->randomSeed = emitter.randomSeed;
	initializeInfoData_->emitterEnabled = ((state.isEmitterEnabled && emitter.enabled) || state.useFreeListEmit) ? 1u : 0u;
	initializeInfoData_->emitterPosition = emitter.position;
	initializeInfoData_->emitterRadius = emitter.radius;
	WriteEmitterShapeSettings(emitter, *initializeInfoData_);
	const uint32_t initializeEmitCount = emitter.pendingEmitCount > 0 ? emitter.pendingEmitCount : emitter.emitCount;
	initializeInfoData_->emitCount = state.useFreeListEmit ? 0u : (std::min)(initializeEmitCount, GpuParticle::kParticleCount);
	initializeInfoData_->particleTypeIndex = emitter.particleTypeIndex;

	commandList->SetComputeRootSignature(initializeRootSignature_.Get());
	commandList->SetPipelineState(initializePipelineState_.Get());
	commandList->SetComputeRootDescriptorTable(0, resources.GetParticleUavHandle());
	commandList->SetComputeRootConstantBufferView(1, initializeInfoResource_->GetGPUVirtualAddress());
	commandList->SetComputeRootDescriptorTable(2, resources.GetParticleTypeSrvHandle());
	commandList->Dispatch((GpuParticle::kParticleCount + 1023) / 1024, 1, 1);
	InsertUavBarrier(commandList);
	resources.TransitionParticleResource(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	state.needsInitializeDispatch = false;
	state.elapsedTimeSinceInitialize = 0.0f;
	GpuParticle::ResetEmitterTimersAndPending(state);
	if (shouldRestoreEmissionAccumulator && !state.emitters.empty()) {
		state.emitters.front().emissionAccumulator = emissionAccumulator;
	}
	state.activeCountEstimate = state.useFreeListEmit
		? 0u
		: (state.isEmitterEnabled ? GpuParticle::GetEnabledEmitterEmitCountSum(state) : GpuParticle::kParticleCount);
}

void GpuParticleCompute::DispatchEmitIfNeeded(
	ID3D12GraphicsCommandList* commandList,
	GpuParticleResources& resources,
	GpuParticle::State& state) {
	if (!state.useFreeListEmit || !state.needsEmitDispatch || state.needsInitializeDispatch) {
		return;
	}

	resources.TransitionParticleResource(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	uint32_t totalEmitCount = 0;
	bool hasPendingEmit = false;
	for (GpuParticle::Emitter& emitter : state.emitters) {
		if (!state.isEmitterEnabled || !emitter.enabled || !emitter.pendingEmit) {
			continue;
		}

		const uint32_t requestedEmitCount = emitter.pendingEmitCount > 0 ? emitter.pendingEmitCount : emitter.emitCount;
		if (requestedEmitCount == 0) {
			emitter.pendingEmit = false;
			emitter.pendingEmitCount = 0;
			continue;
		}
		uint32_t actualEmitCount = (std::min)(requestedEmitCount, GpuParticle::kParticleCount);
		actualEmitCount = (std::min)(actualEmitCount, state.freeListRemainingEstimate);
		if (actualEmitCount == 0) {
			hasPendingEmit = true;
			continue;
		}

		emitterInfoData_->emitCount = actualEmitCount;
		emitterInfoData_->randomSeed = emitter.randomSeed;
		emitterInfoData_->randomEnabled = state.isRandomInitializeEnabled ? 1u : 0u;
		emitterInfoData_->particleTypeIndex = emitter.particleTypeIndex;
		emitterInfoData_->emitterPosition = emitter.position;
		emitterInfoData_->emitterRadius = emitter.radius;
		WriteEmitterShapeSettings(emitter, *emitterInfoData_);
		commandList->SetComputeRootSignature(emitRootSignature_.Get());
		commandList->SetPipelineState(emitPipelineState_.Get());
		commandList->SetComputeRootDescriptorTable(0, resources.GetParticleUavHandle());
		commandList->SetComputeRootDescriptorTable(1, resources.GetFreeListUavHandle());
		commandList->SetComputeRootConstantBufferView(2, emitterInfoResource_->GetGPUVirtualAddress());
		commandList->SetComputeRootDescriptorTable(3, resources.GetParticleTypeSrvHandle());
		commandList->Dispatch((actualEmitCount + 1023) / 1024, 1, 1);
		InsertUavBarrier(commandList);

		state.freeListRemainingEstimate -= actualEmitCount;
		state.activeCountEstimate = (std::min)(state.activeCountEstimate + actualEmitCount, GpuParticle::kParticleCount);
		state.emitBatchEstimates.push_back({ actualEmitCount, 0.0f, GpuParticle::GetParticleType(state, emitter.particleTypeIndex).lifeTimeMax });
		totalEmitCount += actualEmitCount;
		if (actualEmitCount < requestedEmitCount) {
			state.lastSkippedEmitCount = (std::min)(state.lastSkippedEmitCount + requestedEmitCount - actualEmitCount, GpuParticle::kParticleCount);
			emitter.pendingEmitCount = requestedEmitCount - actualEmitCount;
			emitter.pendingEmit = true;
			hasPendingEmit = true;
		} else {
			emitter.pendingEmit = false;
			emitter.pendingEmitCount = 0;
			emitter.emitTimer = 0.0f;
		}
	}
	state.lastEmitDispatchCount = totalEmitCount;
	state.lastActualEmitCount = totalEmitCount;
	state.needsEmitDispatch = hasPendingEmit;
}

void GpuParticleCompute::DispatchUpdate(ID3D12GraphicsCommandList* commandList, GpuParticleResources& resources, GpuParticle::State& state) {
	if (!state.isUpdateEnabled || state.needsInitializeDispatch) {
		return;
	}

	updateInfoData_->particleCount = GpuParticle::kParticleCount;
	updateInfoData_->deltaTime = state.deltaTime;
	updateInfoData_->freeListEnabled = state.useFreeListEmit ? 1u : 0u;
	updateInfoData_->deadListEnabled = (state.useFreeListEmit && state.useDeadList) ? 1u : 0u;
	updateInfoData_->influenceCentersAndRadius = state.influenceCentersAndRadius;
	updateInfoData_->influenceParams = state.influenceParams;
	updateInfoData_->influenceFieldCount = std::clamp(state.influenceFieldCount, 0u, GpuParticle::kMaxInfluenceFields);
	updateInfoData_->enableParticleInfluence = state.enableParticleInfluence ? 1u : 0u;
	updateInfoData_->particleInfluenceResponseScale = std::clamp(state.particleInfluenceResponseScale, 0.0f, 10.0f);
	updateInfoData_->padding = 0.0f;
	updateInfoData_->railFlowDirectionSpeed = { state.railFlowDirection.x, state.railFlowDirection.y, state.railFlowDirection.z, (std::max)(state.railFlowSpeed, 0.0f) };
	updateInfoData_->railFlowSettings = { state.enableRailParticleFlow ? 1.0f : 0.0f, std::clamp(state.railFlowScale, 0.0f, 10.0f), (std::max)(state.railSpawnAheadDistance, 0.0f), (std::max)(state.railDespawnBehindDistance, 0.0f) };
	updateInfoData_->railFlowCameraPosition = { state.railFlowCameraPosition.x, state.railFlowCameraPosition.y, state.railFlowCameraPosition.z, 1.0f };
	updateInfoData_->chargeGatherTargetAndRate = { state.chargeGatherTargetPosition.x, state.chargeGatherTargetPosition.y, state.chargeGatherTargetPosition.z, std::clamp(state.chargeGatherRate, 0.0f, 1.0f) };
	updateInfoData_->chargeGatherSettings = { state.enableChargeGather ? 1.0f : 0.0f, std::clamp(state.chargeGatherStrengthScale, 0.0f, 10.0f), std::clamp(state.chargeGatherSwirlScale, 0.0f, 10.0f), std::clamp(state.chargeGatherBrightnessScale, 0.0f, 10.0f) };
	resources.TransitionParticleResource(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->SetComputeRootSignature(updateRootSignature_.Get());
	commandList->SetPipelineState(updatePipelineState_.Get());
	commandList->SetComputeRootDescriptorTable(0, resources.GetParticleUavHandle());
	commandList->SetComputeRootDescriptorTable(1, resources.GetFreeListUavHandle());
	commandList->SetComputeRootDescriptorTable(2, resources.GetDeadListUavHandle());
	commandList->SetComputeRootConstantBufferView(3, updateInfoResource_->GetGPUVirtualAddress());
	commandList->SetComputeRootDescriptorTable(4, resources.GetParticleTypeSrvHandle());
	commandList->Dispatch((GpuParticle::kParticleCount + 1023) / 1024, 1, 1);
	InsertUavBarrier(commandList);
	resources.TransitionParticleResource(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	state.elapsedTimeSinceInitialize += state.deltaTime;
	if (state.useFreeListEmit) {
		UpdateFreeListEstimate(state, state.deltaTime);
	}
}

void GpuParticleCompute::DispatchRecycleIfNeeded(
	ID3D12GraphicsCommandList* commandList,
	GpuParticleResources& resources,
	GpuParticle::State& state) {
	if (!state.useFreeListEmit || !state.useDeadList) {
		state.needsRecycleDispatch = false;
		return;
	}
	if (state.autoRecycleDeadList && state.deadListCountEstimate > 0) {
		state.needsRecycleDispatch = true;
	}
	if (!state.needsRecycleDispatch || state.deadListCountEstimate == 0) {
		state.needsRecycleDispatch = false;
		return;
	}

	uint32_t count = (std::min)(state.recycleCount, state.deadListCountEstimate);
	count = (std::min)(count, GpuParticle::kParticleCount - state.freeListRemainingEstimate);
	if (count == 0) {
		state.needsRecycleDispatch = false;
		return;
	}

	recycleInfoData_->recycleCount = count;
	commandList->SetComputeRootSignature(recycleRootSignature_.Get());
	commandList->SetPipelineState(recyclePipelineState_.Get());
	commandList->SetComputeRootDescriptorTable(0, resources.GetDeadListUavHandle());
	commandList->SetComputeRootDescriptorTable(1, resources.GetFreeListUavHandle());
	commandList->SetComputeRootConstantBufferView(2, recycleInfoResource_->GetGPUVirtualAddress());
	commandList->Dispatch((count + 1023) / 1024, 1, 1);
	InsertUavBarrier(commandList);

	state.deadListCountEstimate -= count;
	state.freeListRemainingEstimate = (std::min)(state.freeListRemainingEstimate + count, GpuParticle::kParticleCount);
	state.lastRecycleDispatchCount = count;
	state.lastReusedCount = count;
	state.needsRecycleDispatch = false;
}

void GpuParticleCompute::UpdateFreeListEstimate(GpuParticle::State& state, float deltaTime) {
	uint32_t returnedCount = 0;
	for (GpuParticle::EmitBatchEstimate& batch : state.emitBatchEstimates) {
		batch.elapsedTime += deltaTime;
		if (batch.elapsedTime >= batch.returnDelay) {
			returnedCount += batch.particleCount;
			batch.particleCount = 0;
		}
	}
	state.emitBatchEstimates.erase(
		std::remove_if(
			state.emitBatchEstimates.begin(),
			state.emitBatchEstimates.end(),
			[](const GpuParticle::EmitBatchEstimate& batch) { return batch.particleCount == 0; }),
		state.emitBatchEstimates.end());
	if (returnedCount == 0) {
		return;
	}

	state.activeCountEstimate = returnedCount >= state.activeCountEstimate ? 0u : state.activeCountEstimate - returnedCount;
	if (state.useDeadList) {
		state.deadListCountEstimate = (std::min)(state.deadListCountEstimate + returnedCount, GpuParticle::kParticleCount);
	} else {
		state.freeListRemainingEstimate = (std::min)(state.freeListRemainingEstimate + returnedCount, GpuParticle::kParticleCount);
	}
}

