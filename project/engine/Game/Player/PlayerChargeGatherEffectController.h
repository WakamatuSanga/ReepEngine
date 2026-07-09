#pragma once

#include "Engine/math/Matrix4x4.h"

#include <array>
#include <memory>
#include <string>

class Camera;
class DirectXCommon;
class GpuParticleSystem;
class Player;
class PlayerBulletManager;
class SrvManager;
namespace GpuParticle {
struct ParticleEffectData;
}

class PlayerChargeGatherEffectController {
public:
    PlayerChargeGatherEffectController();
    ~PlayerChargeGatherEffectController();

    bool Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, Camera* camera, Player* player, PlayerBulletManager* bulletManager);
    void Finalize();
    void Update(float deltaTime);
    void Draw();
    void DrawImGui();

private:
    bool LoadEffects();
    void ApplyGatherRuntime(float deltaTime);
    void ApplyMaxCoreRuntime(float deltaTime);
    Vector3 ResolvePlayerForward() const;
    Vector3 ResolveTargetPosition() const;
    void ReloadEffects();
    void ClampSettings();
    void SyncPathBuffers();

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Camera* camera_ = nullptr;
    Player* player_ = nullptr;
    PlayerBulletManager* bulletManager_ = nullptr;

    std::unique_ptr<GpuParticleSystem> gatherSystem_;
    std::unique_ptr<GpuParticleSystem> maxCoreSystem_;
    std::unique_ptr<GpuParticle::ParticleEffectData> gatherEffect_;
    std::unique_ptr<GpuParticle::ParticleEffectData> maxCoreEffect_;

    bool initialized_ = false;
    bool enableChargeGather_ = true;
    bool enableMaxCore_ = true;
    bool testGather_ = false;
    bool testMaxCore_ = false;
    bool forceStrongGather_ = false;
    bool disableSwirlForTest_ = false;
    bool disableInitialVelocityForTest_ = false;
    bool showTargetDebug_ = false;
    bool showDebugWindow_ = true;
    bool effectsLoaded_ = false;

    float chargeRate_ = 0.0f;
    float chargeGatherStartDelay_ = 0.22f;
    float rawChargeTimer_ = 0.0f;
    float rawChargeRate_ = 0.0f;
    float visualChargeRate_ = 0.0f;
    float testChargeRate_ = 0.65f;
    float frontOffset_ = 0.85f;
    float gatherRadiusMin_ = 1.20f;
    float gatherRadiusMax_ = 2.00f;
    float gatherEmissionRateMin_ = 100.0f;
    float gatherEmissionRateMax_ = 240.0f;
    float gatherStrengthMin_ = 8.0f;
    float gatherStrengthMax_ = 22.0f;
    float gatherSwirlMin_ = 0.5f;
    float gatherSwirlMax_ = 1.4f;
    float gatherBrightnessScale_ = 1.45f;
    float forceGatherStrength_ = 36.0f;
    float maxCoreRadius_ = 0.18f;
    float maxCoreEmissionRate_ = 34.0f;

    bool isCharging_ = false;
    bool isChargeMax_ = false;
    bool chargeVisualActive_ = false;
    bool shouldEmitGather_ = false;
    bool gatherActive_ = false;
    bool maxCoreActive_ = false;
    uint32_t emittedCount_ = 0;
    uint32_t droppedEmitCount_ = 0;
    uint32_t gatherDeadCountEstimate_ = 0;
    uint32_t killedAtTargetCountEstimate_ = 0;
    float currentEmissionRate_ = 0.0f;
    float currentGatherStrength_ = 0.0f;
    float currentSwirlStrength_ = 0.0f;
    float averageDistancePrevious_ = 0.0f;
    float averageDistanceCurrent_ = 0.0f;
    float distanceTargetFromPlayer_ = 0.0f;
    Vector3 playerPosition_{0.0f, 0.0f, 0.0f};
    Vector3 targetPosition_{0.0f, 0.0f, 0.0f};

    std::string gatherEffectPath_ = "resources/effects/gpu/player_charge_gather.json";
    std::string maxCoreEffectPath_ = "resources/effects/gpu/player_charge_max_core.json";
    std::array<char, 260> gatherEffectPathBuffer_{};
    std::array<char, 260> maxCoreEffectPathBuffer_{};
};