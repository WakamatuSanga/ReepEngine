#pragma once

#include "Engine/math/Matrix4x4.h"

#include <cstdint>
#include <memory>
#include <vector>

class Camera;
class DirectXCommon;
class PlayerJetExhaustBeamRenderer;

class EnemyDefeatEffectController {
public:
    EnemyDefeatEffectController();
    ~EnemyDefeatEffectController();

    bool Initialize(DirectXCommon* dxCommon, Camera* camera);
    void Finalize();
    void BeginFrame();
    void Update(float deltaTime);
    void Draw();
    void DrawAfterCloud();
    void DrawImGui();

    void SpawnDefeatEffect(const Vector3& position, float scale = 1.0f);

private:
    struct DefeatEffect {
        Vector3 center{ 0.0f, 0.0f, 0.0f };
        float age = 0.0f;
        float lifetime = 0.35f;
        float scale = 1.0f;
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
    std::vector<DefeatEffect> effects_;
    Camera* camera_ = nullptr;
    bool initialized_ = false;
    bool enableDefeatEffect_ = true;
    bool drawAfterCloud_ = true;
    int maxActiveEffects_ = 64;
    int maxSpawnPerFrame_ = 8;
    int sparkCount_ = 8;
    int spawnedThisFrame_ = 0;
    int activeEffectCount_ = 0;
    uint64_t droppedEffectCount_ = 0;
    uint32_t nextSeed_ = 1;
    float effectLifetime_ = 0.35f;
    float flashLifetime_ = 0.10f;
    float ringStartRadius_ = 0.15f;
    float ringEndRadius_ = 0.85f;
    float ringThickness_ = 0.035f;
    float sparkLifetime_ = 0.25f;
    float sparkMinLength_ = 0.35f;
    float sparkMaxLength_ = 0.70f;
    float effectAlpha_ = 0.75f;
    float effectBrightness_ = 1.20f;
    float time_ = 0.0f;
    Vector3 lastEffectPosition_{ 0.0f, 0.0f, 0.0f };
};
