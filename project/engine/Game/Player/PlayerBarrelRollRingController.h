#pragma once

#include "Engine/math/Matrix4x4.h"

#include <cstdint>
#include <memory>
#include <vector>

class Camera;
class DirectXCommon;
class PlayerJetExhaustBeamRenderer;

class PlayerBarrelRollRingController {
public:
    PlayerBarrelRollRingController();
    ~PlayerBarrelRollRingController();

    bool Initialize(DirectXCommon* dxCommon, Camera* camera);
    void Finalize();
    void Update(float deltaTime);
    void Draw();
    void DrawAfterCloud();
    void DrawImGui();

    void SpawnRollRings(const Vector3& playerPosition, const Vector3& forward, int directionSign);

private:
    struct RollRing {
        Vector3 center{ 0.0f, 0.0f, 0.0f };
        Vector3 normal{ 0.0f, 0.0f, 1.0f };
        float age = 0.0f;
        float lifetime = 0.30f;
        float startRadius = 0.45f;
        float endRadius = 1.05f;
        float thickness = 0.030f;
        float alpha = 0.25f;
        float brightness = 0.70f;
        bool active = true;
    };

    void DrawLayer(bool afterCloudLayer);
    Vector3 ResolveCameraForward() const;
    Vector3 ResolveCameraUp() const;
    void ClampSettings();
    int CountVisibleRings() const;

    std::unique_ptr<PlayerJetExhaustBeamRenderer> renderer_;
    std::vector<RollRing> rings_;
    Camera* camera_ = nullptr;
    bool initialized_ = false;
    bool enableBarrelRollRings_ = true;
    bool drawAfterCloud_ = true;
    int ringCount_ = 3;
    int maxActiveRings_ = 12;
    int activeRingCount_ = 0;
    uint64_t droppedRingCount_ = 0;
    float ringLifetime_ = 0.30f;
    float ringStartRadius_ = 0.45f;
    float ringEndRadius_ = 1.05f;
    float ringThickness_ = 0.030f;
    float ringAlpha_ = 0.25f;
    float ringBrightness_ = 0.70f;
    float ringFadePower_ = 1.50f;
    float time_ = 0.0f;
    Vector3 depthOffsets_{ -0.45f, 0.0f, 0.45f };
    Vector3 spawnDelays_{ 0.00f, 0.03f, 0.06f };
    Vector3 lastSpawnCenter_{ 0.0f, 0.0f, 0.0f };
    Vector3 lastSpawnNormal_{ 0.0f, 0.0f, 1.0f };
};