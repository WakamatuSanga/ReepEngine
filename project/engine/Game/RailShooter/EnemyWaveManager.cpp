#include "EnemyWaveManager.h"
#include "Engine/Game/Camera/RailShooterCameraRig.h"
#include "Engine/Game/Enemy/Enemy.h"
#include "Engine/Game/Enemy/EnemyManager.h"
#include "Engine/Game/Enemy/EnemyLaserTelegraphController.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Game/RailShooter/EnemyWaveLoader.h"
#include "Engine/Game/UI/AimCorridorVisualController.h"
#include "Engine/Game/UI/WarningUIController.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Level/LevelEventRuntime.h"
#include "Engine/Level/LevelSceneData.h"
#include <unordered_set>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <utility>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr size_t kMaxWaveLogCount = 128;
    constexpr float kMinVectorLength = 0.00001f;

    Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 SubtractVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    }

    Vector3 ScaleVector3(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    float Length(const Vector3& value) {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    }

    float Dot(const Vector3& lhs, const Vector3& rhs) {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
    }

    Vector3 Normalize(const Vector3& value, const Vector3& fallback) {
        const float length = Length(value);
        if (length <= kMinVectorLength || !std::isfinite(length)) {
            return fallback;
        }
        return { value.x / length, value.y / length, value.z / length };
    }

    Vector3 GetCameraForward(const Camera& camera) {
        const Matrix4x4& matrix = camera.GetWorldMatrix();
        return Normalize({ matrix.m[2][0], matrix.m[2][1], matrix.m[2][2] }, { 0.0f, 0.0f, 1.0f });
    }

    Vector3 GetCameraRight(const Camera& camera) {
        const Matrix4x4& matrix = camera.GetWorldMatrix();
        return Normalize({ matrix.m[0][0], matrix.m[0][1], matrix.m[0][2] }, { 1.0f, 0.0f, 0.0f });
    }

    Vector3 GetCameraUp(const Camera& camera) {
        const Matrix4x4& matrix = camera.GetWorldMatrix();
        return Normalize({ matrix.m[1][0], matrix.m[1][1], matrix.m[1][2] }, { 0.0f, 1.0f, 0.0f });
    }

    std::string TrimCopy(const std::string& value) {
        const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
        const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
        if (begin >= end) {
            return {};
        }
        return std::string(begin, end);
    }

    std::vector<std::filesystem::path> BuildWavePathCandidates(const std::string& waveId) {
        std::filesystem::path requested = waveId;
        if (!requested.has_extension()) {
            requested += ".json";
        }

        return {
            std::filesystem::path("resources/waves") / requested,
            std::filesystem::path("project/resources/waves") / requested,
            std::filesystem::path("../resources/waves") / requested,
            std::filesystem::path("../../resources/waves") / requested,
            std::filesystem::path("../../../resources/waves") / requested,
            std::filesystem::path("../../project/resources/waves") / requested,
            std::filesystem::path("../../../project/resources/waves") / requested,
            requested,
        };
    }
}

EnemyWaveManager::EnemyWaveManager() = default;

EnemyWaveManager::~EnemyWaveManager() = default;

void EnemyWaveManager::Initialize(EnemyManager* enemyManager, const Camera* camera) {
    InitializeWaveFoundationState();
    enemyManager_ = enemyManager;
    camera_ = camera;
    ResetManualWaveBuffer();

    std::string result;
    if (LoadWaveById("wave_001", result)) {
        AddLog("Loaded default wave: " + result);
    } else {
        AddLog("Default wave not loaded: " + result);
    }
    if (LoadWaveById("wave_002", result)) {
        AddLog("Loaded optional wave: " + result);
    } else {
        AddLog("Optional wave_002 not loaded yet: " + result);
    }
    LoadStep9AWaves();
    initialized_ = true;
}

