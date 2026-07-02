#include "EnemyWaveManager.h"
#include "Engine/Game/Enemy/Enemy.h"
#include "Engine/Game/Enemy/EnemyManager.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Game/RailShooter/EnemyWaveLoader.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Level/LevelEventRuntime.h"
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
    enemyManager_ = enemyManager;
    camera_ = camera;
    ResetManualWaveBuffer();

    std::string result;
    if (LoadWaveById("wave_001", result)) {
        AddLog("Loaded default wave: " + result);
    } else {
        AddLog("Default wave not loaded: " + result);
    }
}

void EnemyWaveManager::Finalize() {
    enemyManager_ = nullptr;
    camera_ = nullptr;
    player_ = nullptr;
    waves_.clear();
    waveIndexById_.clear();
    activeWaves_.clear();
    waveEnemies_.clear();
    waveLog_.clear();
    gameModeWasActive_ = false;
}

void EnemyWaveManager::SetCurrentBoostPower(float boostPower) {
    currentBoostPower_ = std::isfinite(boostPower) ? std::clamp(boostPower, 0.0f, 1.0f) : 0.0f;
}

void EnemyWaveManager::SetGameModeActive(bool isGameMode) {
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

            const Vector3 spawnPosition = ComputeSpawnPosition(spawn);
            const Vector3 spawnForward = ComputeSpawnForward(spawnPosition);
            activeWave.spawned[i] = true;
            ++activeWave.spawnedCount;
            lastWaveSpawnedCount_ = activeWave.spawnedCount;

            if (Enemy* spawnedEnemy = enemyManager_->SpawnEnemyAt(spawnPosition, spawn.enemyType, spawnForward)) {
                RegisterWaveEnemy(spawnedEnemy, spawn);
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
                AddLog("Enemy spawn failed in wave=" + wave.waveId + " type=" + spawn.enemyType);
            }
        }

        if (activeWave.spawnedCount >= wave.enemies.size()) {
            activeWave.stopped = true;
            AddLog("Wave scheduled all enemies: " + wave.waveId);
        }
    }

    activeWaves_.erase(
        std::remove_if(activeWaves_.begin(), activeWaves_.end(), [](const ActiveWave& wave) { return wave.stopped; }),
        activeWaves_.end());

    UpdateWaveEnemyMovement(deltaTime);
}

