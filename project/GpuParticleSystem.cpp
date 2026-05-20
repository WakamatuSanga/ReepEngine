#include "GpuParticleSystem.h"

#include "Camera.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "SrvManager.h"

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <cassert>
#include <d3dcompiler.h>

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

uint32_t AlignConstantBufferSize(uint32_t size) {
	return (size + 0xff) & ~0xff;
}

uint32_t Hash(uint32_t value) {
	value ^= value >> 16;
	value *= 0x7feb352du;
	value ^= value >> 15;
	value *= 0x846ca68bu;
	value ^= value >> 16;
	return value;
}

float Random01(uint32_t seed) {
	return static_cast<float>(Hash(seed) & 0x00ffffffu) / 16777215.0f;
}

float RandomRange(uint32_t seed, float minValue, float maxValue) {
	return minValue + (maxValue - minValue) * Random01(seed);
}

}

bool GpuParticleSystem::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	assert(dxCommon);
	assert(srvManager);

	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	if (!CreateResources()) {
		return false;
	}
	if (!CreateFreeListResources()) {
		return false;
	}
	if (!CreateDeadListResources()) {
		return false;
	}
	if (!CreateInitializeFreeListRootSignature()) {
		return false;
	}
	if (!CreateInitializeFreeListPipelineState()) {
		return false;
	}
	if (!CreateInitializeRootSignature()) {
		return false;
	}
	if (!CreateInitializePipelineState()) {
		return false;
	}
	if (!CreateEmitRootSignature()) {
		return false;
	}
	if (!CreateEmitPipelineState()) {
		return false;
	}
	if (!CreateUpdateRootSignature()) {
		return false;
	}
	if (!CreateUpdatePipelineState()) {
		return false;
	}
	if (!CreateRecycleRootSignature()) {
		return false;
	}
	if (!CreateRecyclePipelineState()) {
		return false;
	}
	if (!CreateDrawRootSignature()) {
		return false;
	}
	if (!CreateDrawPipelineState()) {
		return false;
	}

	isInitialized_ = true;
	needsInitializeDispatch_ = true;
	return true;
}

void GpuParticleSystem::Update(const Camera* camera) {
	if (!camera || !perViewData_) {
		return;
	}

	Matrix4x4 billboardMatrix = camera->GetWorldMatrix();
	billboardMatrix.m[3][0] = 0.0f;
	billboardMatrix.m[3][1] = 0.0f;
	billboardMatrix.m[3][2] = 0.0f;
	perViewData_->billboardMatrix = billboardMatrix;
	perViewData_->viewProjection = camera->GetViewProjectionMatrix();

	if (updateInfoData_) {
		deltaTime_ = std::clamp(deltaTime_, 0.0f, 1.0f / 15.0f);
		updateInfoData_->particleCount = kParticleCount;
		updateInfoData_->deltaTime = deltaTime_;
		updateInfoData_->freeListEnabled = useFreeListEmit_ ? 1u : 0u;
		updateInfoData_->deadListEnabled = (useFreeListEmit_ && useDeadList_) ? 1u : 0u;
	}

	if (isUpdateEnabled_ && isEmitterEnabled_ && emitInterval_ > 0.0f && !needsInitializeDispatch_) {
		emitTimer_ += deltaTime_;
		if (emitTimer_ >= emitInterval_) {
			if (useFreeListEmit_) {
				RequestEmit();
			} else {
				RequestInitialize();
			}
		}
	}
}

