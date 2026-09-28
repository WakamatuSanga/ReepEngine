#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstdint>
#include <memory>
#include <string>

class Camera;
class Model;
class Object3d;
class Object3dCommon;

class EnemyBullet {
public:
    EnemyBullet();
    ~EnemyBullet();

    bool Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Finalize();
    void Update(float deltaTime);
    void Draw();
    void DrawRadius();
    void DrawImGui();

    void SetPosition(const Vector3& position);
    void SetVelocity(const Vector3& velocity);
    void SetRotation(const Vector3& rotation);
    void SetModelRotationOffset(const Vector3& rotationOffset);
    void SetVisualForwardOverride(const Vector3& forward);
    void ClearVisualForwardOverride();
    void SetScale(const Vector3& scale);
    void SetRadius(float radius);
    void SetLifeTime(float lifeTime);
    void SetUseLightweightVisual(bool useLightweightVisual);
    void SetModelPath(const std::string& modelPath);
    void Kill(const std::string& reason = "外部処理");

    bool IsActive() const { return isActive_; }
    bool IsDead() const { return isDead_; }
    bool IsInitialized() const { return initialized_; }
    const Vector3& GetPosition() const { return position_; }
    const Vector3& GetVelocity() const { return velocity_; }
    const Vector3& GetVisualModelRotation() const { return visualModelRotation_; }
    const Vector3& GetVisualForwardOverride() const { return visualForwardOverride_; }
    const std::string& GetDeathReason() const { return deathReason_; }
    uint64_t GetProjectileRailFrameSequence() const { return projectileRailFrameSequence_; }
    uint64_t GetProjectileSpawnSequence() const { return projectileSpawnSequence_; }
    void SetProjectileRailFrameSequence(uint64_t sequence) { projectileRailFrameSequence_ = sequence; }
    void SetProjectileSpawnSequence(uint64_t sequence) { projectileSpawnSequence_ = sequence; }
    bool HasVisualForwardOverride() const { return hasVisualForwardOverride_; }
    float GetRadius() const { return radius_; }
    float GetLifeTime() const { return lifeTime_; }
    float GetElapsedTime() const { return currentTime_; }

private:
    void LoadModel();
    void UpdateObjectTransform();

    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    std::unique_ptr<Object3d> object_;
    std::unique_ptr<Object3d> radiusObject_;
    Model* model_ = nullptr;
    Model* radiusModel_ = nullptr;

    std::string modelPath_ = "resources/EnemyBullet/EnemyBullet.obj";
    std::string resolvedModelPath_;
    std::string texturePath_;
    std::string loadStatus_ = "Not initialized";
    std::string deathReason_ = "未生成";

    Vector3 position_{ 0.0f, 0.0f, 0.0f };
    Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
    Vector3 rotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 modelRotationOffset_{ 0.0f, 4.71238899f, 0.0f };
    Vector3 visualBaseRotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 visualModelRotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 lastVisualForward_{ 0.0f, 0.0f, 1.0f };
    Vector3 visualForwardOverride_{ 0.0f, 0.0f, 1.0f };
    Vector3 scale_{ 0.35f, 0.35f, 0.35f };
    float radius_ = 0.15f;
    float lifeTime_ = 6.0f;
    float currentTime_ = 0.0f;
    uint64_t projectileRailFrameSequence_ = 0;
    uint64_t projectileSpawnSequence_ = 0;
    bool isActive_ = true;
    bool isDead_ = false;
    bool useFallbackModel_ = false;
    bool useLightweightVisual_ = false;
    bool hasVisualForwardOverride_ = false;
    bool initialized_ = false;
};