void EnemyWaveManager::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(430.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Enemy Wave Debug")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Enable SpawnWave Action", &enabled_);
    ImGui::Checkbox("Auto Load Missing Wave", &autoLoadMissingWave_);
    ImGui::Checkbox("GameMode開始時にWave 1を再生 (Auto Start Wave On GameMode)", &autoStartWaveOnGameMode_);
    ImGui::TextWrapped("Auto Start Wave ID: %s", autoStartWaveId_.c_str());
    ImGui::DragFloat("Spawn Width", &spawnWidth_, 0.1f, 1.0f, 100.0f);
    ImGui::DragFloat("Spawn Height", &spawnHeight_, 0.1f, 1.0f, 100.0f);
    ImGui::DragFloat("接近速度 (Approach Speed)", &approachSpeed_, 0.1f, 0.0f, 60.0f, "%.1f");
    ImGui::DragFloat("Boost接近速度加算 (Boost Approach Bonus)", &boostApproachBonus_, 0.1f, 0.0f, 30.0f, "%.1f");
    ImGui::DragFloat("接近停止距離 (Approach Stop Distance)", &approachStopDistance_, 0.1f, 0.0f, 40.0f, "%.1f");
    ImGui::Text("Current Boost Power: %.2f", currentBoostPower_);
    ImGui::Text("Current Approach Speed: %.2f", approachSpeed_ + currentBoostPower_ * boostApproachBonus_);
    ImGui::Text("Loaded Wave Count: %zu", waves_.size());
    ImGui::Text("Active Wave Count: %zu", activeWaves_.size());
    ImGui::Text("Started Wave Count: %zu", startedWaveCount_);
    ImGui::Text("Failed Wave Count: %zu", failedWaveCount_);
    ImGui::Text("Spawned Enemy Count: %zu", spawnedEnemyCount_);
    ImGui::Text("Tracked Wave Enemy Count: %zu", waveEnemies_.size());
    ImGui::Text("Despawned Out Of Camera Count: %zu", despawnedOutOfCameraCount_);
    ImGui::Text("Last Locked Direction: %.2f, %.2f, %.2f", lastLockedApproachDirection_.x, lastLockedApproachDirection_.y, lastLockedApproachDirection_.z);
    ImGui::Text("GameMode Auto Start Count: %zu", lastGameModeAutoStartCount_);
    ImGui::Text("Last Wave Elapsed Time: %.2f", lastWaveElapsedTime_);
    ImGui::Text("Last Wave Spawned Count: %zu / %zu", lastWaveSpawnedCount_, lastWaveEnemyCount_);
    ImGui::TextWrapped("Current / Last Wave ID: %s", lastWaveId_.c_str());
    ImGui::TextWrapped("Last Result: %s", lastResult_.c_str());
    ImGui::Text("Last Spawn Position: %.2f, %.2f, %.2f", lastSpawnPosition_.x, lastSpawnPosition_.y, lastSpawnPosition_.z);

    if (!activeWaves_.empty() && activeWaves_.front().waveIndex < waves_.size()) {
        const ActiveWave& waveState = activeWaves_.front();
        const EnemyWaveDefinition& wave = waves_[waveState.waveIndex];
        ImGui::SeparatorText("Current Active Wave");
        ImGui::TextWrapped("Wave: %s / %s", wave.waveId.c_str(), wave.name.c_str());
        ImGui::Text("Elapsed Time: %.2f", waveState.elapsedTime);
        ImGui::Text("Spawned: %zu / %zu", waveState.spawnedCount, wave.enemies.size());
    }

    ImGui::SeparatorText("Manual Play Wave");
    ImGui::InputText("Wave ID", manualWaveIdBuffer_.data(), manualWaveIdBuffer_.size());
    if (ImGui::Button("Manual Play Wave")) {
        std::string result;
        PlayWave(TrimCopy(manualWaveIdBuffer_.data()), result);
        lastResult_ = result;
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop Wave")) {
        StopAllWaves();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Log")) {
        waveLog_.clear();
    }

    if (ImGui::TreeNode("Loaded Waves")) {
        if (waves_.empty()) {
            ImGui::TextDisabled("No loaded waves.");
        } else {
            for (const EnemyWaveDefinition& wave : waves_) {
                ImGui::TextWrapped("%s  name=%s  enemies=%zu", wave.waveId.c_str(), wave.name.c_str(), wave.enemies.size());
            }
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Wave Log")) {
        if (waveLog_.empty()) {
            ImGui::TextDisabled("No wave log yet.");
        } else {
            for (const std::string& line : waveLog_) {
                ImGui::TextWrapped("%s", line.c_str());
            }
        }
        ImGui::TreePop();
    }

    ImGui::End();
#endif
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
    lastWaveElapsedTime_ = 0.0f;
    lastWaveSpawnedCount_ = 0;
    lastWaveEnemyCount_ = wave.enemies.size();
    ActiveWave activeWave;
    activeWave.waveIndex = it->second;
    activeWave.elapsedTime = -std::max(0.0f, wave.delay);
    activeWave.spawned.assign(wave.enemies.size(), false);
    activeWaves_.push_back(std::move(activeWave));

    ++startedWaveCount_;
    resultMessage = "Started wave " + wave.waveId + " enemies=" + std::to_string(wave.enemies.size());
    lastResult_ = resultMessage;
    AddLog(resultMessage);
    return true;
}

