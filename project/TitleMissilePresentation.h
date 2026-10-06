#pragma once
#include "Engine/math/Matrix4x4.h"
#include <array>
#include <cstdint>
#include <memory>

class Camera;
class DirectXCommon;
class LockedWingMissileExhaustController;
class Object3d;
class Object3dCommon;
class SrvManager;

// Display only: no gameplay bullets, targets, or collision registration.
class TitleMissilePresentation {
public:
    TitleMissilePresentation();
    ~TitleMissilePresentation();
    void Initialize(Object3dCommon* objectCommon, DirectXCommon* dxCommon,
        SrvManager* srvManager, Camera* camera);
    void Finalize();
    void Update(float deltaTime, const Transform& aircraftTransform);
    void DrawModels();
    void DrawExhaust();

private:
    struct Missile {
        std::unique_ptr<Object3d> object;
        Vector3 position{};
        Vector3 forward{ 0.0f, 0.0f, 1.0f };
        float age = 0.0f;
        uint64_t exhaustHandle = 0;
        bool active = false;
    };
    void Launch(const Transform& aircraftTransform);

    Object3dCommon* objectCommon_ = nullptr;
    std::unique_ptr<LockedWingMissileExhaustController> exhaust_;
    std::array<Missile, 4> missiles_{};
    float nextLaunchRemaining_ = 0.0f;
};
