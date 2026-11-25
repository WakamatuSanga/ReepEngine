#pragma once

#include <cstdint>
#include <string>
#include <string_view>

class AimCorridorTargetingController;
class EnemyManager;
class GameViewport;

// One controller survives scene changes. It keeps handles and timing only;
// SceneManager, the wave manager and the combat owners perform the actions.
class ParticleContestDemo {
public:
    enum class Screen { Busy, Title, TitleDeparture, GameIntro, Game, Other };
    enum class SceneAction { None, StartFromTitle, ReturnToTitle };

    struct FireCommand {
        bool normalFire = false;
        std::string_view missileTarget;
    };

    SceneAction UpdateScreen(Screen screen, bool f8Pressed, float seconds, bool loadFailed);
    bool BlocksManualInput() const { return mode_ != Mode::Idle; }
    bool IsAcquiringLock() const { return mode_ == Mode::Playing && step_ == Step::Lock; }
    FireCommand UpdateGameplay(float seconds, const EnemyManager& enemies,
        const AimCorridorTargetingController& targeting, GameViewport& viewport);
    void RecordMissileLaunch(std::uint64_t projectileId);
    void RecordHit(std::string_view target, std::uint64_t projectileId, bool missile, bool lethal);

private:
    enum class Mode { Idle, Departing, Playing, Returning, TitleWait };
    enum class Step { Aim, Fire, DefeatBeat, Lock, Launch, MissileFlight, MissileBeat };

    SceneAction StartPass();
    FireCommand UpdateCombat(float seconds, bool onTarget, bool lockedOn);
    void ChangeMode(Mode mode);
    void ChangeStep(Step step);
    void Fail(std::string_view reason);

    Mode mode_ = Mode::Idle;
    Step step_ = Step::Aim;
    bool armed_ = false;
    bool returnIssued_ = false;
    bool returnTimedOut_ = false;
    bool completedPass_ = false;
    int failures_ = 0;
    int normalDefeats_ = 0;
    unsigned pass_ = 0;
    float screenSeconds_ = 0.0f;
    float stepSeconds_ = 0.0f;
    float lockVisibleSeconds_ = 0.0f;
    float aimX_ = 0.5f;
    float aimY_ = 0.5f;
    std::string target_;
    std::uint64_t missileId_ = 0;
};