void EnemyWaveManager::StopAllWaves() {
    activeWaves_.clear();
    ClearTrackedWaveEnemies();
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
    if (!camera_) {
        return { spawn.screenX * spawnWidth_, spawn.screenY * spawnHeight_, spawn.depth };
    }

    const Vector3 cameraPosition = camera_->GetTranslate();
    const Vector3 cameraForward = GetCameraForward(*camera_);
    const Vector3 cameraRight = GetCameraRight(*camera_);
    const Vector3 cameraUp = GetCameraUp(*camera_);

    return AddVector3(
        AddVector3(
            AddVector3(cameraPosition, ScaleVector3(cameraForward, spawn.depth)),
            ScaleVector3(cameraRight, spawn.screenX * spawnWidth_)),
        ScaleVector3(cameraUp, spawn.screenY * spawnHeight_));
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

void EnemyWaveManager::RegisterWaveEnemy(Enemy* enemy, const EnemyWaveSpawnEntry& spawn) {
    if (!enemy) {
        return;
    }
    waveEnemies_.push_back({ enemy, spawn.movePattern });
}

void EnemyWaveManager::UpdateWaveEnemyMovement(float deltaTime) {
    if (waveEnemies_.empty() || !enemyManager_) {
        return;
    }

    const std::vector<Enemy*> aliveEnemies = enemyManager_->GetActiveEnemies();
    auto isAlive = [&aliveEnemies](Enemy* enemy) {
        return enemy && std::find(aliveEnemies.begin(), aliveEnemies.end(), enemy) != aliveEnemies.end();
        };
    waveEnemies_.erase(
        std::remove_if(waveEnemies_.begin(), waveEnemies_.end(), [&isAlive](const WaveEnemyRuntime& runtime) { return !isAlive(runtime.enemy); }),
        waveEnemies_.end());

    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    const Vector3 approachTarget = ResolveApproachTarget();
    const Vector3 cameraForward = camera_ ? GetCameraForward(*camera_) : Vector3{ 0.0f, 0.0f, 1.0f };
    const Vector3 cameraRight = camera_ ? GetCameraRight(*camera_) : Vector3{ 1.0f, 0.0f, 0.0f };
    const Vector3 cameraUp = camera_ ? GetCameraUp(*camera_) : Vector3{ 0.0f, 1.0f, 0.0f };
    const Vector3 cameraPosition = camera_ ? camera_->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f };
    size_t despawnedThisFrame = 0;

    for (WaveEnemyRuntime& runtime : waveEnemies_) {
        Enemy* enemy = runtime.enemy;
        if (!enemy || !enemy->CanAttack()) {
            continue;
        }

        const Vector3 enemyPosition = enemy->GetPosition();
        if (camera_) {
            const Vector3 fromCamera = SubtractVector3(enemyPosition, cameraPosition);
            const float depth = Dot(fromCamera, cameraForward);
            const float side = Dot(fromCamera, cameraRight);
            const float height = Dot(fromCamera, cameraUp);
            if (depth < -5.0f || depth > 180.0f || std::fabs(side) > 90.0f || std::fabs(height) > 70.0f || Length(fromCamera) > 220.0f) {
                enemy->Kill();
                runtime.enemy = nullptr;
                ++despawnedThisFrame;
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

        const float speed = (std::max)(0.0f, approachSpeed_ + currentBoostPower_ * boostApproachBonus_);
        enemy->SetPosition(AddVector3(enemyPosition, ScaleVector3(direction, speed * safeDeltaTime)));
    }

    despawnedOutOfCameraCount_ += despawnedThisFrame;
    waveEnemies_.erase(
        std::remove_if(waveEnemies_.begin(), waveEnemies_.end(), [](const WaveEnemyRuntime& runtime) { return !runtime.enemy || runtime.enemy->IsDead(); }),
        waveEnemies_.end());
}
void EnemyWaveManager::ClearTrackedWaveEnemies() {
    if (!enemyManager_) {
        waveEnemies_.clear();
        return;
    }

    const std::vector<Enemy*> aliveEnemies = enemyManager_->GetActiveEnemies();
    for (const WaveEnemyRuntime& runtime : waveEnemies_) {
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
