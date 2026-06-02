#pragma once

#include <string>

namespace GpuParticle {
struct ParticleEffectData;
struct State;

class GpuParticleEffectSerializer {
public:
	static constexpr const char* kDefaultPath = "resources/effects/GpuParticleEffect.json";

	static bool Save(const ParticleEffectData& effectData, const std::string& filePath = kDefaultPath);
	static bool Load(const std::string& filePath, ParticleEffectData& effectData);
	static bool Save(const State& state, const std::string& filePath = kDefaultPath);
	static bool Load(const std::string& filePath, State& state);
};
} // namespace GpuParticle
