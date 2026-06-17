#include "PlayerBulletEnemyCollision.h"
#include "Engine/Game/Effect/CombatEffectController.h"
#include "Engine/Game/Enemy/Enemy.h"
#include "Engine/Game/Enemy/EnemyManager.h"
#include "Engine/Game/Player/PlayerBulletManager.h"
#include <algorithm>
#include <vector>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

PlayerBulletEnemyCollision::PlayerBulletEnemyCollision() = default;

PlayerBulletEnemyCollision::~PlayerBulletEnemyCollision() = default;

void PlayerBulletEnemyCollision::Initialize(
    PlayerBulletManager* bulletManager,
    EnemyManager* enemyManager,
    CombatEffectController* combatEffectController) {
    bulletManager_ = bulletManager;
    enemyManager_ = enemyManager;
    combatEffectController_ = combatEffectController;
    if (enemyManager_) {
        enemyHitRadiusForDebug_ = enemyManager_->GetDefaultHitRadius();
    }
}

void PlayerBulletEnemyCollision::Finalize() {
    bulletManager_ = nullptr;
    enemyManager_ = nullptr;
    combatEffectController_ = nullptr;
}

void PlayerBulletEnemyCollision::Update() {
    lastHitResult_ = false;
    lastDamage_ = 0;
    lastDistance_ = -1.0f;
    lastRadiusSum_ = 0.0f;
    lastBulletRadius_ = 0.0f;
    lastEnemyRadius_ = 0.0f;
    if (!enableEnemyHitCollision_ || !bulletManager_ || !enemyManager_) {
        return;
    }

    const std::vector<Enemy*> activeEnemies = enemyManager_->GetActiveEnemies();
    for (Enemy* enemy : activeEnemies) {
        if (!enemy || enemy->IsDead() || !enemy->CanReceivePlayerBullet()) {
            continue;
        }

        Vector3 hitPosition{};
        int damage = 0;
        float lastBulletRadius = 0.0f;
        float lastDistance = -1.0f;
        float lastRadiusSum = 0.0f;
        const float enemyRadius = enemy->GetHitRadius();
        if (!bulletManager_->CheckHitAndKillFirstSphere(
            enemy->GetPosition(),
            enemyRadius,
            &hitPosition,
            &damage,
            &lastDistance,
            &lastRadiusSum,
            &lastBulletRadius)) {
            if (lastDistance_ < 0.0f || (lastDistance >= 0.0f && lastDistance < lastDistance_)) {
                lastDistance_ = lastDistance;
                lastRadiusSum_ = lastRadiusSum;
                lastBulletRadius_ = lastBulletRadius;
                lastEnemyRadius_ = enemyRadius;
            }
            continue;
        }

        const bool wasDeadBeforeDamage = enemy->IsDead();
        enemy->Damage((std::max)(1, damage));
        lastHitResult_ = true;
        lastHitEnemy_ = enemy->GetEnemyId();
        lastHitPosition_ = hitPosition;
        lastDamage_ = damage;
        lastDistance_ = lastDistance;
        lastRadiusSum_ = lastRadiusSum;
        lastBulletRadius_ = lastBulletRadius;
        lastEnemyRadius_ = enemyRadius;
        ++hitCount_;

        if (combatEffectController_ && playCombatEffect_) {
            combatEffectController_->PlayPlayerBulletHitEnemy(hitPosition);
            if (!wasDeadBeforeDamage && enemy->IsDead()) {
                combatEffectController_->PlayEnemyDeathExplosion(enemy->GetPosition());
            }
        }
    }
}

void PlayerBulletEnemyCollision::DrawImGui() {
#ifdef _DEBUG
    ImGui::SetNextWindowSize(ImVec2(360.0f, 310.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("敵ヒット判定確認 (Enemy Hit Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Enable Enemy Hit Collision", &enableEnemyHitCollision_);
    if (ImGui::DragFloat("Enemy Hit Radius", &enemyHitRadiusForDebug_, 0.01f, 0.001f, 20.0f, "%.3f")) {
        if (enemyManager_) {
            enemyManager_->SetDefaultHitRadius(enemyHitRadiusForDebug_);
            enemyManager_->ApplyDefaultHitRadiusToAllEnemies();
        }
    }
    ImGui::Checkbox("Play Combat Effect", &playCombatEffect_);
    ImGui::TextWrapped("Last Hit Enemy: %s", lastHitEnemy_.c_str());
    ImGui::Text("Last Hit Position: %.2f, %.2f, %.2f",
        lastHitPosition_.x,
        lastHitPosition_.y,
        lastHitPosition_.z);
    ImGui::Text("Last Hit Result: %s", lastHitResult_ ? "true" : "false");
    ImGui::Text("Last Distance: %.3f", lastDistance_);
    ImGui::Text("Last Radius Sum: %.3f", lastRadiusSum_);
    ImGui::Text("Last Bullet Radius: %.3f", lastBulletRadius_);
    ImGui::Text("Last Enemy Radius: %.3f", lastEnemyRadius_);
    ImGui::Text("Last Damage: %d", lastDamage_);
    ImGui::Text("Hit Count: %zu", hitCount_);
    ImGui::End();
#endif
}
