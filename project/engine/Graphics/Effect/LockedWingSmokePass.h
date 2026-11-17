#pragma once

#include "Engine/math/Matrix4x4.h"

#include <deque>
#include <memory>

class Camera;
class DirectXCommon;
class SrvManager;

class LockedWingSmokePass {
public:
    LockedWingSmokePass(DirectXCommon& dx, SrvManager& srv);
    ~LockedWingSmokePass();

    bool IsEnabled() const { return enabled_ && !renderInitFailed_; }
    void Emit(const Vector3& nozzle, const Vector3& exhaustDirection);
    void Update(float deltaTime);
    void Draw(const Camera& camera);
    void Reset();
    void DrawImGui();

    static constexpr float kSpacing = 0.45f;

private:
    struct Puff {
        Vector3 origin{};
        Vector3 drift{};
        float birthTime = 0.0f;
        float seed = 0.0f;
    };
    struct RenderResources;

    DirectXCommon& dx_;
    SrvManager& srv_;
    std::unique_ptr<RenderResources> render_;
    std::deque<Puff> puffs_;
    unsigned sequence_ = 0;
    float time_ = 0.0f;
    float density_ = 0.87f;
    float expansion_ = 1.0f;
    float lifetime_ = 1.55f;
    bool enabled_ = true;
    bool renderInitFailed_ = false;
};