bool EnemyWaveManager::PrepareForPresentation(const LevelSceneData& sceneData) {
    std::vector<std::string> pending;
    if (autoStartWaveOnGameMode_) pending.push_back(autoStartWaveId_);
    const auto collect = [&](const auto& self, const LevelObject& object) -> void {
        for (const auto& action : object.eventFlag.objectActions) {
            std::string type = action.actionType;
            std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (type == "spawnwave") pending.push_back(!action.waveId.empty() ? action.waveId :
                !action.targetObjectName.empty() ? action.targetObjectName : action.targetObjectId);
        }
        for (const auto& child : object.children) self(self, child);
    };
    for (const auto& object : sceneData.objects) collect(collect, object);
    std::unordered_set<std::string> visited;
    while (!pending.empty()) {
        std::string id = TrimCopy(pending.back());
        pending.pop_back();
        if (id.empty() || !visited.insert(id).second) continue;
        std::string result;
        if (!LoadWaveById(id, result)) { lastResult_ = result; return false; }
        const auto found = waveIndexById_.find(id);
        if (found == waveIndexById_.end() || found->second >= waves_.size()) return false;
        const auto& wave = waves_[found->second];
        if (!wave.nextWaveId.empty()) pending.push_back(wave.nextWaveId);
        // enemyType/attackPattern select behavior, not asset paths in the current format.
        // All live entries use Enemy/EnemyBullet shared visuals and the common FX owner.
    }
    return true;
}

void EnemyWaveManager::Finalize() {
    finalizing_ = true;
    enemyManager_ = nullptr;
    camera_ = nullptr;
    player_ = nullptr;
    railRig_ = nullptr;
    warningUIController_ = nullptr;
    waves_.clear();
    waveIndexById_.clear();
    activeWaves_.clear();
    waveEnemies_.clear();
    waveLog_.clear();
    ClearPendingStartWarning();
    ClearPendingNextWave();
    gameModeWasActive_ = false;
    FinalizeWaveFoundationState();
}

void EnemyWaveManager::SetGameModeActive(bool isGameMode) {
    if (!isGameMode && gameModeWasActive_) {
        StopAllWaves();
    }
    if (isGameMode && !gameModeWasActive_ && autoStartWaveOnGameMode_) {
        std::string result;
        if (RestartAutoStartWave(result)) {
            ++lastGameModeAutoStartCount_;
        }
        lastResult_ = result;
    }
    gameModeWasActive_ = isGameMode;
}

void EnemyWaveManager::Update(float deltaTime) {
    if (!enabled_ || !enemyManager_) {
        return;
    }

    UpdatePendingStartWarning(deltaTime);

    for (ActiveWave& activeWave : activeWaves_) {
        if (activeWave.stopped || activeWave.waveIndex >= waves_.size()) {
            activeWave.stopped = true;
            continue;
        }

        const EnemyWaveDefinition& wave = waves_[activeWave.waveIndex];
        lastWaveEnemyCount_ = wave.enemies.size();
        activeWave.elapsedTime += deltaTime;
        lastWaveElapsedTime_ = activeWave.elapsedTime;
        lastWaveSpawnedCount_ = activeWave.spawnedCount;
        if (activeWave.elapsedTime < 0.0f) {
            continue;
        }

        for (size_t i = 0; i < wave.enemies.size(); ++i) {
            if (i >= activeWave.spawned.size() || activeWave.spawned[i]) {
                continue;
            }

            const EnemyWaveSpawnEntry& spawn = wave.enemies[i];
            if (activeWave.elapsedTime < spawn.spawnTime) {
                continue;
            }

            const bool followsRail = spawn.movePattern == "RailFormation" && railRig_ && railRig_->HasGameplayRail();
            const float railDistance = followsRail ? railRig_->GetSceneryDistance() : 0.0f;
            Vector3 spawnForward;
            const Vector3 spawnPosition = followsRail
                ? ComputeRailPosition(railDistance + spawn.depth, spawn.screenX, spawn.screenY, &spawnForward)
                : ComputeSpawnPosition(spawn);
            if (!followsRail) spawnForward = ComputeSpawnForward(spawnPosition);
            activeWave.spawned[i] = true;
            ++activeWave.spawnedCount;
            lastWaveSpawnedCount_ = activeWave.spawnedCount;

            if (Enemy* spawnedEnemy = enemyManager_->SpawnEnemy(spawn.enemyType, spawnPosition, spawnForward,
                spawn.hitPoints, followsRail)) {
                if (IsScreenAnchorMovePattern(spawn.movePattern)) {
                    ConfigureScreenAnchorSpawn(spawnedEnemy, spawn, spawnPosition);
                }
                RegisterWaveEnemy(spawnedEnemy, spawn, activeWave.waveIndex, railDistance, spawnPosition);
                ++spawnedEnemyCount_;
                lastWaveSpawnedCount_ = activeWave.spawnedCount;
                lastSpawnPosition_ = spawnPosition;
                AddLog(
                    "Spawned enemy from wave=" + wave.waveId +
                    " type=" + spawn.enemyType +
                    " move=" + spawn.movePattern +
                    " attack=" + spawn.attackPattern +
                    " pos=(" + std::to_string(spawnPosition.x) + ", " +
                    std::to_string(spawnPosition.y) + ", " +
                    std::to_string(spawnPosition.z) + ")");
            } else {
                ++activeWave.endedCount;
                ++activeWave.escapedCount;
                AddLog("Enemy spawn failed in wave=" + wave.waveId + " type=" + spawn.enemyType);
            }
        }

        if (activeWave.spawnedCount >= wave.enemies.size() && !activeWave.allSpawnsScheduled) {
            activeWave.allSpawnsScheduled = true;
            AddLog("Wave scheduled all enemies: " + wave.waveId);
        }
    }

    UpdateWaveEnemyMovement(deltaTime);
    RefreshCurrentWaveDiagnostics();
    UpdateWaveProgression(deltaTime);

    activeWaves_.erase(
        std::remove_if(activeWaves_.begin(), activeWaves_.end(), [](const ActiveWave& wave) { return wave.stopped; }),
        activeWaves_.end());
    RefreshCurrentWaveDiagnostics();
}

