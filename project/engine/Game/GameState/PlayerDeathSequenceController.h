#pragma once
#include <memory>
#include <string>
class Camera;
class CameraShakeController;
class DirectXCommon;
class SpriteCommon;
class Player;
class BoostController;
class PlayerDamageFeedbackController;
class GpuParticleEffectPlayer;
class CombatEffectController;
class EnemyWaveManager;
class KrakenTentacleWaveEncounterController;
class Object3dCommon;
class ModelCommon;

// One death owner: captures the combat route once and runs isolated visual actors.
class PlayerDeathSequenceController {
public:
    enum class Route { RearEnemy, KrakenGrip };
    struct Settings {
        float cameraSeconds = .85f;
        float cameraDistance = 10.5f;
        float cameraHeight = .7f;
        float cameraFov = .90f;
        float bossCameraSeconds = .50f;
        float bossCameraAzimuth = .42f;
        float bossCameraHeight = 1.6f;
        float bossCameraDistance = 12;
        float bossCameraFocusY = .6f;
        float enemyAppearDelay = .20f;
        float enemyRiseSeconds = .40f;
        float fireDelay = .45f;
        float enemyDistance = 8;
        float enemySide = 3.2f;
        float enemyHeight = 1.1f;
        float enemyScale = .65f;
        float bulletSpeed = 17;
        float bulletScale = .55f;
        float bulletGlowWidth = .18f;
        float bulletTrailLength = 1.1f;
        float hitToExplosion = .12f;
        float tentacleRiseSeconds = .55f;
        float tentacleApproachSeconds = .40f;
        float gripHoldSeconds = .15f;
        float squeezeSeconds = .40f;
        float tentacleStartSpread = 4.4f;
        float tentacleRootY = -12;
        float tentacleScale = 1.65f;
        float tentacleGripWidth = 1.7f;
        float tentacleCurl = .8f;
        float squeezeAmount = .65f;
        float explosionScale = 1.4f;
        float explosionBrightness = 1;
        float explosionAmount = 1;
        float explosionLifetime = 1;
        float debrisLifetime = 2;
        int debrisCount = 12;
        float shakeStrength = 1;
        float resultDelay = .8f;
    };
    static Settings& GetTuning();
    PlayerDeathSequenceController();
    ~PlayerDeathSequenceController();
    void Initialize(DirectXCommon*, SpriteCommon*, CameraShakeController*);
    void BindFlight(Player*, Camera*, BoostController*, PlayerDamageFeedbackController*,
        GpuParticleEffectPlayer*, CombatEffectController*, EnemyWaveManager*,
        KrakenTentacleWaveEncounterController*, ModelCommon*, Object3dCommon*);
    bool PrepareForPresentation();
    void Finalize();
    void StartDeathSequence();
    void StartPreview(Route route);
    void SetPreviewPaused(bool paused);
    bool IsPreview() const;
    void Update(float unscaledDeltaTime);
    void Draw();
    void DrawImGui();
    void Reset();
    float GetElapsedTime() const { return elapsedTime_; }
    float GetDuration() const;
    float GetExplosionTime() const;
    float GetPresentationDeltaTime(float unscaledDeltaTime) const;
    float GetHudVisibility() const;
    Route GetRoute() const;
    const std::string& GetCapturedWaveId() const;
    bool IsAircraftVisible() const;
    bool HasFired() const;
    bool HasHit() const;
    bool HasExploded() const;
    bool IsPlaying() const { return isPlaying_; }
    bool IsFinished() const { return isFinished_; }
    bool IsActiveOrFinished() const { return isPlaying_ || isFinished_; }
private:
    struct Flight;
    void Start(bool preview, Route route);
    void ApplyPresentation();
    void Explode();
    std::unique_ptr<Flight> flight_;
    CameraShakeController* cameraShake_ = nullptr;
    bool isPlaying_ = false;
    bool isFinished_ = false;
    float elapsedTime_ = 0;
};
