#include "StageCombatState.h"
#include "Engine/Game/Enemy/Enemy.h"
#include "Engine/Game/Enemy/EnemyBullet.h"
#include "Engine/Game/Player/PlayerBulletManager.h"
#include <algorithm>
#include <cmath>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

StageCombatState::Tuning StageCombatState::tuning_{};

float StageCombatState::GetNearMissScale() { return tuning_.nearMissScale; }

void StageCombatState::ApplyTuning(Tuning next) {
    next.defeat = std::clamp(next.defeat, 0, 99999);
    next.cancel = std::clamp(next.cancel, 0, 99999);
    next.evade = std::clamp(next.evade, 0, 99999);
    next.progressPerSecond = std::isfinite(next.progressPerSecond)
        ? std::clamp(next.progressPerSecond, 0.0f, 100.0f) : tuning_.progressPerSecond;
    next.nearMissScale = std::isfinite(next.nearMissScale)
        ? std::clamp(next.nearMissScale, 1.05f, 3.0f) : tuning_.nearMissScale;
    tuning_ = next;
}

void StageCombatState::Initialize(PlayerBulletManager* bullets) { bullets_ = bullets; Reset(); }
void StageCombatState::Reset() { score_ = 0; defeatCount_ = 0; progressFraction_ = 0; lastKrakenSequence_ = 0; gameplayActive_ = false; paused_ = false; }
void StageCombatState::AddScore(uint64_t amount) {
    const auto room = static_cast<uint64_t>(99999 - score_);
    score_ += static_cast<int>((std::min)(room, amount));
}
void StageCombatState::UpdateProgress(float seconds, bool enabled) {
    if (!enabled || !gameplayActive_ || !std::isfinite(seconds) || seconds <= 0) return;
    progressFraction_ += static_cast<double>(seconds) * tuning_.progressPerSecond;
    const double whole = std::floor(progressFraction_ + 0.000001);
    if (whole >= 1) { AddScore(static_cast<uint64_t>((std::min)(whole, 99999.0))); progressFraction_ = (std::max)(0.0, progressFraction_ - whole); }
}
void StageCombatState::DefeatReward() {
    ++defeatCount_;
    AddScore(static_cast<uint64_t>(tuning_.defeat));
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
    if (gameplayActive_ && bullet.TryClaimScore()) AddScore(static_cast<uint64_t>(tuning_.cancel));
}
void StageCombatState::RecordEvade(EnemyBullet& bullet) {
    if (gameplayActive_ && bullet.TryClaimScore()) AddScore(static_cast<uint64_t>(tuning_.evade));
}
void StageCombatState::DrawImGui() {
#ifdef USE_IMGUI
    Tuning next = tuning_;
    ImGui::Text("スコア %05d / 99999、進行端数 %.3f", score_, progressFraction_);
    ImGui::TextWrapped("仮値：敵撃破100／実弾消去10／回避接触・ニアミス20／進行1点/ゲーム秒");
    bool edited = ImGui::DragInt("敵1体の撃破点", &next.defeat, 1, 0, 99999);
    edited |= ImGui::DragInt("実際の敵弾消去点", &next.cancel, 1, 0, 99999);
    edited |= ImGui::DragInt("回避成功点", &next.evade, 1, 0, 99999);
    edited |= ImGui::DragFloat("通常進行の点/ゲーム秒", &next.progressPerSecond, .1f, 0, 100);
    edited |= ImGui::SliderFloat("ニアミス外周倍率（被弾半径和）", &next.nearMissScale, 1.05f, 3);
    if (edited) {
        ApplyTuning(next);
    }
#endif
}