bool EnemyWaveManager::HandleSpawnWaveAction(const FiredEventAction& action, std::string& resultMessage) {
    if (!enabled_) {
        resultMessage = "EnemyWaveManager is disabled.";
        ++failedWaveCount_;
        return false;
    }

    const std::string waveId = ResolveWaveIdFromAction(action);
    if (waveId.empty()) {
        resultMessage = "SpawnWave missing waveId or targetObjectName.";
        ++failedWaveCount_;
        AddLog("SpawnWave failed: " + resultMessage);
        return false;
    }

    return PlayWave(waveId, resultMessage);
}

bool EnemyWaveManager::PlayWave(const std::string& waveId, std::string& resultMessage) {
    const std::string trimmedWaveId = TrimCopy(waveId);
    lastWaveId_ = trimmedWaveId.empty() ? "(none)" : trimmedWaveId;

    if (!enemyManager_) {
        resultMessage = "EnemyManager is missing.";
        lastResult_ = resultMessage;
        ++failedWaveCount_;
        AddLog("SpawnWave failed: " + resultMessage);
        return false;
    }
    if (trimmedWaveId.empty()) {
        resultMessage = "waveId is empty.";
        lastResult_ = resultMessage;
        ++failedWaveCount_;
        AddLog("SpawnWave failed: " + resultMessage);
        return false;
    }

    if (pendingNextWaveActive_ && pendingNextWaveId_ == trimmedWaveId) {
        ClearPendingNextWave();
    }

    if (!IsWaveLoaded(trimmedWaveId)) {
        if (!autoLoadMissingWave_ || !LoadWaveById(trimmedWaveId, resultMessage)) {
            lastResult_ = resultMessage;
            ++failedWaveCount_;
            AddLog("SpawnWave failed: " + resultMessage);
            return false;
        }
    }

    const auto it = waveIndexById_.find(trimmedWaveId);
    if (it == waveIndexById_.end() || it->second >= waves_.size()) {
        resultMessage = "Loaded wave index is invalid: " + trimmedWaveId;
        lastResult_ = resultMessage;
        ++failedWaveCount_;
        AddLog("SpawnWave failed: " + resultMessage);
        return false;
    }

    const EnemyWaveDefinition& wave = waves_[it->second];
    if (wave.showWarningOnStart && wave.waitForWarningBeforeSpawn) {
        return QueueWaveStartWarning(it->second, resultMessage);
    }

    return StartWaveNow(it->second, resultMessage, true);
}

