#include "EnemyManager.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Game/Player/Player.h"
#include "Enemy.h"
#include <algorithm>
#include <cmath>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kMinVectorLength = 0.00001f;

    Vector3 ScaleVector3(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    Vector3 SubtractVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    }

    Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
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

    Vector3 GetCameraRight(const Camera& camera) {
        const Matrix4x4& matrix = camera.GetWorldMatrix();
        return Normalize({ matrix.m[0][0], matrix.m[0][1], matrix.m[0][2] }, { 1.0f, 0.0f, 0.0f });
    }

    Vector3 GetCameraUp(const Camera& camera) {
        const Matrix4x4& matrix = camera.GetWorldMatrix();
        return Normalize({ matrix.m[1][0], matrix.m[1][1], matrix.m[1][2] }, { 0.0f, 1.0f, 0.0f });
    }
}

EnemyManager::EnemyManager() = default;

EnemyManager::~EnemyManager() = default;

void EnemyManager::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    object3dCommon_ = object3dCommon;
    camera_ = camera;
}

void EnemyManager::Finalize() {
    DeleteAllEnemies();
    object3dCommon_ = nullptr;
    camera_ = nullptr;
    player_ = nullptr;
}

void EnemyManager::Update(float deltaTime) {
    for (std::unique_ptr<Enemy>& enemy : enemies_) {
        if (enemy) {
            enemy->Update(deltaTime);
        }
    }

    if (autoRemoveDeadEnemies_) {
        RemoveDeadEnemies();
    }
}

void EnemyManager::Draw() {
    for (std::unique_ptr<Enemy>& enemy : enemies_) {
        if (enemy) {
            enemy->Draw();
        }
    }
}

