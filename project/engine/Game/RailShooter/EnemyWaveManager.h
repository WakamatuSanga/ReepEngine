#pragma once
#include "Engine/Game/RailShooter/EnemyWaveData.h"
#include "Engine/math/Matrix4x4.h"
#include <array>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

class Camera;
class Enemy;
class EnemyManager;
class Player;
struct FiredEventAction;

class EnemyWaveManager {
public:
    EnemyWaveManager();
    ~EnemyWaveManager();

    void Initialize(EnemyManager* enemyManager, const Camera* camera);
    void Finalize();
    void Update(float deltaTime);
    void DrawImGui();

    void SetPlayer(const Player* player) { player_ = player; }
    void SetCurrentBoostPower(float boostPower);
    void SetGameModeActive(bool isGameMode);

    bool HandleSpawnWaveAction(const FiredEventAction& action, std::string& resultMessage);
    bool PlayWave(const std::string& waveId, std::string& resultMessage);
    void StopAllWaves();

    size_t GetLoadedWaveCount() const { return waves_.size(); }
    size_t GetActiveWaveCount() const { return activeWaves_.size(); }

private:
    struct ActiveWave {
        size_t waveIndex = 0;
        float elapsedTime = 0.0f;
        std::vector<bool> spawned;
        size_t spawnedCount = 0;
        bool stopped = false;
    };

    struct WaveEnemyRuntime {
        Enemy* enemy = nullptr;
        std::string movePattern;
        Vector3 lockedApproachDirection{ 0.0f, 0.0f, -1.0f };
        bool hasLockedApproachDirection = false;
    };

    bool LoadWaveById(const std::string& waveId, std::string& resultMessage);
    bool LoadWaveFile(const std::string& filePath, std::string& resultMessage);
    bool IsWaveLoaded(const std::string& waveId) const;
    std::string ResolveWaveIdFromAction(const FiredEventAction& action) const;
    Vector3 ComputeSpawnPosition(const EnemyWaveSpawnEntry& spawn) const;
    Vector3 ComputeSpawnForward(const Vector3& spawnPosition) const;
    Vector3 ResolveApproachTarget() const;
    void RegisterWaveEnemy(Enemy* enemy, const EnemyWaveSpawnEntry& spawn);
    void UpdateWaveEnemyMovement(float deltaTime);
    void ClearTrackedWaveEnemies();
    bool IsHomingApproachMovePattern(const std::string& movePattern) const;
    bool IsStraightApproachMovePattern(const std::string& movePattern) const;
    bool RestartAutoStartWave(std::string& resultMessage);
    void AddLog(const std::string& message);
    void ResetManualWaveBuffer();

    EnemyManager* enemyManager_ = nullptr;
    const Camera* camera_ = nullptr;
    const Player* player_ = nullptr;
    std::vector<EnemyWaveDefinition> waves_;
    std::unordered_map<std::string, size_t> waveIndexById_;
    std::vector<ActiveWave> activeWaves_;
    std::vector<WaveEnemyRuntime> waveEnemies_;
    std::vector<std::string> waveLog_;
    std::string lastWaveId_ = "(none)";
    std::string lastResult_ = "(none)";
    Vector3 lastSpawnPosition_{ 0.0f, 0.0f, 0.0f };
    float lastWaveElapsedTime_ = 0.0f;
    size_t lastWaveSpawnedCount_ = 0;
    size_t lastWaveEnemyCount_ = 0;
    std::array<char, 64> manualWaveIdBuffer_{};
    bool enabled_ = true;
    bool autoLoadMissingWave_ = true;
    bool autoStartWaveOnGameMode_ = true;
    bool gameModeWasActive_ = false;
    std::string autoStartWaveId_ = "wave_001";
    float spawnWidth_ = 20.0f;
    float spawnHeight_ = 12.0f;
    float approachSpeed_ = 3.0f;
    float boostApproachBonus_ = 6.0f;
    float approachStopDistance_ = 6.0f;
    float currentBoostPower_ = 0.0f;
    Vector3 lastLockedApproachDirection_{ 0.0f, 0.0f, -1.0f };
    size_t startedWaveCount_ = 0;
    size_t failedWaveCount_ = 0;
    size_t spawnedEnemyCount_ = 0;
    size_t lastGameModeAutoStartCount_ = 0;
    size_t despawnedOutOfCameraCount_ = 0;
};
