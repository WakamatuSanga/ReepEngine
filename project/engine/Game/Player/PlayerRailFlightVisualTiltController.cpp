#include "PlayerRailFlightVisualTiltController.h"

#include "Engine/Game/Camera/RailShooterCameraRig.h"
#include "Player.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kRadiansToDegrees = 180.0f / kPi;
constexpr float kDegreesToRadians = kPi / 180.0f;
constexpr float kMinimumDirectionLengthSquared = 0.0000001f;

float Dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 Cross(const Vector3& a, const Vector3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

Vector3 Subtract(const Vector3& a, const Vector3& b) {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

Vector3 Scale(const Vector3& value, float scale) {
    return { value.x * scale, value.y * scale, value.z * scale };
}

float LengthSquared(const Vector3& value) {
    return Dot(value, value);
}

bool IsFinite(const Vector3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool IsUsableDirection(const Vector3& value) {
    return IsFinite(value) && LengthSquared(value) > kMinimumDirectionLengthSquared;
}

Vector3 Normalize(const Vector3& value, const Vector3& fallback) {
    if (!IsUsableDirection(value)) return fallback;
    const float inverseLength = 1.0f / std::sqrt(LengthSquared(value));
    return Scale(value, inverseLength);
}

float ComputePitchDegrees(const Vector3& forward) {
    const Vector3 safe = Normalize(forward, { 0.0f, 0.0f, 1.0f });
    const float horizontal = std::sqrt(safe.x * safe.x + safe.z * safe.z);
    return std::atan2(-safe.y, horizontal) * kRadiansToDegrees;
}
}

PlayerRailFlightVisualTiltController::PlayerRailFlightVisualTiltController() = default;
PlayerRailFlightVisualTiltController::~PlayerRailFlightVisualTiltController() = default;

void PlayerRailFlightVisualTiltController::Initialize(
    Player* player,
    RailShooterCameraRig* railCameraRig) {
    if (initialized_) Finalize();

    player_ = player;
    railCameraRig_ = railCameraRig;
    ApplyRecommendedSettings();
    ResetBankState(true);
    hasRailIdentity_ = false;
    previousGameModeActive_ = false;
    previousPlayerAlive_ = true;
    initialized_ = player_ && railCameraRig_;
    bankDisabledReason_ = initialized_ ? "なし" : "必要な参照がありません";
    if (player_) player_->SetRailFlightVisualBankRadians(0.0f);
}

void PlayerRailFlightVisualTiltController::Finalize() {
    if (player_) player_->SetRailFlightVisualBankRadians(0.0f);
    ResetBankState(true);
    hasRailIdentity_ = false;
    initialized_ = false;
    player_ = nullptr;
    railCameraRig_ = nullptr;
    bankDisabledReason_ = "終了済み";
}

void PlayerRailFlightVisualTiltController::Update(
    float unscaledDeltaTime,
    bool gameModeActive,
    bool playerAlive) {
    gameModeActive_ = gameModeActive;
    playerAlive_ = playerAlive;
    playerEnabled_ = player_ && player_->IsEnabled();

    const bool gameModeStarted = gameModeActive_ && !previousGameModeActive_;
    const bool debugModeStarted = !gameModeActive_ && previousGameModeActive_;
    const bool playerRespawned = playerAlive_ && !previousPlayerAlive_;
    if (gameModeStarted || playerRespawned) {
        ResetBankState(true);
    }
    if (debugModeStarted) {
        forcedBankMode_ = ForcedBankMode::None;
    }

    ClampSettings();
    targetRailBankDegrees_ = 0.0f;
    CalculateRailBankTarget();
    SmoothRailBank(unscaledDeltaTime);

    if (player_) {
        player_->SetRailFlightVisualBankRadians(currentRailBankDegrees_ * kDegreesToRadians);
        if (std::fabs(currentRailBankDegrees_) > railBankSnapEpsilonDegrees_) {
            ++bankApplyCount_;
        }
    }

    UpdatePlayerDiagnostics();
    previousGameModeActive_ = gameModeActive_;
    previousPlayerAlive_ = playerAlive_;
}

void PlayerRailFlightVisualTiltController::Reset() {
    ResetBankState(true);
    hasRailIdentity_ = false;
    if (player_) player_->SetRailFlightVisualBankRadians(0.0f);
    bankDisabledReason_ = initialized_ ? "状態をリセットしました" : "未初期化";
}

bool PlayerRailFlightVisualTiltController::CalculateRailBankTarget() {
    railRunning_ = false;
    railPoseValid_ = false;
    runtimeV2Active_ = false;
    horizontalForwardValid_ = false;
    curveDirection_ = CurveDirection::Straight;
    signedTurnDegrees_ = 0.0f;

    if (!initialized_ || !player_ || !railCameraRig_) {
        bankDisabledReason_ = "必要な参照がありません";
        return false;
    }

    const RailShooterCameraRig::RailFlightPoseSnapshot pose =
        railCameraRig_->GetRailFlightPoseSnapshot(railBankLookAheadDistance_);
    currentRailForward_ = pose.currentForward;
    aheadRailForward_ = pose.aheadForward;
    railUp_ = pose.up;
    railDistance_ = pose.railDistance;
    currentRailRevision_ = pose.railRevision;
    currentRailIndex_ = pose.railIndex;
    railRunning_ = pose.running;
    railPoseValid_ = pose.valid;
    runtimeV2Active_ = pose.runtimeV2Active;

    if (hasRailIdentity_ &&
        (currentRailRevision_ != previousRailRevision_ ||
         currentRailIndex_ != previousRailIndex_)) {
        ResetBankState(true);
    }
    previousRailRevision_ = currentRailRevision_;
    previousRailIndex_ = currentRailIndex_;
    hasRailIdentity_ = true;

    if (!railBankEnabled_) {
        bankDisabledReason_ = "レールカーブBankが無効です";
        return false;
    }
    if (forcedBankMode_ != ForcedBankMode::None) {
        if (!playerAlive_) {
            bankDisabledReason_ = "死亡演出またはGameOver中です";
            return false;
        }
        if (!playerEnabled_) {
            bankDisabledReason_ = "Playerが無効です";
            return false;
        }

        switch (forcedBankMode_) {
        case ForcedBankMode::Right:
            targetRailBankDegrees_ = -maxRailBankDegrees_;
            curveDirection_ = CurveDirection::Right;
            break;
        case ForcedBankMode::Left:
            targetRailBankDegrees_ = maxRailBankDegrees_;
            curveDirection_ = CurveDirection::Left;
            break;
        case ForcedBankMode::Zero:
            targetRailBankDegrees_ = 0.0f;
            curveDirection_ = CurveDirection::Straight;
            break;
        case ForcedBankMode::None:
        default:
            break;
        }
        bankDisabledReason_ = "テスト操作でBankを強制中です";
        return true;
    }
    if (!gameModeActive_) {
        bankDisabledReason_ = "DebugModeでは適用しません";
        return false;
    }
    if (!playerAlive_) {
        bankDisabledReason_ = "死亡演出またはGameOver中です";
        return false;
    }
    if (!playerEnabled_) {
        bankDisabledReason_ = "Playerが無効です";
        return false;
    }
    if (!railRunning_) {
        bankDisabledReason_ = "Railが走行中ではありません";
        return false;
    }
    if (!railPoseValid_) {
        bankDisabledReason_ = "Runtime V2 / Legacy Rail Poseが無効です";
        return false;
    }

    const Vector3 safeUp = Normalize(railUp_, { 0.0f, 1.0f, 0.0f });
    Vector3 currentHorizontal = Subtract(
        currentRailForward_,
        Scale(safeUp, Dot(currentRailForward_, safeUp)));
    Vector3 aheadHorizontal = Subtract(
        aheadRailForward_,
        Scale(safeUp, Dot(aheadRailForward_, safeUp)));
    if (!IsUsableDirection(currentHorizontal) || !IsUsableDirection(aheadHorizontal)) {
        bankDisabledReason_ = "水平Rail Forwardを正規化できません";
        return false;
    }

    currentHorizontal = Normalize(currentHorizontal, { 0.0f, 0.0f, 1.0f });
    aheadHorizontal = Normalize(aheadHorizontal, currentHorizontal);
    horizontalForwardValid_ = true;
    const float sinAngle = Dot(Cross(currentHorizontal, aheadHorizontal), safeUp);
    const float cosAngle = std::clamp(Dot(currentHorizontal, aheadHorizontal), -1.0f, 1.0f);
    const float signedTurnRadians = std::atan2(sinAngle, cosAngle);
    if (!std::isfinite(signedTurnRadians)) {
        horizontalForwardValid_ = false;
        bankDisabledReason_ = "Signed Turn Angleが有限値ではありません";
        return false;
    }

    signedTurnDegrees_ = signedTurnRadians * kRadiansToDegrees;
    if (signedTurnDegrees_ > railBankSnapEpsilonDegrees_) {
        curveDirection_ = CurveDirection::Right;
    } else if (signedTurnDegrees_ < -railBankSnapEpsilonDegrees_) {
        curveDirection_ = CurveDirection::Left;
    }

    targetRailBankDegrees_ = std::clamp(
        -signedTurnDegrees_ * railBankGain_,
        -maxRailBankDegrees_,
        maxRailBankDegrees_);

    if (std::fabs(targetRailBankDegrees_) <= railBankSnapEpsilonDegrees_) {
        targetRailBankDegrees_ = 0.0f;
    }

    bankDisabledReason_ = "なし";
    return true;
}

void PlayerRailFlightVisualTiltController::SmoothRailBank(float unscaledDeltaTime) {
    if (!std::isfinite(targetRailBankDegrees_)) targetRailBankDegrees_ = 0.0f;
    const float safeDeltaTime = std::clamp(unscaledDeltaTime, 0.0f, 0.25f);
    const float responseTime =
        std::fabs(targetRailBankDegrees_) > railBankSnapEpsilonDegrees_
        ? railBankResponseTime_
        : railBankReturnTime_;
    const float alpha = 1.0f - std::exp(
        -safeDeltaTime / (std::max)(responseTime, 0.001f));
    currentRailBankDegrees_ +=
        (targetRailBankDegrees_ - currentRailBankDegrees_) * alpha;

    if (!std::isfinite(currentRailBankDegrees_)) currentRailBankDegrees_ = 0.0f;
    if (std::fabs(currentRailBankDegrees_ - targetRailBankDegrees_) <=
        railBankSnapEpsilonDegrees_) {
        currentRailBankDegrees_ = targetRailBankDegrees_;
    }
    if (targetRailBankDegrees_ == 0.0f &&
        std::fabs(currentRailBankDegrees_) <= railBankSnapEpsilonDegrees_) {
        currentRailBankDegrees_ = 0.0f;
    }
    visualTiltApplying_ =
        playerEnabled_ &&
        std::fabs(currentRailBankDegrees_) > railBankSnapEpsilonDegrees_;
}

void PlayerRailFlightVisualTiltController::ApplyRecommendedSettings() {
    railBankEnabled_ = true;
    railBankLookAheadDistance_ = 10.0f;
    railBankGain_ = 1.20f;
    maxRailBankDegrees_ = 15.0f;
    railBankResponseTime_ = 0.18f;
    railBankReturnTime_ = 0.28f;
    railBankSnapEpsilonDegrees_ = 0.05f;
}

void PlayerRailFlightVisualTiltController::ClampSettings() {
    railBankLookAheadDistance_ = std::clamp(railBankLookAheadDistance_, 0.1f, 100.0f);
    railBankGain_ = std::clamp(railBankGain_, 0.0f, 3.0f);
    maxRailBankDegrees_ = std::clamp(maxRailBankDegrees_, 0.0f, 30.0f);
    railBankResponseTime_ = std::clamp(railBankResponseTime_, 0.01f, 1.0f);
    railBankReturnTime_ = std::clamp(railBankReturnTime_, 0.01f, 1.5f);
    railBankSnapEpsilonDegrees_ = std::clamp(railBankSnapEpsilonDegrees_, 0.0f, 5.0f);
}

void PlayerRailFlightVisualTiltController::ResetBankState(bool clearForcedState) {
    signedTurnDegrees_ = 0.0f;
    targetRailBankDegrees_ = 0.0f;
    currentRailBankDegrees_ = 0.0f;
    horizontalForwardValid_ = false;
    visualTiltApplying_ = false;
    curveDirection_ = CurveDirection::Straight;
    if (clearForcedState) forcedBankMode_ = ForcedBankMode::None;
}

void PlayerRailFlightVisualTiltController::UpdatePlayerDiagnostics() {
    if (!player_) {
        playerBaseForward_ = { 0.0f, 0.0f, 1.0f };
        playerDisplayForward_ = playerBaseForward_;
        playerVisualBaseRotation_ = {};
        playerActionRotation_ = {};
        playerFinalRotation_ = {};
        playerBasePitchDegrees_ = 0.0f;
        playerFinalRollDegrees_ = 0.0f;
        visualRotationFinite_ = true;
        actionRotationActive_ = false;
        return;
    }

    playerBaseForward_ = player_->GetBaseForward();
    playerDisplayForward_ = player_->GetVisualDisplayForward();
    playerVisualBaseRotation_ = player_->GetVisualBaseRotation();
    playerActionRotation_ = player_->GetActionVisualRotationOffset();
    playerFinalRotation_ = player_->GetVisualModelRotation();
    playerBasePitchDegrees_ = ComputePitchDegrees(playerBaseForward_);
    playerFinalRollDegrees_ = playerFinalRotation_.z * kRadiansToDegrees;
    visualRotationFinite_ = IsFinite(playerFinalRotation_);
    actionRotationActive_ = LengthSquared(playerActionRotation_) > kMinimumDirectionLengthSquared;
}