void EnemyManager::DrawImGui() {
#ifdef _DEBUG
    ImGui::SetNextWindowSize(ImVec2(380.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("敵確認 (Enemy Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Enemy Count: %zu", GetEnemyCount());
    ImGui::Text("Active Count: %zu", GetActiveCount());
    ImGui::Checkbox("Auto Remove Dead Enemies", &autoRemoveDeadEnemies_);
    ImGui::Checkbox("Spawn Faces Camera Opposite", &debugSpawnFaceCameraOpposite_);
    const Vector3 defaultForward = GetDefaultSpawnForward();
    ImGui::Text("Default Spawn Forward: %.3f, %.3f, %.3f", defaultForward.x, defaultForward.y, defaultForward.z);
    ImGui::DragFloat3("Spawn Position", &debugSpawnPosition_.x, 0.05f, -100.0f, 100.0f, "%.2f");
    ImGui::DragFloat3("Spawn Rotation", &debugSpawnRotation_.x, 0.01f, -6.28318f, 6.28318f, "%.3f");
    ImGui::DragFloat3("Spawn Scale", &debugSpawnScale_.x, 0.01f, 0.001f, 20.0f, "%.3f");
    if (ImGui::DragFloat("Default Enemy Hit Radius", &defaultHitRadius_, 0.01f, 0.001f, 20.0f, "%.3f")) {
        defaultHitRadius_ = (std::max)(0.001f, defaultHitRadius_);
    }
    if (ImGui::Button("Apply Hit Radius To All Enemies")) {
        ApplyDefaultHitRadiusToAllEnemies();
    }
    ImGui::SeparatorText("敵出現演出 (Enemy Spawn Presentation)");
    ImGui::Checkbox("Camera基準の画面外Entry (Use Camera Relative Spawn Entry)", &useCameraRelativeSpawnEntry_);
    ImGui::DragFloat("Spawn Entry Height", &spawnEntryHeight_, 0.1f, 0.0f, 100.0f, "%.2f");
    ImGui::DragFloat("Spawn Entry Depth", &spawnEntryDepth_, 0.1f, -100.0f, 100.0f, "%.2f");
    ImGui::DragFloat("Spawn Entry Side Offset", &spawnEntrySideOffset_, 0.1f, -100.0f, 100.0f, "%.2f");
    ImGui::DragFloat("Spawn Duration", &spawnDuration_, 0.05f, 0.01f, 10.0f, "%.2f");
    ImGui::DragFloat("Spawn Spin Speed deg/sec", &spawnSpinSpeedDegrees_, 5.0f, -3600.0f, 3600.0f, "%.1f");
    ImGui::DragFloat("Spawn Attack Delay", &spawnAttackDelay_, 0.05f, 0.0f, 10.0f, "%.2f");
    ImGui::Checkbox("Spawn中は真下を向く (Spawn Facing Down)", &spawnFaceDownDuringSpawn_);
    ImGui::Checkbox("前方向軸でスピン (Spawn Spin Around Forward)", &spawnSpinAroundForward_);
    ImGui::Checkbox("Spawn完了時にPlayerを見る (Face Player On Complete)", &spawnFacePlayerOnComplete_);
    ImGui::Checkbox("Active時にPitchを戻す (Reset Pitch On Active)", &spawnResetPitchOnActive_);
    ImGui::Checkbox("Active時にRollを戻す (Reset Roll On Active)", &spawnResetRollOnActive_);
    ImGui::Checkbox("Spawn中も当たり判定ON (Enable Collision During Spawn)", &spawnCollisionDuringSpawn_);
    const Vector3 spawnLookTarget = GetSpawnLookTarget();
    const Vector3 previewStartPosition = BuildSpawnStartPosition(debugSpawnPosition_);
    ImGui::Text("Spawn Look Target: %.2f, %.2f, %.2f", spawnLookTarget.x, spawnLookTarget.y, spawnLookTarget.z);
    ImGui::Text("Preview Spawn Start: %.2f, %.2f, %.2f",
        previewStartPosition.x,
        previewStartPosition.y,
        previewStartPosition.z);
    if (ImGui::Checkbox("Use Lightweight Enemy Visual", &useLightweightEnemyVisual_)) {
        SetUseLightweightEnemyVisual(useLightweightEnemyVisual_);
    }

    if (ImGui::Button("Preview Spawn Animation")) {
        Enemy* enemy = debugSpawnFaceCameraOpposite_
            ? SpawnEnemy("Debug", debugSpawnPosition_, GetDefaultSpawnForward())
            : SpawnEnemy("Debug", debugSpawnPosition_);
        if (enemy) {
            if (!debugSpawnFaceCameraOpposite_) {
                enemy->SetRotation(debugSpawnRotation_);
            }
            enemy->SetScale(debugSpawnScale_);
            selectedEnemyIndex_ = static_cast<int>(enemies_.size()) - 1;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete All Enemies")) {
        DeleteAllEnemies();
    }
    if (ImGui::Button("Remove Dead Enemies")) {
        RemoveDeadEnemies();
    }

    ImGui::SeparatorText("敵一覧 (Enemy List)");
    if (enemies_.empty()) {
        ImGui::TextDisabled("No enemies.");
    } else {
        if (selectedEnemyIndex_ < 0 || selectedEnemyIndex_ >= static_cast<int>(enemies_.size())) {
            selectedEnemyIndex_ = 0;
        }
        for (int index = 0; index < static_cast<int>(enemies_.size()); ++index) {
            Enemy* enemy = enemies_[index].get();
            const std::string label =
                enemy
                    ? enemy->GetEnemyId() + " (" + enemy->GetEnemyType() + ")"
                    : "Enemy " + std::to_string(index);
            if (ImGui::Selectable(label.c_str(), selectedEnemyIndex_ == index)) {
                selectedEnemyIndex_ = index;
            }
        }
    }

    if (selectedEnemyIndex_ >= 0 && selectedEnemyIndex_ < static_cast<int>(enemies_.size())) {
        ImGui::SeparatorText("選択中Enemy情報 (Selected Enemy Info)");
        if (Enemy* selectedEnemy = enemies_[selectedEnemyIndex_].get()) {
            selectedEnemy->DrawImGui();
        }
    }

    ImGui::End();
#endif
}

Enemy* EnemyManager::SpawnEnemy(const std::string& enemyType, Vector3 position) {
    return SpawnEnemy(enemyType, position, GetDefaultSpawnForward());
}

Enemy* EnemyManager::SpawnEnemy(const std::string& enemyType, Vector3 position, const Vector3& forward) {
    if (!object3dCommon_ || !camera_) {
        return nullptr;
    }

    auto enemy = std::make_unique<Enemy>();
    enemy->Initialize(object3dCommon_, camera_, MakeEnemyId());
    enemy->SetEnemyType(enemyType);
    enemy->SetUseLightweightVisual(useLightweightEnemyVisual_);
    enemy->SetHitRadius(defaultHitRadius_);
    const Vector3 spawnLookTarget = GetSpawnLookTarget();
    enemy->SetSpawnPresentationOptions(
        spawnFaceDownDuringSpawn_,
        spawnFacePlayerOnComplete_,
        spawnResetRollOnActive_,
        spawnResetPitchOnActive_,
        spawnCollisionDuringSpawn_,
        spawnSpinAroundForward_ ? Enemy::SpawnSpinAxisMode::AroundForward : Enemy::SpawnSpinAxisMode::AroundWorldY,
        spawnLookTarget);
    if (spawnFacePlayerOnComplete_) {
        enemy->SetForward(SubtractVector3(spawnLookTarget, position));
    } else {
        enemy->SetForward(forward);
    }
    enemy->StartSpawnAnimationFrom(
        BuildSpawnStartPosition(position),
        position,
        spawnDuration_,
        spawnSpinSpeedDegrees_,
        spawnAttackDelay_);
    Enemy* enemyPtr = enemy.get();
    enemies_.push_back(std::move(enemy));
    return enemyPtr;
}

Enemy* EnemyManager::SpawnEnemyAt(const Vector3& position) {
    return SpawnEnemyAt(position, "Default");
}

Enemy* EnemyManager::SpawnEnemyAt(const Vector3& position, const std::string& enemyType) {
    return SpawnEnemy(enemyType, position);
}

Enemy* EnemyManager::SpawnEnemyAt(const Vector3& position, const std::string& enemyType, const Vector3& forward) {
    return SpawnEnemy(enemyType, position, forward);
}

void EnemyManager::DeleteAllEnemies() {
    for (std::unique_ptr<Enemy>& enemy : enemies_) {
        if (enemy) {
            enemy->Finalize();
        }
    }
    enemies_.clear();
    selectedEnemyIndex_ = -1;
}

size_t EnemyManager::GetEnemyCount() const {
    return enemies_.size();
}

size_t EnemyManager::GetActiveCount() const {
    size_t activeCount = 0;
    for (const std::unique_ptr<Enemy>& enemy : enemies_) {
        if (enemy && enemy->CanAttack()) {
            ++activeCount;
        }
    }
    return activeCount;
}

std::vector<Vector3> EnemyManager::GetActiveEnemyPositions() const {
    std::vector<Vector3> positions;
    positions.reserve(enemies_.size());
    for (const std::unique_ptr<Enemy>& enemy : enemies_) {
        if (enemy && enemy->CanAttack()) {
            positions.push_back(enemy->GetPosition());
        }
    }
    return positions;
}

std::vector<Enemy*> EnemyManager::GetActiveEnemies() const {
    std::vector<Enemy*> activeEnemies;
    activeEnemies.reserve(enemies_.size());
    for (const std::unique_ptr<Enemy>& enemy : enemies_) {
        if (enemy && enemy->IsActive() && !enemy->IsDead()) {
            activeEnemies.push_back(enemy.get());
        }
    }
    return activeEnemies;
}

void EnemyManager::SetDefaultHitRadius(float hitRadius) {
    defaultHitRadius_ = (std::max)(0.001f, hitRadius);
}

void EnemyManager::ApplyDefaultHitRadiusToAllEnemies() {
    for (std::unique_ptr<Enemy>& enemy : enemies_) {
        if (enemy) {
            enemy->SetHitRadius(defaultHitRadius_);
        }
    }
}

void EnemyManager::SetUseLightweightEnemyVisual(bool useLightweightVisual) {
    useLightweightEnemyVisual_ = useLightweightVisual;
    for (std::unique_ptr<Enemy>& enemy : enemies_) {
        if (enemy) {
            enemy->SetUseLightweightVisual(useLightweightEnemyVisual_);
        }
    }
}

void EnemyManager::SetPlayer(Player* player) {
    player_ = player;
}

std::string EnemyManager::MakeEnemyId() {
    return "Enemy_" + std::to_string(nextEnemySerial_++);
}

Vector3 EnemyManager::GetDefaultSpawnForward() const {
    if (!camera_) {
        return { 0.0f, 0.0f, -1.0f };
    }
    return ScaleVector3(GetCameraForward(*camera_), -1.0f);
}

Vector3 EnemyManager::GetSpawnLookTarget() const {
    if (player_) {
        return player_->GetWorldPosition();
    }
    if (camera_) {
        return camera_->GetTranslate();
    }
    return { 0.0f, 0.0f, 0.0f };
}

Vector3 EnemyManager::BuildSpawnStartPosition(const Vector3& targetPosition) const {
    if (useCameraRelativeSpawnEntry_ && camera_) {
        const Vector3 cameraUp = GetCameraUp(*camera_);
        const Vector3 cameraForward = GetCameraForward(*camera_);
        const Vector3 cameraRight = GetCameraRight(*camera_);
        return AddVector3(
            AddVector3(
                AddVector3(targetPosition, ScaleVector3(cameraUp, spawnEntryHeight_)),
                ScaleVector3(cameraForward, -spawnEntryDepth_)),
            ScaleVector3(cameraRight, spawnEntrySideOffset_));
    }

    return AddVector3(targetPosition, { 0.0f, spawnEntryHeight_, -spawnEntryDepth_ });
}

void EnemyManager::RemoveDeadEnemies() {
    enemies_.erase(
        std::remove_if(
            enemies_.begin(),
            enemies_.end(),
            [](const std::unique_ptr<Enemy>& enemy) {
                return !enemy || enemy->IsDead();
            }),
        enemies_.end());

    if (selectedEnemyIndex_ >= static_cast<int>(enemies_.size())) {
        selectedEnemyIndex_ = enemies_.empty() ? -1 : static_cast<int>(enemies_.size()) - 1;
    }
}
