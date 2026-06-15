#pragma once
#include "Engine/math/Matrix4x4.h"
#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class Camera;
class EnemyBullet;
class Object3dCommon;
class Player;

class PlayerBulletManager {
public:
    PlayerBulletManager();
    ~PlayerBulletManager();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera, Player* player);
    void Finalize();
    void Update(float deltaTime);
    void Draw();
    void DrawImGui();

    void SetGameViewInputActive(bool isActive);
    EnemyBullet* SpawnBullet(const Vector3& position, const Vector3& velocity, int damage);
    void DeleteAllBullets();
    bool CheckHitAndKillFirstSphere(
        const Vector3& center,
        float radius,
        Vector3* hitPosition,
        int* damage,
        float* lastDistance,
        float* lastRadiusSum,
        float* lastBulletRadius);
    size_t GetBulletCount() const;
    size_t GetActiveCount() const;

private:
    struct PlayerBulletInstance {
        std::unique_ptr<EnemyBullet> bullet;
        int damage = 1;
    };

    void FireFromPlayer();
    void RemoveDeadBullets();
    void SyncModelPathBuffer();
    bool ShouldBlockFireInput();

    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    Player* player_ = nullptr;
    std::vector<PlayerBulletInstance> bullets_;
    std::array<char, 260> modelPathBuffer_{};
    std::string modelPath_ = "resources/EnemyBullet/EnemyBullet.obj";
    std::string inputBlockedReason_ = "Not initialized";
    Vector3 defaultScale_{ 0.35f, 0.35f, 0.35f };
    Vector3 defaultRotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 lastFirePosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 lastFireDirection_{ 0.0f, 0.0f, 1.0f };
    float bulletSpeed_ = 28.0f;
    float bulletRadius_ = 0.16f;
    float bulletLifeTime_ = 3.0f;
    float fireInterval_ = 0.16f;
    float fireTimer_ = 0.0f;
    float muzzleOffset_ = 0.7f;
    int bulletDamage_ = 1;
    int selectedBulletIndex_ = -1;
    bool enablePlayerShot_ = true;
    bool gameViewInputActive_ = false;
    bool showBulletCollisionRadius_ = false;
    bool autoRemoveDeadBullets_ = true;
    size_t firedBulletCount_ = 0;
};
