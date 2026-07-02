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
class EnemyLaserTelegraphController;
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
    void SetLaserTelegraphController(EnemyLaserTelegraphController* laserController) { laserController_ = laserController; }
    void SetCurrentBoostPower(float boostPower);
    void SetGameModeActive(bool isGameMode);

    bool HandleSpawnWaveAction(const FiredEventAction& action, std::string& resultMessage);
    bool PlayWave(const std::string& waveId, std::string& resultMessage);
    void StopAllWaves();

    size_t GetLoadedWaveCount() const { return waves_.size(); }
    size_t GetActiveWaveCount() const { return activeWaves_.size(); }

private:
    enum class LaserState {
        Waiting,
        WarningTracking,
        WarningLocked,
        Beam,
        Cooldown,
    };

    enum class WaveEnemyEndReason {
        Unknown,
        Dead,
        Escaped,
    };

    struct ActiveWave {
        size_t waveIndex = 0;
        float elapsedTime = 0.0f;
        std::vector<bool> spawned;
        size_t spawnedCount = 0;
        size_t endedCount = 0;
        size_t deadCount = 0;
        size_t escapedCount = 0;
        bool allSpawnsScheduled = false;
        bool completionQueued = false;
        bool stopped = false;
    };

    struct WaveEnemyRuntime {
        Enemy* enemy = nullptr;
        size_t waveIndex = 0;
        std::string movePattern;
        std::string attackPattern;
        float screenX = 0.0f;
        float screenY = 0.0f;
        float depth = 80.0f;
        float spawnScreenY = 0.95f;
        float dropDuration = 0.6f;
        float enemyScale = 1.4f;
        float rotationDuringDrop = 720.0f;
        Vector3 lockedApproachDirection{ 0.0f, 0.0f, -1.0f };
        Vector3 lockedLaserDirection{ 0.0f, 0.0f, 1.0f };
        Vector3 lockedLaserTargetPosition{ 0.0f, 0.0f, 0.0f };
        bool hasLockedApproachDirection = false;
        bool laserAimLocked = false;
        bool ended = false;
        WaveEnemyEndReason endReason = WaveEnemyEndReason::Unknown;
        LaserState laserState = LaserState::Waiting;
        float laserTimer = 0.0f;
    };

    bool LoadWaveById(const std::string& waveId, std::string& resultMessage);
    bool LoadWaveFile(const std::string& filePath, std::string& resultMessage);
    bool IsWaveLoaded(const std::string& waveId) const;
    std::string ResolveWaveIdFromAction(const FiredEventAction& action) const;
    Vector3 ComputeSpawnPosition(const EnemyWaveSpawnEntry& spawn) const;
    Vector3 ComputeScreenPosition(float screenX, float screenY, float depth) const;
    Vector3 ComputeSpawnForward(const Vector3& spawnPosition) const;
    Vector3 ResolveApproachTarget() const;
    void RegisterWaveEnemy(Enemy* enemy, const EnemyWaveSpawnEntry& spawn, size_t waveIndex);
    void UpdateWaveEnemyMovement(float deltaTime);
    void ClearTrackedWaveEnemies();
    void ConfigureScreenAnchorSpawn(Enemy* enemy, const EnemyWaveSpawnEntry& spawn, const Vector3& targetPosition);
    void UpdateLaserTelegraph(WaveEnemyRuntime& runtime, float deltaTime);
    Vector3 ComputeLaserDirection(const Enemy& enemy) const;
    Vector3 ComputeLaserTargetPosition(const Enemy& enemy) const;
    bool IsHomingApproachMovePattern(const std::string& movePattern) const;
    bool IsStraightApproachMovePattern(const std::string& movePattern) const;
    bool IsScreenAnchorMovePattern(const std::string& movePattern) const;
    bool IsLaserTelegraphAttackPattern(const std::string& attackPattern) const;
    bool RestartAutoStartWave(std::string& resultMessage);
    void UpdateWaveProgression(float deltaTime);
    void MarkWaveEnemyEnded(WaveEnemyRuntime& runtime, WaveEnemyEndReason reason);
    ActiveWave* FindActiveWave(size_t waveIndex);
    bool IsWaveCurrentlyActive(const std::string& waveId) const;
    void ClearPendingNextWave();
    static const char* ToWaveEndReasonText(WaveEnemyEndReason reason);
    void AddLog(const std::string& message);
    void ResetManualWaveBuffer();

    EnemyManager* enemyManager_ = nullptr;
    const Camera* camera_ = nullptr;
    const Player* player_ = nullptr;
    EnemyLaserTelegraphController* laserController_ = nullptr;
    std::vector<EnemyWaveDefinition> waves_;
    std::unordered_map<std::string, size_t> waveIndexById_;
    std::vector<ActiveWave> activeWaves_;
    std::vector<WaveEnemyRuntime> waveEnemies_;
    std::vector<std::string> waveLog_;
    std::string lastWaveId_ = "(none)";
    std::string lastResult_ = "(none)";
    std::string lastCompletedWaveId_ = "(none)";
    std::string lastCompletedReason_ = "(none)";
    std::string pendingNextWaveId_;
    Vector3 lastSpawnPosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 lastScreenAnchorPosition_{ 0.0f, 0.0f, 0.0f };
    float lastWaveElapsedTime_ = 0.0f;
    float pendingNextWaveTimer_ = 0.0f;
    size_t lastWaveSpawnedCount_ = 0;
    size_t lastWaveEnemyCount_ = 0;
    std::array<char, 64> manualWaveIdBuffer_{};
    bool enabled_ = true;
    bool autoLoadMissingWave_ = true;
    bool autoStartWaveOnGameMode_ = true;
    bool autoProgressEnabled_ = true;
    bool pendingNextWaveActive_ = false;
    bool gameModeWasActive_ = false;
    bool screenAnchorEnabled_ = true;
    std::string autoStartWaveId_ = "wave_001";
    float spawnWidth_ = 20.0f;
    float spawnHeight_ = 12.0f;
    float approachSpeed_ = 3.0f;
    float boostApproachBonus_ = 6.0f;
    float approachStopDistance_ = 6.0f;
    float screenAnchorDropDuration_ = 0.6f;
    float screenAnchorSpawnScreenY_ = 0.95f;
    float screenAnchorEnemyScale_ = 1.45f;
    float screenAnchorRotationDuringDrop_ = 720.0f;
    float firstWarningDelay_ = 0.4f;
    float leftFirstWarningDelay_ = 0.4f;
    float rightFirstWarningDelay_ = 0.55f;
    float laserCooldown_ = 1.45f;
    float currentBoostPower_ = 0.0f;
    Vector3 lastLockedApproachDirection_{ 0.0f, 0.0f, -1.0f };
    size_t startedWaveCount_ = 0;
    size_t failedWaveCount_ = 0;
    size_t spawnedEnemyCount_ = 0;
    size_t lastGameModeAutoStartCount_ = 0;
    size_t despawnedOutOfCameraCount_ = 0;
    size_t screenAnchorEnemyCount_ = 0;
};

