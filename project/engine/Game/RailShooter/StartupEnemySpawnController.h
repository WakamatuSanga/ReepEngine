#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstddef>
#include <string>

class Camera;
class EnemyManager;
class LevelSceneRuntime;

class StartupEnemySpawnController {
public:
    StartupEnemySpawnController();
    ~StartupEnemySpawnController();

    void Initialize(EnemyManager* enemyManager, LevelSceneRuntime* levelSceneRuntime, const Camera* camera);
    void Finalize();
    void Update(float deltaTime);
    void DrawImGui();

private:
    bool SpawnNow();
    bool TryFindSpawnPosition(Vector3& outPosition, std::string& outSource) const;
    bool TryFindNamedSpawnPosition(const std::string& targetName, Vector3& outPosition, std::string& outSource) const;
    Vector3 AdjustStartupSpawnPosition(const Vector3& basePosition);
    Vector3 BuildFallbackPosition() const;
    void SetLastResult(const std::string& result);

    EnemyManager* enemyManager_ = nullptr;
    LevelSceneRuntime* levelSceneRuntime_ = nullptr;
    const Camera* camera_ = nullptr;

#ifdef USE_IMGUI
    bool enableStartupEnemySpawn_ = false;
#else
    bool enableStartupEnemySpawn_ = true;
#endif
    bool spawnOnGameStart_ = true;
    bool hasSpawned_ = false;
    float spawnDelay_ = 0.2f;
    float elapsedTime_ = 0.0f;
    float startupSpawnDistance_ = 16.0f;
    float startupSpawnHeight_ = 2.0f;
    float startupSpawnSideOffset_ = 0.0f;
    float pullSpawnPointTowardCamera_ = 8.0f;
    bool useCameraRelativeStartupSpawn_ = true;
    bool overrideSpawnPointInRelease_ = true;
    std::string startupSpawnTargetName_ = "EnemySpawn_01";
    std::string lastSpawnSource_ = "(none)";
    std::string lastSpawnResult_ = "Not spawned";
    Vector3 lastRawSpawnPosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 lastSpawnPosition_{ 0.0f, 0.0f, 0.0f };
    float lastRawCameraDistance_ = 0.0f;
    float lastAdjustedCameraDistance_ = 0.0f;
    bool lastUsedStartupAdjustment_ = false;
    size_t spawnCount_ = 0;
    size_t failedSpawnCount_ = 0;
};
