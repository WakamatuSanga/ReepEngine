#include "GpuParticleEffectManager.h"

#include "GpuParticleEffectSerializer.h"
#include "GpuParticleSystem.h"

#include <utility>

bool GpuParticleEffectManager::LoadEffect(const std::string& name, const std::string& path) {
	GpuParticle::ParticleEffectData effectData;
	if (!GpuParticle::GpuParticleEffectSerializer::Load(path, effectData)) {
		return false;
	}
	return RegisterEffect(name, effectData);
}

bool GpuParticleEffectManager::RegisterEffect(const std::string& name, const GpuParticle::ParticleEffectData& effectData) {
	if (name.empty()) {
		return false;
	}

	GpuParticle::ParticleEffectData normalizedEffect = effectData;
	GpuParticle::NormalizeParticleEffectData(normalizedEffect);
	effects_[name] = std::move(normalizedEffect);
	return true;
}

const GpuParticle::ParticleEffectData* GpuParticleEffectManager::GetEffect(const std::string& name) const {
	const auto it = effects_.find(name);
	return it == effects_.end() ? nullptr : &it->second;
}

GpuParticle::ParticleEffectData* GpuParticleEffectManager::GetEffect(const std::string& name) {
	const auto it = effects_.find(name);
	return it == effects_.end() ? nullptr : &it->second;
}

bool GpuParticleEffectManager::ApplyToSystem(const std::string& name, GpuParticleSystem& system) const {
	const GpuParticle::ParticleEffectData* effectData = GetEffect(name);
	if (!effectData) {
		return false;
	}

	system.ApplyEffectData(*effectData);
	return true;
}

void GpuParticleEffectManager::Clear() {
	effects_.clear();
}
