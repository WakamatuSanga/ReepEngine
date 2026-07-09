#pragma once

#include "Engine/math/Matrix4x4.h"

#include <memory>
#include <vector>

class Camera;
class DirectXCommon;
class Player;
class PlayerBulletManager;
class PlayerJetExhaustBeamRenderer;

class PlayerChargeFeedbackController {
public:
    PlayerChargeFeedbackController();
    ~PlayerChargeFeedbackController();

    bool Initialize(DirectXCommon* dxCommon, Camera* camera, Player* player, PlayerBulletManager* bulletManager);
    void Finalize();
    void Update(float deltaTime);
    void Draw();
    void DrawAfterCloud();
    void DrawImGui();

private:
    struct ChargePulse {
        Vector3 center{ 0.0f, 0.0f, 0.0f };
        Vector3 normal{ 0.0f, 0.0f, 1.0f };
        float age = 0.0f;
        float lifetime = 0.25f;
        float startRadius = 0.08f;
        float endRadius = 0.18f;
        float thickness = 0.015f;
        float brightness = 0.85f;
        float alpha = 0.65f;
        bool active = true;
    };

    void SpawnMaxChargePulse();
    void DrawLayer(bool afterCloudLayer);
    void DrawRing(const Vector3& center, const Vector3& normal, float radius, float thickness, float brightness, float alpha);
    Vector3 ResolveCameraForward() const;
    Vector3 ResolveCameraUp() const;
    Vector3 ResolvePlayerForward() const;
    Vector3 ResolveReticleCenter() const;
    Vector3 ResolveFrontGlowCenter() const;
    void ClampSettings();

    Camera* camera_ = nullptr;
    Player* player_ = nullptr;
    PlayerBulletManager* bulletManager_ = nullptr;
    std::unique_ptr<PlayerJetExhaustBeamRenderer> renderer_;
    std::vector<ChargePulse> pulses_;

    bool initialized_ = false;
    bool enableChargeFeedback_ = true;
    bool forceChargeMax_ = false;
    bool drawAfterCloud_ = true;
    bool frontGlowEnabled_ = true;
    bool wasChargeMax_ = false;
    bool isChargeMax_ = false;
    bool chargeVisualActive_ = false;

    float chargeGatherStartDelay_ = 0.22f;
    float rawChargeTimer_ = 0.0f;
    float rawChargeRate_ = 0.0f;
    float visualChargeRate_ = 0.0f;
    float chargeRate_ = 0.0f;
    float time_ = 0.0f;
    float reticleGlowAlpha_ = 0.65f;
    float maxChargeGlowPulse_ = 0.20f;
    float pulseLifetime_ = 0.25f;
    float pulseStartRadius_ = 0.08f;
    float pulseEndRadius_ = 0.18f;
    float pulseThickness_ = 0.015f;
    float pulseBrightness_ = 0.85f;
    float frontGlowSize_ = 0.22f;
    float frontGlowAlpha_ = 0.45f;
    float frontGlowBrightness_ = 0.90f;
    float frontGlowPulseRate_ = 5.0f;
    float frontOffset_ = 0.9f;

    int activePulseCount_ = 0;
    Vector3 lastReticleCenter_{ 0.0f, 0.0f, 0.0f };
    Vector3 lastFrontGlowCenter_{ 0.0f, 0.0f, 0.0f };
};