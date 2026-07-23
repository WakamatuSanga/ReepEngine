#pragma once

#include "Engine/Graphics/Particle/GpuParticleEffectData.h"
#include "Engine/math/Matrix4x4.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Camera;
class DirectXCommon;
class GpuParticleSystem;
class SrvManager;

class LockedWingMissileExhaustController {
public:
    enum class SlotState : uint8_t {
        Free,
        Emitting,
        Draining,
    };

    LockedWingMissileExhaustController();
    ~LockedWingMissileExhaustController();

    bool Initialize(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        Camera* camera);
    void Finalize();
    void Update(float scaledDeltaTime);
    void Draw();
    void DrawImGui();

    uint64_t Start(
        uint64_t existingHandle,
        const Vector3& projectilePosition,
        const Vector3& flightDirection);
    bool UpdateMissile(
        uint64_t handle,
        const Vector3& projectilePosition,
        const Vector3& flightDirection);
    void Stop(uint64_t handle);
    void Reset(bool resetStatistics);

private:
    static constexpr uint32_t kHardSlotCapacity = 32;

    struct Slot {
        SlotState state = SlotState::Free;
        uint32_t generation = 1;
        float drainRemaining = 0.0f;
        Vector3 position{};
        Vector3 direction{ 0.0f, 0.0f, -1.0f };
    };

    bool LoadPreset();
    bool DecodeHandle(uint64_t handle, uint32_t& slotIndex) const;
    uint64_t MakeHandle(uint32_t slotIndex) const;
    bool SetSlotEmitters(
        uint32_t slotIndex,
        bool enabled,
        const Vector3& position,
        const Vector3& direction);
    void ApplyRuntimeScale();
    void ReleaseDrainedSlots(float scaledDeltaTime);

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Camera* camera_ = nullptr;
    std::unique_ptr<GpuParticleSystem> particleSystem_;
    std::vector<GpuParticle::Emitter> emitterTemplates_;
    std::vector<GpuParticle::ParticleType> particleTypeTemplates_;
    std::array<Slot, kHardSlotCapacity> slots_{};

    std::string presetPath_ =
        "resources/effects/gpu/locked_wing_missile_exhaust.json";
    std::string loadStatus_ = "未初期化";
    uint32_t emittersPerSlot_ = 0;
    int maxEmitterCount_ = 16;
    float exhaustRearOffset_ = 0.20f;
    float runtimeScaleMultiplier_ = 1.0f;
    float appliedRuntimeScaleMultiplier_ = -1.0f;
    float jsonBaseScale_ = 0.55f;
    float finalAppliedScale_ = 0.55f;
    float maxParticleLifetime_ = 0.25f;

    uint64_t lastHandle_ = 0;
    SlotState lastSlotState_ = SlotState::Free;
    Vector3 lastExhaustPosition_{};
    Vector3 lastExhaustDirection_{ 0.0f, 0.0f, -1.0f };
    uint64_t emitterAcquireSuccessCount_ = 0;
    uint64_t emitterShortageCount_ = 0;
    uint64_t emitterStopCount_ = 0;
    uint64_t drainingStartCount_ = 0;
    uint64_t slotReleaseCount_ = 0;
    uint64_t doubleStartPreventionCount_ = 0;
    uint64_t doubleReleasePreventionCount_ = 0;
    uint64_t invalidTransformStopCount_ = 0;
    bool initialized_ = false;
};