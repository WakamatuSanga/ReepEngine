#pragma once

#include <cstdint>
#include <memory>

class Camera;
class ModelCommon;
class Object3dCommon;
class Player;
class PlayerBulletManager;

enum class KrakenTentacleMidbossState : std::uint8_t {
    Hidden,
    Idle,
    Windup,
    WindupHold,
    Slam,
    ImpactHold,
    Recovery,
};

class KrakenTentacleMidbossController {
public:
    KrakenTentacleMidbossController();
    ~KrakenTentacleMidbossController();

    KrakenTentacleMidbossController(
        const KrakenTentacleMidbossController&) = delete;
    KrakenTentacleMidbossController& operator=(
        const KrakenTentacleMidbossController&) = delete;

    bool Initialize(
        ModelCommon* modelCommon,
        Object3dCommon* object3dCommon);
    void SetCamera(Camera* camera);
    void SetCollisionQueryContext(
        const Player* player,
        const PlayerBulletManager* playerBulletManager);
    void Update(float scaledDeltaTime);
    void Draw();
    void DrawDebug(
        float viewX,
        float viewY,
        float viewWidth,
        float viewHeight) const;
    void DrawImGui();
    void Reset();
    void Finalize();

    bool IsInitialized() const;
    bool IsVisible() const;
    KrakenTentacleMidbossState GetState() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
