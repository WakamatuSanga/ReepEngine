#pragma once

#include "Engine/math/Matrix4x4.h"

#include <cstdint>
#include <memory>
#include <vector>

class BoostController;
class Camera;
class DirectXCommon;
class Player;
class PlayerJetExhaustBeamRenderer;

class PlayerSonicBoostRingController {
public:
    PlayerSonicBoostRingController();
    ~PlayerSonicBoostRingController();

    bool Initialize(DirectXCommon* dxCommon, Camera* camera, Player* player, BoostController* boostController);
    void Finalize();
    void Update(float deltaTime);
    void Draw();
    void DrawAfterCloud();
    void DrawImGui();

    void SetDebugVisualsEnabled(bool isEnabled) { debugVisualsEnabled_ = isEnabled; }

private:
    struct SonicBoostRing {
        Vector3 center{ 0.0f, 0.0f, 0.0f };
        Vector3 normal{ 0.0f, 0.0f, 1.0f };
        Vector3 velocity{ 0.0f, 0.0f, 0.0f };
        float age = 0.0f;
        float lifetime = 0.28f;
        float startRadius = 0.30f;
        float endRadius = 0.82f;
        float thickness = 0.030f;
        float brightness = 0.8f;
        float alpha = 0.15f;
        bool active = true;
    };

    void SpawnRing(float initialAge);
    void SpawnBurst();
    void DrawLayer(bool afterCloudLayer);
    void ApplySubtlePreset();
    void ApplyStandardPreset();
    void ApplyStrongPreset();
    Vector3 ResolveRingForward() const;
    Vector3 ResolveRingUp() const;
    Vector3 ResolveRingCenter(const Vector3& normal) const;
    Vector3 ResolveRingFlowDirection() const;
    float ResolveRingBackwardSpeed(float boostPower) const;
    void ClampSettings();
    int CountVisibleRings() const;

    std::unique_ptr<PlayerJetExhaustBeamRenderer> renderer_;
    std::vector<SonicBoostRing> rings_;
    Camera* camera_ = nullptr;
    Player* player_ = nullptr;
    BoostController* boostController_ = nullptr;

    bool enableSonicBoostRing_ = true;
    bool drawSonicRingAfterCloud_ = true;
    bool showRingDebug_ = false;
    bool debugVisualsEnabled_ = false;
    bool spawnBurstOnBoostStart_ = false;
    bool enableRingBackwardFlow_ = true;
    bool useRailFlowDirectionForRing_ = true;
    bool useCameraForwardForRingFlow_ = false;
    bool lockRingVelocityOnSpawn_ = true;
    bool usePlayerForward_ = true;
    bool useCameraForward_ = false;
    bool wasBoosting_ = false;
    bool initialized_ = false;

    float ringLifetime_ = 0.28f;
    float ringStartRadius_ = 0.30f;
    float ringEndRadius_ = 0.82f;
    float ringThickness_ = 0.030f;
    float ringBrightness_ = 0.8f;
    float ringAlpha_ = 0.15f;
    float ringFadePower_ = 1.6f;
    float ringSpawnInterval_ = 0.18f;
    float boostRingStartThreshold_ = 0.15f;
    float burstInterval_ = 0.04f;
    float ringForwardOffset_ = 0.4f;
    float ringVerticalOffset_ = 0.0f;
    float ringBackwardSpeed_ = 8.0f;
    float boostRingBackwardSpeedMultiplier_ = 1.25f;
    float ringSpawnTimer_ = 0.0f;
    float currentBoostPower_ = 0.0f;
    float currentRingBackwardSpeed_ = 0.0f;
    float time_ = 0.0f;
    float lastRingRadius_ = 0.0f;
    int maxActiveRings_ = 16;
    int burstRingCount_ = 8;
    int activeRingCount_ = 0;
    Vector3 lastRingCenter_{ 0.0f, 0.0f, 0.0f };
    Vector3 lastRingNormal_{ 0.0f, 0.0f, 1.0f };
    Vector3 lastRingFlowDirection_{ 0.0f, 0.0f, -1.0f };
};