void EnemyWaveManager::StopAllWaves() {
    activeWaves_.clear();
    ClearPendingStartWarning();
    ClearPendingNextWave();
    ClearTrackedWaveEnemies();
    if (warningUIController_) {
        warningUIController_->HideWarning();
    }
    completedStageWaves_.clear();
    ResetWaveFoundationForRestart();
    AddLog("Stopped all active waves and tracked wave enemies.");
}

bool EnemyWaveManager::LoadWaveById(const std::string& waveId, std::string& resultMessage) {
    if (IsWaveLoaded(waveId)) {
        resultMessage = "Wave already loaded: " + waveId;
        return true;
    }

    for (const std::filesystem::path& candidate : BuildWavePathCandidates(waveId)) {
        if (std::filesystem::exists(candidate)) {
            return LoadWaveFile(candidate.string(), resultMessage);
        }
    }

    resultMessage = "Wave file not found for waveId=" + waveId;
    return false;
}

bool EnemyWaveManager::LoadWaveFile(const std::string& filePath, std::string& resultMessage) {
    EnemyWaveDefinition wave;
    if (!EnemyWaveLoader::LoadFromFile(filePath, wave, resultMessage)) {
        return false;
    }

    const auto existing = waveIndexById_.find(wave.waveId);
    if (existing != waveIndexById_.end() && existing->second < waves_.size()) {
        ++waveDuplicateIdCount_;
        waves_[existing->second] = std::move(wave);
    } else {
        waveIndexById_[wave.waveId] = waves_.size();
        waves_.push_back(std::move(wave));
    }
    return true;
}

bool EnemyWaveManager::IsWaveLoaded(const std::string& waveId) const {
    return waveIndexById_.find(waveId) != waveIndexById_.end();
}

std::string EnemyWaveManager::ResolveWaveIdFromAction(const FiredEventAction& action) const {
    if (!action.waveId.empty()) {
        return action.waveId;
    }
    if (!action.targetObjectName.empty()) {
        return action.targetObjectName;
    }
    if (!action.targetObjectId.empty()) {
        return action.targetObjectId;
    }
    return {};
}

Vector3 EnemyWaveManager::ComputeSpawnPosition(const EnemyWaveSpawnEntry& spawn) const {
    if (IsScreenAnchorMovePattern(spawn.movePattern)) return ComputeAnchorPosition(spawn.screenX, spawn.screenY, spawn.depth);
    return ComputeScreenPosition(spawn.screenX, spawn.screenY, spawn.depth);
}

Vector3 EnemyWaveManager::ComputeAnchorPosition(float screenX, float screenY, float depth) const {
    if (!railRig_ || !railRig_->UsesGameplayComposition()) return ComputeScreenPosition(screenX, screenY, depth);
    const auto& frame = railRig_->GetGameplayReferenceFrame();
    const Vector3 right{frame.m[0][0], frame.m[0][1], frame.m[0][2]};
    const Vector3 up{frame.m[1][0], frame.m[1][1], frame.m[1][2]};
    const Vector3 forward{frame.m[2][0], frame.m[2][1], frame.m[2][2]};
    const Vector3 origin{frame.m[3][0], frame.m[3][1], frame.m[3][2]};
    return AddVector3(AddVector3(origin, ScaleVector3(forward, depth)),
        AddVector3(ScaleVector3(right, screenX * spawnWidth_), ScaleVector3(up, screenY * spawnHeight_)));
}

Vector3 EnemyWaveManager::ComputeRailPosition(float distance, float screenX, float screenY, Vector3* forward) const {
    const auto frame = railRig_->SampleSceneryFrame(distance);
    if (forward) *forward = {frame.m[2][0], frame.m[2][1], frame.m[2][2]};
    const Vector3 right{frame.m[0][0], frame.m[0][1], frame.m[0][2]};
    const Vector3 up{frame.m[1][0], frame.m[1][1], frame.m[1][2]};
    const Vector3 origin{frame.m[3][0], frame.m[3][1], frame.m[3][2]};
    return AddVector3(origin, AddVector3(ScaleVector3(right, screenX * spawnWidth_), ScaleVector3(up, screenY * spawnHeight_)));
}

