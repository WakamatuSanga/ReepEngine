#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstddef>
#include <string>
#include <vector>

class EnemyManager;
class LevelSceneRuntime;
struct FiredEventAction;

class EnemySpawnActionBridge {
public:
    EnemySpawnActionBridge();
    ~EnemySpawnActionBridge();

    void Initialize(EnemyManager* enemyManager, LevelSceneRuntime* levelSceneRuntime);
    void Finalize();
    void DrawImGui();
    bool HandleAction(const FiredEventAction& action, std::string& resultMessage);

private:
    void AddLog(const std::string& message);
    static bool IsSpawnEnemyAction(const std::string& actionType);

    EnemyManager* enemyManager_ = nullptr;
    LevelSceneRuntime* levelSceneRuntime_ = nullptr;
    std::vector<std::string> actionLog_;
    std::string lastTarget_ = "(none)";
    std::string lastResult_ = "(none)";
    Vector3 lastSpawnPosition_{ 0.0f, 0.0f, 0.0f };
    bool enableSpawnEnemyAction_ = true;
    size_t spawnCount_ = 0;
    size_t failedSpawnCount_ = 0;
};
