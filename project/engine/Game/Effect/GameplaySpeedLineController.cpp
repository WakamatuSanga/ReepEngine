#include "GameplaySpeedLineController.h"
#include "Engine/Game/Camera/RailShooterCameraRig.h"
#include "Engine/Game/Player/BoostController.h"
#include "Engine/Graphics/Particle/GpuParticleEffectData.h"
#include "Engine/Graphics/Particle/GpuParticleSystem.h"
#include <algorithm>

namespace {
constexpr float kSpawnAhead = 8.0f;
constexpr float kDespawnBehind = 0.5f;
constexpr float kSpeed = 28.0f;
constexpr float kRate = 48.0f;
constexpr uint32_t kMaxParticles = 128;
constexpr uint32_t kMaxEmitPerFrame = 8;
}

GameplaySpeedLineController::GameplaySpeedLineController() = default;
GameplaySpeedLineController::~GameplaySpeedLineController() = default;

bool GameplaySpeedLineController::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager,
    Camera* camera, RailShooterCameraRig* rig, BoostController* boost) {
    Finalize();
    camera_ = camera;
    rig_ = rig;
    boost_ = boost;
    particles_ = std::make_unique<GpuParticleSystem>();
    if (!particles_->Initialize(dxCommon, srvManager)) {
        Finalize();
        return false;
    }
    GpuParticle::ParticleEffectData effect;
    auto type = GpuParticle::MakeDefaultParticleEffectType(0);
    type.name = "Gameplay wind";
    type.texturePath = "resources/particle/gradationLine.png";
    type.baseColor = { 0.85f, 0.94f, 1.0f, 0.35f };
    type.startColor = type.baseColor;
    type.endColor = { 0.85f, 0.94f, 1.0f, 0.0f };
    type.startScale = type.endScale = 1.0f;
    type.lifeTimeMin = 0.45f;
    type.lifeTimeMax = 0.55f;
    // Rail Flow supplies speed; this tiny velocity supplies the ribbon's axis.
    type.speedMin = type.speedMax = 0.01f;
    type.gravity = type.drag = 0.0f;
    type.affectedByInfluenceField = false;
    type.affectedByRailFlow = true;
    type.streakLength = 0.9f;
    type.streakWidth = 0.012f;
    effect.particleTypes.push_back(type);
    GpuParticle::Emitter emitter;
    emitter.position = { 0, 0, kSpawnAhead };
    emitter.direction = { 0, 0, -1 };
    emitter.shape = GpuParticle::EmitterShape::Box;
    emitter.boxSize = { 1.5f, 0.90f, 0.20f };
    emitter.emitCount = 0;
    emitter.emitInterval = 0;
    emitter.emissionRate = kRate;
    effect.emitters.push_back(emitter);
    effect.runtime.useFreeListEmit = effect.runtime.generateUnusedList = true;
    effect.runtime.useDeadList = effect.runtime.autoRecycleDeadList = effect.runtime.autoReuseDeadParticles = true;
    effect.runtime.maxActiveParticles = kMaxParticles;
    effect.runtime.maxEmitPerFrame = kMaxEmitPerFrame;
    particles_->ApplyEffectData(effect);
    particles_->SetCounterReadbackEnabled(false);
    Reset();
    return true;
}

void GameplaySpeedLineController::Finalize() {
    particles_.reset();
    camera_ = nullptr;
    rig_ = nullptr;
    boost_ = nullptr;
    active_ = false;
    seed_ = 1;
}

void GameplaySpeedLineController::Reset() {
    active_ = false;
    if (particles_) particles_->ResetParticlePool();
}

void GameplaySpeedLineController::Update(float deltaTime, bool ordinaryGameplayActive) {
    if (!particles_ || !camera_ || !rig_) return;
    const bool shouldRun = ordinaryGameplayActive && rig_->UsesGameplayComposition()
        && !rig_->IsGameplayPresentationActive();
    if (!shouldRun) {
        if (active_) Reset();
        return;
    }
    active_ = true;
    const float power = boost_ ? std::clamp(boost_->GetCurrentBoostPower(), 0.0f, 1.0f) : 0.0f;
    GpuParticle::Emitter emitter = particles_->GetState().emitters.front();
    emitter.randomSeed = seed_++;
    emitter.emissionRate = kRate * (1.0f + power);
    particles_->SetEmitterRuntime(0, emitter);
    auto type = particles_->GetState().particleTypes.front();
    type.streakLength = 0.9f * (1.0f + 0.7f * power);
    particles_->SetParticleTypeRuntime(0, type);
    // Simulate within the unshaken rail frame, render through the final camera.
    particles_->SetRailParticleFlow(true, {}, { 0, 0, -1 }, kSpeed * (1.0f + 0.7f * power),
        1.0f, kSpawnAhead + 0.2f, kDespawnBehind);
    particles_->SetDeltaTime(std::clamp(deltaTime, 0.0f, 1.0f / 15.0f));
    particles_->Update(camera_, &rig_->GetGameplayReferenceFrame());
}

void GameplaySpeedLineController::Draw() {
    if (active_ && particles_) particles_->Draw();
}

uint32_t GameplaySpeedLineController::GetActiveParticleEstimate() const {
    return active_ && particles_ ? particles_->GetActiveCountEstimate() : 0;
}
