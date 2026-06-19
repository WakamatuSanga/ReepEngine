#pragma once

#include "Engine/math/Matrix4x4.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Camera;
class DirectXCommon;
class GpuParticleSystem;
class SrvManager;

class GpuParticleEffectPlayer {
public:
    GpuParticleEffectPlayer();
    ~GpuParticleEffectPlayer();

    bool Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void Finalize();
    void Update(float deltaTime, const Camera* camera);
    void Draw();
    void DrawImGui();

    bool PlayGpuParticleEffectAt(const std::string& jsonPath, const Vector3& position);
    bool ReloadEffect(const std::string& jsonPath);

    const std::string& GetLastEffectPath() const { return lastEffectPath_; }
    const std::string& GetLastResult() const { return lastResult_; }
    const Vector3& GetLastPosition() const { return lastPosition_; }
    uint64_t GetPlayCount() const { return playCount_; }
    uint64_t GetMissingJsonCount() const { return missingJsonCount_; }
    uint64_t GetFailedPlayCount() const { return failedPlayCount_; }
    uint32_t GetActiveParticleEstimate() const { return activeParticleEstimate_; }

private:
    struct EffectInstance;

    EffectInstance* FindInstance(const std::string& jsonPath);
    EffectInstance* FindOrCreateInstance(const std::string& jsonPath);
    bool LoadInstance(EffectInstance& instance);
    void RecordResult(const std::string& path, const Vector3& position, const std::string& result);

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    std::vector<std::unique_ptr<EffectInstance>> effects_;
    std::string lastEffectPath_ = "None";
    std::string lastResult_ = "Not initialized";
    Vector3 lastPosition_{ 0.0f, 0.0f, 0.0f };
    uint64_t playCount_ = 0;
    uint64_t missingJsonCount_ = 0;
    uint64_t failedPlayCount_ = 0;
    uint32_t activeParticleEstimate_ = 0;
    bool isInitialized_ = false;
};
