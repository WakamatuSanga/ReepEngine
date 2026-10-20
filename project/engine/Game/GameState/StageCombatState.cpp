#include "StageCombatState.h"
#include "Engine/Game/Enemy/Enemy.h"
#include "Engine/Game/Enemy/EnemyBullet.h"
#include "Engine/Game/Player/PlayerBulletManager.h"
#include <algorithm>
#include <cmath>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif
namespace { StageCombatState::Tuning tuning; }
StageCombatState::Tuning& StageCombatState::GetTuning() { return tuning; }
void StageCombatState::Initialize(PlayerBulletManager* bullets) { bullets_ = bullets; Reset(); }
void StageCombatState::Reset() { score_ = 0; progressFraction_ = 0; lastKrakenSequence_ = 0; gameplayActive_ = false; paused_ = false; }
void StageCombatState::AddScore(uint64_t amount) {
    const auto room = static_cast<uint64_t>(99999 - score_);
    score_ += static_cast<int>((std::min)(room, amount));
}
void StageCombatState::UpdateProgress(float seconds, bool enabled) {
    if (!enabled || !gameplayActive_ || !std::isfinite(seconds) || seconds <= 0) return;
    progressFraction_ += static_cast<double>(seconds) * std::clamp(GetTuning().progressPerSecond, 0.0f, 100.0f);
    const double whole = std::floor(progressFraction_ + 0.000001);
    if (whole >= 1) { AddScore(static_cast<uint64_t>((std::min)(whole, 99999.0))); progressFraction_ = (std::max)(0.0, progressFraction_ - whole); }
}
void StageCombatState::DefeatReward() {
    AddScore(static_cast<uint64_t>((std::max)(0, GetTuning().defeat)));
    if (bullets_) bullets_->RecoverLockAmmo();
}
void StageCombatState::RecordEnemyDefeat(Enemy& enemy) {
    if (gameplayActive_ && enemy.TryClaimDefeatReward()) DefeatReward();
}
void StageCombatState::RecordKrakenDefeat(uint64_t sequence) {
    if (!gameplayActive_ || sequence == 0 || sequence <= lastKrakenSequence_) return;
    lastKrakenSequence_ = sequence; DefeatReward();
}
void StageCombatState::RecordBulletCancel(EnemyBullet& bullet) {
    if (gameplayActive_ && bullet.TryClaimScore()) AddScore(static_cast<uint64_t>((std::max)(0, GetTuning().cancel)));
}
void StageCombatState::RecordEvade(EnemyBullet& bullet) {
    if (gameplayActive_ && bullet.TryClaimScore()) AddScore(static_cast<uint64_t>((std::max)(0, GetTuning().evade)));
}
void StageCombatState::DrawImGui() {
#ifdef USE_IMGUI
    auto& t = GetTuning();
    ImGui::Text("スコア %05d / 99999、進行端数 %.3f", score_, progressFraction_);
    ImGui::TextWrapped("仮値：敵撃破100／実弾消去10／回避接触・ニアミス20／進行1点/ゲーム秒");
    ImGui::DragInt("敵1体の撃破点", &t.defeat, 1, 0, 99999);
    ImGui::DragInt("実際の敵弾消去点", &t.cancel, 1, 0, 99999);
    ImGui::DragInt("回避成功点", &t.evade, 1, 0, 99999);
    ImGui::DragFloat("通常進行の点/ゲーム秒", &t.progressPerSecond, .1f, 0, 100);
    ImGui::SliderFloat("ニアミス外周倍率（被弾半径和）", &t.nearMissScale, 1.05f, 3);
#endif
}
