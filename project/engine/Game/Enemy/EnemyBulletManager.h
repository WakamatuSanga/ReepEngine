#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstddef>
#include <memory>
#include <vector>

class Camera;
class EnemyBullet;
class Object3dCommon;
class ProjectileRailMotionAdapter;

class EnemyBulletManager {
public:
    EnemyBulletManager();
    ~EnemyBulletManager();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Finalize();
    void Update(float deltaTime);
    void Draw();
    void DrawImGui();

    EnemyBullet* SpawnBullet(const Vector3& position, const Vector3& velocity);
    void SetProjectileRailMotionAdapter(ProjectileRailMotionAdapter* adapter) { projectileRailMotionAdapter_ = adapter; }
    void RecordProjectileShot(
        const Vector3& muzzle,
        const Vector3& target,
        const Vector3& direction,
        const Vector3& relativeVelocity);
    void DeleteAllBullets();
    void SetUseLightweightBulletVisual(bool useLightweightVisual);
    size_t ClearBulletsInRadius(const Vector3& center, float radius);
    size_t ClearBulletsInRadius(const Vector3& center, float radius, std::vector<Vector3>* clearedPositions, size_t maxRecordedPositions);
    bool CheckHitAndKillFirstSphere(const Vector3& center, float radius, Vector3* hitPosition);
    bool CheckHitAndKillFirstSphere(const Vector3& center, float radius, Vector3* hitPosition, float* lastDistance, float* lastRadiusSum, float* lastBulletRadius);
    size_t GetBulletCount() const;
    size_t GetActiveCount() const;

private:
    void RemoveDeadBullets();
    void ApplyModelRotationOffsetToAllBullets();

    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    ProjectileRailMotionAdapter* projectileRailMotionAdapter_ = nullptr;
    std::vector<std::unique_ptr<EnemyBullet>> bullets_;
    int selectedBulletIndex_ = -1;
    bool autoRemoveDeadBullets_ = true;
    Vector3 defaultScale_{ 0.35f, 0.35f, 0.35f };
    Vector3 defaultRotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 defaultModelRotationOffset_{ 0.0f, 4.71238899f, 0.0f };
    float defaultRadius_ = 0.15f;
    bool showEnemyBulletRadius_ = false;
    bool useLightweightBulletVisual_ = false;
};
