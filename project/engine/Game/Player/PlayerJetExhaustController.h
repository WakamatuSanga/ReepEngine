#pragma once

#include "Engine/math/Matrix4x4.h"

#include <memory>
#include <string>

class BoostController;
class Camera;
class DirectXCommon;
class GpuParticleSystem;
class Model;
class Object3d;
class Object3dCommon;
class Player;
class PlayerJetExhaustBeamCore;
class SrvManager;

class PlayerJetExhaustController {
public:
    PlayerJetExhaustController();
    ~PlayerJetExhaustController();

    bool Initialize(
        Object3dCommon* object3dCommon,
        Camera* camera,
        Player* player,
        BoostController* boostController,
        DirectXCommon* dxCommon,
        SrvManager* srvManager);
    void Finalize();
    void Update(float deltaTime);
    void Draw();
    void DrawImGui();

    void SetPlayerAlive(bool isAlive) { isPlayerAlive_ = isAlive; }
    void SetDebugVisualsEnabled(bool isEnabled) { debugVisualsEnabled_ = isEnabled; }

private:
    bool LoadPreset();
    void ApplyRuntimeSettings(float deltaTime);
    void UpdateDebugObjects();
    void EnsureDebugObjects();
    void ResetToRecommendedSettings();

    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    Player* player_ = nullptr;
    BoostController* boostController_ = nullptr;
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    std::unique_ptr<GpuParticleSystem> particleSystem_;
    std::unique_ptr<PlayerJetExhaustBeamCore> beamCore_;
    std::unique_ptr<Object3d> nozzleDebugObject_;
    std::unique_ptr<Object3d> directionDebugObject_;
    Model* nozzleDebugModel_ = nullptr;
    Model* directionDebugModel_ = nullptr;

    std::string presetPath_ = "resources/effects/gpu/player_jet_exhaust.json";
    std::string loadStatus_ = "Not initialized";

    bool enableJetExhaust_ = true;
    bool hideWhenPlayerDead_ = true;
    bool isPlayerAlive_ = true;
    bool showDebugVisuals_ = false;
    bool debugVisualsEnabled_ = false;
    bool invertExhaustDirection_ = false;
    bool affectedByRailFlow_ = false;

    float nozzleBackOffset_ = 0.55f;
    float nozzleUpOffset_ = -0.02f;
    float nozzleSideOffset_ = 0.0f;
    float coneAngleDegrees_ = 12.0f;
    float emitterConeHeight_ = 0.08f;
    float spawnRate_ = 260.0f;
    float outerSpawnRate_ = 90.0f;
    float exhaustSpeed_ = 12.0f;
    float outerExhaustSpeed_ = 8.0f;
    float lifeTimeMin_ = 0.16f;
    float lifeTimeMax_ = 0.28f;
    float outerLifeTimeMin_ = 0.10f;
    float outerLifeTimeMax_ = 0.22f;
    float coreStartSize_ = 0.055f;
    float coreEndSize_ = 0.020f;
    float outerStartSize_ = 0.12f;
    float outerEndSize_ = 0.28f;
    float brightness_ = 1.0f;
    float boostLengthMultiplier_ = 1.8f;
    float boostSpeedMultiplier_ = 1.65f;
    float boostSpawnRateMultiplier_ = 1.75f;
    float boostBrightnessMultiplier_ = 1.55f;
    float boostSmoothSpeed_ = 12.0f;
    float railFlowScale_ = 0.0f;
    float debugDirectionLength_ = 0.9f;

    float smoothedBoostPower_ = 0.0f;
    float currentLengthMultiplier_ = 1.0f;
    float currentSpeedMultiplier_ = 1.0f;
    float currentSpawnRateMultiplier_ = 1.0f;
    float currentBrightness_ = 1.0f;
    uint64_t updateCount_ = 0;
    Vector3 currentNozzlePosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 currentExhaustDirection_{ 0.0f, 0.0f, -1.0f };
};