#include "StartupEnemySpawnController.h"

#include "Engine/Game/Enemy/EnemyManager.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Level/LevelSceneRuntime.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>

namespace {
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

    Vector3 GetCameraUp(const Camera& camera) {
        const Matrix4x4& matrix = camera.GetWorldMatrix();
        return Normalize({ matrix.m[1][0], matrix.m[1][1], matrix.m[1][2] }, { 0.0f, 1.0f, 0.0f });
    }

    Vector3 GetCameraRight(const Camera& camera) {
        const Matrix4x4& matrix = camera.GetWorldMatrix();
        return Normalize({ matrix.m[0][0], matrix.m[0][1], matrix.m[0][2] }, { 1.0f, 0.0f, 0.0f });
    }

#ifdef USE_IMGUI
    void DrawStringInput(const char* label, std::string& value) {
        std::array<char, 128> buffer{};
        strncpy_s(buffer.data(), buffer.size(), value.c_str(), _TRUNCATE);
        if (ImGui::InputText(label, buffer.data(), buffer.size())) {
            value = buffer.data();
        }
    }
#endif
}

StartupEnemySpawnController::StartupEnemySpawnController() = default;

StartupEnemySpawnController::~StartupEnemySpawnController() = default;

void StartupEnemySpawnController::Initialize(
    EnemyManager* enemyManager,
    LevelSceneRuntime* levelSceneRuntime,
    const Camera* camera) {
    enemyManager_ = enemyManager;
    levelSceneRuntime_ = levelSceneRuntime;
    camera_ = camera;
    elapsedTime_ = 0.0f;
    hasSpawned_ = false;
    SetLastResult("Initialized");
}

void StartupEnemySpawnController::Finalize() {
    enemyManager_ = nullptr;
    levelSceneRuntime_ = nullptr;
    camera_ = nullptr;
}

void StartupEnemySpawnController::Update(float deltaTime) {
    if (!enableStartupEnemySpawn_ || !spawnOnGameStart_ || hasSpawned_) {
        return;
    }

    elapsedTime_ += (std::max)(0.0f, deltaTime);
    if (elapsedTime_ < spawnDelay_) {
        return;
    }

    SpawnNow();
}