void GpuParticleSystem::Draw() {
	if (!isInitialized_ || !isEnabled_) {
		return;
	}

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	ID3D12DescriptorHeap* descriptorHeaps[] = { srvManager_->GetSrvDescriptorHeap() };
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	DispatchInitializeFreeListIfNeeded(commandList);
	ResetDeadListCounterIfNeeded(commandList);
	DispatchInitializeIfNeeded(commandList);
	DispatchEmitIfNeeded(commandList);
	DispatchUpdate(commandList);
	DispatchRecycleIfNeeded(commandList);

	TransitionParticleResource(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	commandList->SetGraphicsRootSignature(drawRootSignature_.Get());
	commandList->SetPipelineState(drawPipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(0, perViewResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(1, particleSrvHandle_);
	commandList->DrawInstanced(6, kParticleCount, 0, 0);
}

void GpuParticleSystem::DrawImGui() {
#ifdef _DEBUG
	if (ImGui::Begin("Gpu Particle Debug")) {
		auto resetListsForFreeListMode = [this]() {
			needsInitializeFreeListDispatch_ = true;
			isFreeListInitialized_ = false;
			needsDeadListCounterReset_ = true;
			isDeadListReady_ = false;
			emitBatchEstimates_.clear();
			freeListRemainingEstimate_ = 0;
			deadListCountEstimate_ = 0;
			lastRecycleDispatchCount_ = 0;
		};

		ImGui::SeparatorText("General");
		ImGui::Checkbox("Show GPU Particle", &isEnabled_);
		ImGui::Checkbox("Update Particle CS", &isUpdateEnabled_);
		ImGui::DragFloat("Delta Time", &deltaTime_, 0.001f, 0.0f, 1.0f / 15.0f, "%.4f");
		ImGui::Text("Alive Count Estimate (Approx): %u", activeCountEstimate_);

		bool needsReinitialize = false;

		ImGui::SeparatorText("Random");
		needsReinitialize |= ImGui::Checkbox("Random Initialize", &isRandomInitializeEnabled_);
		needsReinitialize |= ImGui::InputScalar("Random Seed", ImGuiDataType_U32, &randomSeed_);

		ImGui::SeparatorText("Emitter");
		needsReinitialize |= ImGui::Checkbox("Emitter Enabled", &isEmitterEnabled_);
		if (ImGui::Checkbox("Use FreeList Emit", &useFreeListEmit_)) {
			resetListsForFreeListMode();
			RequestInitialize();
			if (useFreeListEmit_) {
				RequestEmit();
			}
		}
		needsReinitialize |= ImGui::DragFloat3("Emitter Position", &emitterPosition_.x, 0.05f, -10.0f, 10.0f, "%.2f");
		needsReinitialize |= ImGui::DragFloat("Emitter Radius", &emitterRadius_, 0.01f, 0.0f, 5.0f, "%.2f");
		int emitCount = static_cast<int>(emitCount_);
		if (ImGui::DragInt("Emit Count", &emitCount, 1.0f, 0, static_cast<int>(kParticleCount))) {
			emitCount_ = static_cast<uint32_t>(std::clamp(emitCount, 0, static_cast<int>(kParticleCount)));
			needsReinitialize = true;
		}
		if (ImGui::DragFloat("Emit Interval", &emitInterval_, 0.01f, 0.0f, 10.0f, "%.2f sec")) {
			emitInterval_ = (std::max)(0.0f, emitInterval_);
			emitTimer_ = 0.0f;
		}
		ImGui::Text("Emit Timer: %.2f / %.2f", emitTimer_, emitInterval_);
		if (needsReinitialize) {
			if (useFreeListEmit_) {
				resetListsForFreeListMode();
				RequestInitialize();
				RequestEmit();
			} else {
				RequestInitialize();
			}
		}
		ImGui::Text("Emitter Active Limit: %u", isEmitterEnabled_ ? (std::min)(emitCount_, kParticleCount) : kParticleCount);

		ImGui::SeparatorText("FreeList");
		ImGui::Text("FreeList Ready: %s", isFreeListInitialized_ ? "true" : "false");
		ImGui::Text("FreeList UAV Index: %u", freeListUavIndex_);
		ImGui::Text("Counter Ready: %s", freeListCounterResource_ ? "true" : "false");
		ImGui::Text("FreeList Remaining Estimate (Approx): %u", freeListRemainingEstimate_);

		ImGui::SeparatorText("DeadList");
		if (ImGui::Checkbox("Use DeadList", &useDeadList_)) {
			resetListsForFreeListMode();
			if (useFreeListEmit_) {
				RequestInitialize();
				RequestEmit();
			}
		}
		ImGui::Text("DeadList Ready: %s", isDeadListReady_ ? "true" : "false");
		ImGui::Text("DeadList UAV Index: %u", deadListUavIndex_);
		ImGui::Text("Counter Ready: %s", deadListCounterResource_ ? "true" : "false");
		ImGui::Text("DeadList Count Estimate (Approx): %u", deadListCountEstimate_);

		ImGui::SeparatorText("Recycle");
		ImGui::Checkbox("Auto Recycle DeadList", &autoRecycleDeadList_);
		int recycleCount = static_cast<int>(recycleCount_);
		if (ImGui::DragInt("Recycle Count", &recycleCount, 1.0f, 0, static_cast<int>(kParticleCount))) {
			recycleCount_ = static_cast<uint32_t>(std::clamp(recycleCount, 0, static_cast<int>(kParticleCount)));
		}
		if (ImGui::Button("Recycle DeadList To FreeList")) {
			RequestRecycle();
		}
		ImGui::Text("Last Recycle Dispatch Count: %u", lastRecycleDispatchCount_);

		ImGui::SeparatorText("Dispatch / Resource");
		ImGui::Text("Particle Count: %u", kParticleCount);
		ImGui::Text("Draw: DrawInstanced(6, %u, 0, 0)", kParticleCount);
		ImGui::Text("Initialize Dispatch Groups: %u", (kParticleCount + 1023) / 1024);
		ImGui::Text("Update Dispatch Groups: %u", (kParticleCount + 1023) / 1024);
		ImGui::Text("InitializeFreeList Dispatch Groups: %u", (kParticleCount + 1023) / 1024);
		ImGui::Text("Recycle Dispatch: %s", needsRecycleDispatch_ ? "pending" : "idle");
		ImGui::Text("Emit Dispatch: %s", needsEmitDispatch_ ? "pending" : "idle");
		ImGui::Text("Last Emit Dispatch Count: %u", lastEmitDispatchCount_);
		ImGui::Text("Initialized By CS: %s", needsInitializeDispatch_ ? "pending" : "done");
		ImGui::Text("Particle SRV Index: %u", particleSrvIndex_);
		ImGui::Text("Particle UAV Index: %u", particleUavIndex_);
		ImGui::TextUnformatted("Counters are CPU estimates / approx. GPU counter readback is not implemented yet.");

		ImGui::SeparatorText("Debug Actions");
		if (ImGui::Button("Emit Particles From FreeList")) {
			RequestEmit();
		}
		if (ImGui::Button("Reinitialize GPU Particles")) {
			if (useFreeListEmit_) {
				resetListsForFreeListMode();
				RequestEmit();
			}
			RequestInitialize();
		}
		if (ImGui::Button("Next Seed + Reinitialize")) {
			++randomSeed_;
			if (useFreeListEmit_) {
				resetListsForFreeListMode();
				RequestEmit();
			}
			RequestInitialize();
		}
		if (ImGui::Button("Reset Lists / Reinitialize All")) {
			resetListsForFreeListMode();
			RequestInitialize();
			if (useFreeListEmit_) {
				RequestEmit();
			}
		}
	}
	ImGui::End();
#endif
}

bool GpuParticleSystem::CreateResources() {
	ID3D12Device* device = dxCommon_->GetDevice();

	particleResource_ = CreateUavBufferResource(device, sizeof(Particle) * kParticleCount);
	particleResourceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	particleUavIndex_ = srvManager_->Allocate();
	srvManager_->CreateUAVforStructuredBuffer(particleUavIndex_, particleResource_.Get(), kParticleCount, sizeof(Particle));
	particleUavHandle_ = srvManager_->GetGPUDescriptorHandle(particleUavIndex_);

	particleSrvIndex_ = srvManager_->Allocate();
	srvManager_->CreateSRVforStructuredBuffer(particleSrvIndex_, particleResource_.Get(), kParticleCount, sizeof(Particle));
	particleSrvHandle_ = srvManager_->GetGPUDescriptorHandle(particleSrvIndex_);

	initializeInfoResource_ = dxCommon_->CreateBufferResource(AlignConstantBufferSize(sizeof(InitializeInfo)));
	initializeInfoResource_->Map(0, nullptr, reinterpret_cast<void**>(&initializeInfoData_));
	initializeInfoData_->particleCount = kParticleCount;
	initializeInfoData_->randomEnabled = isRandomInitializeEnabled_ ? 1u : 0u;
	initializeInfoData_->randomSeed = randomSeed_;
	initializeInfoData_->emitterEnabled = isEmitterEnabled_ ? 1u : 0u;
	initializeInfoData_->emitterPosition = emitterPosition_;
	initializeInfoData_->emitterRadius = emitterRadius_;
	initializeInfoData_->emitCount = emitCount_;

	updateInfoResource_ = dxCommon_->CreateBufferResource(AlignConstantBufferSize(sizeof(UpdateInfo)));
	updateInfoResource_->Map(0, nullptr, reinterpret_cast<void**>(&updateInfoData_));
	updateInfoData_->particleCount = kParticleCount;
	updateInfoData_->deltaTime = deltaTime_;
	updateInfoData_->freeListEnabled = useFreeListEmit_ ? 1u : 0u;
	updateInfoData_->deadListEnabled = (useFreeListEmit_ && useDeadList_) ? 1u : 0u;

	emitterInfoResource_ = dxCommon_->CreateBufferResource(AlignConstantBufferSize(sizeof(EmitterInfo)));
	emitterInfoResource_->Map(0, nullptr, reinterpret_cast<void**>(&emitterInfoData_));
	emitterInfoData_->emitCount = emitCount_;
	emitterInfoData_->randomSeed = randomSeed_;
	emitterInfoData_->randomEnabled = isRandomInitializeEnabled_ ? 1u : 0u;
	emitterInfoData_->emitterPosition = emitterPosition_;
	emitterInfoData_->emitterRadius = emitterRadius_;

	recycleInfoResource_ = dxCommon_->CreateBufferResource(AlignConstantBufferSize(sizeof(RecycleInfo)));
	recycleInfoResource_->Map(0, nullptr, reinterpret_cast<void**>(&recycleInfoData_));
	recycleInfoData_->recycleCount = 0;

	perViewResource_ = dxCommon_->CreateBufferResource(AlignConstantBufferSize(sizeof(PerView)));
	perViewResource_->Map(0, nullptr, reinterpret_cast<void**>(&perViewData_));

	return true;
}

bool GpuParticleSystem::CreateFreeListResources() {
	ID3D12Device* device = dxCommon_->GetDevice();

	freeListResource_ = CreateUavBufferResource(device, sizeof(uint32_t) * kParticleCount);
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
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = kParticleCount;
	uavDesc.Buffer.StructureByteStride = sizeof(uint32_t);
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	device->CreateUnorderedAccessView(
		freeListResource_.Get(),
		freeListCounterResource_.Get(),
		&uavDesc,
		srvManager_->GetCPUDescriptorHandle(freeListUavIndex_));
	freeListUavHandle_ = srvManager_->GetGPUDescriptorHandle(freeListUavIndex_);

	return true;
}

bool GpuParticleSystem::CreateDeadListResources() {
	ID3D12Device* device = dxCommon_->GetDevice();

	deadListResource_ = CreateUavBufferResource(device, sizeof(uint32_t) * kParticleCount);
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
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = kParticleCount;
	uavDesc.Buffer.StructureByteStride = sizeof(uint32_t);
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	device->CreateUnorderedAccessView(
		deadListResource_.Get(),
		deadListCounterResource_.Get(),
		&uavDesc,
		srvManager_->GetCPUDescriptorHandle(deadListUavIndex_));
	deadListUavHandle_ = srvManager_->GetGPUDescriptorHandle(deadListUavIndex_);

	return true;
}

bool GpuParticleSystem::CreateInitializeFreeListRootSignature() {
	D3D12_DESCRIPTOR_RANGE uavRange{};
	uavRange.BaseShaderRegister = 0;
	uavRange.NumDescriptors = 1;
	uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameter{};
	rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameter.DescriptorTable.pDescriptorRanges = &uavRange;
	rootParameter.DescriptorTable.NumDescriptorRanges = 1;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	rootSignatureDesc.pParameters = &rootParameter;
	rootSignatureDesc.NumParameters = 1;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
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
		IID_PPV_ARGS(&initializeFreeListRootSignature_));
	assert(SUCCEEDED(hr));
	return SUCCEEDED(hr);
}

bool GpuParticleSystem::CreateInitializeFreeListPipelineState() {
	Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob = dxCommon_->CompileShader(
		L"resources/shaders/InitializeFreeList.CS.hlsl",
		L"cs_6_0");
	assert(computeShaderBlob);

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc{};
	computePipelineStateDesc.pRootSignature = initializeFreeListRootSignature_.Get();
	computePipelineStateDesc.CS = { computeShaderBlob->GetBufferPointer(), computeShaderBlob->GetBufferSize() };

	HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(
		&computePipelineStateDesc,
		IID_PPV_ARGS(&initializeFreeListPipelineState_));
	assert(SUCCEEDED(hr));
	return SUCCEEDED(hr);
}

bool GpuParticleSystem::CreateInitializeRootSignature() {
	D3D12_DESCRIPTOR_RANGE uavRange{};
	uavRange.BaseShaderRegister = 0;
	uavRange.NumDescriptors = 1;
	uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[2]{};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &uavRange;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
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
		IID_PPV_ARGS(&initializeRootSignature_));
	assert(SUCCEEDED(hr));
	return SUCCEEDED(hr);
}

bool GpuParticleSystem::CreateInitializePipelineState() {
	Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob = dxCommon_->CompileShader(
		L"resources/shaders/InitializeParticle.CS.hlsl",
		L"cs_6_0");
	assert(computeShaderBlob);

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc{};
	computePipelineStateDesc.pRootSignature = initializeRootSignature_.Get();
	computePipelineStateDesc.CS = { computeShaderBlob->GetBufferPointer(), computeShaderBlob->GetBufferSize() };

	HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(
		&computePipelineStateDesc,
		IID_PPV_ARGS(&initializePipelineState_));
	assert(SUCCEEDED(hr));
	return SUCCEEDED(hr);
}

bool GpuParticleSystem::CreateEmitRootSignature() {
	D3D12_DESCRIPTOR_RANGE uavRanges[2]{};
	uavRanges[0].BaseShaderRegister = 0;
	uavRanges[0].NumDescriptors = 1;
	uavRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	uavRanges[1].BaseShaderRegister = 1;
	uavRanges[1].NumDescriptors = 1;
	uavRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRanges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[3]{};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &uavRanges[0];
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &uavRanges[1];
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].Descriptor.ShaderRegister = 0;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
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
		IID_PPV_ARGS(&emitRootSignature_));
	assert(SUCCEEDED(hr));
	return SUCCEEDED(hr);
}

