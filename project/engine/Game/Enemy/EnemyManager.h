#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Camera;
class Enemy;
class Object3dCommon;
class Player;

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
    Enemy* SpawnEnemy(const std::string& enemyType, Vector3 position, const Vector3& forward);
    Enemy* SpawnEnemyAt(const Vector3& position);
    Enemy* SpawnEnemyAt(const Vector3& position, const std::string& enemyType);
    Enemy* SpawnEnemyAt(const Vector3& position, const std::string& enemyType, const Vector3& forward);
    void DeleteAllEnemies();
    size_t GetEnemyCount() const;
    size_t GetActiveCount() const;
    std::vector<Vector3> GetActiveEnemyPositions() const;
    std::vector<Enemy*> GetActiveEnemies() const;
    void SetDefaultHitRadius(float hitRadius);
    void ApplyDefaultHitRadiusToAllEnemies();
    float GetDefaultHitRadius() const { return defaultHitRadius_; }
    void SetUseLightweightEnemyVisual(bool useLightweightVisual);
    void SetPlayer(Player* player);

private:
    std::string MakeEnemyId();
    void RemoveDeadEnemies();
    Vector3 GetDefaultSpawnForward() const;
    Vector3 GetSpawnLookTarget() const;
    Vector3 BuildSpawnStartPosition(const Vector3& targetPosition) const;

    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    Player* player_ = nullptr;
    std::vector<std::unique_ptr<Enemy>> enemies_;
    uint32_t nextEnemySerial_ = 1;
    int selectedEnemyIndex_ = -1;
    bool autoRemoveDeadEnemies_ = false;
    bool debugSpawnFaceCameraOpposite_ = true;
    Vector3 debugSpawnPosition_{ 0.0f, 0.0f, 10.0f };
    Vector3 debugSpawnRotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 debugSpawnScale_{ 0.8f, 0.8f, 0.8f };
    float defaultHitRadius_ = 0.6f;
    float spawnEntryHeight_ = 12.0f;
    float spawnEntryDepth_ = 8.0f;
    float spawnEntrySideOffset_ = 0.0f;
    float spawnDuration_ = 1.0f;
    float spawnSpinSpeedDegrees_ = 720.0f;
    float spawnAttackDelay_ = 1.0f;
    float spawnFacePlayerStartT_ = 0.35f;
    float spawnFacePlayerEndT_ = 0.95f;
    float spawnSpinFadeStartT_ = 0.25f;
    float spawnSpinFadeEndT_ = 0.85f;
    float spawnAlignDuration_ = 0.2f;
    bool useLightweightEnemyVisual_ = false;
    bool useCameraRelativeSpawnEntry_ = true;
    bool spawnFaceDownDuringSpawn_ = true;
    bool spawnFacePlayerOnComplete_ = true;
    bool spawnResetRollOnActive_ = true;
    bool spawnResetPitchOnActive_ = true;
    bool spawnCollisionDuringSpawn_ = false;
    bool spawnSpinAroundForward_ = true;
    bool spawnFacePlayerDuringSpawn_ = true;
    bool spawnAlignAfterSpawn_ = true;
    int spawnAlignSmoothType_ = 1;
};
