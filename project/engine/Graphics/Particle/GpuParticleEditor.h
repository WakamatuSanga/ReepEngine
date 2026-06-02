#pragma once

#include "GpuParticleEffectData.h"

#include <cstdint>
#include <string>

class GpuParticleResources;
class GpuParticleRenderer;
namespace GpuParticle {
struct State;
}

class GpuParticleEditor {
public:
	GpuParticleEditor();

	void DrawImGui(GpuParticle::State& state, GpuParticleResources& resources, GpuParticleRenderer& renderer, uint32_t particleTextureIndex);

private:
	void SyncEditingEffectDataFromState(const GpuParticle::State& state);
	void ApplyEditingEffectDataToState(GpuParticle::State& state, GpuParticleResources& resources, GpuParticleRenderer& renderer);

	GpuParticle::ParticleEffectData editingEffectData_;
	char effectPath_[260]{};
	std::string effectIoStatus_;
	int selectedEmitterIndex_ = 0;
	int selectedParticleTypeIndex_ = 0;
};