bool GpuParticleSystem::CreateEmitPipelineState() {
	Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob = dxCommon_->CompileShader(
		L"resources/shaders/EmitParticle.CS.hlsl",
		L"cs_6_0");
	assert(computeShaderBlob);

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc{};
	computePipelineStateDesc.pRootSignature = emitRootSignature_.Get();
	computePipelineStateDesc.CS = { computeShaderBlob->GetBufferPointer(), computeShaderBlob->GetBufferSize() };

	HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(
		&computePipelineStateDesc,
		IID_PPV_ARGS(&emitPipelineState_));
	assert(SUCCEEDED(hr));
	return SUCCEEDED(hr);
}

bool GpuParticleSystem::CreateUpdateRootSignature() {
	D3D12_DESCRIPTOR_RANGE uavRanges[3]{};
	uavRanges[0].BaseShaderRegister = 0;
	uavRanges[0].NumDescriptors = 1;
	uavRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	uavRanges[1].BaseShaderRegister = 1;
	uavRanges[1].NumDescriptors = 1;
	uavRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRanges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	uavRanges[2].BaseShaderRegister = 2;
	uavRanges[2].NumDescriptors = 1;
	uavRanges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRanges[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[4]{};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &uavRanges[0];
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &uavRanges[1];
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &uavRanges[2];
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[3].Descriptor.ShaderRegister = 0;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
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
		IID_PPV_ARGS(&updateRootSignature_));
	assert(SUCCEEDED(hr));
	return SUCCEEDED(hr);
}

