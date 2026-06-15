#pragma once
#include "Engine/math/Matrix4x4.h"
#include <memory>
#include <string>

class Camera;
class Model;
class Object3d;
class Object3dCommon;

class Enemy {
public:
    Enemy();
    ~Enemy();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera, const std::string& enemyId);
    void Finalize();
    void Update(float deltaTime);
    void Draw();
    void DrawImGui();

    void SetEnemyId(const std::string& enemyId);
    void SetEnemyType(const std::string& enemyType);
    void SetPosition(const Vector3& position);
    void SetRotation(const Vector3& rotation);
    void SetScale(const Vector3& scale);
    void SetVelocity(const Vector3& velocity);
    void SetHitRadius(float hitRadius);
    void SetModelPath(const std::string& modelPath);

    void Damage(int amount);
    void Kill();
    void Revive(int hp);

    bool IsActive() const { return isActive_; }
    bool IsDead() const { return isDead_; }
    const std::string& GetEnemyId() const { return enemyId_; }
    const std::string& GetEnemyType() const { return enemyType_; }
    const Vector3& GetPosition() const { return position_; }
    float GetHitRadius() const { return hitRadius_; }
    int GetHp() const { return hp_; }

private:
    void LoadModel();
    void UpdateObjectTransform();

    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    std::unique_ptr<Object3d> object_;
    Model* model_ = nullptr;

    std::string enemyId_;
    std::string enemyType_ = "Default";
    std::string modelPath_ = "resources/Enemy/Enemy.obj";
    std::string resolvedModelPath_;
    std::string texturePath_;
    std::string loadStatus_ = "Not initialized";

    Vector3 position_{ 0.0f, 0.0f, 10.0f };
    Vector3 rotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 modelRotationOffset_{ 0.0f, 1.57079637f, 0.0f };
    Vector3 visualModelRotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 scale_{ 0.8f, 0.8f, 0.8f };
    Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
    float hitRadius_ = 0.6f;
    int hp_ = 10;
    bool isActive_ = true;
    bool isDead_ = false;
    bool useFallbackModel_ = false;
};
