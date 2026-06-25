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

uint32_t ConsumeEmissionRateCount(GpuParticle::Emitter& emitter, float deltaTime) {
	if (emitter.emissionRate <= 0.0f || deltaTime <= 0.0f) {
		return 0;
	}

	emitter.emissionAccumulator += emitter.emissionRate * deltaTime;
	const uint32_t emitCount = static_cast<uint32_t>(emitter.emissionAccumulator);
	if (emitCount == 0) {
		return 0;
	}
	emitter.emissionAccumulator -= static_cast<float>(emitCount);
	return (std::min)(emitCount, GpuParticle::kParticleCount);
}

void QueueEmitterEmit(GpuParticle::State& state, GpuParticle::Emitter& emitter, uint32_t emitCount) {
	if (emitCount == 0) {
		return;
	}
	emitter.pendingEmit = true;
	emitter.pendingEmitCount = (std::min)(emitter.pendingEmitCount + emitCount, GpuParticle::kParticleCount);
	state.needsEmitDispatch = true;
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

bool GpuParticleSystem::PlayEffectDataAt(const GpuParticle::ParticleEffectData& effectData, const Vector3& position) {
	if (!isInitialized_) {
		return false;
	}

	ApplyEffectData(effectData);
	for (GpuParticle::Emitter& emitter : state_.emitters) {
		emitter.position = position;
	}
	state_.isEnabled = true;
	state_.isEmitterEnabled = true;
	state_.isUpdateEnabled = true;
	GpuParticle::RequestInitialize(state_);
	if (state_.useFreeListEmit) {
		GpuParticle::RequestEmit(state_);
	}
	return true;
}

bool GpuParticleSystem::SetEmitterRuntime(size_t index, const GpuParticle::Emitter& emitter) {
	if (index >= state_.emitters.size()) {
		return false;
	}

	GpuParticle::Emitter& target = state_.emitters[index];
	target.enabled = emitter.enabled;
	target.position = emitter.position;
	target.direction = emitter.direction;
	target.radius = (std::max)(emitter.radius, 0.0f);
	target.shape = GpuParticle::ClampEmitterShape(emitter.shape);
	target.boxSize = {
		(std::max)(emitter.boxSize.x, 0.0f),
		(std::max)(emitter.boxSize.y, 0.0f),
		(std::max)(emitter.boxSize.z, 0.0f),
	};
	target.coneHeight = (std::max)(emitter.coneHeight, 0.001f);
	target.emitCount = (std::min)(emitter.emitCount, GpuParticle::kParticleCount);
	target.emitInterval = (std::max)(emitter.emitInterval, 0.0f);
	target.emissionRate = (std::max)(emitter.emissionRate, 0.0f);
	target.randomSeed = emitter.randomSeed;
	const uint32_t maxTypeIndex = state_.particleTypes.empty() ? 0u : static_cast<uint32_t>(state_.particleTypes.size() - 1);
	target.particleTypeIndex = (std::min)(emitter.particleTypeIndex, maxTypeIndex);
	return true;
}

bool GpuParticleSystem::SetParticleTypeRuntime(size_t index, const GpuParticle::ParticleType& type) {
	if (index >= state_.particleTypes.size()) {
		return false;
	}

	GpuParticle::ParticleType normalizedType = type;
	GpuParticle::NormalizeParticleEffectType(normalizedType);
	const int textureIndex = state_.particleTypes[index].textureIndex;
	if (normalizedType.texturePath == state_.particleTypes[index].texturePath) {
		normalizedType.textureIndex = textureIndex;
	}
	state_.particleTypes[index] = normalizedType;
	return true;
}

void GpuParticleSystem::SetDeltaTime(float deltaTime) {
	state_.deltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
}

void GpuParticleSystem::SetInfluenceFields(const Vector4* centersAndRadius, const Vector4* params, uint32_t count) {
	const uint32_t clampedCount = std::clamp(count, 0u, GpuParticle::kMaxInfluenceFields);
	state_.influenceFieldCount = clampedCount;
	state_.influenceCentersAndRadius.fill({ 0.0f, 0.0f, 0.0f, 0.0f });
	state_.influenceParams.fill({ 0.0f, 0.0f, 1.0f, 0.0f });
	if (!centersAndRadius || !params) {
		state_.influenceFieldCount = 0;
		return;
	}
	for (uint32_t index = 0; index < clampedCount; ++index) {
		state_.influenceCentersAndRadius[index] = centersAndRadius[index];
		state_.influenceParams[index] = params[index];
	}
}

void GpuParticleSystem::SetParticleInfluenceEnabled(bool enabled) {
	state_.enableParticleInfluence = enabled;
}

void GpuParticleSystem::SetParticleInfluenceResponseScale(float scale) {
	state_.particleInfluenceResponseScale = std::clamp(scale, 0.0f, 10.0f);
}

void GpuParticleSystem::SetRailParticleFlow(bool enabled, const Vector3& cameraPosition, const Vector3& direction, float speed, float scale, float spawnAheadDistance, float despawnBehindDistance) {
	state_.enableRailParticleFlow = enabled;
	state_.railFlowCameraPosition = cameraPosition;
	state_.railFlowDirection = direction;
	state_.railFlowSpeed = (std::max)(speed, 0.0f);
	state_.railFlowScale = std::clamp(scale, 0.0f, 10.0f);
	state_.railSpawnAheadDistance = (std::max)(spawnAheadDistance, 0.0f);
	state_.railDespawnBehindDistance = (std::max)(despawnBehindDistance, 0.0f);
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
		if (!emitter.enabled) {
			continue;
		}

		if (emitter.emissionRate > 0.0f) {
			const uint32_t emitCount = ConsumeEmissionRateCount(emitter, state_.deltaTime);
			if (emitCount == 0) {
				continue;
			}
			if (state_.useFreeListEmit) {
				QueueEmitterEmit(state_, emitter, emitCount);
			} else {
				const float remainingAccumulator = emitter.emissionAccumulator;
				GpuParticle::RequestInitialize(state_);
				emitter.pendingEmit = true;
				emitter.pendingEmitCount = emitCount;
				emitter.emissionAccumulator = remainingAccumulator;
				break;
			}
			continue;
		}

		if (emitter.emitInterval <= 0.0f) {
			continue;
		}
		emitter.emitTimer += state_.deltaTime;
		if (emitter.emitTimer < emitter.emitInterval) {
			continue;
		}
		if (state_.useFreeListEmit) {
			QueueEmitterEmit(state_, emitter, emitter.emitCount);
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
