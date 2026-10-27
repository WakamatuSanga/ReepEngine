#pragma once
#include "GpuParticleEffectPlayer.h"
#include "Engine/Graphics/Particle/GpuParticleEffectData.h"

// Private storage for the existing FX owner's bounded following instances.
struct GpuParticleEffectPlayer::FollowingEffect {
    std::unique_ptr<GpuParticleSystem> system;
    GpuParticle::ParticleEffectData data;
    DamageSource source;
    FollowHandle handle = 0;
    float age = 0;
    float tail = 0;
    bool emitting = false;
};