bool GpuParticleSystem::CreateUpdatePipelineState() {
	Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob = dxCommon_->CompileShader(
		L"resources/shaders/UpdateParticle.CS.hlsl",
		L"cs_6_0");
	assert(computeShaderBlob);

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc{};
	computePipelineStateDesc.pRootSignature = updateRootSignature_.Get();
	computePipelineStateDesc.CS = { computeShaderBlob->GetBufferPointer(), computeShaderBlob->GetBufferSize() };

	HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(
		&computePipelineStateDesc,
		IID_PPV_ARGS(&updatePipelineState_));
	assert(SUCCEEDED(hr));
	return SUCCEEDED(hr);
}

bool GpuParticleSystem::CreateRecycleRootSignature() {
	D3D12_DESCRIPTOR_RANGE uavRanges[2]{};
	uavRanges[0].BaseShaderRegister = 0;
	uavRanges[0].NumDescriptors = 1;
	uavRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	uavRanges[1].BaseShaderRegister = 1;
	uavRanges[1].NumDescriptors = 1;
	uavRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRanges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[3]{};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &uavRanges[0];
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &uavRanges[1];
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].Descriptor.ShaderRegister = 0;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
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
		IID_PPV_ARGS(&recycleRootSignature_));
	assert(SUCCEEDED(hr));
	return SUCCEEDED(hr);
}

