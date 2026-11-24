#pragma once

#include "GpuParticleEffectData.h"

#include <cstddef>
#include <string>
#include <unordered_map>

class GpuParticleSystem;

class GpuParticleEffectManager {
public:
	bool LoadEffect(const std::string& name, const std::string& path);
	bool RegisterEffect(const std::string& name, const GpuParticle::ParticleEffectData& effectData);
	bool ApplyToSystem(const std::string& name, GpuParticleSystem& system) const;
	void Clear();
	size_t GetEffectCount() const { return effects_.size(); }

private:
	const GpuParticle::ParticleEffectData* GetEffect(const std::string& name) const;
	std::unordered_map<std::string, GpuParticle::ParticleEffectData> effects_;
};
