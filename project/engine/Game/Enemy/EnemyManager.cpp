#include "EnemyManager.h"
#include "Enemy.h"
#include <algorithm>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

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
    ImGui::DragFloat3("Spawn Position", &debugSpawnPosition_.x, 0.05f, -100.0f, 100.0f, "%.2f");
    ImGui::DragFloat3("Spawn Rotation", &debugSpawnRotation_.x, 0.01f, -6.28318f, 6.28318f, "%.3f");
    ImGui::DragFloat3("Spawn Scale", &debugSpawnScale_.x, 0.01f, 0.001f, 20.0f, "%.3f");

    if (ImGui::Button("Spawn Test Enemy")) {
        Enemy* enemy = SpawnEnemy("Debug", debugSpawnPosition_);
        if (enemy) {
            enemy->SetRotation(debugSpawnRotation_);
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
    if (!object3dCommon_ || !camera_) {
        return nullptr;
    }

    auto enemy = std::make_unique<Enemy>();
    enemy->Initialize(object3dCommon_, camera_, MakeEnemyId());
    enemy->SetEnemyType(enemyType);
    enemy->SetPosition(position);
    Enemy* enemyPtr = enemy.get();
    enemies_.push_back(std::move(enemy));
    return enemyPtr;
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
        if (enemy && enemy->IsActive() && !enemy->IsDead()) {
            ++activeCount;
        }
    }
    return activeCount;
}

std::vector<Vector3> EnemyManager::GetActiveEnemyPositions() const {
    std::vector<Vector3> positions;
    positions.reserve(enemies_.size());
    for (const std::unique_ptr<Enemy>& enemy : enemies_) {
        if (enemy && enemy->IsActive() && !enemy->IsDead()) {
            positions.push_back(enemy->GetPosition());
        }
    }
    return positions;
}

std::string EnemyManager::MakeEnemyId() {
    return "Enemy_" + std::to_string(nextEnemySerial_++);
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