bool GpuParticleSystem::CreateRecyclePipelineState() {
	Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob = dxCommon_->CompileShader(
		L"resources/shaders/RecycleDeadParticle.CS.hlsl",
		L"cs_6_0");
	assert(computeShaderBlob);

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc{};
	computePipelineStateDesc.pRootSignature = recycleRootSignature_.Get();
	computePipelineStateDesc.CS = { computeShaderBlob->GetBufferPointer(), computeShaderBlob->GetBufferSize() };

	HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(
		&computePipelineStateDesc,
		IID_PPV_ARGS(&recyclePipelineState_));
	assert(SUCCEEDED(hr));
	return SUCCEEDED(hr);
}

bool GpuParticleSystem::CreateDrawRootSignature() {
	D3D12_DESCRIPTOR_RANGE srvRange{};
	srvRange.BaseShaderRegister = 0;
	srvRange.NumDescriptors = 1;
	srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[2]{};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &srvRange;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
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

bool GpuParticleSystem::CreateDrawPipelineState() {
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(
		L"resources/shaders/GpuParticle.VS.hlsl",
		L"vs_6_0");
	assert(vertexShaderBlob);
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(
		L"resources/shaders/GpuParticle.PS.hlsl",
		L"ps_6_0");
	assert(pixelShaderBlob);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = drawRootSignature_.Get();
	graphicsPipelineStateDesc.InputLayout = { nullptr, 0 };
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };

	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[1].RenderTargetWriteMask = 0;
	graphicsPipelineStateDesc.BlendState = blendDesc;

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;

	graphicsPipelineStateDesc.DepthStencilState.DepthEnable = TRUE;
	graphicsPipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	graphicsPipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	graphicsPipelineStateDesc.NumRenderTargets = 2;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	graphicsPipelineStateDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
		&graphicsPipelineStateDesc,
		IID_PPV_ARGS(&drawPipelineState_));
	assert(SUCCEEDED(hr));
	return SUCCEEDED(hr);
}

