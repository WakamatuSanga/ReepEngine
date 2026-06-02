#include "GpuParticleSystem.h"

#include "Engine/Core/DirectXCommon.h"
#include "GpuParticleCompute.h"
#include "GpuParticleEffectData.h"
#include "GpuParticleEditor.h"
#include "GpuParticleRenderer.h"
#include "GpuParticleResources.h"
#include "Engine/Core/SrvManager.h"

#include <algorithm>
#include <cassert>

namespace {

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

GpuParticleSystem::GpuParticleSystem() = default;
GpuParticleSystem::~GpuParticleSystem() = default;

bool GpuParticleSystem::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	assert(dxCommon);
	assert(srvManager);
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	GpuParticle::EnsureDefaultParticleTypes(state_);
	GpuParticle::EnsureDefaultEmitter(state_);

	resources_ = std::make_unique<GpuParticleResources>();
	compute_ = std::make_unique<GpuParticleCompute>();
	renderer_ = std::make_unique<GpuParticleRenderer>();
	editor_ = std::make_unique<GpuParticleEditor>();
	if (!resources_->Initialize(dxCommon_, srvManager_, state_)) {
		return false;
	}
	if (!compute_->Initialize(dxCommon_, state_)) {
		return false;
	}
	if (!renderer_->Initialize(dxCommon_)) {
		return false;
	}

	isInitialized_ = true;
	state_.needsInitializeDispatch = true;
	return true;
}

void GpuParticleSystem::ApplyEffectData(const GpuParticle::ParticleEffectData& effectData) {
	GpuParticle::ApplyParticleEffectDataToState(effectData, state_);
	if (renderer_) {
		renderer_->ReloadParticleTypeTextures(state_);
	}
	if (resources_) {
		resources_->UploadParticleTypes(state_);
	}
}

void GpuParticleSystem::Update(const Camera* camera) {
	if (!isInitialized_) {
		return;
	}

	renderer_->UpdateView(camera);
	state_.deltaTime = std::clamp(state_.deltaTime, 0.0f, 1.0f / 15.0f);
	if (!state_.isUpdateEnabled || !state_.isEmitterEnabled || state_.needsInitializeDispatch) {
		return;
	}

	for (GpuParticle::Emitter& emitter : state_.emitters) {
		if (!emitter.enabled || emitter.emitInterval <= 0.0f) {
			continue;
		}
		emitter.emitTimer += state_.deltaTime;
		if (emitter.emitTimer < emitter.emitInterval) {
			continue;
		}
		if (state_.useFreeListEmit) {
			emitter.pendingEmit = true;
			state_.needsEmitDispatch = true;
		} else {
			GpuParticle::RequestInitialize(state_);
			break;
		}
	}
}

void GpuParticleSystem::Draw() {
	if (!isInitialized_ || !state_.isEnabled) {
		return;
	}

	resources_->RefreshCounterReadbackValues(state_);
	GpuParticle::ClampEmitterParticleTypeIndices(state_);
	renderer_->RefreshParticleTypeTextures(state_);
	resources_->UploadParticleTypes(state_);

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	ID3D12DescriptorHeap* descriptorHeaps[] = { srvManager_->GetSrvDescriptorHeap() };
	commandList->SetDescriptorHeaps(1, descriptorHeaps);
	compute_->Dispatch(commandList, *resources_, state_);
	if (!state_.useFreeListEmit && state_.isUpdateEnabled && !state_.needsInitializeDispatch) {
		state_.activeCountEstimate = EstimateActiveParticleCount();
	}

	if (state_.autoReadbackCounters || state_.needsCounterReadback) {
		resources_->CopyCounterResourcesForReadback(commandList);
		state_.needsCounterReadback = false;
		state_.isCounterReadbackPending = true;
	}
	renderer_->Draw(commandList, *resources_, state_);
}

void GpuParticleSystem::DrawImGui() {
	if (editor_ && renderer_ && resources_) {
		editor_->DrawImGui(state_, *resources_, *renderer_, renderer_->GetTextureIndex());
	}
}

uint32_t GpuParticleSystem::EstimateActiveParticleCount() const {
	constexpr uint32_t kColumns = 32;
	const GpuParticle::Emitter& primaryEmitter = GpuParticle::GetPrimaryEmitter(state_);
	const GpuParticle::ParticleType& type = GpuParticle::GetParticleType(state_, primaryEmitter.particleTypeIndex);
	const uint32_t limit = state_.isEmitterEnabled
		? (std::min)(primaryEmitter.emitCount, GpuParticle::kParticleCount)
		: GpuParticle::kParticleCount;
	uint32_t activeCount = 0;
	for (uint32_t index = 0; index < GpuParticle::kParticleCount; ++index) {
		if (index >= limit) {
			continue;
		}
		const uint32_t x = index % kColumns;
		const uint32_t y = index / kColumns;
		const uint32_t baseSeed = index ^ (primaryEmitter.randomSeed * 747796405u) ^ 2891336453u;
		const float lifeRate = static_cast<float>((x + y) % 16) / 15.0f;
		const float lifeTime = state_.isRandomInitializeEnabled
			? RandomRange(baseSeed ^ 0x85ebca6bu, type.lifeTimeMin, type.lifeTimeMax)
			: type.lifeTimeMin + (type.lifeTimeMax - type.lifeTimeMin) * lifeRate;
		if (state_.elapsedTimeSinceInitialize < lifeTime) {
			++activeCount;
		}
	}
	return activeCount;
}
