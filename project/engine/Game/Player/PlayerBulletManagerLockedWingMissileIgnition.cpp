#include "PlayerBulletManager.h"
#include "LockedWingMissileExhaustController.h"

#include "Engine/Game/Enemy/EnemyBullet.h"
#include "Engine/Game/RailShooter/ProjectileRailMotionAdapter.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
constexpr float kMinimumVectorLength = 0.00001f;
constexpr float kRecommendedDropDuration = 0.06f;
constexpr float kRecommendedDropDistance = 0.20f;
constexpr float kRecommendedHoldDuration = 0.24f;
constexpr float kRecommendedIgnitionRampDuration = 0.15f;

Vector3 Add(const Vector3& lhs, const Vector3& rhs) {
    return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

Vector3 Scale(const Vector3& value, float scale) {
    return { value.x * scale, value.y * scale, value.z * scale };
}

float Length(const Vector3& value) {
    return std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z);
}

bool IsFinite(const Vector3& value) {
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

bool TryNormalize(const Vector3& value, Vector3& normalized) {
    const float length = Length(value);
    if (!IsFinite(value) || !std::isfinite(length)
        || length <= kMinimumVectorLength) {
        return false;
    }
    normalized = Scale(value, 1.0f / length);
    return IsFinite(normalized);
}

float SmoothStep(float value) {
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float SmoothStepIntegral(float value) {
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * t - 0.5f * t * t * t * t;
}

const char* ToJapaneseBool(bool value) {
    return value ? "はい" : "いいえ";
}

const char* ToJapanesePhase(
    PlayerBulletManager::LockedWingLaunchPhase phase) {
    switch (phase) {
    case PlayerBulletManager::LockedWingLaunchPhase::EjectionDrop:
        return "下方分離";
    case PlayerBulletManager::LockedWingLaunchPhase::PreIgnitionHold:
        return "点火前待機";
    case PlayerBulletManager::LockedWingLaunchPhase::IgnitionRamp:
        return "点火加速";
    case PlayerBulletManager::LockedWingLaunchPhase::Cruise:
        return "巡航";
    default:
        return "不明";
    }
}
}

void PlayerBulletManager::UpdateLockedWingShot(
    PlayerBulletInstance& instance, float scaledDeltaTime) {
    if (instance.projectileType != PlayerProjectileType::LockedWingShot
        || !instance.lockedWingLaunch || !instance.bullet) {
        return;
    }

    LockedWingLaunchState& state = *instance.lockedWingLaunch;
    EnemyBullet& bullet = *instance.bullet;
    state.homingEnabled = false;
    if (bullet.IsDead()) {
        if (state.exhaustHandle != 0 && lockedWingMissileExhaustController_) {
            lockedWingMissileExhaustController_->Stop(state.exhaustHandle);
        }
        state.exhaustHandle = 0;
        state.exhaustEnabled = false;
        if (state.sequence == lastLockedWingShotSequence_) {
            lastLockedWingExhaustEnabled_ = false;
        }
        return;
    }

    if (projectileRailMotionAdapter_) {
        Vector3 transported{};
        if (projectileRailMotionAdapter_->TransportDirectionForProjectile(
                bullet,
                ProjectileRailMotionAdapter::ProjectileKind::Player,
                state.currentFlightDirection,
                transported)) {
            Vector3 normalized{};
            if (TryNormalize(transported, normalized)) {
                state.currentFlightDirection = normalized;
            } else {
                ++lockedWingDirectionFallbackCount_;
            }
        }
        if (projectileRailMotionAdapter_->TransportDirectionForProjectile(
                bullet,
                ProjectileRailMotionAdapter::ProjectileKind::Player,
                state.currentEjectionDownDirection,
                transported)) {
            Vector3 normalized{};
            if (TryNormalize(transported, normalized)) {
                state.currentEjectionDownDirection = normalized;
            } else {
                ++lockedWingDirectionFallbackCount_;
            }
        }
    }

    Vector3 flightDirection{};
    if (!TryNormalize(state.currentFlightDirection, flightDirection)) {
        ++lockedWingDirectionFallbackCount_;
        if (!TryNormalize(bullet.GetVisualForwardOverride(), flightDirection)) {
            flightDirection = { 0.0f, 0.0f, 1.0f };
        }
    }
    Vector3 ejectionDownDirection{};
    if (!TryNormalize(
            state.currentEjectionDownDirection, ejectionDownDirection)) {
        ++lockedWingDirectionFallbackCount_;
        ejectionDownDirection = { 0.0f, -1.0f, 0.0f };
    }
    state.currentFlightDirection = flightDirection;
    state.currentEjectionDownDirection = ejectionDownDirection;

    const float safeDeltaTime = std::isfinite(scaledDeltaTime)
        ? (std::max)(scaledDeltaTime, 0.0f)
        : 0.0f;
    const float previousElapsed = state.totalElapsed;
    state.totalElapsed += safeDeltaTime;
    const float dropDuration =
        (std::max)(state.ejectionDropDuration, 0.001f);
    const float holdDuration =
        (std::max)(state.preIgnitionHoldDuration, 0.0f);
    const float rampDuration =
        (std::max)(state.ignitionRampDuration, 0.001f);
    const float holdEnd = dropDuration + holdDuration;
    const float ignitionEnd = holdEnd + rampDuration;

    const LockedWingLaunchPhase previousPhase = state.phase;
    Vector3 endVelocity{};
    float speedRate = 0.0f;
    if (state.totalElapsed < dropDuration) {
        state.phase = LockedWingLaunchPhase::EjectionDrop;
        const float dropSpeed = state.ejectionDropDistance / dropDuration;
        endVelocity = Scale(ejectionDownDirection, dropSpeed);
    } else if (state.totalElapsed < holdEnd) {
        state.phase = LockedWingLaunchPhase::PreIgnitionHold;
    } else if (state.totalElapsed < ignitionEnd) {
        state.phase = LockedWingLaunchPhase::IgnitionRamp;
        const float ignitionElapsed = state.totalElapsed - holdEnd;
        speedRate = SmoothStep(ignitionElapsed / rampDuration);
        endVelocity = Scale(
            flightDirection, state.baseBulletSpeed * speedRate);
    } else {
        state.phase = LockedWingLaunchPhase::Cruise;
        speedRate = 1.0f;
        endVelocity = Scale(flightDirection, state.baseBulletSpeed);
    }

    const float dropOverlap = (std::max)(
        (std::min)(state.totalElapsed, dropDuration)
            - (std::min)(previousElapsed, dropDuration),
        0.0f);
    const float ignitionT0 = std::clamp(
        (previousElapsed - holdEnd) / rampDuration, 0.0f, 1.0f);
    const float ignitionT1 = std::clamp(
        (state.totalElapsed - holdEnd) / rampDuration, 0.0f, 1.0f);
    const float ignitionTravelTime = rampDuration
        * (SmoothStepIntegral(ignitionT1)
            - SmoothStepIntegral(ignitionT0));
    const float cruiseOverlap = (std::max)(
        state.totalElapsed - (std::max)(previousElapsed, ignitionEnd),
        0.0f);
    Vector3 frameDisplacement = Scale(
        ejectionDownDirection,
        (state.ejectionDropDistance / dropDuration) * dropOverlap);
    frameDisplacement = Add(
        frameDisplacement,
        Scale(
            flightDirection,
            state.baseBulletSpeed
                * (ignitionTravelTime + cruiseOverlap)));
    Vector3 movementVelocity = safeDeltaTime > kMinimumVectorLength
        ? Scale(frameDisplacement, 1.0f / safeDeltaTime)
        : endVelocity;

    if (previousPhase == LockedWingLaunchPhase::EjectionDrop
        && state.phase != LockedWingLaunchPhase::EjectionDrop) {
        ++lockedWingPreIgnitionHoldStartCount_;
    }
    if (static_cast<uint8_t>(previousPhase)
            < static_cast<uint8_t>(LockedWingLaunchPhase::IgnitionRamp)
        && static_cast<uint8_t>(state.phase)
            >= static_cast<uint8_t>(LockedWingLaunchPhase::IgnitionRamp)) {
        ++lockedWingIgnitionStartCount_;
    }
    if (previousPhase != LockedWingLaunchPhase::Cruise
        && state.phase == LockedWingLaunchPhase::Cruise) {
        ++lockedWingCruiseTransitionCount_;
    }

    const bool shouldIgnite =
        state.phase == LockedWingLaunchPhase::IgnitionRamp
        || state.phase == LockedWingLaunchPhase::Cruise;
    if (shouldIgnite && !state.ignitionStarted) {
        state.ignitionStarted = true;
        if (lockedWingMissileExhaustController_) {
            state.exhaustHandle = lockedWingMissileExhaustController_->Start(
                state.exhaustHandle,
                bullet.GetPosition(),
                flightDirection);
        }
        state.exhaustEnabled = state.exhaustHandle != 0;
    }

    if (!IsFinite(movementVelocity) || !IsFinite(endVelocity)) {
        ++lockedWingNonFiniteVelocityCount_;
        movementVelocity = {};
        endVelocity = {};
    }
    state.currentSpeedRate = speedRate;
    state.homingReady = state.phase == LockedWingLaunchPhase::Cruise;
    state.homingEnabled = false;
    bullet.SetVelocity(movementVelocity);
    bullet.SetVisualForwardOverride(flightDirection);

    if (state.sequence == lastLockedWingShotSequence_) {
        lastLockedWingLaunchPhase_ = state.phase;
        hasLastLockedWingLaunchPhase_ = true;
        lastLockedWingLaunchElapsed_ = state.totalElapsed;
        lastLockedWingSpeedRate_ = speedRate;
        lastLockedWingCurrentSpeed_ = Length(endVelocity);
        lastLockedWingLaunchDirection_ = flightDirection;
        lastLockedWingEjectionDownDirection_ = ejectionDownDirection;
        lastLockedWingRelativeVelocity_ = endVelocity;
        lastLockedWingExhaustEnabled_ = state.exhaustEnabled;
        lastLockedWingIgnitionStarted_ = state.ignitionStarted;
        lastLockedWingHomingReady_ = state.homingReady;
        lastLockedWingHomingEnabled_ = false;
        switch (state.phase) {
        case LockedWingLaunchPhase::EjectionDrop:
            lastLockedWingStatus_ = "翼下から下方向へ分離中です";
            break;
        case LockedWingLaunchPhase::PreIgnitionHold:
            lastLockedWingStatus_ = "Rail基準で点火前待機中です";
            break;
        case LockedWingLaunchPhase::IgnitionRamp:
            lastLockedWingStatus_ = "噴射炎を点火し、加速中です";
            break;
        case LockedWingLaunchPhase::Cruise:
            lastLockedWingStatus_ = "噴射炎を維持して直進巡航中です";
            break;
        }
    }
}

void PlayerBulletManager::UpdateLockedWingMissileExhaust(
    PlayerBulletInstance& instance) {
    if (instance.projectileType != PlayerProjectileType::LockedWingShot
        || !instance.lockedWingLaunch) {
        return;
    }
    LockedWingLaunchState& state = *instance.lockedWingLaunch;
    if (!lockedWingMissileExhaustController_ || !instance.bullet
        || instance.bullet->IsDead()) {
        if (state.exhaustHandle != 0 && lockedWingMissileExhaustController_) {
            lockedWingMissileExhaustController_->Stop(state.exhaustHandle);
        }
        state.exhaustHandle = 0;
        state.exhaustEnabled = false;
        if (state.sequence == lastLockedWingShotSequence_) {
            lastLockedWingExhaustEnabled_ = false;
        }
        return;
    }
    Vector3 phaseEndVelocity{};
    if (state.phase == LockedWingLaunchPhase::EjectionDrop) {
        const float dropSpeed = state.ejectionDropDistance
            / (std::max)(state.ejectionDropDuration, 0.001f);
        phaseEndVelocity =
            Scale(state.currentEjectionDownDirection, dropSpeed);
    } else if (state.phase == LockedWingLaunchPhase::IgnitionRamp
        || state.phase == LockedWingLaunchPhase::Cruise) {
        phaseEndVelocity = Scale(
            state.currentFlightDirection,
            state.baseBulletSpeed * state.currentSpeedRate);
    }
    instance.bullet->SetVelocity(phaseEndVelocity);
    if (state.sequence == lastLockedWingShotSequence_) {
        lastLockedWingCurrentSpeed_ = Length(phaseEndVelocity);
        lastLockedWingRelativeVelocity_ = phaseEndVelocity;
    }
    if (!state.exhaustEnabled || state.exhaustHandle == 0) {
        return;
    }
    if (!lockedWingMissileExhaustController_->UpdateMissile(
            state.exhaustHandle,
            instance.bullet->GetPosition(),
            state.currentFlightDirection)) {
        state.exhaustHandle = 0;
        state.exhaustEnabled = false;
        if (state.sequence == lastLockedWingShotSequence_) {
            lastLockedWingExhaustEnabled_ = false;
        }
    }
}

void PlayerBulletManager::DrawLockedWingMissileIgnitionImGui() {
#ifdef USE_IMGUI
    ImGui::SeparatorText("ミサイル分離・待機・点火");
    ImGui::DragFloat(
        "下方分離時間##MissileDropDuration",
        &lockedWingEjectionDropDuration_,
        0.005f,
        0.01f,
        0.30f,
        "%.3f 秒");
    ImGui::DragFloat(
        "下方分離距離##MissileDropDistance",
        &lockedWingEjectionDropDistance_,
        0.01f,
        0.01f,
        2.0f,
        "%.2f");
    ImGui::DragFloat(
        "点火前待機時間##MissilePreIgnitionHold",
        &lockedWingPreIgnitionHoldDuration_,
        0.01f,
        0.15f,
        0.50f,
        "%.2f 秒");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "翼下から少し下へ分離した後、\n"
            "ミサイルエンジンを点火するまで待つ時間です。");
    }
    ImGui::DragFloat(
        "点火加速時間##MissileIgnitionRamp",
        &lockedWingIgnitionRampDuration_,
        0.005f,
        0.01f,
        0.50f,
        "%.3f 秒");
    const float ignitionStart =
        lockedWingEjectionDropDuration_
        + lockedWingPreIgnitionHoldDuration_;
    ImGui::Text("点火開始までの合計時間: %.3f 秒", ignitionStart);
    ImGui::Text(
        "通常速度到達までの合計時間: %.3f 秒",
        ignitionStart + lockedWingIgnitionRampDuration_);
    if (ImGui::Button(
            "分離・待機・点火を推奨値へ戻す##ResetMissileIgnitionSettings")) {
        lockedWingEjectionDropDuration_ = kRecommendedDropDuration;
        lockedWingEjectionDropDistance_ = kRecommendedDropDistance;
        lockedWingPreIgnitionHoldDuration_ = kRecommendedHoldDuration;
        lockedWingIgnitionRampDuration_ =
            kRecommendedIgnitionRampDuration;
    }

    ImGui::SeparatorText("直近ミサイルの状態");
    ImGui::Text(
        "現在フェーズ: %s",
        hasLastLockedWingLaunchPhase_
            ? ToJapanesePhase(lastLockedWingLaunchPhase_)
            : "なし");
    ImGui::Text("経過時間: %.3f 秒", lastLockedWingLaunchElapsed_);
    ImGui::Text("現在速度倍率: %.3f", lastLockedWingSpeedRate_);
    ImGui::Text("現在速度: %.3f", lastLockedWingCurrentSpeed_);
    ImGui::Text(
        "Flight Direction: %.3f, %.3f, %.3f",
        lastLockedWingLaunchDirection_.x,
        lastLockedWingLaunchDirection_.y,
        lastLockedWingLaunchDirection_.z);
    ImGui::Text(
        "Ejection Down Direction: %.3f, %.3f, %.3f",
        lastLockedWingEjectionDownDirection_.x,
        lastLockedWingEjectionDownDirection_.y,
        lastLockedWingEjectionDownDirection_.z);
    ImGui::Text(
        "Exhaust有効: %s",
        ToJapaneseBool(lastLockedWingExhaustEnabled_));
    ImGui::Text(
        "点火開始済み: %s",
        ToJapaneseBool(lastLockedWingIgnitionStarted_));
    ImGui::Text(
        "Homing Ready: %s",
        ToJapaneseBool(lastLockedWingHomingReady_));
    ImGui::Text(
        "Homing Enabled: %s",
        ToJapaneseBool(lastLockedWingHomingEnabled_));
    ImGui::Text("Target方向を未使用: はい");
    ImGui::Text("Rail Speedを未加算: はい");
    ImGui::Text("Camera Velocityを未加算: はい");

    ImGui::SeparatorText("分離・待機・点火の統計");
    ImGui::Text("下方分離開始回数: %zu", lockedWingEjectionDropStartCount_);
    ImGui::Text(
        "待機開始回数: %zu",
        lockedWingPreIgnitionHoldStartCount_);
    ImGui::Text("点火開始回数: %zu", lockedWingIgnitionStartCount_);
    ImGui::Text("巡航移行回数: %zu", lockedWingCruiseTransitionCount_);
    ImGui::Text(
        "不正Direction Fallback回数: %zu",
        lockedWingDirectionFallbackCount_);
    ImGui::Text(
        "非有限Velocity検出数: %zu",
        lockedWingNonFiniteVelocityCount_);
#endif
}