void StartupEnemySpawnController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(430.0f, 320.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("開幕敵出現確認 (Startup Enemy Spawn Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("開幕敵出現を有効化 (Enable Startup Enemy Spawn)", &enableStartupEnemySpawn_);
    ImGui::Checkbox("GameScene開始時に出現 (Spawn On Game Start)", &spawnOnGameStart_);
    DrawStringInput("開幕Spawn対象名 (Startup Spawn Target Name)", startupSpawnTargetName_);
    ImGui::DragFloat("Spawn Delay", &spawnDelay_, 0.01f, 0.0f, 10.0f, "%.2f");
    ImGui::SeparatorText("Release開幕Spawn位置 (Release Startup Spawn Position)");
    ImGui::Checkbox("カメラ基準の開幕Spawnを使う (Use Camera Relative Startup Spawn)", &useCameraRelativeStartupSpawn_);
    ImGui::Checkbox("Release開幕はSpawnPoint距離を上書き (Override SpawnPoint In Release)", &overrideSpawnPointInRelease_);
    ImGui::DragFloat("開幕Spawn距離 (Startup Spawn Distance)", &startupSpawnDistance_, 0.1f, 1.0f, 100.0f, "%.1f");
    ImGui::DragFloat("開幕Spawn高さ (Startup Spawn Height)", &startupSpawnHeight_, 0.1f, -20.0f, 20.0f, "%.1f");
    ImGui::DragFloat("開幕Spawn横オフセット (Startup Spawn Side Offset)", &startupSpawnSideOffset_, 0.1f, -20.0f, 20.0f, "%.1f");
    ImGui::DragFloat("SpawnPointをカメラ側へ寄せる距離 (Pull Spawn Point Toward Camera)", &pullSpawnPointTowardCamera_, 0.1f, 0.0f, 50.0f, "%.1f");
    if (ImGui::Button("今すぐSpawn (Spawn Now)")) {
        SpawnNow();
    }
    ImGui::SameLine();
    if (ImGui::Button("自動Spawnをリセット (Reset Auto Spawn)")) {
        hasSpawned_ = false;
        elapsedTime_ = 0.0f;
        SetLastResult("Auto spawn reset");
    }

    ImGui::Text("Has Spawned: %s", hasSpawned_ ? "true" : "false");
    ImGui::Text("Elapsed / Delay: %.2f / %.2f", elapsedTime_, spawnDelay_);
    ImGui::TextWrapped("Last Spawn Source: %s", lastSpawnSource_.c_str());
    ImGui::Text("Last Raw Spawn Position: %.2f, %.2f, %.2f",
        lastRawSpawnPosition_.x,
        lastRawSpawnPosition_.y,
        lastRawSpawnPosition_.z);
    ImGui::Text("Last Spawn Position: %.2f, %.2f, %.2f",
        lastSpawnPosition_.x,
        lastSpawnPosition_.y,
        lastSpawnPosition_.z);
    ImGui::Text("Camera Distance Raw / Adjusted: %.2f / %.2f",
        lastRawCameraDistance_,
        lastAdjustedCameraDistance_);
    ImGui::Text("Used Startup Adjustment: %s", lastUsedStartupAdjustment_ ? "true" : "false");
    ImGui::TextWrapped("Last Spawn Result: %s", lastSpawnResult_.c_str());
    ImGui::Text("Spawn Count: %zu", spawnCount_);
    ImGui::Text("Failed Spawn Count: %zu", failedSpawnCount_);
    ImGui::End();
#endif
}

bool StartupEnemySpawnController::SpawnNow() {
    if (!enemyManager_) {
        ++failedSpawnCount_;
        SetLastResult("EnemyManager is missing");
        return false;
    }

    Vector3 spawnPosition{};
    std::string spawnSource;
    if (!TryFindSpawnPosition(spawnPosition, spawnSource)) {
        spawnPosition = BuildFallbackPosition();
        spawnSource = "camera fallback";
        lastRawSpawnPosition_ = spawnPosition;
        lastSpawnPosition_ = spawnPosition;
        if (camera_) {
            const Vector3 cameraPosition = camera_->GetTranslate();
            lastRawCameraDistance_ = Length(SubtractVector3(spawnPosition, cameraPosition));
            lastAdjustedCameraDistance_ = lastRawCameraDistance_;
        } else {
            lastRawCameraDistance_ = 0.0f;
            lastAdjustedCameraDistance_ = 0.0f;
        }
        lastUsedStartupAdjustment_ = false;
    } else {
        spawnPosition = AdjustStartupSpawnPosition(spawnPosition);
        spawnSource += " -> startup adjusted";
    }

    if (!enemyManager_->SpawnEnemyAt(spawnPosition, "Startup")) {
        ++failedSpawnCount_;
        lastSpawnSource_ = spawnSource;
        lastSpawnPosition_ = spawnPosition;
        SetLastResult("EnemyManager failed to spawn startup enemy");
        return false;
    }

    hasSpawned_ = true;
    ++spawnCount_;
    lastSpawnSource_ = spawnSource;
    lastSpawnPosition_ = spawnPosition;
    SetLastResult("Spawned startup enemy");
    return true;
}

bool StartupEnemySpawnController::TryFindSpawnPosition(Vector3& outPosition, std::string& outSource) const {
    if (!startupSpawnTargetName_.empty() &&
        TryFindNamedSpawnPosition(startupSpawnTargetName_, outPosition, outSource)) {
        return true;
    }
    if (startupSpawnTargetName_ != "EnemySpawn_01" &&
        TryFindNamedSpawnPosition("EnemySpawn_01", outPosition, outSource)) {
        return true;
    }
    return TryFindNamedSpawnPosition("enemy_spawn_start", outPosition, outSource);
}

bool StartupEnemySpawnController::TryFindNamedSpawnPosition(
    const std::string& targetName,
    Vector3& outPosition,
    std::string& outSource) const {
    if (!levelSceneRuntime_ || targetName.empty()) {
        return false;
    }

    if (levelSceneRuntime_->TryFindObjectWorldPosition(targetName, std::string{}, outPosition)) {
        outSource = "objectId:" + targetName;
        return true;
    }
    if (levelSceneRuntime_->TryFindObjectWorldPosition(std::string{}, targetName, outPosition)) {
        outSource = "objectName:" + targetName;
        return true;
    }
    return false;
}

Vector3 StartupEnemySpawnController::AdjustStartupSpawnPosition(const Vector3& basePosition) {
    lastRawSpawnPosition_ = basePosition;
    lastUsedStartupAdjustment_ = false;

    if (!camera_) {
        lastRawCameraDistance_ = 0.0f;
        lastAdjustedCameraDistance_ = 0.0f;
        return basePosition;
    }

    const Vector3 cameraPosition = camera_->GetTranslate();
    const Vector3 cameraForward = GetCameraForward(*camera_);
    const Vector3 cameraUp = GetCameraUp(*camera_);
    const Vector3 cameraRight = GetCameraRight(*camera_);
    const Vector3 toSpawn = SubtractVector3(basePosition, cameraPosition);
    const float rawDistance = Length(toSpawn);
    Vector3 direction = Normalize(toSpawn, cameraForward);

    float desiredDistance = rawDistance;
    if (pullSpawnPointTowardCamera_ > 0.0f) {
        desiredDistance = (std::max)(1.0f, desiredDistance - pullSpawnPointTowardCamera_);
    }
    if (overrideSpawnPointInRelease_) {
        desiredDistance = (std::min)(desiredDistance, (std::max)(1.0f, startupSpawnDistance_));
    }

    Vector3 adjustedPosition = AddVector3(cameraPosition, ScaleVector3(direction, desiredDistance));
    if (useCameraRelativeStartupSpawn_) {
        adjustedPosition = AddVector3(adjustedPosition, ScaleVector3(cameraUp, startupSpawnHeight_));
        adjustedPosition = AddVector3(adjustedPosition, ScaleVector3(cameraRight, startupSpawnSideOffset_));
    }

    lastRawCameraDistance_ = rawDistance;
    lastAdjustedCameraDistance_ = Length(SubtractVector3(adjustedPosition, cameraPosition));
    lastUsedStartupAdjustment_ = true;
    return adjustedPosition;
}

Vector3 StartupEnemySpawnController::BuildFallbackPosition() const {
    if (!camera_) {
        return { 0.0f, 2.0f, 25.0f };
    }

    const Vector3 cameraPosition = camera_->GetTranslate();
    const Vector3 cameraForward = GetCameraForward(*camera_);
    const Vector3 cameraUp = GetCameraUp(*camera_);
    const Vector3 cameraRight = GetCameraRight(*camera_);
    return AddVector3(
        AddVector3(
            AddVector3(cameraPosition, ScaleVector3(cameraForward, (std::max)(1.0f, startupSpawnDistance_))),
            ScaleVector3(cameraUp, startupSpawnHeight_)),
        ScaleVector3(cameraRight, startupSpawnSideOffset_));
}

void StartupEnemySpawnController::SetLastResult(const std::string& result) {
    lastSpawnResult_ = result;
}
