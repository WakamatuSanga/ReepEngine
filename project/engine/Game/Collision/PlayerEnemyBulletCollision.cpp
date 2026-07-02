#include "PlayerEnemyBulletCollision.h"
#include "Engine/Game/Effect/CombatEffectController.h"
#include "Engine/Game/Enemy/EnemyBulletManager.h"
#include "Engine/Game/GameState/PlayerDeathSequenceController.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Game/Player/PlayerBulletCancelEffectController.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

PlayerEnemyBulletCollision::PlayerEnemyBulletCollision() = default;

PlayerEnemyBulletCollision::~PlayerEnemyBulletCollision() = default;

void PlayerEnemyBulletCollision::Initialize(
    Player* player,
    EnemyBulletManager* bulletManager,
    PlayerDeathSequenceController* deathSequence,
    CombatEffectController* combatEffectController) {
    player_ = player;
    bulletManager_ = bulletManager;
    deathSequence_ = deathSequence;
    combatEffectController_ = combatEffectController;
    lastBlockedReason_ = "Initialized";
}

void PlayerEnemyBulletCollision::Finalize() {
    player_ = nullptr;
    bulletManager_ = nullptr;
    deathSequence_ = nullptr;
    combatEffectController_ = nullptr;
    bulletCancelEffectController_ = nullptr;
}

void PlayerEnemyBulletCollision::SetBulletCancelEffectController(PlayerBulletCancelEffectController* controller) {
    bulletCancelEffectController_ = controller;
}

void PlayerEnemyBulletCollision::Update() {
    lastHit_ = false;
    lastBlockedByBarrelRoll_ = false;
    lastBlockedReason_ = "None";
    if (!enableCollision_) {
        lastBlockedReason_ = "Collision disabled";
        return;
    }
    if (!player_ || !bulletManager_ || !deathSequence_) {
        lastBlockedReason_ = "Missing dependency";
        return;
    }
    if (deathSequence_->IsActiveOrFinished()) {
        lastBlockedReason_ = "Death sequence active";
        return;
    }

    lastPlayerPosition_ = player_->GetWorldPosition();
    lastPlayerHitRadius_ = player_->GetHitRadius();
    lastPlayerDamageReduction_ = player_->GetDamageReduction();
    if (bulletManager_->CheckHitAndKillFirstSphere(
        lastPlayerPosition_,
        lastPlayerHitRadius_,
        &lastHitPosition_,
        &lastDistance_,
        &lastRadiusSum_,
        &lastBulletRadius_)) {
        lastHit_ = true;
        ++hitCount_;
        if (player_->IsInvincible()) {
            lastBlockedByBarrelRoll_ = true;
            ++barrelRollBlockCount_;
            lastBlockedReason_ = "Blocked by Barrel Roll";
            if (bulletCancelEffectController_) {
                bulletCancelEffectController_->SpawnCancelEffect(lastHitPosition_);
            }
            if (player_->IsBarrelRollEffectEnabled() && combatEffectController_) {
                combatEffectController_->PlayEnemyBulletHitPlayer(lastHitPosition_);
            }
            return;
        }
        if (combatEffectController_) {
            combatEffectController_->PlayEnemyBulletHitPlayer(lastHitPosition_);
            combatEffectController_->PlayPlayerDeathExplosion(lastPlayerPosition_);
        }
        deathSequence_->StartDeathSequence();
    }
}

void PlayerEnemyBulletCollision::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(340.0f, 240.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Player被弾確認 (Player Enemy Bullet Collision)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Enable Collision", &enableCollision_);
    ImGui::Text("Last Hit: %s", lastHit_ ? "true" : "false");
    ImGui::Text("Blocked By Barrel Roll: %s", lastBlockedByBarrelRoll_ ? "true" : "false");
    ImGui::Text("Hit Count: %llu", static_cast<unsigned long long>(hitCount_));
    ImGui::Text("Barrel Roll Block Count: %llu", static_cast<unsigned long long>(barrelRollBlockCount_));
    ImGui::Text("Last Distance: %.3f", lastDistance_);
    ImGui::Text("Last Radius Sum: %.3f", lastRadiusSum_);
    ImGui::Text("Player Hit Radius: %.3f", lastPlayerHitRadius_);
    ImGui::Text("Player Damage Reduction: %.3f", lastPlayerDamageReduction_);
    ImGui::Text("Bullet Radius: %.3f", lastBulletRadius_);
    ImGui::Text("Player Position: %.2f, %.2f, %.2f", lastPlayerPosition_.x, lastPlayerPosition_.y, lastPlayerPosition_.z);
    ImGui::Text("Last Hit Position: %.2f, %.2f, %.2f", lastHitPosition_.x, lastHitPosition_.y, lastHitPosition_.z);
    ImGui::TextWrapped("Blocked Reason: %s", lastBlockedReason_);

    ImGui::End();
#endif
}

