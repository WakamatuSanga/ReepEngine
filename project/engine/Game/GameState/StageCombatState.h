#pragma once
#include <cstdint>
class Enemy;
class EnemyBullet;
class PlayerBulletManager;

// Scene-lifetime score ledger. The corresponding combat object owns its one-shot claim.
class StageCombatState {
public:
    struct Tuning { int defeat = 100; int cancel = 10; int evade = 20; float progressPerSecond = 1; float nearMissScale = 1.5f; };
    static Tuning& GetTuning();
    void Initialize(PlayerBulletManager* bullets);
    void Reset();
    void SetGameplayActive(bool active) { gameplayActive_ = active; }
    bool IsGameplayActive() const { return gameplayActive_; }
    void SetPaused(bool paused) { paused_ = paused; if (paused) gameplayActive_ = false; }
    bool IsPaused() const { return paused_; }
    void UpdateProgress(float gameSeconds, bool progressionEnabled);
    void RecordEnemyDefeat(Enemy& enemy);
    void RecordKrakenDefeat(uint64_t sequence);
    void RecordBulletCancel(EnemyBullet& bullet);
    void RecordEvade(EnemyBullet& bullet);
    void AddScore(uint64_t amount);
    int GetScore() const { return score_; }
    void DrawImGui();
private:
    void DefeatReward();
    PlayerBulletManager* bullets_ = nullptr;
    int score_ = 0;
    double progressFraction_ = 0;
    uint64_t lastKrakenSequence_ = 0;
    bool gameplayActive_ = false;
    bool paused_ = false;
};
