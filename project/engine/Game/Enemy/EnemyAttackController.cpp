#include "EnemyAttackController.h"
#include "EnemyBulletManager.h"
#include "EnemyManager.h"
#include "Engine/Game/GameState/PlayerDeathSequenceController.h"
#include "Engine/Game/Player/Player.h"
#include <algorithm>
#include <cmath>
#include <vector>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kMinVectorLength = 0.00001f;

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
        if (length <= kMinVectorLength) {
            return fallback;
        }
        return { value.x / length, value.y / length, value.z / length };
    }
}

EnemyAttackController::EnemyAttackController() = default;

EnemyAttackController::~EnemyAttackController() = default;

void EnemyAttackController::Initialize(
    EnemyManager* enemyManager,
    EnemyBulletManager* bulletManager,
    Player* player,
    PlayerDeathSequenceController* deathSequence) {
    enemyManager_ = enemyManager;
    bulletManager_ = bulletManager;
    player_ = player;
    deathSequence_ = deathSequence;
    fireTimer_ = 0.0f;
    firedBulletCount_ = 0;
    lastBlockedReason_ = "Initialized";
}

void EnemyAttackController::Finalize() {
    enemyManager_ = nullptr;
    bulletManager_ = nullptr;
    player_ = nullptr;
    deathSequence_ = nullptr;
}

void EnemyAttackController::Update(float deltaTime) {
    lastBlockedReason_ = "None";
    if (!enableShooting_) {
        lastBlockedReason_ = "Shooting disabled";
        return;
    }
    if (!enemyManager_ || !bulletManager_ || !player_) {
        lastBlockedReason_ = "Missing dependency";
        return;
    }
    if (deathSequence_ && deathSequence_->IsActiveOrFinished()) {
        lastBlockedReason_ = "Death sequence active";
        return;
    }

    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    fireTimer_ += safeDeltaTime;
    if (fireTimer_ < fireInterval_) {
        return;
    }

    fireTimer_ = 0.0f;
    FireFromActiveEnemies();
}

void EnemyAttackController::DrawImGui() {
#ifdef _DEBUG
    ImGui::SetNextWindowSize(ImVec2(340.0f, 260.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("敵攻撃確認 (Enemy Attack Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Enable Shooting", &enableShooting_);
    ImGui::DragFloat("Fire Interval", &fireInterval_, 0.05f, 0.05f, 20.0f, "%.2f");
    ImGui::DragFloat("Bullet Speed", &bulletSpeed_, 0.05f, 0.0f, 100.0f, "%.2f");
    ImGui::Text("Fire Timer: %.2f", fireTimer_);
    ImGui::Text("Fired Bullet Count: %llu", static_cast<unsigned long long>(firedBulletCount_));
    ImGui::Text("Last Shooter Count: %d", lastShooterCount_);
    ImGui::Text("Last Target: %.2f, %.2f, %.2f", lastTargetPosition_.x, lastTargetPosition_.y, lastTargetPosition_.z);
    ImGui::Text("Last Direction: %.2f, %.2f, %.2f", lastFireDirection_.x, lastFireDirection_.y, lastFireDirection_.z);
    ImGui::TextWrapped("Blocked Reason: %s", lastBlockedReason_);
    if (ImGui::Button("Fire Once")) {
        FireFromActiveEnemies();
    }

    ImGui::End();
#endif
}

void EnemyAttackController::FireFromActiveEnemies() {
    if (!enemyManager_ || !bulletManager_ || !player_) {
        lastBlockedReason_ = "Missing dependency";
        return;
    }

    const std::vector<Vector3> enemyPositions = enemyManager_->GetActiveEnemyPositions();
    lastShooterCount_ = static_cast<int>(enemyPositions.size());
    if (enemyPositions.empty()) {
        lastBlockedReason_ = "No active enemies";
        return;
    }

    lastTargetPosition_ = player_->GetWorldPosition();
    for (const Vector3& enemyPosition : enemyPositions) {
        const Vector3 direction = Normalize(SubtractVector3(lastTargetPosition_, enemyPosition), { 0.0f, 0.0f, -1.0f });
        lastFireDirection_ = direction;
        if (bulletManager_->SpawnBullet(enemyPosition, ScaleVector3(direction, bulletSpeed_))) {
            ++firedBulletCount_;
        }
    }
}
