#include "EnemyAttackController.h"
#include "Enemy.h"
#include "EnemyBulletManager.h"
#include "EnemyManager.h"
#include "Engine/Game/GameState/PlayerDeathSequenceController.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Graphics/Camera/Camera.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <vector>

#ifdef USE_IMGUI
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
        if (length <= kMinVectorLength || !std::isfinite(length)) {
            return fallback;
        }
        return { value.x / length, value.y / length, value.z / length };
    }

    Vector4 TransformPoint(const Vector3& value, const Matrix4x4& matrix) {
        return {
            value.x * matrix.m[0][0] + value.y * matrix.m[1][0] + value.z * matrix.m[2][0] + matrix.m[3][0],
            value.x * matrix.m[0][1] + value.y * matrix.m[1][1] + value.z * matrix.m[2][1] + matrix.m[3][1],
            value.x * matrix.m[0][2] + value.y * matrix.m[1][2] + value.z * matrix.m[2][2] + matrix.m[3][2],
            value.x * matrix.m[0][3] + value.y * matrix.m[1][3] + value.z * matrix.m[2][3] + matrix.m[3][3],
        };
    }

    std::string ToLowerCopy(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
            });
        return value;
    }
}

EnemyAttackController::EnemyAttackController() = default;

EnemyAttackController::~EnemyAttackController() = default;

void EnemyAttackController::Initialize(
    EnemyManager* enemyManager,
    EnemyBulletManager* bulletManager,
    Player* player,
    PlayerDeathSequenceController* deathSequence,
    const Camera* camera) {
    enemyManager_ = enemyManager;
    bulletManager_ = bulletManager;
    player_ = player;
    deathSequence_ = deathSequence;
    camera_ = camera;
    fireTimer_ = 0.0f;
    firedBulletCount_ = 0;
    blockedAttackCount_ = 0;
    lastBlockedReason_ = "Initialized";
}

void EnemyAttackController::Finalize() {
    enemyManager_ = nullptr;
    bulletManager_ = nullptr;
    player_ = nullptr;
    deathSequence_ = nullptr;
    camera_ = nullptr;
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
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(380.0f, 320.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("敵攻撃確認 (Enemy Attack Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Enable Shooting", &enableShooting_);
    ImGui::Checkbox("Enemy Attack Visible Gate", &enableOffscreenAttackGate_);
    ImGui::DragFloat("Fire Interval", &fireInterval_, 0.05f, 0.05f, 20.0f, "%.2f");
    ImGui::DragFloat("Bullet Speed", &bulletSpeed_, 0.05f, 0.0f, 100.0f, "%.2f");
    ImGui::SliderFloat("Gate NDC Limit X", &screenGateNdcLimitX_, 1.0f, 2.0f, "%.2f");
    ImGui::SliderFloat("Gate NDC Limit Y", &screenGateNdcLimitY_, 1.0f, 2.0f, "%.2f");
    ImGui::Text("Fire Timer: %.2f", fireTimer_);
    ImGui::Text("Fired Bullet Count: %llu", static_cast<unsigned long long>(firedBulletCount_));
    ImGui::Text("Offscreen Attack Block Count: %llu", static_cast<unsigned long long>(blockedAttackCount_));
    ImGui::Text("Last Shooter Count: %d", lastShooterCount_);
    ImGui::Text("Last Target: %.2f, %.2f, %.2f", lastTargetPosition_.x, lastTargetPosition_.y, lastTargetPosition_.z);
    ImGui::Text("Last Direction: %.2f, %.2f, %.2f", lastFireDirection_.x, lastFireDirection_.y, lastFireDirection_.z);
    ImGui::TextWrapped("Last Blocked Enemy: %s / %s", lastBlockedEnemyId_.c_str(), lastBlockedEnemyType_.c_str());
    ImGui::TextWrapped("Blocked Reason: %s", lastBlockedReason_.c_str());
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

    const std::vector<Enemy*> enemies = enemyManager_->GetActiveEnemies();
    lastShooterCount_ = 0;
    if (enemies.empty()) {
        lastBlockedReason_ = "No active enemies";
        return;
    }

    lastTargetPosition_ = player_->GetWorldPosition();
    for (Enemy* enemy : enemies) {
        if (!enemy || !enemy->CanAttack()) {
            continue;
        }
        if (IsExternalAttackEnemy(*enemy)) {
            lastBlockedReason_ = "External attack pattern";
            lastBlockedEnemyId_ = enemy->GetEnemyId();
            lastBlockedEnemyType_ = enemy->GetEnemyType();
            continue;
        }

        std::string blockReason;
        if (!IsEnemyAllowedToAttack(*enemy, blockReason)) {
            ++blockedAttackCount_;
            lastBlockedReason_ = blockReason;
            lastBlockedEnemyId_ = enemy->GetEnemyId();
            lastBlockedEnemyType_ = enemy->GetEnemyType();
            continue;
        }

        const Vector3 enemyPosition = enemy->GetPosition();
        const Vector3 direction = Normalize(SubtractVector3(lastTargetPosition_, enemyPosition), { 0.0f, 0.0f, -1.0f });
        lastFireDirection_ = direction;
        if (bulletManager_->SpawnBullet(enemyPosition, ScaleVector3(direction, bulletSpeed_))) {
            ++firedBulletCount_;
            ++lastShooterCount_;
        }
    }
}

bool EnemyAttackController::IsEnemyAllowedToAttack(const Enemy& enemy, std::string& reason) const {
    if (!enableOffscreenAttackGate_) {
        return true;
    }
    if (!camera_) {
        reason = "Camera missing for visible gate";
        return true;
    }

    const Vector4 clip = TransformPoint(enemy.GetPosition(), camera_->GetViewProjectionMatrix());
    if (clip.w <= 0.0001f || !std::isfinite(clip.w)) {
        reason = "behind camera";
        return false;
    }

    const float invW = 1.0f / clip.w;
    const float ndcX = clip.x * invW;
    const float ndcY = clip.y * invW;
    const float ndcZ = clip.z * invW;
    if (!std::isfinite(ndcX) || !std::isfinite(ndcY) || !std::isfinite(ndcZ)) {
        reason = "invalid camera projection";
        return false;
    }
    if (ndcZ < -0.05f || ndcZ > 1.25f) {
        reason = "outside depth clip";
        return false;
    }
    if (std::fabs(ndcX) > (std::max)(1.0f, screenGateNdcLimitX_)) {
        reason = "outside screen X";
        return false;
    }
    if (std::fabs(ndcY) > (std::max)(1.0f, screenGateNdcLimitY_)) {
        reason = "outside screen Y";
        return false;
    }
    return true;
}

bool EnemyAttackController::IsExternalAttackEnemy(const Enemy& enemy) const {
    const std::string type = ToLowerCopy(enemy.GetEnemyType());
    return type.find("laser") != std::string::npos || type.find("telegraph") != std::string::npos;
}
