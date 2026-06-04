#pragma once

#include "Matrix4x4.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace GpuParticle {

inline constexpr uint32_t kParticleCount = 1024;
inline constexpr uint32_t kMaxParticleTypes = 8;
inline constexpr uint32_t kMaxParticleTextureDescriptors = 512;
inline constexpr const char* kFallbackParticleTexturePath = "resources/particle/circle2.png";
inline constexpr uint32_t kParticleDebugViewNormal = 0;
inline constexpr uint32_t kParticleDebugViewSolidColor = 1;
inline constexpr uint32_t kParticleDebugViewTextureAlpha = 2;
inline constexpr uint32_t kParticleDebugViewTextureRgb = 3;
inline constexpr uint32_t kParticleDebugViewFinalAlpha = 4;
inline constexpr uint32_t kParticleDebugViewForceMagenta = 5;
inline constexpr uint32_t kParticleDebugViewForceDiscardAll = 6;
inline constexpr uint32_t kParticleDebugViewParticleColor = 7;
inline constexpr uint32_t kParticleDebugViewTextureAlphaTransparent = 8;
inline constexpr uint32_t kParticleDebugViewProceduralCircleMask = 9;

struct Particle {
	Vector3 translate;
	float scale;
	Vector3 velocity;
	float currentTime;
	Vector4 color;
	float lifeTime;
	uint32_t alive;
	uint32_t type;
	float padding;
};
static_assert(sizeof(Particle) == 64, "Gpu particle stride must match the HLSL StructuredBuffer layout.");

struct ParticleType {
	std::string name;
	std::string texturePath;
	int textureIndex = 0;
	Vector4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4 startColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4 endColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	float startScale;
	float endScale;
	float lifeTimeMin;
	float lifeTimeMax;
	float speedMin;
	float speedMax;
	float gravity;
	float drag = 0.0f;
	bool useAtlas = false;
	uint32_t atlasRows = 1;
	uint32_t atlasColumns = 1;
	uint32_t frameCount = 1;
	float frameSpeed = 8.0f;
	bool loopAtlas = true;
};

struct ParticleTypeForGPU {
	Vector4 baseColor;
	Vector4 startColor;
	Vector4 endColor;
	float startScale;
	float endScale;
	float lifeTimeMin;
	float lifeTimeMax;
	float speedMin;
	float speedMax;
	float gravity;
	float drag;
	uint32_t useAtlas;
	uint32_t atlasRows;
	uint32_t atlasColumns;
	uint32_t frameCount;
	float frameSpeed;
	uint32_t loopAtlas;
	uint32_t textureIndex;
	uint32_t padding1;
	float materialPadding[4];
};
static_assert(sizeof(ParticleTypeForGPU) == 128, "Gpu particle type stride must match the HLSL StructuredBuffer layout.");
static_assert(offsetof(ParticleTypeForGPU, startColor) == 16, "startColor offset must match HLSL ParticleType.");
static_assert(offsetof(ParticleTypeForGPU, endColor) == 32, "endColor offset must match HLSL ParticleType.");
static_assert(offsetof(ParticleTypeForGPU, drag) == 76, "drag offset must match HLSL ParticleType.");
static_assert(offsetof(ParticleTypeForGPU, useAtlas) == 80, "useAtlas offset must match HLSL ParticleType.");
static_assert(offsetof(ParticleTypeForGPU, atlasRows) == 84, "atlasRows offset must match HLSL ParticleType.");
static_assert(offsetof(ParticleTypeForGPU, atlasColumns) == 88, "atlasColumns offset must match HLSL ParticleType.");
static_assert(offsetof(ParticleTypeForGPU, frameCount) == 92, "frameCount offset must match HLSL ParticleType.");
static_assert(offsetof(ParticleTypeForGPU, frameSpeed) == 96, "frameSpeed offset must match HLSL ParticleType.");
static_assert(offsetof(ParticleTypeForGPU, loopAtlas) == 100, "loopAtlas offset must match HLSL ParticleType.");
static_assert(offsetof(ParticleTypeForGPU, textureIndex) == 104, "textureIndex offset must match HLSL ParticleType.");

enum class EmitterShape : uint32_t {
	Sphere = 0,
	Box = 1,
	Cone = 2,
};
inline constexpr uint32_t kEmitterShapeCount = 3;

