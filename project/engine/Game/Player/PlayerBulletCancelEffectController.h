#pragma once

#include "Engine/math/Matrix4x4.h"

#include <cstdint>
#include <memory>
#include <vector>

class Camera;
class DirectXCommon;
class PlayerJetExhaustBeamRenderer;

class PlayerBulletCancelEffectController {
public:
    PlayerBulletCancelEffectController();
    ~PlayerBulletCancelEffectController();

    bool Initialize(DirectXCommon* dxCommon, Camera* camera);
    void Finalize();
    void BeginFrame();
    void Update(float deltaTime);
    void Draw();
    void DrawAfterCloud();
    void DrawImGui();

    void SpawnCancelEffect(const Vector3& position);
    int GetMaxEffectsPerFrame() const { return maxSpawnPerFrame_; }

private:
    struct CancelEffect {
        Vector3 center{ 0.0f, 0.0f, 0.0f };
        float age = 0.0f;
        float lifetime = 0.16f;
        uint32_t seed = 0;
        bool active = false;
    };

    void DrawLayer(bool afterCloudLayer);
    Vector3 ResolveCameraForward() const;
    Vector3 ResolveCameraRight() const;
    Vector3 ResolveCameraUp() const;
    void ClampSettings();
    void EnsureEffectSlots();
    int CountActiveEffects() const;

    std::unique_ptr<PlayerJetExhaustBeamRenderer> renderer_;
    std::vector<CancelEffect> effects_;
    Camera* camera_ = nullptr;
    bool initialized_ = false;
    bool enableBulletCancelEffect_ = true;
    bool drawAfterCloud_ = true;
    int maxSpawnPerFrame_ = 4;
    int maxActiveEffects_ = 32;
    int ringsPerEffect_ = 5;
    int ringDirectionMode_ = 1;
    bool useDiagonalRings_ = true;
    int spawnedThisFrame_ = 0;
    int activeEffectCount_ = 0;
    uint64_t droppedEffectCount_ = 0;
    uint32_t nextSeed_ = 1;
    float effectLifetime_ = 0.16f;
    float startRadius_ = 0.08f;
    float endRadius_ = 0.30f;
    float ringThickness_ = 0.022f;
    float ringAlpha_ = 0.23f;
    float ringBrightness_ = 0.78f;
    float ringFadePower_ = 1.45f;
    float time_ = 0.0f;
    Vector3 lastEffectPosition_{ 0.0f, 0.0f, 0.0f };
};