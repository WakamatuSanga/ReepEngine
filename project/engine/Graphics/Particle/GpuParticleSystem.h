#pragma once

#include "GpuParticleTypes.h"

#include <cstddef>
#include <memory>

class Camera;
class DirectXCommon;
class GpuParticleCompute;
class GpuParticleEditor;
class GpuParticleRenderer;
class GpuParticleResources;
class SrvManager;
namespace GpuParticle {
struct ParticleEffectData;
}

class GpuParticleSystem {
public:
	GpuParticleSystem();
	~GpuParticleSystem();

	bool Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void ApplyEffectData(const GpuParticle::ParticleEffectData& effectData);
	bool PlayEffectDataAt(const GpuParticle::ParticleEffectData& effectData, const Vector3& position);
	bool SetEmitterRuntime(size_t index, const GpuParticle::Emitter& emitter);
	bool SetParticleTypeRuntime(size_t index, const GpuParticle::ParticleType& type);
	void SetDeltaTime(float deltaTime);
	void SetInfluenceFields(const Vector4* centersAndRadius, const Vector4* params, uint32_t count);
	void SetParticleInfluenceEnabled(bool enabled);
	void SetParticleInfluenceResponseScale(float scale);
	void SetRailParticleFlow(bool enabled, const Vector3& cameraPosition, const Vector3& direction, float speed, float scale, float spawnAheadDistance, float despawnBehindDistance);
	void SetChargeGather(bool enabled, const Vector3& targetPosition, float chargeRate, float strengthScale, float swirlScale, float brightnessScale);
	void SetRuntimePoolOptions(bool generateUnusedList, bool useDeadList, bool autoReuseDeadParticles);
	void SetRuntimeParticleLimits(uint32_t maxActiveParticles, uint32_t maxEmitPerFrame);
	void SetCounterReadbackEnabled(bool enabled);
	void RequestCounterReadback();
	void ResetParticlePool();
	void Update(const Camera* camera);
	void Draw();
	void DrawImGui();
	uint32_t GetActiveCountEstimate() const { return state_.activeCountEstimate; }
	uint32_t GetPoolCapacity() const { return GpuParticle::kParticleCount; }
	const GpuParticle::State& GetState() const { return state_; }
	bool IsInitialized() const { return isInitialized_; }

private:
	uint32_t EstimateActiveParticleCount() const;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	bool isInitialized_ = false;
	GpuParticle::State state_;
	std::unique_ptr<GpuParticleResources> resources_;
	std::unique_ptr<GpuParticleCompute> compute_;
	std::unique_ptr<GpuParticleRenderer> renderer_;
	std::unique_ptr<GpuParticleEditor> editor_;
};

