#pragma once

#include "Engine/math/Matrix4x4.h"

#include <deque>
#include <memory>

class Camera;
class DirectXCommon;
class SrvManager;

class LockedWingSmokePass {
public:
    enum class Style { Exhaust, Burst };

    LockedWingSmokePass(DirectXCommon& dx, SrvManager& srv, Style style = Style::Exhaust);
    ~LockedWingSmokePass();

    bool IsEnabled() const { return enabled_ && !renderInitFailed_; }
    void Emit(const Vector3& nozzle, const Vector3& exhaustDirection);
    bool EmitBurst(const Vector3& center, float scale);
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
        float scale = 1.0f;
    };
    struct RenderResources;
    float NextVariation();
    void AppendPuff(const Vector3& origin, const Vector3& drift, float variation, float scale);

    DirectXCommon& dx_;
    SrvManager& srv_;
    const Style style_;
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