struct InitializeInfo {
	uint32_t particleCount;
	uint32_t randomEnabled;
	uint32_t randomSeed;
	uint32_t emitterEnabled;
	Vector3 emitterPosition;
	float emitterRadius;
	Vector3 emitterBoxSize;
	float emitterConeHeight;
	uint32_t emitCount;
	uint32_t particleTypeIndex;
	uint32_t emitterShape;
	float padding;
};
static_assert(sizeof(InitializeInfo) == 64, "InitializeInfo must match the HLSL constant buffer layout.");
static_assert(offsetof(InitializeInfo, emitterBoxSize) == 32, "emitterBoxSize offset must match HLSL InitializeInfo.");
static_assert(offsetof(InitializeInfo, emitCount) == 48, "emitCount offset must match HLSL InitializeInfo.");

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
	uint32_t particleTypeIndex;
	Vector3 emitterPosition;
	float emitterRadius;
	Vector3 emitterBoxSize;
	float emitterConeHeight;
	uint32_t emitterShape;
	float padding[3];
};
static_assert(sizeof(EmitterInfo) == 64, "EmitterInfo must match the HLSL constant buffer layout.");
static_assert(offsetof(EmitterInfo, emitterBoxSize) == 32, "emitterBoxSize offset must match HLSL EmitterInfo.");
static_assert(offsetof(EmitterInfo, emitterShape) == 48, "emitterShape offset must match HLSL EmitterInfo.");

struct Emitter {
	bool enabled = true;
	Vector3 position = { 0.0f, 1.5f, 3.0f };
	float radius = 0.8f;
	EmitterShape shape = EmitterShape::Sphere;
	Vector3 boxSize = { 1.0f, 1.0f, 1.0f };
	float coneHeight = 1.2f;
	uint32_t emitCount = 256;
	float emitInterval = 1.5f;
	float emissionRate = 0.0f;
	float emitTimer = 0.0f;
	float emissionAccumulator = 0.0f;
	uint32_t pendingEmitCount = 0;
	uint32_t randomSeed = 1;
	uint32_t particleTypeIndex = 0;
	bool pendingEmit = false;
};

struct RecycleInfo {
	uint32_t recycleCount;
	float padding[3];
};

struct PerView {
	Matrix4x4 viewProjection;
	Matrix4x4 billboardMatrix;
};

struct ParticleDebugInfo {
	uint32_t debugViewMode = kParticleDebugViewNormal;
	float padding[3]{};
};
static_assert(sizeof(ParticleDebugInfo) == 16, "Gpu particle debug info must match the HLSL constant buffer layout.");

struct EmitBatchEstimate {
	uint32_t particleCount = 0;
	float elapsedTime = 0.0f;
	float returnDelay = 0.0f;
};

struct State {
	bool isEnabled = true;
	bool isUpdateEnabled = true;
	bool isRandomInitializeEnabled = true;
	bool isEmitterEnabled = true;
	bool useFreeListEmit = false;
	bool useDeadList = false;
	bool autoRecycleDeadList = false;
	bool autoReadbackCounters = false;
	bool needsInitializeDispatch = true;
	bool needsInitializeFreeListDispatch = true;
	bool needsDeadListCounterReset = true;
	bool needsEmitDispatch = false;
	bool needsRecycleDispatch = false;
	bool needsCounterReadback = false;
	bool isCounterReadbackPending = false;
	bool isCounterReadbackValid = false;
	bool isFreeListInitialized = false;
	bool isDeadListReady = false;
	float deltaTime = 1.0f / 60.0f;
	float elapsedTimeSinceInitialize = 0.0f;
	uint32_t activeCountEstimate = kParticleCount;
	uint32_t freeListRemainingEstimate = 0;
	uint32_t deadListCountEstimate = 0;
	uint32_t lastEmitDispatchCount = 0;
	uint32_t recycleCount = kParticleCount;
	uint32_t lastRecycleDispatchCount = 0;
	uint32_t actualFreeListCount = 0;
	uint32_t actualDeadListCount = 0;
	uint32_t particleDebugViewMode = kParticleDebugViewNormal;
	std::vector<ParticleType> particleTypes;
	std::vector<Emitter> emitters;
	std::vector<EmitBatchEstimate> emitBatchEstimates;
};

inline uint32_t AlignConstantBufferSize(uint32_t size) {
	return (size + 0xff) & ~0xff;
}

