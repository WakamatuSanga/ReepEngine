#pragma once
#include "Engine/math/Matrix4x4.h"

class Camera;

class CameraShakeController {
public:
    CameraShakeController();
    ~CameraShakeController();

    void Initialize();
    void Finalize();
    void BeginFrame(Camera* camera);
    void Start(float duration, float amplitude, float frequency);
    void UpdateAndApply(float deltaTime, Camera* camera);
    void Reset(Camera* camera);
    void DrawImGui();

    bool IsPlaying() const { return isPlaying_; }
    const Vector3& GetCurrentOffset() const { return currentOffset_; }

private:
    void RemoveAppliedOffset(Camera* camera);

    bool isPlaying_ = false;
    bool hasAppliedOffset_ = false;
    float elapsedTime_ = 0.0f;
    float duration_ = 0.7f;
    float amplitude_ = 0.08f;
    float frequency_ = 24.0f;
    Vector3 currentOffset_{ 0.0f, 0.0f, 0.0f };
};
