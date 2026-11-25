#pragma once

#include "Engine/math/Matrix4x4.h"
#include <cstdint>
#include <memory>

class Camera;
class DirectXCommon;
class GameViewport;
class Input;
class Player;
class PlayerJetExhaustController;
class SpriteCommon;
class StageClearResultController;
class TitleCloudTransition;

class StageClearSequenceController {
public:
    StageClearSequenceController();
    ~StageClearSequenceController();
    static bool IsMidbossClearEnabled();
    static void DrawImGui();
    void Initialize(SpriteCommon* sprites, Camera* camera, Player* player,
        PlayerJetExhaustController* exhaust, GameViewport* viewport, Input* input);
    void Begin(int score, std::uint32_t defeats);
    void Update(float deltaTime, bool tentaclesGone);
    void DrawDepartureEffects();
    void DrawOverlay();
    void OnFramePresented();
    bool IsActive() const;
    bool NeedsTentacleUpdate() const;
    bool IsAircraftVisible() const;
    bool IsResultBackground() const { return resultBackground_; }

private:
    enum class Phase { Idle, Tentacles, Departure, ExitedFrame, Clouds, Results };
    class State;
    class Contrails;
    void StartDeparture();
    void AdvanceDeparturePose(float deltaTime);
    void UpdateDepartureEffects(float deltaTime);
    bool HasAircraftPassedUpperEdge() const;

    std::unique_ptr<TitleCloudTransition> clouds_;
    std::unique_ptr<StageClearResultController> result_;
    std::unique_ptr<Contrails> contrails_;
    DirectXCommon* dx_ = nullptr;
    Camera* camera_ = nullptr;
    Player* player_ = nullptr;
    PlayerJetExhaustController* exhaust_ = nullptr;
    const State* state_;
    float elapsed_ = 0.0f;
    int score_ = 0;
    std::uint32_t defeats_ = 0;
    bool resultBackground_ = false;
    bool transitionRequested_ = false;
    Transform aircraftStart_{};
    Vector3 aircraftOffset_{};
    Vector3 aircraftBoundsMin_{};
    Vector3 aircraftBoundsMax_{};
    Vector3 aircraftEndRotation_{};
};