Vector3 EnemyWaveManager::ComputeScreenPosition(float screenX, float screenY, float depth) const {
    if (!camera_) {
        return { screenX * spawnWidth_, screenY * spawnHeight_, depth };
    }

    const Vector3 cameraPosition = camera_->GetTranslate();
    const Vector3 cameraForward = GetCameraForward(*camera_);
    const Vector3 cameraRight = GetCameraRight(*camera_);
    const Vector3 cameraUp = GetCameraUp(*camera_);

    return AddVector3(
        AddVector3(
            AddVector3(cameraPosition, ScaleVector3(cameraForward, depth)),
            ScaleVector3(cameraRight, screenX * spawnWidth_)),
        ScaleVector3(cameraUp, screenY * spawnHeight_));
}

Vector3 EnemyWaveManager::ComputeSpawnForward(const Vector3& spawnPosition) const {
    if (!camera_) {
        return { 0.0f, 0.0f, -1.0f };
    }
    return Normalize(SubtractVector3(camera_->GetTranslate(), spawnPosition), ScaleVector3(GetCameraForward(*camera_), -1.0f));
}

Vector3 EnemyWaveManager::ResolveApproachTarget() const {
    if (player_) {
        return player_->GetWorldPosition();
    }
    if (camera_) {
        return camera_->GetTranslate();
    }
    return { 0.0f, 0.0f, 0.0f };
}

void EnemyWaveManager::RegisterWaveEnemy(Enemy* enemy, const EnemyWaveSpawnEntry& spawn, size_t waveIndex,
    float railDistance, const Vector3& spawnPosition) {
    if (!enemy) {
        return;
    }
    WaveEnemyRuntime runtime;
    runtime.enemy = enemy;
    runtime.waveIndex = waveIndex;
    runtime.movePattern = spawn.movePattern;
    runtime.attackPattern = spawn.attackPattern;
    runtime.screenX = spawn.screenX;
    runtime.screenY = spawn.screenY;
    runtime.depth = spawn.depth;
    runtime.spawnScreenY = spawn.spawnScreenY;
    runtime.dropDuration = spawn.dropDuration;
    runtime.enemyScale = spawn.enemyScale;
    runtime.rotationDuringDrop = spawn.rotationDuringDrop;
    runtime.railFormation = spawn.movePattern == "RailFormation";
    runtime.formationForwardSpeed = spawn.formationForwardSpeed;
    runtime.formationApproachDepth = spawn.formationApproachDepth;
    runtime.railDistance = railDistance;
    runtime.railPosition = spawnPosition;
    runtime.anchorHold = spawn.anchorHold;
    runtime.anchorExitDuration = spawn.anchorExitDuration;
    waveEnemies_.push_back(std::move(runtime));
}

