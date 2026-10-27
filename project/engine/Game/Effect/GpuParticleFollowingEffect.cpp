#include "GpuParticleFollowingEffect.h"
#include "Engine/Graphics/Particle/GpuParticleSystem.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr size_t kMaxFollowingSources = 4;
constexpr float kTailSeconds = 1.25f;
GpuParticle::ParticleEffectData DamageData() {
    GpuParticle::ParticleEffectData data;
    auto smoke = GpuParticle::MakeDefaultParticleEffectType(0);
    smoke.name = "Damage dark flame / smoke";
    smoke.texturePath = "resources/particle/circle2.png";
    smoke.baseColor = {1,1,1,1};
    smoke.startColor = {.025f,.018f,.023f,.62f};
    smoke.baseColor = smoke.startColor;
    smoke.endColor = {.08f,.07f,.085f,0};
    smoke.startScale = .22f; smoke.endScale = .65f;
    smoke.lifeTimeMin = .65f; smoke.lifeTimeMax = 1.2f;
    smoke.speedMin = 1; smoke.speedMax = 2.8f; smoke.gravity = 0;
    smoke.affectedByInfluenceField = false; smoke.affectedByRailFlow = false;
    auto flame = smoke;
    flame.name = "Damage ember flame";
    flame.startColor = {.6f,.055f,.015f,.85f}; flame.endColor = {.025f,.01f,.018f,0};
    flame.baseColor = flame.startColor;
    flame.startScale = .20f; flame.endScale = .07f;
    flame.lifeTimeMin = .18f; flame.lifeTimeMax = .42f;
    flame.speedMin = 1.4f; flame.speedMax = 3.2f;
    flame.streakLength = 3.5f; flame.streakWidth = 1.1f;
    data.particleTypes = {smoke,flame};
    GpuParticle::Emitter emitter;
    emitter.shape = GpuParticle::EmitterShape::Cone; emitter.radius = .16f;
    emitter.coneHeight = .6f; emitter.emitInterval = 0; emitter.emitCount = 0;
    emitter.emissionRate = 95; emitter.particleTypeIndex = 0;
    data.emitters.push_back(emitter);
    emitter.emissionRate = 60; emitter.particleTypeIndex = 1; emitter.randomSeed = 772;
    data.emitters.push_back(emitter);
    data.runtime.useFreeListEmit = data.runtime.generateUnusedList = true;
    data.runtime.useDeadList = data.runtime.autoRecycleDeadList = data.runtime.autoReuseDeadParticles = true;
    data.runtime.maxActiveParticles = 256; data.runtime.maxEmitPerFrame = 24;
    return data;
}
}

bool GpuParticleEffectPlayer::PrepareDamageEffect() {
    if (!isInitialized_) return false;
    if (!followingEffects_.empty()) return true;
    auto effect = std::make_unique<FollowingEffect>();
    effect->data = DamageData();
    effect->system = std::make_unique<GpuParticleSystem>();
    if (!effect->system->Initialize(dxCommon_, srvManager_)) return false;
    effect->system->ApplyEffectData(effect->data);
    followingEffects_.push_back(std::move(effect));
    return true;
}

GpuParticleEffectPlayer::FollowHandle GpuParticleEffectPlayer::StartDamageEffect(const DamageSource& source) {
    if (!PrepareDamageEffect()) return 0;
    FollowingEffect* effect = nullptr;
    for (auto& slot : followingEffects_) if (slot->handle == 0) {effect = slot.get(); break;}
    if (!effect) {
        if (followingEffects_.size() >= kMaxFollowingSources) return 0;
        auto slot = std::make_unique<FollowingEffect>();
        slot->data = DamageData(); slot->system = std::make_unique<GpuParticleSystem>();
        if (!slot->system->Initialize(dxCommon_, srvManager_)) return 0;
        slot->system->ApplyEffectData(slot->data);
        effect = slot.get(); followingEffects_.push_back(std::move(slot));
    }
    effect->handle = ++nextFollowHandle_;
    effect->source = source; effect->age = effect->tail = 0; effect->emitting = true;
    effect->system->PlayEffectDataAt(effect->data, source.position);
    ConfigureFollowingEffect(*effect);
    return effect->handle;
}

void GpuParticleEffectPlayer::ConfigureFollowingEffect(FollowingEffect& effect) {
    const float scale = std::clamp(effect.source.scale, .05f, 5.0f);
    const float power = std::clamp(effect.source.intensity, 0.0f, 2.0f);
    const float mix = std::clamp(effect.source.smokeMix, 0.0f, 1.0f);
    for (size_t i = 0; i < effect.data.emitters.size(); ++i) {
        auto emitter = effect.system->GetState().emitters[i];
        emitter.enabled = effect.emitting;
        emitter.position = effect.source.position; emitter.direction = effect.source.direction;
        emitter.radius = effect.data.emitters[i].radius * scale;
        emitter.coneHeight = effect.data.emitters[i].coneHeight * scale;
        emitter.emissionRate = 155 * power * (i == 0 ? mix : 1-mix);
        effect.system->SetEmitterRuntime(i, emitter);
        auto type = effect.data.particleTypes[i];
        type.startScale *= scale; type.endScale *= scale;
        type.speedMin *= scale; type.speedMax *= scale;
        effect.system->SetParticleTypeRuntime(i, type);
    }
}

bool GpuParticleEffectPlayer::FollowDamageEffect(FollowHandle handle, const DamageSource& source) {
    for (auto& effect : followingEffects_) if (handle && effect->handle == handle) {
        effect->source = source; ConfigureFollowingEffect(*effect); return true;
    }
    return false;
}
void GpuParticleEffectPlayer::StopDamageEmission(FollowHandle handle) {
    for (auto& effect : followingEffects_) if (handle && effect->handle == handle) {
        effect->emitting = false; ConfigureFollowingEffect(*effect);
    }
}
void GpuParticleEffectPlayer::EndDamageEffect(FollowHandle handle) {
    for (auto& effect : followingEffects_) if (handle && effect->handle == handle) {
        effect->emitting = false; ConfigureFollowingEffect(*effect);
        effect->system->ResetParticlePool(); effect->handle = 0;
    }
}
bool GpuParticleEffectPlayer::IsDamageEffectAlive(FollowHandle handle) const {
    for (const auto& effect : followingEffects_) if (handle && effect->handle == handle) return true;
    return false;
}
void GpuParticleEffectPlayer::UpdateFollowingEffects(float dt, const Camera* camera) {
    for (auto& effect : followingEffects_) if (effect->handle) {
        const float step = std::clamp(dt, 0.0f, .1f);
        effect->age += step;
        if (effect->emitting && effect->age >= (std::max)(0.0f, effect->source.duration)) StopDamageEmission(effect->handle);
        if (!effect->emitting) effect->tail += step;
        if (effect->tail >= kTailSeconds) {EndDamageEffect(effect->handle); continue;}
        effect->system->SetDeltaTime(step); effect->system->Update(camera);
        activeParticleEstimate_ += effect->system->GetActiveCountEstimate();
    }
}
void GpuParticleEffectPlayer::DrawFollowingEffects() {
    for (auto& effect : followingEffects_) if (effect->handle) effect->system->Draw();
}