void GpuParticleSystem::DispatchInitializeFreeListIfNeeded(ID3D12GraphicsCommandList* commandList) {
	if (!needsInitializeFreeListDispatch_) {
		return;
	}

	ResetFreeListCounter(commandList);

	commandList->SetComputeRootSignature(initializeFreeListRootSignature_.Get());
	commandList->SetPipelineState(initializeFreeListPipelineState_.Get());
	commandList->SetComputeRootDescriptorTable(0, freeListUavHandle_);
	commandList->Dispatch((kParticleCount + 1023) / 1024, 1, 1);

	D3D12_RESOURCE_BARRIER uavBarrier{};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = nullptr;
	commandList->ResourceBarrier(1, &uavBarrier);

	needsInitializeFreeListDispatch_ = false;
	isFreeListInitialized_ = true;
	freeListRemainingEstimate_ = kParticleCount;
	emitBatchEstimates_.clear();
}

void GpuParticleSystem::DispatchInitializeIfNeeded(ID3D12GraphicsCommandList* commandList) {
	if (!needsInitializeDispatch_) {
		return;
	}

	TransitionParticleResource(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	initializeInfoData_->particleCount = kParticleCount;
	initializeInfoData_->randomEnabled = isRandomInitializeEnabled_ ? 1u : 0u;
	initializeInfoData_->randomSeed = randomSeed_;
	initializeInfoData_->emitterEnabled = (isEmitterEnabled_ || useFreeListEmit_) ? 1u : 0u;
	initializeInfoData_->emitterPosition = emitterPosition_;
	initializeInfoData_->emitterRadius = emitterRadius_;
	initializeInfoData_->emitCount = useFreeListEmit_ ? 0u : (std::min)(emitCount_, kParticleCount);

	commandList->SetComputeRootSignature(initializeRootSignature_.Get());
	commandList->SetPipelineState(initializePipelineState_.Get());
	commandList->SetComputeRootDescriptorTable(0, particleUavHandle_);
	commandList->SetComputeRootConstantBufferView(1, initializeInfoResource_->GetGPUVirtualAddress());
	commandList->Dispatch((kParticleCount + 1023) / 1024, 1, 1);

	D3D12_RESOURCE_BARRIER uavBarrier{};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = nullptr;
	commandList->ResourceBarrier(1, &uavBarrier);

	TransitionParticleResource(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	needsInitializeDispatch_ = false;
	elapsedTimeSinceInitialize_ = 0.0f;
	emitTimer_ = 0.0f;
	activeCountEstimate_ = useFreeListEmit_ ? 0u : (isEmitterEnabled_ ? (std::min)(emitCount_, kParticleCount) : kParticleCount);
}

void GpuParticleSystem::DispatchEmitIfNeeded(ID3D12GraphicsCommandList* commandList) {
	if (!useFreeListEmit_ || !needsEmitDispatch_ || needsInitializeDispatch_) {
		return;
	}

	uint32_t actualEmitCount = (std::min)(emitCount_, kParticleCount);
	actualEmitCount = (std::min)(actualEmitCount, freeListRemainingEstimate_);
	if (actualEmitCount == 0) {
		needsEmitDispatch_ = false;
		lastEmitDispatchCount_ = 0;
		return;
	}

	TransitionParticleResource(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	emitterInfoData_->emitCount = actualEmitCount;
	emitterInfoData_->randomSeed = randomSeed_;
	emitterInfoData_->randomEnabled = isRandomInitializeEnabled_ ? 1u : 0u;
	emitterInfoData_->emitterPosition = emitterPosition_;
	emitterInfoData_->emitterRadius = emitterRadius_;

	commandList->SetComputeRootSignature(emitRootSignature_.Get());
	commandList->SetPipelineState(emitPipelineState_.Get());
	commandList->SetComputeRootDescriptorTable(0, particleUavHandle_);
	commandList->SetComputeRootDescriptorTable(1, freeListUavHandle_);
	commandList->SetComputeRootConstantBufferView(2, emitterInfoResource_->GetGPUVirtualAddress());
	commandList->Dispatch((actualEmitCount + 1023) / 1024, 1, 1);

	D3D12_RESOURCE_BARRIER uavBarrier{};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = nullptr;
	commandList->ResourceBarrier(1, &uavBarrier);

	freeListRemainingEstimate_ -= actualEmitCount;
	activeCountEstimate_ = (std::min)(activeCountEstimate_ + actualEmitCount, kParticleCount);
	lastEmitDispatchCount_ = actualEmitCount;
	emitBatchEstimates_.push_back({ actualEmitCount, 0.0f, isRandomInitializeEnabled_ ? 3.5f : 2.0f });
	needsEmitDispatch_ = false;
	emitTimer_ = 0.0f;
}

void GpuParticleSystem::DispatchUpdate(ID3D12GraphicsCommandList* commandList) {
	if (!isUpdateEnabled_ || needsInitializeDispatch_) {
		return;
	}

	TransitionParticleResource(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	commandList->SetComputeRootSignature(updateRootSignature_.Get());
	commandList->SetPipelineState(updatePipelineState_.Get());
	commandList->SetComputeRootDescriptorTable(0, particleUavHandle_);
	commandList->SetComputeRootDescriptorTable(1, freeListUavHandle_);
	commandList->SetComputeRootDescriptorTable(2, deadListUavHandle_);
	commandList->SetComputeRootConstantBufferView(3, updateInfoResource_->GetGPUVirtualAddress());
	commandList->Dispatch((kParticleCount + 1023) / 1024, 1, 1);

	D3D12_RESOURCE_BARRIER uavBarrier{};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = nullptr;
	commandList->ResourceBarrier(1, &uavBarrier);

	TransitionParticleResource(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	elapsedTimeSinceInitialize_ += deltaTime_;
	if (useFreeListEmit_) {
		UpdateFreeListEstimate(deltaTime_);
	} else {
		activeCountEstimate_ = EstimateActiveParticleCount();
	}
}

void GpuParticleSystem::DispatchRecycleIfNeeded(ID3D12GraphicsCommandList* commandList) {
	if (!useFreeListEmit_ || !useDeadList_) {
		needsRecycleDispatch_ = false;
		return;
	}
	if (autoRecycleDeadList_ && deadListCountEstimate_ > 0) {
		needsRecycleDispatch_ = true;
	}
	if (!needsRecycleDispatch_ || deadListCountEstimate_ == 0) {
		needsRecycleDispatch_ = false;
		return;
	}

	uint32_t actualRecycleCount = (std::min)(recycleCount_, deadListCountEstimate_);
	actualRecycleCount = (std::min)(actualRecycleCount, kParticleCount - freeListRemainingEstimate_);
	if (actualRecycleCount == 0) {
		needsRecycleDispatch_ = false;
		return;
	}

	recycleInfoData_->recycleCount = actualRecycleCount;

	commandList->SetComputeRootSignature(recycleRootSignature_.Get());
	commandList->SetPipelineState(recyclePipelineState_.Get());
	commandList->SetComputeRootDescriptorTable(0, deadListUavHandle_);
	commandList->SetComputeRootDescriptorTable(1, freeListUavHandle_);
	commandList->SetComputeRootConstantBufferView(2, recycleInfoResource_->GetGPUVirtualAddress());
	commandList->Dispatch((actualRecycleCount + 1023) / 1024, 1, 1);

	D3D12_RESOURCE_BARRIER uavBarrier{};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = nullptr;
	commandList->ResourceBarrier(1, &uavBarrier);

	deadListCountEstimate_ -= actualRecycleCount;
	freeListRemainingEstimate_ = (std::min)(freeListRemainingEstimate_ + actualRecycleCount, kParticleCount);
	lastRecycleDispatchCount_ = actualRecycleCount;
	needsRecycleDispatch_ = false;
}

void GpuParticleSystem::TransitionParticleResource(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES afterState) {
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

void GpuParticleSystem::TransitionFreeListCounterResource(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES afterState) {
	if (freeListCounterResourceState_ == afterState) {
		return;
	}

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = freeListCounterResource_.Get();
	barrier.Transition.StateBefore = freeListCounterResourceState_;
	barrier.Transition.StateAfter = afterState;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);
	freeListCounterResourceState_ = afterState;
}

void GpuParticleSystem::TransitionDeadListCounterResource(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES afterState) {
	if (deadListCounterResourceState_ == afterState) {
		return;
	}

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = deadListCounterResource_.Get();
	barrier.Transition.StateBefore = deadListCounterResourceState_;
	barrier.Transition.StateAfter = afterState;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);
	deadListCounterResourceState_ = afterState;
}

void GpuParticleSystem::ResetFreeListCounter(ID3D12GraphicsCommandList* commandList) {
	TransitionFreeListCounterResource(commandList, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->CopyBufferRegion(
		freeListCounterResource_.Get(),
		0,
		freeListCounterResetResource_.Get(),
		0,
		sizeof(uint32_t));
	TransitionFreeListCounterResource(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void GpuParticleSystem::ResetDeadListCounter(ID3D12GraphicsCommandList* commandList) {
	TransitionDeadListCounterResource(commandList, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->CopyBufferRegion(
		deadListCounterResource_.Get(),
		0,
		deadListCounterResetResource_.Get(),
		0,
		sizeof(uint32_t));
	TransitionDeadListCounterResource(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void GpuParticleSystem::ResetDeadListCounterIfNeeded(ID3D12GraphicsCommandList* commandList) {
	if (!needsDeadListCounterReset_) {
		return;
	}

	ResetDeadListCounter(commandList);

	D3D12_RESOURCE_BARRIER uavBarrier{};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = nullptr;
	commandList->ResourceBarrier(1, &uavBarrier);

	needsDeadListCounterReset_ = false;
	isDeadListReady_ = true;
	deadListCountEstimate_ = 0;
}

void GpuParticleSystem::RequestInitialize() {
	needsInitializeDispatch_ = true;
	needsDeadListCounterReset_ = true;
	isDeadListReady_ = false;
	elapsedTimeSinceInitialize_ = 0.0f;
	emitTimer_ = 0.0f;
	activeCountEstimate_ = useFreeListEmit_ ? 0u : (isEmitterEnabled_ ? (std::min)(emitCount_, kParticleCount) : kParticleCount);
	deadListCountEstimate_ = 0;
	if (useFreeListEmit_) {
		emitBatchEstimates_.clear();
	}
}

void GpuParticleSystem::RequestEmit() {
	needsEmitDispatch_ = true;
	lastEmitDispatchCount_ = 0;
}

void GpuParticleSystem::RequestRecycle() {
	needsRecycleDispatch_ = true;
	lastRecycleDispatchCount_ = 0;
}

void GpuParticleSystem::UpdateFreeListEstimate(float deltaTime) {
	uint32_t returnedCount = 0;
	for (EmitBatchEstimate& batch : emitBatchEstimates_) {
		batch.elapsedTime += deltaTime;
		if (batch.elapsedTime >= batch.returnDelay) {
			returnedCount += batch.particleCount;
			batch.particleCount = 0;
		}
	}

	emitBatchEstimates_.erase(
		std::remove_if(
			emitBatchEstimates_.begin(),
			emitBatchEstimates_.end(),
			[](const EmitBatchEstimate& batch) { return batch.particleCount == 0; }),
		emitBatchEstimates_.end());

	if (returnedCount == 0) {
		return;
	}

	activeCountEstimate_ = returnedCount >= activeCountEstimate_ ? 0u : activeCountEstimate_ - returnedCount;
	if (useDeadList_) {
		deadListCountEstimate_ = (std::min)(deadListCountEstimate_ + returnedCount, kParticleCount);
	} else {
		freeListRemainingEstimate_ = (std::min)(freeListRemainingEstimate_ + returnedCount, kParticleCount);
	}
}

uint32_t GpuParticleSystem::EstimateActiveParticleCount() const {
	uint32_t activeCount = 0;
	constexpr uint32_t kColumns = 32;
	const uint32_t activeLimit = isEmitterEnabled_ ? (std::min)(emitCount_, kParticleCount) : kParticleCount;
	for (uint32_t index = 0; index < kParticleCount; ++index) {
		if (index >= activeLimit) {
			continue;
		}
		uint32_t x = index % kColumns;
		uint32_t y = index / kColumns;
		uint32_t baseSeed = index ^ (randomSeed_ * 747796405u) ^ 2891336453u;
		float lifeRate = static_cast<float>((x + y) % 16) / 15.0f;
		float lifeTime = isRandomInitializeEnabled_
			? RandomRange(baseSeed ^ 0x85ebca6bu, 1.0f, 3.5f)
			: 1.5f + (3.0f - 1.5f) * lifeRate;
		if (elapsedTimeSinceInitialize_ < lifeTime) {
			++activeCount;
		}
	}
	return activeCount;
}