void EnemyWaveManager::UpdateWaveEnemyMovement(float deltaTime) {
    if (waveEnemies_.empty() || !enemyManager_) {
        return;
    }

    const std::vector<Enemy*> aliveEnemies = enemyManager_->GetActiveEnemies();
    auto isAlive = [&aliveEnemies](Enemy* enemy) {
        return enemy && std::find(aliveEnemies.begin(), aliveEnemies.end(), enemy) != aliveEnemies.end();
        };
    for (WaveEnemyRuntime& runtime : waveEnemies_) {
        if (!isAlive(runtime.enemy)) {
            if (laserController_) {
                laserController_->ClearOwner(runtime.enemy);
            }
            const bool died = runtime.enemy && enemyManager_->OwnsEnemy(runtime.enemy) && runtime.enemy->IsDead();
            MarkWaveEnemyEnded(runtime, died ? WaveEnemyEndReason::Dead : WaveEnemyEndReason::Escaped);
        }
    }
    waveEnemies_.erase(
        std::remove_if(waveEnemies_.begin(), waveEnemies_.end(), [](const WaveEnemyRuntime& runtime) { return runtime.ended; }),
        waveEnemies_.end());

    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    const Vector3 approachTarget = ResolveApproachTarget();
    const Vector3 cameraForward = camera_ ? GetCameraForward(*camera_) : Vector3{ 0.0f, 0.0f, 1.0f };
    const Vector3 cameraRight = camera_ ? GetCameraRight(*camera_) : Vector3{ 1.0f, 0.0f, 0.0f };
    const Vector3 cameraUp = camera_ ? GetCameraUp(*camera_) : Vector3{ 0.0f, 1.0f, 0.0f };
    const Vector3 cameraPosition = camera_ ? camera_->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f };
    screenAnchorEnemyCount_ = 0;

    for (WaveEnemyRuntime& runtime : waveEnemies_) {
        Enemy* enemy = runtime.enemy;
        if (!enemy || runtime.ended) {
            continue;
        }

        if (screenAnchorEnabled_ && IsScreenAnchorMovePattern(runtime.movePattern)) {
            ++screenAnchorEnemyCount_;
            UpdateScreenAnchorEnemy(runtime, safeDeltaTime);
            continue;
        }

        if (runtime.railFormation) {
            UpdateRailFormation(runtime, safeDeltaTime);
            continue;
        }

        if (!enemy->CanAttack()) {
            continue;
        }

        const Vector3 enemyPosition = enemy->GetPosition();
        if (camera_) {
            const Vector3 fromCamera = SubtractVector3(enemyPosition, cameraPosition);
            const float depth = Dot(fromCamera, cameraForward);
            const float side = Dot(fromCamera, cameraRight);
            const float height = Dot(fromCamera, cameraUp);
            if (depth < -5.0f || depth > 180.0f || std::fabs(side) > 90.0f || std::fabs(height) > 70.0f || Length(fromCamera) > 220.0f) {
                EscapeWaveEnemy(runtime);
                continue;
            }
        }

        const bool straightApproach = IsStraightApproachMovePattern(runtime.movePattern);
        const bool homingApproach = IsHomingApproachMovePattern(runtime.movePattern);
        if (!straightApproach && !homingApproach) {
            continue;
        }

        const Vector3 fallbackForward = camera_ ? ScaleVector3(cameraForward, -1.0f) : enemy->GetForward();
        Vector3 direction = fallbackForward;
        if (straightApproach) {
            if (!runtime.hasLockedApproachDirection) {
                runtime.lockedApproachDirection = Normalize(SubtractVector3(approachTarget, enemyPosition), fallbackForward);
                runtime.hasLockedApproachDirection = true;
                lastLockedApproachDirection_ = runtime.lockedApproachDirection;
                enemy->SetForward(runtime.lockedApproachDirection);
            }
            direction = runtime.lockedApproachDirection;
        } else {
            const Vector3 toTarget = SubtractVector3(approachTarget, enemyPosition);
            const float distance = Length(toTarget);
            if (distance <= (std::max)(0.0f, approachStopDistance_) + kMinVectorLength) {
                continue;
            }
            direction = Normalize(toTarget, fallbackForward);
            enemy->SetForward(direction);
        }

        const float speed = (std::max)(0.0f, approachSpeed_);
        enemy->SetPosition(AddVector3(enemyPosition, ScaleVector3(direction, speed * safeDeltaTime)));
    }

    for (WaveEnemyRuntime& runtime : waveEnemies_) {
        if (!runtime.ended && (!runtime.enemy || runtime.enemy->IsDead())) {
            MarkWaveEnemyEnded(runtime, WaveEnemyEndReason::Dead);
        }
    }
    waveEnemies_.erase(
        std::remove_if(waveEnemies_.begin(), waveEnemies_.end(), [](const WaveEnemyRuntime& runtime) { return runtime.ended || !runtime.enemy || runtime.enemy->IsDead(); }),
        waveEnemies_.end());
}

