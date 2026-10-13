#pragma once
#include "Engine/math/Matrix4x4.h"

class Camera;

class CameraShakeController {
public:
    enum class GameplayEvent { Shot, EnemyDefeat, Explosion };
    CameraShakeController();
    ~CameraShakeController();

    void Initialize();
    void Finalize();
    void BeginFrame(Camera* camera);
    void Start(float duration, float amplitude, float frequency);
    void SetGameplayEventsEnabled(bool enabled);
    void RequestGameplayShake(GameplayEvent event);
    void UpdateAndApply(float deltaTime, Camera* camera, bool viewOnly = false);
    void Reset(Camera* camera);
    void DrawImGui();

    bool IsPlaying() const { return isPlaying_; }
    const Vector3& GetCurrentOffset() const { return currentOffset_; }

private:
    struct RotationPulse {
        float age = 0.0f;
        float duration = 0.0f;
        float amplitudeDegrees = 0.0f;
    };
    void UpdateGameplayShake(float deltaTime, Camera* camera);
    void RemoveAppliedOffset(Camera* camera);

    bool isPlaying_ = false;
    bool hasAppliedOffset_ = false;
    bool appliedToView_ = false;
    float elapsedTime_ = 0.0f;
    float duration_ = 0.7f;
    float amplitude_ = 0.08f;
    float frequency_ = 24.0f;
    Vector3 currentOffset_{ 0.0f, 0.0f, 0.0f };
    RotationPulse shotPulse_{};
    RotationPulse impactPulse_{};
    Vector3 currentRotationOffset_{};
    bool gameplayEventsEnabled_ = false;
    bool hasAppliedRotation_ = false;
    float rotationClock_ = 0.0f;
    float shotCooldown_ = 0.0f;
    float impactCooldown_ = 0.0f;
    float rotationAmplitude_ = 0.0f;
};
