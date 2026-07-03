#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstdint>
#include <memory>
#include <vector>

class Camera;
class DirectXCommon;
class PlayerJetExhaustBeamRenderer;

class EnemyLaserTelegraphController {
public:
    EnemyLaserTelegraphController();
    ~EnemyLaserTelegraphController();

    bool Initialize(DirectXCommon* dxCommon, const Camera* camera);
    void Finalize();
    void Update(float deltaTime);
    void Draw();
    void DrawAfterCloud();
    void DrawImGui();

    void StartWarning(const void* owner, const Vector3& origin, const Vector3& direction);
    void UpdateWarning(const void* owner, const Vector3& origin, const Vector3& direction);
    void LockWarning(const void* owner, const Vector3& origin, const Vector3& targetPosition, const Vector3& direction);
    void StartBeam(const void* owner, const Vector3& origin, const Vector3& direction);
    void ClearOwner(const void* owner);

    bool IsEnabled() const { return enableLaserTelegraph_; }
    bool IsLoopLaserEnabled() const { return loopLaser_; }
    bool IsAimLockEnabled() const { return aimLockEnabled_; }
    float GetWarningDuration() const { return warningDuration_; }
    float GetBeamDuration() const { return beamDuration_; }
    float GetLockBeforeFireTime() const { return lockBeforeFireTime_; }

private:
    enum class EffectType {
        Warning,
        Beam,
    };

    struct LaserEffect {
        const void* owner = nullptr;
        Vector3 origin{ 0.0f, 0.0f, 0.0f };
        Vector3 direction{ 0.0f, 0.0f, 1.0f };
        Vector3 lockedTargetPosition{ 0.0f, 0.0f, 0.0f };
        float age = 0.0f;
        float duration = 0.1f;
        EffectType type = EffectType::Warning;
        bool aimLocked = false;
        bool active = false;
    };

    LaserEffect* FindEffect(const void* owner, EffectType type);
    LaserEffect* AllocateEffect();
    void DrawLayer(bool afterCloudLayer);
    int CountActiveEffects(EffectType type) const;
    int CountLockedWarnings(bool locked) const;
    int CountActiveOwners() const;
    void ClampSettings();

    std::unique_ptr<PlayerJetExhaustBeamRenderer> renderer_;
    std::vector<LaserEffect> effects_;
    const Camera* camera_ = nullptr;
    bool initialized_ = false;
    bool enableLaserTelegraph_ = true;
    bool drawAfterCloud_ = true;
    bool loopLaser_ = true;
    bool aimLockEnabled_ = true;
    bool showLockedDirectionDebug_ = false;
    int maxActiveEffects_ = 8;
    int activeWarningCount_ = 0;
    int activeTrackingWarningCount_ = 0;
    int activeLockedWarningCount_ = 0;
    int activeBeamCount_ = 0;
    int activeLaserInstanceCount_ = 0;
    uint64_t droppedEffectCount_ = 0;
    float warningDuration_ = 1.05f;
    float lockBeforeFireTime_ = 0.25f;
    float blinkRate_ = 8.5f;
    float lockedBlinkRate_ = 16.0f;
    float warningLineWidth_ = 0.085f;
    float warningLineLength_ = 70.0f;
    float warningAlpha_ = 0.55f;
    float lockedWarningAlpha_ = 0.78f;
    float beamDuration_ = 0.45f;
    float beamCoreWidth_ = 0.13f;
    float beamGlowWidth_ = 0.34f;
    float beamLength_ = 80.0f;
    float beamAlpha_ = 0.85f;
    float beamGlowAlpha_ = 0.45f;
    float beamBrightness_ = 1.75f;
    float time_ = 0.0f;
    Vector3 lastOrigin_{ 0.0f, 0.0f, 0.0f };
    Vector3 lastDirection_{ 0.0f, 0.0f, 1.0f };
    Vector3 lastLockedTargetPosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 lastLockedDirection_{ 0.0f, 0.0f, 1.0f };
    const void* lastStartedOwner_ = nullptr;
    const void* lastEndedOwner_ = nullptr;
    int testOwnerLeft_ = 0;
    int testOwnerRight_ = 0;
};