Vector3 EnemyWaveManager::ComputeFormationPosition(const WaveEnemyRuntime& runtime, float railDistance,
    float age, Vector3* forward) const {
    const float exitTime = std::clamp((age - runtime.anchorHold) / runtime.anchorExitDuration, 0.0f, 1.0f);
    const float exitBlend = exitTime * exitTime;
    const float exitSide = runtime.screenX < 0.0f ? -1.0f : 1.0f;
    float laneScale = 1.0f;
    float depth = std::lerp(runtime.depth, runtime.depth * 0.5f, exitBlend) + runtime.formationForwardSpeed * age;
    if (runtime.formationApproachDepth > 0.0f && runtime.formationApproachDepth < runtime.depth && runtime.anchorHold > 0.0f) {
        // Reach the approach depth before peeling away. Match the initial drift and ease to a stop.
        const float approachDuration = runtime.anchorHold * 0.85f;
        const float t = std::clamp(age / approachDuration, 0.0f, 1.0f);
        const float smooth = t * t * (3.0f - 2.0f * t);
        const float initialTravel = std::clamp(-runtime.formationForwardSpeed * approachDuration,
            0.0f, runtime.depth - runtime.formationApproachDepth);
        depth = std::lerp(runtime.depth, runtime.formationApproachDepth, smooth) - initialTravel * t * (1.0f - t) * (1.0f - t);
        // Tighten the lanes in proportion to depth so the drones stay in view as they close.
        laneScale = depth / runtime.depth;
    }
    const float x = std::lerp(runtime.screenX * laneScale, exitSide * 1.45f, exitBlend);
    const float y = runtime.screenY * laneScale + 0.35f * exitBlend;
    if (railRig_ && railRig_->HasGameplayRail()) {
        return ComputeRailPosition(railDistance + depth, x, y, forward);
    }
    if (forward) *forward = camera_ ? GetCameraForward(*camera_) : Vector3{0.0f, 0.0f, 1.0f};
    return ComputeAnchorPosition(x, y, depth);
}

void EnemyWaveManager::UpdateRailFormation(WaveEnemyRuntime& runtime, float deltaTime) {
    Enemy& enemy = *runtime.enemy;
    const bool flying = enemy.CanAttack();
    if (flying) runtime.anchorAge += deltaTime;
    if (runtime.anchorAge >= runtime.anchorHold + runtime.anchorExitDuration) {
        EscapeWaveEnemy(runtime);
        return;
    }
    runtime.railDistance = railRig_ && railRig_->HasGameplayRail() ? railRig_->GetSceneryDistance() : 0.0f;
    Vector3 forward{};
    const Vector3 position = ComputeFormationPosition(runtime, runtime.railDistance, runtime.anchorAge, &forward);
    if (flying) {
        enemy.SetPosition(position);
        enemy.SetForward(forward);
    } else {
        enemy.TranslateSpawnPath(SubtractVector3(position, runtime.railPosition));
    }
    runtime.railPosition = position;
}

bool EnemyWaveManager::TryGetFormationAimPoint(const AimReticleScreenRect& reticle, const Vector3& muzzle,
    float bulletSpeed, float bulletLifetime, Vector3& aimPoint) const {
    if (!reticle.valid || reticle.halfSizeUV.x <= 0.0f || reticle.halfSizeUV.y <= 0.0f
        || !camera_ || !enemyManager_ || !railRig_ || !railRig_->HasGameplayRail()
        || !std::isfinite(bulletSpeed) || bulletSpeed <= 0.0f
        || !std::isfinite(bulletLifetime) || bulletLifetime <= 0.0f) return false;
    const auto& projection = camera_->GetViewProjectionMatrix();
    const float railSpeed = railRig_->GetProjectileRailFrame().running ? railRig_->GetEffectiveRailSpeed() : 0.0f;
    float bestScore = 1.0f;
    bool found = false;
    for (const auto& runtime : waveEnemies_) {
        const Enemy* enemy = runtime.enemy;
        if (!runtime.railFormation || runtime.ended || !enemy || !enemyManager_->OwnsEnemy(enemy) || !enemy->CanAttack()) continue;
        const Vector3 position = enemy->GetPosition();
        const auto clip = [&projection, &position](int axis) {
            return position.x * projection.m[0][axis] + position.y * projection.m[1][axis]
                + position.z * projection.m[2][axis] + projection.m[3][axis];
        };
        const float w = clip(3);
        if (w <= kMinVectorLength) continue;
        const Vector2 uv{0.5f + 0.5f * clip(0) / w, 0.5f - 0.5f * clip(1) / w};
        const float score = (std::max)(std::fabs(uv.x - reticle.centerUV.x) / reticle.halfSizeUV.x,
            std::fabs(uv.y - reticle.centerUV.y) / reticle.halfSizeUV.y);
        if (!std::isfinite(score) || score >= bestScore) continue;

        // Solve the flight time once against the same path the formation uses.
        // Subsequent camera or rail changes cannot steer an already fired round.
        const auto predict = [&](float time) {
            return ComputeFormationPosition(runtime, runtime.railDistance + railSpeed * time, runtime.anchorAge + time);
        };
        const float horizon = (std::min)(bulletLifetime, runtime.anchorHold + runtime.anchorExitDuration - runtime.anchorAge);
        if (horizon <= 0.0f) continue;
        const auto canReach = [&](float time) {
            return Length(SubtractVector3(predict(time), muzzle)) <= bulletSpeed * time;
        };
        // A drone can cross the shot's reach before leaving it again during the lateral exit.
        constexpr int samples = 12;
        float early = 0.0f;
        float late = 0.0f;
        for (int sample = 1; sample <= samples; ++sample) {
            late = horizon * (static_cast<float>(sample) / samples);
            if (canReach(late)) break;
            early = late;
        }
        if (early == late) continue;
        for (int iteration = 0; iteration < 12; ++iteration) {
            const float time = 0.5f * (early + late);
            if (!canReach(time)) early = time;
            else late = time;
        }
        aimPoint = predict(late);
        bestScore = score;
        found = true;
    }
    return found;
}

