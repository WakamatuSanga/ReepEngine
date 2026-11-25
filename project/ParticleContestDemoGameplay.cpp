#include "ParticleContestDemo.h"
#include "Engine/Core/GameViewport.h"
#include "Engine/Game/Enemy/EnemyManager.h"
#include "Engine/Game/Targeting/AimCorridorTargetingController.h"

#include <algorithm>
#include <cmath>

namespace {
bool ProjectOnScreen(const AimCorridorTargetingController& targeting,
    const Vector3& position, Vector2& uv) {
    float depth = 0.0f;
    return targeting.ProjectWorldToScreen(position, uv, depth) &&
        uv.x > 0.02f && uv.x < 0.98f && uv.y > 0.02f && uv.y < 0.98f;
}
}

ParticleContestDemo::FireCommand ParticleContestDemo::UpdateGameplay(
    float seconds, const EnemyManager& enemies, const AimCorridorTargetingController& targeting,
    GameViewport& viewport) {
    if (mode_ != Mode::Playing) return {};

    if (step_ == Step::Aim && target_.empty()) {
        std::vector<EnemyTargetView> candidates;
        enemies.CollectTargetableEnemies(candidates);
        for (const auto& candidate : candidates) {
            Vector2 projected{};
            if (ProjectOnScreen(targeting, candidate.worldPosition, projected)) {
                target_ = candidate.runtimeId;
                break;
            }
        }
    }

    bool onTarget = false;
    EnemyTargetSnapshot target;
    Vector2 projected{};
    if (enemies.TryGetEnemyTargetSnapshot(target_, target) && target.valid &&
        ProjectOnScreen(targeting, target.worldPosition, projected)) {
        constexpr float kAimResponse = 11.0f;
        constexpr float kAlignmentTolerance = 0.004f;
        const float blend = 1.0f - std::exp(-kAimResponse * std::clamp(seconds, 0.0f, 0.1f));
        aimX_ = std::lerp(aimX_, projected.x, blend);
        aimY_ = std::lerp(aimY_, projected.y, blend);
        onTarget = std::hypot(aimX_ - projected.x, aimY_ - projected.y) <= kAlignmentTolerance;
    }
    viewport.SetAutomatedAim({ aimX_, aimY_ });

    const bool lockedOn = targeting.HasLockedTarget() && targeting.GetLockedTargetId() == target_;
    return UpdateCombat(seconds, onTarget, lockedOn);
}
