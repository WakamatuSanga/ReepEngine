#include "ParticleContestDemo.h"
#include "Engine/Utility/Logger.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr int kNormalDefeats = 3;
constexpr int kFailureLimit = 3;
constexpr float kTransitionTimeout = 30.0f;
constexpr float kAimOrFireTimeout = 8.0f;
constexpr float kMissileTimeout = 7.0f;
constexpr float kDefeatBeat = 1.1f;
constexpr float kLockBeat = 0.18f;
constexpr float kMissileBeat = 1.5f;
constexpr float kTitleBeat = 0.9f;

float SafeStep(float seconds, float maxStep) {
    return std::isfinite(seconds) ? std::clamp(seconds, 0.0f, maxStep) : 0.0f;
}
}

ParticleContestDemo::SceneAction ParticleContestDemo::StartPass() {
    ChangeMode(Mode::Departing);
    ChangeStep(Step::Aim);
    normalDefeats_ = 0;
    completedPass_ = false;
    returnIssued_ = false;
    returnTimedOut_ = false;
    target_.clear();
    missileId_ = 0;
    aimX_ = 0.5f;
    aimY_ = 0.5f;
    Logger::Log("[Particle Contest] Start pass " + std::to_string(++pass_));
    return SceneAction::StartFromTitle;
}

ParticleContestDemo::SceneAction ParticleContestDemo::UpdateScreen(
    Screen screen, bool f8Pressed, float seconds, bool loadFailed) {
    const float dt = SafeStep(seconds, 0.25f);
    if (f8Pressed) {
        if (armed_) {
            armed_ = false;
            Logger::Log("[Particle Contest] Stopped by F8");
            if (mode_ != Mode::Returning) ChangeMode(Mode::Returning);
        } else if (mode_ == Mode::Idle && screen == Screen::Title) {
            armed_ = true;
            failures_ = 0;
            pass_ = 0;
            return StartPass();
        }
    }

    if (mode_ == Mode::Departing) {
        screenSeconds_ += dt;
        if (screen == Screen::Game) ChangeMode(Mode::Playing);
        else if (screen == Screen::Title && loadFailed) Fail("Stage preparation failed");
        else if (screen == Screen::Other || screenSeconds_ >= kTransitionTimeout)
            Fail("The stage did not become playable");
    } else if (mode_ == Mode::Playing && screen != Screen::Game) {
        Fail("Gameplay was interrupted");
    }

    if (mode_ == Mode::Returning) {
        if (screen == Screen::Title) {
            if (completedPass_) {
                failures_ = 0;
                Logger::Log("[Particle Contest] Completed pass " + std::to_string(pass_) + " (title shown)");
                completedPass_ = false;
            }
            ChangeMode(armed_ ? Mode::TitleWait : Mode::Idle);
        } else {
            // A pending return is fulfilled only after the cloud transition
            // and the replacement scene's first presentation have finished.
            if (!returnIssued_ && screen != Screen::Busy) {
                returnIssued_ = true;
                return SceneAction::ReturnToTitle;
            }
            screenSeconds_ += dt;
            if (!returnTimedOut_ && screenSeconds_ >= kTransitionTimeout) {
                armed_ = false;
                returnTimedOut_ = true;
                Logger::Log("[Particle Contest] Title return timed out. Waiting for a safe scene boundary.");
            }
        }
    } else if (mode_ == Mode::TitleWait && screen == Screen::Title) {
        screenSeconds_ += dt;
        if (screenSeconds_ >= kTitleBeat) return StartPass();
    }
    return SceneAction::None;
}

ParticleContestDemo::FireCommand ParticleContestDemo::UpdateCombat(float seconds, bool onTarget, bool lockedOn) {
    if (mode_ != Mode::Playing) return {};
    const float dt = SafeStep(seconds, 0.1f);
    stepSeconds_ += dt;

    switch (step_) {
    case Step::Aim:
        if (onTarget) ChangeStep(normalDefeats_ < kNormalDefeats ? Step::Fire : Step::Lock);
        else if (stepSeconds_ >= kAimOrFireTimeout) Fail("No target in view or aim did not settle");
        break;
    case Step::Fire:
        if (stepSeconds_ >= kAimOrFireTimeout) Fail("Selected target was not defeated by normal fire");
        else return { onTarget, {} };
        break;
    case Step::DefeatBeat:
        if (stepSeconds_ >= kDefeatBeat) {
            target_.clear();
            ChangeStep(Step::Aim);
        }
        break;
    case Step::Lock:
        lockVisibleSeconds_ = onTarget && lockedOn ? lockVisibleSeconds_ + dt : 0.0f;
        if (lockVisibleSeconds_ >= kLockBeat) {
            Logger::Log("[Particle Contest] Lock confirmed on " + target_);
            ChangeStep(Step::Launch);
            return { false, target_ };
        }
        if (stepSeconds_ >= kAimOrFireTimeout) Fail("Lock did not complete");
        break;
    case Step::Launch:
        Fail("Locked missile did not spawn");
        break;
    case Step::MissileFlight:
        if (stepSeconds_ >= kMissileTimeout) Fail("Launched missile did not hit its target");
        break;
    case Step::MissileBeat:
        if (stepSeconds_ >= kMissileBeat) {
            completedPass_ = true;
            ChangeMode(Mode::Returning);
        }
        break;
    }
    return {};
}

void ParticleContestDemo::RecordMissileLaunch(std::uint64_t projectileId) {
    if (mode_ != Mode::Playing || step_ != Step::Launch || projectileId == 0) return;
    missileId_ = projectileId;
    Logger::Log("[Particle Contest] Missile " + std::to_string(projectileId) + " launched at " + target_);
    ChangeStep(Step::MissileFlight);
}

void ParticleContestDemo::RecordHit(std::string_view target, std::uint64_t projectileId, bool missile, bool lethal) {
    if (mode_ != Mode::Playing || target != target_) return;
    if (step_ == Step::Fire && !missile && lethal) {
        Logger::Log("[Particle Contest] Normal defeat " + std::to_string(++normalDefeats_) + " (" + target_ + ")");
        ChangeStep(Step::DefeatBeat);
    } else if (step_ == Step::MissileFlight && missile && projectileId == missileId_) {
        Logger::Log("[Particle Contest] Missile " + std::to_string(projectileId) + " hit " + target_);
        ChangeStep(Step::MissileBeat);
    }
}

void ParticleContestDemo::ChangeMode(Mode mode) {
    mode_ = mode;
    screenSeconds_ = 0.0f;
}

void ParticleContestDemo::ChangeStep(Step step) {
    step_ = step;
    stepSeconds_ = 0.0f;
    lockVisibleSeconds_ = 0.0f;
}

void ParticleContestDemo::Fail(std::string_view reason) {
    ++failures_;
    Logger::Log("[Particle Contest] Pass " + std::to_string(pass_) + " failed (" +
        std::to_string(failures_) + "/" + std::to_string(kFailureLimit) + "): " + std::string(reason));
    if (failures_ >= kFailureLimit) armed_ = false;
    ChangeMode(Mode::Returning);
}