void EnemyWaveManager::EscapeWaveEnemy(WaveEnemyRuntime& runtime) {
    MarkWaveEnemyEnded(runtime, WaveEnemyEndReason::Escaped);
    if (laserController_) laserController_->ClearOwner(runtime.enemy);
    runtime.enemy->Kill();
    runtime.enemy = nullptr;
    ++despawnedOutOfCameraCount_;
}

void EnemyWaveManager::ClearTrackedWaveEnemies() {
    if (!enemyManager_) {
        waveEnemies_.clear();
        return;
    }

    const std::vector<Enemy*> aliveEnemies = enemyManager_->GetActiveEnemies();
    for (const WaveEnemyRuntime& runtime : waveEnemies_) {
        if (laserController_) {
            laserController_->ClearOwner(runtime.enemy);
        }
        if (runtime.enemy && std::find(aliveEnemies.begin(), aliveEnemies.end(), runtime.enemy) != aliveEnemies.end()) {
            runtime.enemy->Kill();
        }
    }
    waveEnemies_.clear();
}

bool EnemyWaveManager::IsHomingApproachMovePattern(const std::string& movePattern) const {
    std::string normalized = TrimCopy(movePattern);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
        });
    return normalized == "approachplayer" || normalized == "approach_player" || normalized == "approach-player";
}

bool EnemyWaveManager::IsStraightApproachMovePattern(const std::string& movePattern) const {
    std::string normalized = TrimCopy(movePattern);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
        });
    return normalized == "straightapproach" || normalized == "straight_approach" ||
        normalized == "forwardapproach" || normalized == "lockedforwardapproach";
}

bool EnemyWaveManager::RestartAutoStartWave(std::string& resultMessage) {
    if (!enabled_) {
        resultMessage = "GameMode auto wave skipped: EnemyWaveManager is disabled.";
        AddLog(resultMessage);
        return false;
    }

    StopAllWaves();
    const bool started = PlayWave(autoStartWaveId_, resultMessage);
    AddLog(std::string("GameMode auto start wave: ") + resultMessage);
    return started;
}

void EnemyWaveManager::AddLog(const std::string& message) {
    waveLog_.push_back(message);
    if (waveLog_.size() > kMaxWaveLogCount) {
        waveLog_.erase(waveLog_.begin(), waveLog_.begin() + (waveLog_.size() - kMaxWaveLogCount));
    }
}

void EnemyWaveManager::ResetManualWaveBuffer() {
    manualWaveIdBuffer_.fill('\0');
    constexpr const char* kDefaultWaveId = "wave_001";
    for (size_t i = 0; kDefaultWaveId[i] != '\0' && i + 1 < manualWaveIdBuffer_.size(); ++i) {
        manualWaveIdBuffer_[i] = kDefaultWaveId[i];
    }
}
