#pragma once
#include "Engine/math/Matrix4x4.h"
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

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Finalize();
    void Update(float deltaTime);
    void Draw();
    void DrawRadius();
    void DrawImGui();

    void SetPosition(const Vector3& position);
    void SetVelocity(const Vector3& velocity);
    void SetRotation(const Vector3& rotation);
    void SetScale(const Vector3& scale);
    void SetRadius(float radius);
    void SetLifeTime(float lifeTime);
    void SetModelPath(const std::string& modelPath);
    void Kill();

    bool IsActive() const { return isActive_; }
    bool IsDead() const { return isDead_; }
    const Vector3& GetPosition() const { return position_; }
    float GetRadius() const { return radius_; }

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

    Vector3 position_{ 0.0f, 0.0f, 0.0f };
    Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
    Vector3 rotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 modelRotationOffset_{ 0.0f, 1.57079637f, 0.0f };
    Vector3 visualBaseRotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 visualModelRotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 lastVisualForward_{ 0.0f, 0.0f, 1.0f };
    Vector3 scale_{ 0.35f, 0.35f, 0.35f };
    float radius_ = 0.15f;
    float lifeTime_ = 6.0f;
    float currentTime_ = 0.0f;
    bool isActive_ = true;
    bool isDead_ = false;
    bool useFallbackModel_ = false;
};