inline void EnsureDefaultParticleTypes(State& state) {
	if (!state.particleTypes.empty()) {
		return;
	}

	state.particleTypes.push_back({
		"Spark",
		"",
		0,
		{ 1.0f, 0.60f, 0.18f, 0.85f },
		{ 1.0f, 0.82f, 0.18f, 0.90f },
		{ 1.0f, 0.08f, 0.02f, 0.0f },
		0.065f,
		0.018f,
		0.45f,
		1.25f,
		0.55f,
		1.60f,
		-2.8f,
		0.20f,
		});
	state.particleTypes.push_back({
		"Mist",
		"",
		0,
		{ 0.20f, 0.85f, 1.0f, 0.72f },
		{ 0.28f, 0.34f, 0.36f, 0.70f },
		{ 0.78f, 0.82f, 0.84f, 0.0f },
		0.055f,
		0.110f,
		1.25f,
		3.50f,
		0.20f,
		0.85f,
		-0.08f,
		1.00f,
		});
	state.particleTypes.push_back({
		"Float",
		"",
		0,
		{ 0.56f, 1.0f, 0.35f, 0.72f },
		{ 0.56f, 1.0f, 0.35f, 0.72f },
		{ 0.20f, 0.80f, 1.0f, 0.0f },
		0.040f,
		0.070f,
		0.90f,
		2.40f,
		0.35f,
		1.05f,
		0.35f,
		0.40f,
		});
}

inline void EnsureDefaultEmitter(State& state) {
	if (state.emitters.empty()) {
		state.emitters.push_back(Emitter{});
	}
}

inline Emitter& GetPrimaryEmitter(State& state) {
	EnsureDefaultEmitter(state);
	return state.emitters.front();
}

inline const Emitter& GetPrimaryEmitter(const State& state) {
	assert(!state.emitters.empty());
	return state.emitters.front();
}

inline const ParticleType& GetParticleType(const State& state, uint32_t index) {
	assert(!state.particleTypes.empty());
	return state.particleTypes[(std::min)(index, static_cast<uint32_t>(state.particleTypes.size() - 1))];
}

inline void ClampEmitterParticleTypeIndices(State& state) {
	EnsureDefaultParticleTypes(state);
	const uint32_t lastParticleTypeIndex = static_cast<uint32_t>(state.particleTypes.size() - 1);
	for (Emitter& emitter : state.emitters) {
		emitter.particleTypeIndex = (std::min)(emitter.particleTypeIndex, lastParticleTypeIndex);
	}
}

inline void ResetEmitterTimersAndPending(State& state) {
	for (Emitter& emitter : state.emitters) {
		emitter.emitTimer = 0.0f;
		emitter.emissionAccumulator = 0.0f;
		emitter.pendingEmitCount = 0;
		emitter.pendingEmit = false;
	}
}

inline EmitterShape ClampEmitterShape(EmitterShape shape) {
	const uint32_t shapeIndex = static_cast<uint32_t>(shape);
	return shapeIndex < kEmitterShapeCount ? shape : EmitterShape::Sphere;
}

inline uint32_t GetEnabledEmitterEmitCountSum(const State& state) {
	uint32_t totalEmitCount = 0;
	for (const Emitter& emitter : state.emitters) {
		if (emitter.enabled) {
			totalEmitCount = (std::min)(totalEmitCount + emitter.emitCount, kParticleCount);
		}
	}
	return totalEmitCount;
}

inline void RequestInitialize(State& state) {
	state.needsInitializeDispatch = true;
	state.needsDeadListCounterReset = true;
	state.isDeadListReady = false;
	state.elapsedTimeSinceInitialize = 0.0f;
	ResetEmitterTimersAndPending(state);
	state.activeCountEstimate = state.useFreeListEmit
		? 0u
		: (state.isEmitterEnabled ? GetEnabledEmitterEmitCountSum(state) : kParticleCount);
	state.deadListCountEstimate = 0;
	if (state.useFreeListEmit) {
		state.emitBatchEstimates.clear();
	}
}

inline void RequestEmit(State& state) {
	state.needsEmitDispatch = true;
	state.lastEmitDispatchCount = 0;
	for (Emitter& emitter : state.emitters) {
		if (emitter.enabled && emitter.emitCount > 0) {
			emitter.pendingEmit = true;
			emitter.pendingEmitCount = (std::min)(emitter.emitCount, kParticleCount);
		}
	}
}

inline void RequestRecycle(State& state) {
	state.needsRecycleDispatch = true;
	state.lastRecycleDispatchCount = 0;
}

inline void RequestCounterReadback(State& state) {
	state.needsCounterReadback = true;
}

inline void ResetListsForFreeListMode(State& state) {
	state.needsInitializeFreeListDispatch = true;
	state.isFreeListInitialized = false;
	state.needsDeadListCounterReset = true;
	state.isDeadListReady = false;
	state.emitBatchEstimates.clear();
	state.freeListRemainingEstimate = 0;
	state.deadListCountEstimate = 0;
	state.lastRecycleDispatchCount = 0;
	state.isCounterReadbackValid = false;
	state.isCounterReadbackPending = false;
}

} // namespace GpuParticle
