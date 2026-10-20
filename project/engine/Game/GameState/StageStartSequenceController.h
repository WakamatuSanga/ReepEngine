#pragma once

#include "Engine/math/Matrix4x4.h"
#include <array>
#include <memory>

class Camera;
class Player;
class PlayerJetExhaustController;
class RailShooterCameraRig;
class Sprite;
class SpriteCommon;

// Owns display poses and clocks only. Simulation starts after the final Present.
class StageStartSequenceController {
public:
    enum class Phase { AwaitingReveal, FadeIn, Title, FadeOut, Crossing, CrossingExit, Gap, Entry, Settle, Complete };
    struct Settings {
        float crossingSeconds = 1.28f;
        float gapSeconds = 0.12f;
        float entrySeconds = 1.28f;
        float crossingWidth = 0.40f;
        float crossingDistanceScale = 1.0f;
        float exitMargin = 0.15f;
        float entryBelowScreen = 1.15f;
        float entryCameraLift = 0.10f;
    };
    static constexpr float kFadeInSeconds = 0.35f;
    static constexpr float kTitleSeconds = 1.35f;
    static constexpr float kFadeOutSeconds = 0.55f;
    static Settings& GetTuning(); // Session-only; captured at the next Restart, never writes JSON.

    StageStartSequenceController();
    ~StageStartSequenceController();
    void Initialize(SpriteCommon* sprites, Camera* camera, Player* player,
        RailShooterCameraRig* rig, PlayerJetExhaustController* exhaust);
    void OnPresentationRevealed();
    void OnFramePresented();
    void Update(float deltaTime);
    void DrawOverlay();
    void DrawImGui();
    void Cancel();
    void SetPaused(bool paused) { paused_ = paused; }
    bool IsPaused() const { return paused_; }
    bool IsActive() const { return phase_ != Phase::Complete; }
    bool IsAircraftVisible() const;
    Phase GetPhase() const { return phase_; }
    float GetTitleAlpha() const;
    float GetHudTopReveal() const;
    float GetHudBottomReveal() const;
private:
    void ApplyDisplayPose();
    void ApplyCrossingPose(float progress);
    void ApplyEntryPose(float progress);
    void RestoreDisplayPose();
    void SetCameraLift(float lift);
    Phase phase_ = Phase::AwaitingReveal;
    float elapsed_ = 0.0f;
    bool paused_ = false;
    Settings settings_{};
    SpriteCommon* sprites_ = nullptr;
    Camera* camera_ = nullptr;
    Player* player_ = nullptr;
    RailShooterCameraRig* rig_ = nullptr;
    PlayerJetExhaustController* exhaust_ = nullptr;
    Vector3 cameraTranslationOffset_{};
    Vector3 cameraRotationOffset_{};
    float cameraFovY_ = 0.45f;
    Matrix4x4 referenceFrame_{};
    Transform normalPose_{};
    Vector3 crossingRotation_{};
    std::array<Vector3, 8> modelCorners_{}; // Scaled real-model bounds, never changes model Scale.
    float crossingDepth_ = 0.0f;
    float crossingExitX_ = 0.0f;
    std::array<std::unique_ptr<Sprite>, 2> titles_;
    std::unique_ptr<Sprite> shade_;
};
