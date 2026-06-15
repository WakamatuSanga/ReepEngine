#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Camera;
class Enemy;
class Object3dCommon;

class EnemyManager {
public:
    EnemyManager();
    ~EnemyManager();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Finalize();
    void Update(float deltaTime);
    void Draw();
    void DrawImGui();

    Enemy* SpawnEnemy(const std::string& enemyType = "Default", Vector3 position = { 0.0f, 0.0f, 10.0f });
    Enemy* SpawnEnemyAt(const Vector3& position);
    Enemy* SpawnEnemyAt(const Vector3& position, const std::string& enemyType);
    void DeleteAllEnemies();
    size_t GetEnemyCount() const;
    size_t GetActiveCount() const;
    std::vector<Vector3> GetActiveEnemyPositions() const;
    std::vector<Enemy*> GetActiveEnemies() const;
    void SetDefaultHitRadius(float hitRadius);
    void ApplyDefaultHitRadiusToAllEnemies();
    float GetDefaultHitRadius() const { return defaultHitRadius_; }

private:
    std::string MakeEnemyId();
    void RemoveDeadEnemies();

    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    std::vector<std::unique_ptr<Enemy>> enemies_;
    uint32_t nextEnemySerial_ = 1;
    int selectedEnemyIndex_ = -1;
    bool autoRemoveDeadEnemies_ = false;
    Vector3 debugSpawnPosition_{ 0.0f, 0.0f, 10.0f };
    Vector3 debugSpawnRotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 debugSpawnScale_{ 0.8f, 0.8f, 0.8f };
    float defaultHitRadius_ = 0.6f;
};
