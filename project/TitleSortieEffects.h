#pragma once
#include <array>
#include <cstdint>
#include <memory>

class Camera;
class DirectXCommon;
class PlayerSonicBoostRingController;
struct Transform;

// Title-owned effects; no gameplay boost state or input is used.
class TitleSortieEffects {
public:
    TitleSortieEffects();
    ~TitleSortieEffects();
    void Initialize(DirectXCommon* dx, Camera* camera);
    void Finalize();
    void Update(float dt, const Transform* aircraft, bool flying, float visibility, float flightProgress);
    void Draw();
private:
    struct BlurState {
        uint32_t enabled = 0;
        float strength = 0.0f;
        std::array<float, 2> center{ 0.5f, 0.5f };
        uint32_t samples = 6;
        float clearRadius = 0.0f;
        float outerRadius = 0.72f;
    } saved_;
    // One draw per renderer: mapped vertex/constant buffers cannot be overwritten
    // by a second ring in the same command list. Slots live for the whole title.
    std::array<std::unique_ptr<PlayerSonicBoostRingController>, 4> rings_;
    size_t nextRing_ = 0;
    float ringTimer_ = 0.0f;
    float blurStartProgress_ = -1.0f;
    float blurReleaseElapsed_ = -1.0f;
    DirectXCommon* dx_ = nullptr;
    Camera* camera_ = nullptr;
};
