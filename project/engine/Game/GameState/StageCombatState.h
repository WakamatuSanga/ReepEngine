#pragma once
#include <cstdint>
class Enemy;
class EnemyBullet;
class PlayerBulletManager;

// Scene-lifetime score ledger. The corresponding combat object owns its one-shot claim.
class StageCombatState {
public:
    static float GetNearMissScale();
    void Initialize(PlayerBulletManager* bullets);
    void Reset();
    void SetGameplayActive(bool active) { gameplayActive_ = active && !paused_; }
    bool IsGameplayActive() const { return gameplayActive_; }
    void SetPaused(bool paused) { paused_ = paused; if (paused) gameplayActive_ = false; }
    bool IsPaused() const { return paused_; }
    void UpdateProgress(float gameSeconds, bool progressionEnabled);
    void RecordEnemyDefeat(Enemy& enemy);
    void RecordKrakenDefeat(uint64_t sequence);
    void RecordBulletCancel(EnemyBullet& bullet);
    void RecordEvade(EnemyBullet& bullet);
    int GetScore() const { return score_; }
    uint32_t GetDefeatCount() const { return defeatCount_; }
    void DrawImGui();
private:
    struct Tuning {
        int defeat = 100;
        int cancel = 10;
        int evade = 20;
        float progressPerSecond = 1.0f;
        float nearMissScale = 1.5f;
    };

    static void ApplyTuning(Tuning next);
    static Tuning tuning_;
    void AddScore(uint64_t amount);
    void DefeatReward();
    PlayerBulletManager* bullets_ = nullptr;
    int score_ = 0;
    uint32_t defeatCount_ = 0;
    double progressFraction_ = 0;
    uint64_t lastKrakenSequence_ = 0;
    bool gameplayActive_ = false;
    bool paused_ = false;
};
