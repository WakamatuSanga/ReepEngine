#pragma once
#include <cstdint>
#include <memory>

class BoostController;
class Camera;
class DirectXCommon;
class GpuParticleSystem;
class RailShooterCameraRig;
class SrvManager;

class GameplaySpeedLineController {
public:
    GameplaySpeedLineController();
    ~GameplaySpeedLineController();
    bool Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, Camera* camera,
        RailShooterCameraRig* rig, BoostController* boost);
    void Finalize();
    void Reset();
    void Update(float deltaTime, bool ordinaryGameplayActive);
    void Draw();
    uint32_t GetActiveParticleEstimate() const;
private:
    std::unique_ptr<GpuParticleSystem> particles_;
    DirectXCommon* dxCommon_ = nullptr;
    Camera* camera_ = nullptr;
    RailShooterCameraRig* rig_ = nullptr;
    BoostController* boost_ = nullptr;
    bool active_ = false;
    uint32_t seed_ = 1;
};
