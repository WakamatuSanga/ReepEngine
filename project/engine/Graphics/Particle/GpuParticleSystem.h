#pragma once

#include "GpuParticleTypes.h"

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
	void Update(const Camera* camera);
	void Draw();
	void DrawImGui();

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
