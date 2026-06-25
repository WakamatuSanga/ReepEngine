#pragma once

#include "GpuParticleTypes.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace GpuParticle {

struct ParticleEffectRuntimeSettings {
	bool randomEnabled = true;
	bool useFreeListEmit = false;
	bool useDeadList = false;
	bool autoRecycleDeadList = false;
	bool updateEnabled = true;
};

struct ParticleEffectData {
	std::vector<Emitter> emitters;
	std::vector<ParticleType> particleTypes;
	ParticleEffectRuntimeSettings runtime;
};

inline ParticleType MakeDefaultParticleEffectType(size_t index) {
	State defaultState;
	EnsureDefaultParticleTypes(defaultState);

	ParticleType type = GetParticleType(defaultState, static_cast<uint32_t>(index));
	if (index >= defaultState.particleTypes.size()) {
		type.name = "Particle Type " + std::to_string(index);
	}
	return type;
}

inline void NormalizeParticleEffectType(ParticleType& type) {
	if (type.textureIndex < 0) {
		type.textureIndex = 0;
	}
	type.startScale = (std::max)(type.startScale, 0.001f);
	type.endScale = (std::max)(type.endScale, 0.001f);
	type.lifeTimeMin = (std::max)(type.lifeTimeMin, 0.01f);
	type.lifeTimeMax = (std::max)(type.lifeTimeMax, type.lifeTimeMin);
	type.speedMin = (std::max)(type.speedMin, 0.0f);
	type.speedMax = (std::max)(type.speedMax, type.speedMin);
	type.drag = (std::max)(type.drag, 0.0f);
	type.restitution = std::clamp(type.restitution, 0.0f, 1.0f);
	type.friction = std::clamp(type.friction, 0.0f, 1.0f);
	type.bounceVelocityThreshold = (std::max)(type.bounceVelocityThreshold, 0.0f);
	type.maxBounceCount = std::clamp(type.maxBounceCount, 0u, 255u);
	type.collisionDamping = std::clamp(type.collisionDamping, 0.0f, 2.0f);
	type.influenceResponseScale = std::clamp(type.influenceResponseScale, 0.0f, 10.0f);
	type.railFlowScale = std::clamp(type.railFlowScale, 0.0f, 10.0f);
	type.atlasRows = std::clamp(type.atlasRows, 1u, 64u);
	type.atlasColumns = std::clamp(type.atlasColumns, 1u, 64u);
	type.frameCount = std::clamp(type.frameCount, 1u, type.atlasRows * type.atlasColumns);
	type.frameSpeed = (std::max)(type.frameSpeed, 0.0f);
}

inline void NormalizeParticleEffectEmitter(Emitter& emitter) {
	emitter.radius = (std::max)(emitter.radius, 0.0f);
	emitter.shape = ClampEmitterShape(emitter.shape);
	emitter.boxSize.x = (std::max)(emitter.boxSize.x, 0.0f);
	emitter.boxSize.y = (std::max)(emitter.boxSize.y, 0.0f);
	emitter.boxSize.z = (std::max)(emitter.boxSize.z, 0.0f);
	emitter.coneHeight = (std::max)(emitter.coneHeight, 0.001f);
	emitter.emitCount = (std::min)(emitter.emitCount, kParticleCount);
	emitter.emitInterval = (std::max)(emitter.emitInterval, 0.0f);
	emitter.emissionRate = (std::max)(emitter.emissionRate, 0.0f);
	emitter.emitTimer = 0.0f;
	emitter.emissionAccumulator = 0.0f;
	emitter.pendingEmitCount = 0;
	emitter.pendingEmit = false;
}

inline void NormalizeParticleEffectData(ParticleEffectData& effectData) {
	if (effectData.particleTypes.size() > kMaxParticleTypes) {
		effectData.particleTypes.resize(kMaxParticleTypes);
	}
	if (effectData.particleTypes.empty()) {
		State defaultState;
		EnsureDefaultParticleTypes(defaultState);
		effectData.particleTypes = defaultState.particleTypes;
	}
	if (effectData.emitters.empty()) {
		effectData.emitters.push_back(Emitter{});
	}

	for (ParticleType& type : effectData.particleTypes) {
		NormalizeParticleEffectType(type);
	}
	for (Emitter& emitter : effectData.emitters) {
		NormalizeParticleEffectEmitter(emitter);
		emitter.particleTypeIndex = (std::min)(emitter.particleTypeIndex, static_cast<uint32_t>(effectData.particleTypes.size() - 1));
	}
}

inline ParticleEffectData CreateParticleEffectDataFromState(const State& state) {
	ParticleEffectData effectData;
	effectData.emitters = state.emitters;
	effectData.particleTypes = state.particleTypes;
	effectData.runtime.randomEnabled = state.isRandomInitializeEnabled;
	effectData.runtime.useFreeListEmit = state.useFreeListEmit;
	effectData.runtime.useDeadList = state.useDeadList;
	effectData.runtime.autoRecycleDeadList = state.autoRecycleDeadList;
	effectData.runtime.updateEnabled = state.isUpdateEnabled;
	NormalizeParticleEffectData(effectData);
	return effectData;
}

inline void ResetTransientStateForEffect(State& state) {
	state.needsEmitDispatch = false;
	state.needsRecycleDispatch = false;
	state.needsCounterReadback = false;
	state.isCounterReadbackPending = false;
	state.isCounterReadbackValid = false;
	state.actualFreeListCount = 0;
	state.actualDeadListCount = 0;
	state.lastEmitDispatchCount = 0;
	state.lastRecycleDispatchCount = 0;
	ResetListsForFreeListMode(state);
	RequestInitialize(state);
	if (state.useFreeListEmit) {
		RequestEmit(state);
	}
}

inline void ApplyParticleEffectDataToState(const ParticleEffectData& effectData, State& state) {
	ParticleEffectData normalizedEffect = effectData;
	NormalizeParticleEffectData(normalizedEffect);

	state.emitters = std::move(normalizedEffect.emitters);
	state.particleTypes = std::move(normalizedEffect.particleTypes);
	state.isRandomInitializeEnabled = normalizedEffect.runtime.randomEnabled;
	state.useFreeListEmit = normalizedEffect.runtime.useFreeListEmit;
	state.useDeadList = normalizedEffect.runtime.useDeadList;
	state.autoRecycleDeadList = normalizedEffect.runtime.autoRecycleDeadList;
	state.isUpdateEnabled = normalizedEffect.runtime.updateEnabled;

	ClampEmitterParticleTypeIndices(state);
	ResetTransientStateForEffect(state);
}

} // namespace GpuParticle
