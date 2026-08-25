#include "PlayerBulletManager.h"
#include "PlayerBulletManagerLockedWingHomingMath.h"

#include "Engine/Game/Enemy/EnemyBullet.h"
#include "Engine/Game/Targeting/AimCorridorTargetingController.h"
#include "Engine/Game/Targeting/PlayerLockOnTargetProvider.h"

#include <algorithm>
#include <cmath>
#include <string>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kRadiansToDegrees = 180.0f / kPi;
constexpr float kDegreesToRadians = kPi / 180.0f;
constexpr float kRecommendedTurnRateDegreesPerSecond = 360.0f;
constexpr float kRecommendedMinimumTargetDistance = 0.05f;

Vector3 Subtract(const Vector3& lhs, const Vector3& rhs) {
    return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
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

const char* ToJapaneseBool(bool value) {
    return value ? "はい" : "いいえ";
}

const char* ToJapaneseWing(PlayerBulletManager::WingSide wing) {
    return wing == PlayerBulletManager::WingSide::Left ? "左翼" : "右翼";
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

const char* ToJapaneseLockState(
    AimCorridorTargetingController::AimLockState state) {
    switch (state) {
    case AimCorridorTargetingController::AimLockState::Candidate:
        return "候補";
    case AimCorridorTargetingController::AimLockState::Acquiring:
        return "取得中";
    case AimCorridorTargetingController::AimLockState::Locked:
        return "ロック完了";
    case AimCorridorTargetingController::AimLockState::None:
    default:
        return "対象なし";
    }
}

const char* ToJapaneseTargetKind(PlayerLockOnTargetKind kind) {
    switch (kind) {
    case PlayerLockOnTargetKind::KrakenWeakPoint:
        return "クラーケン触手の弱点";
    case PlayerLockOnTargetKind::NormalEnemy:
    default:
        return "通常敵";
    }
}

const char* ToJapaneseTargetSource(PlayerLockOnTargetKind kind) {
    return kind == PlayerLockOnTargetKind::KrakenWeakPoint
        ? "クラーケン実行"
        : "通常敵管理";
}
} // namespace

void PlayerBulletManager::UpdateLockedWingHoming(
    PlayerBulletInstance& instance,
    float cruiseDeltaTime) {
    if (instance.projectileType != PlayerProjectileType::LockedWingShot
        || !instance.lockedWingLaunch || !instance.bullet) {
        return;
    }

    LockedWingLaunchState& state = *instance.lockedWingLaunch;
    state.homingEnabled = false;
    state.targetQuerySucceeded = false;
    state.targetAlive = false;
    state.targetDistance = -1.0f;
    state.desiredDirection = state.currentFlightDirection;
    state.directionDot = 1.0f;
    state.angleErrorDegrees = 0.0f;
    state.frameTurnDegrees = 0.0f;
    state.maximumFrameTurnDegrees = 0.0f;
    if (state.phase != LockedWingLaunchPhase::Cruise) {
        return;
    }

    auto markTargetLost = [&](LockedWingTargetLostReason reason) {
        state.targetLost = true;
        state.targetLostReason = reason;
        state.homingEnabled = false;
        if (!state.targetLostStraightRecorded) {
            state.targetLostStraightRecorded = true;
            ++lockedWingTargetLostStraightCount_;
        }
    };

    if (state.targetLost) {
        return;
    }
    if (state.lockedTargetId.empty()) {
        ++lockedWingTargetQueryFailureCount_;
        markTargetLost(LockedWingTargetLostReason::MissingTargetId);
        return;
    }
    if (!targetProvider_) {
        ++lockedWingTargetQueryFailureCount_;
        markTargetLost(
            LockedWingTargetLostReason::TargetProviderUnavailable);
        return;
    }

    PlayerLockOnTargetSnapshot targetSnapshot{};
    if (!targetProvider_->TryGetTargetSnapshot(
            state.lockedTargetId, targetSnapshot)) {
        ++lockedWingTargetQueryFailureCount_;
        ++lockedWingTargetRemovalCount_;
        markTargetLost(LockedWingTargetLostReason::TargetRemoved);
        return;
    }

    state.targetWorldPosition = targetSnapshot.worldPosition;
    state.targetAlive = targetSnapshot.alive;
    if (!IsFinite(targetSnapshot.worldPosition)) {
        ++lockedWingTargetQueryFailureCount_;
        ++lockedWingNonFiniteTargetCount_;
        markTargetLost(LockedWingTargetLostReason::NonFiniteTarget);
        return;
    }
    if (!targetSnapshot.alive) {
        ++lockedWingTargetQueryFailureCount_;
        ++lockedWingTargetDeathCount_;
        markTargetLost(LockedWingTargetLostReason::TargetDead);
        return;
    }
    if (!targetSnapshot.valid) {
        ++lockedWingTargetQueryFailureCount_;
        ++lockedWingTargetRemovalCount_;
        markTargetLost(LockedWingTargetLostReason::TargetInactive);
        return;
    }

    state.targetQuerySucceeded = true;
    ++lockedWingTargetQuerySuccessCount_;
    const Vector3 toTarget = Subtract(
        targetSnapshot.worldPosition, instance.bullet->GetPosition());
    const float targetDistance = Length(toTarget);
    if (!IsFinite(toTarget) || !std::isfinite(targetDistance)) {
        ++lockedWingNonFiniteDirectionCount_;
        markTargetLost(LockedWingTargetLostReason::NonFiniteDirection);
        return;
    }
    state.targetDistance = targetDistance;

    const float minimumDistance = std::clamp(
        lockedWingHomingMinimumTargetDistance_, 0.001f, 10.0f);
    if (targetDistance <= minimumDistance) {
        state.homingEnabled = lockedWingHomingEnabled_;
        if (state.homingEnabled) {
            if (!state.homingStarted) {
                state.homingStarted = true;
                ++lockedWingHomingStartCount_;
            }
            ++lockedWingHomingUpdateCount_;
        }
        return;
    }

    state.desiredDirection = Scale(toTarget, 1.0f / targetDistance);
    if (!lockedWingHomingEnabled_) {
        return;
    }

    const float safeCruiseDeltaTime = std::isfinite(cruiseDeltaTime)
        ? (std::max)(cruiseDeltaTime, 0.0f)
        : 0.0f;
    const float turnRate = std::clamp(
        lockedWingHomingTurnRateDegreesPerSecond_, 30.0f, 1080.0f);
    const float maximumTurnRadians =
        turnRate * kDegreesToRadians * safeCruiseDeltaTime;
    state.maximumFrameTurnDegrees =
        maximumTurnRadians * kRadiansToDegrees;

    const PlayerLockedWingHomingMath::RotateDirectionResult rotateResult =
        PlayerLockedWingHomingMath::RotateDirectionTowards(
            state.currentFlightDirection,
            state.desiredDirection,
            maximumTurnRadians,
            Scale(state.currentEjectionDownDirection, -1.0f));
    state.directionDot = rotateResult.directionDot;
    state.angleErrorDegrees =
        rotateResult.angleRadians * kRadiansToDegrees;
    state.frameTurnDegrees =
        rotateResult.appliedTurnRadians * kRadiansToDegrees;
    if (rotateResult.status
        != PlayerLockedWingHomingMath::RotateDirectionStatus::Success) {
        if (!IsFinite(state.currentFlightDirection)
            || !IsFinite(state.desiredDirection)) {
            ++lockedWingNonFiniteDirectionCount_;
        }
        return;
    }

    state.currentFlightDirection = rotateResult.direction;
    state.homingEnabled = true;
    if (!state.homingStarted) {
        state.homingStarted = true;
        ++lockedWingHomingStartCount_;
    }
    ++lockedWingHomingUpdateCount_;
    if (rotateResult.usedOppositeFallback) {
        ++lockedWingOppositeFallbackCount_;
    }
    if (rotateResult.clampedByMaximumTurn) {
        ++lockedWingMaximumTurnClampCount_;
    }
}

void PlayerBulletManager::RecordLockedWingHomingHit(
    const PlayerBulletInstance& instance) {
    if (instance.projectileType == PlayerProjectileType::LockedWingShot
        && instance.lockedWingLaunch
        && instance.lockedWingLaunch->homingEnabled) {
        ++lockedWingHomingHitCount_;
    }
}

void PlayerBulletManager::ResetLockedWingHomingDiagnostics(
    bool resetStatistics) {
    lockOnTargetProviderDiagnostics_ = {};
    for (PlayerBulletInstance& instance : bullets_) {
        if (!instance.lockedWingLaunch) {
            continue;
        }
        LockedWingLaunchState& state = *instance.lockedWingLaunch;
        state.targetQuerySucceeded = false;
        state.targetAlive = false;
        state.targetWorldPosition = {};
        state.targetDistance = -1.0f;
        state.desiredDirection = state.currentFlightDirection;
        state.directionDot = 1.0f;
        state.angleErrorDegrees = 0.0f;
        state.frameTurnDegrees = 0.0f;
        state.maximumFrameTurnDegrees = 0.0f;
        state.exhaustFollowingDirection = false;
    }
    if (!resetStatistics) {
        return;
    }
    lockedWingHomingStartCount_ = 0;
    lockedWingHomingUpdateCount_ = 0;
    lockedWingTargetQuerySuccessCount_ = 0;
    lockedWingTargetQueryFailureCount_ = 0;
    lockedWingTargetDeathCount_ = 0;
    lockedWingTargetRemovalCount_ = 0;
    lockedWingNonFiniteTargetCount_ = 0;
    lockedWingNonFiniteDirectionCount_ = 0;
    lockedWingOppositeFallbackCount_ = 0;
    lockedWingMaximumTurnClampCount_ = 0;
    lockedWingHomingHitCount_ = 0;
    lockedWingTargetLostStraightCount_ = 0;
    lockedWingTargetSwitchCount_ = 0;
    normalShotHomingApplyCount_ = 0;
}

void PlayerBulletManager::RefreshLockOnTargetProviderDiagnostics() {
    lockOnTargetProviderDiagnostics_ = {};
    if (!targetProvider_ || !targetProvider_->IsInitialized() ||
        !aimCorridorTargetingController_ ||
        !aimCorridorTargetingController_->IsUsingTargetProvider(
            targetProvider_)) {
        return;
    }

    lockOnTargetProviderDiagnostics_.targetId =
        aimCorridorTargetingController_->GetLockedTargetId();
    if (lockOnTargetProviderDiagnostics_.targetId.empty()) {
        return;
    }

    PlayerLockOnTargetSnapshot snapshot{};
    lockOnTargetProviderDiagnostics_.querySucceeded =
        aimCorridorTargetingController_->TryGetLockedTargetSnapshot(snapshot) &&
        snapshot.id == lockOnTargetProviderDiagnostics_.targetId;
    if (!lockOnTargetProviderDiagnostics_.querySucceeded) {
        return;
    }

    lockOnTargetProviderDiagnostics_.worldPosition = snapshot.worldPosition;
    lockOnTargetProviderDiagnostics_.kind = snapshot.kind;
    lockOnTargetProviderDiagnostics_.alive = snapshot.alive;
    lockOnTargetProviderDiagnostics_.targetable = snapshot.targetable;
    lockOnTargetProviderDiagnostics_.valid = snapshot.valid;
}

void PlayerBulletManager::DrawLockOnTargetProviderImGui() {
#ifdef USE_IMGUI
    ImGui::SeparatorText("ロックオン対象プロバイダー");
    const bool providerConnected =
        targetProvider_ && targetProvider_->IsInitialized();
    const bool aimTargetingConnected = providerConnected &&
        aimCorridorTargetingController_ &&
        aimCorridorTargetingController_->IsUsingTargetProvider(
            targetProvider_);
    const bool missileHomingConnected = providerConnected;
    const auto lockState = aimCorridorTargetingController_
        ? aimCorridorTargetingController_->GetLockState()
        : AimCorridorTargetingController::AimLockState::None;
    const std::string currentTargetId = aimCorridorTargetingController_
        ? aimCorridorTargetingController_->GetLockedTargetId()
        : std::string{};
    if (!aimTargetingConnected || currentTargetId.empty()) {
        lockOnTargetProviderDiagnostics_ = {};
    } else if (lockOnTargetProviderDiagnostics_.targetId != currentTargetId) {
        RefreshLockOnTargetProviderDiagnostics();
    }

    ImGui::Text(
        "対象プロバイダー接続: %s", ToJapaneseBool(providerConnected));
    ImGui::Text(
        "通常敵管理接続: %s",
        ToJapaneseBool(
            providerConnected && targetProvider_->IsEnemyManagerConnected()));
    ImGui::Text(
        "クラーケン実行接続: %s",
        ToJapaneseBool(
            providerConnected && targetProvider_->IsKrakenRuntimeConnected()));
    ImGui::Text(
        "エイム照準接続: %s", ToJapaneseBool(aimTargetingConnected));
    ImGui::Text(
        "ミサイル追尾接続: %s", ToJapaneseBool(missileHomingConnected));

    ImGui::SeparatorText("現在のロック");
    ImGui::Text("ロック状態: %s", ToJapaneseLockState(lockState));
    ImGui::TextWrapped(
        "ロック対象識別子: %s",
        currentTargetId.empty() ? "なし" : currentTargetId.c_str());
    ImGui::Text(
        "対象種別: %s",
        lockOnTargetProviderDiagnostics_.querySucceeded
            ? ToJapaneseTargetKind(lockOnTargetProviderDiagnostics_.kind)
            : "未取得");
    if (lockOnTargetProviderDiagnostics_.querySucceeded) {
        ImGui::Text(
            "対象ワールド位置: %.3f, %.3f, %.3f",
            lockOnTargetProviderDiagnostics_.worldPosition.x,
            lockOnTargetProviderDiagnostics_.worldPosition.y,
            lockOnTargetProviderDiagnostics_.worldPosition.z);
    } else {
        ImGui::Text("対象ワールド位置: 未取得");
    }
    ImGui::Text(
        "対象生存中: %s",
        ToJapaneseBool(lockOnTargetProviderDiagnostics_.alive));
    ImGui::Text(
        "対象を選択可能: %s",
        ToJapaneseBool(lockOnTargetProviderDiagnostics_.targetable));
    ImGui::Text(
        "対象スナップショット有効: %s",
        ToJapaneseBool(lockOnTargetProviderDiagnostics_.valid));
    ImGui::Text(
        "プロバイダー検索成功: %s",
        ToJapaneseBool(lockOnTargetProviderDiagnostics_.querySucceeded));
    ImGui::Text(
        "対象取得元: %s",
        lockOnTargetProviderDiagnostics_.querySucceeded
            ? ToJapaneseTargetSource(lockOnTargetProviderDiagnostics_.kind)
            : "なし");
    if (ImGui::Button(
            "現在ロック情報を再取得##RefreshCurrentProviderLock")) {
        RefreshLockOnTargetProviderDiagnostics();
    }
#endif
}

void PlayerBulletManager::DrawLockedWingHomingImGui() {
#ifdef USE_IMGUI
    DrawLockOnTargetProviderImGui();
    ImGui::SeparatorText("ロックオンミサイル追尾");
    ImGui::Text(
        "対象プロバイダー接続: %s",
        ToJapaneseBool(targetProvider_ != nullptr));
    ImGui::Checkbox(
        "追尾を有効化##LockedWingHomingEnabled",
        &lockedWingHomingEnabled_);
    ImGui::DragFloat(
        "最大旋回速度##LockedWingHomingTurnRate",
        &lockedWingHomingTurnRateDegreesPerSecond_,
        5.0f,
        30.0f,
        1080.0f,
        "%.1f 度/秒");
    ImGui::Text("追尾開始フェーズ: 巡航固定");
    ImGui::Text("対象接近最小距離: %.3f", lockedWingHomingMinimumTargetDistance_);
    ImGui::Text("対象喪失時: 直進固定");
    ImGui::Text("対象乗り換え: なし");
    ImGui::Text("対象位置予測: なし");
    if (ImGui::Button("追尾を有効化##EnableLockedWingHoming")) {
        lockedWingHomingEnabled_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("追尾を無効化##DisableLockedWingHoming")) {
        lockedWingHomingEnabled_ = false;
    }
    if (ImGui::Button("推奨設定へ戻す##ResetLockedWingHomingSettings")) {
        lockedWingHomingEnabled_ = true;
        lockedWingHomingTurnRateDegreesPerSecond_ =
            kRecommendedTurnRateDegreesPerSecond;
        lockedWingHomingMinimumTargetDistance_ =
            kRecommendedMinimumTargetDistance;
    }
    if (ImGui::Button("追尾診断をリセット##ResetLockedWingHomingDiagnostics")) {
        ResetLockedWingHomingDiagnostics(true);
    }

    ImGui::SeparatorText("現在のミサイル");
    size_t activeMissileCount = 0;
    for (const PlayerBulletInstance& instance : bullets_) {
        if (instance.projectileType != PlayerProjectileType::LockedWingShot
            || !instance.lockedWingLaunch || !instance.bullet
            || instance.bullet->IsDead()) {
            continue;
        }
        ++activeMissileCount;
        const LockedWingLaunchState& state = *instance.lockedWingLaunch;
        const std::string header = "弾実行時ID: "
            + std::to_string(instance.runtimeId)
            + "##LockedWingHomingMissile"
            + std::to_string(instance.runtimeId);
        if (!ImGui::TreeNode(header.c_str())) {
            continue;
        }

        const auto targetLostReasonLabel = [&]() -> const char* {
            switch (state.targetLostReason) {
            case LockedWingTargetLostReason::MissingTargetId:
                return "対象IDなし";
            case LockedWingTargetLostReason::TargetProviderUnavailable:
                return "対象プロバイダーが未接続";
            case LockedWingTargetLostReason::TargetRemoved:
                return "対象が削除済み";
            case LockedWingTargetLostReason::TargetDead:
                return "対象が死亡";
            case LockedWingTargetLostReason::TargetInactive:
                return "対象が非アクティブ";
            case LockedWingTargetLostReason::NonFiniteTarget:
                return "対象位置が非有限値";
            case LockedWingTargetLostReason::NonFiniteDirection:
                return "対象への方向が非有限値";
            case LockedWingTargetLostReason::None:
            default:
                return "なし";
            }
        };
        float phaseElapsed = state.totalElapsed;
        if (state.phase != LockedWingLaunchPhase::EjectionDrop) {
            phaseElapsed -= state.ejectionDropDuration;
        }
        if (state.phase == LockedWingLaunchPhase::IgnitionRamp
            || state.phase == LockedWingLaunchPhase::Cruise) {
            phaseElapsed -= state.preIgnitionHoldDuration;
        }
        if (state.phase == LockedWingLaunchPhase::Cruise) {
            phaseElapsed -= state.ignitionRampDuration;
        }
        phaseElapsed = (std::max)(phaseElapsed, 0.0f);

        ImGui::Text("発射翼: %s", ToJapaneseWing(state.launchWing));
        ImGui::Text("発射フェーズ: %s", ToJapanesePhase(state.phase));
        ImGui::Text("フェーズ経過時間: %.3f 秒", phaseElapsed);
        ImGui::Text("追尾準備完了: %s", ToJapaneseBool(state.homingReady));
        ImGui::Text("追尾有効: %s", ToJapaneseBool(state.homingEnabled));
        ImGui::TextWrapped(
            "対象ID: %s",
            state.lockedTargetId.empty() ? "なし" : state.lockedTargetId.c_str());
        ImGui::Text(
            "対象取得成功: %s",
            ToJapaneseBool(state.targetQuerySucceeded));
        ImGui::Text("対象生存中: %s", ToJapaneseBool(state.targetAlive));
        ImGui::Text(
            "対象ワールド位置: %.3f, %.3f, %.3f",
            state.targetWorldPosition.x,
            state.targetWorldPosition.y,
            state.targetWorldPosition.z);
        const Vector3& projectilePosition = instance.bullet->GetPosition();
        ImGui::Text(
            "弾ワールド位置: %.3f, %.3f, %.3f",
            projectilePosition.x,
            projectilePosition.y,
            projectilePosition.z);
        if (state.targetDistance >= 0.0f) {
            ImGui::Text("対象までの距離: %.3f", state.targetDistance);
        } else {
            ImGui::Text("対象までの距離: 未取得");
        }
        ImGui::Text(
            "現在飛行方向: %.3f, %.3f, %.3f",
            state.currentFlightDirection.x,
            state.currentFlightDirection.y,
            state.currentFlightDirection.z);
        ImGui::Text(
            "希望方向: %.3f, %.3f, %.3f",
            state.desiredDirection.x,
            state.desiredDirection.y,
            state.desiredDirection.z);
        ImGui::Text("方向内積: %.6f", state.directionDot);
        ImGui::Text("角度誤差: %.3f 度", state.angleErrorDegrees);
        ImGui::Text("今フレーム旋回角: %.3f 度", state.frameTurnDegrees);
        ImGui::Text(
            "最大旋回角: %.3f 度",
            state.maximumFrameTurnDegrees);
        ImGui::Text("現在速度: %.3f", Length(instance.bullet->GetVelocity()));
        ImGui::Text("対象喪失: %s", ToJapaneseBool(state.targetLost));
        ImGui::Text("対象喪失理由: %s", targetLostReasonLabel());
        ImGui::Text(
            "排気方向追従: %s",
            ToJapaneseBool(state.exhaustFollowingDirection));
        ImGui::TreePop();
    }
    if (activeMissileCount == 0) {
        ImGui::TextDisabled("現在有効なロックオンミサイルはありません");
    }

    ImGui::SeparatorText("追尾統計");
    ImGui::Text("ロック翼下弾の発射数: %zu", lockedWingShotCount_);
    ImGui::Text("追尾開始数: %zu", lockedWingHomingStartCount_);
    ImGui::Text("追尾更新数: %zu", lockedWingHomingUpdateCount_);
    ImGui::Text("対象検索成功数: %zu", lockedWingTargetQuerySuccessCount_);
    ImGui::Text("対象検索失敗数: %zu", lockedWingTargetQueryFailureCount_);
    ImGui::Text("対象死亡数: %zu", lockedWingTargetDeathCount_);
    ImGui::Text("対象消失数: %zu", lockedWingTargetRemovalCount_);
    ImGui::Text("非有限対象数: %zu", lockedWingNonFiniteTargetCount_);
    ImGui::Text("非有限方向数: %zu", lockedWingNonFiniteDirectionCount_);
    ImGui::Text("反対方向代替軸使用数: %zu", lockedWingOppositeFallbackCount_);
    ImGui::Text("最大旋回角制限数: %zu", lockedWingMaximumTurnClampCount_);
    ImGui::Text("追尾中命中数: %zu", lockedWingHomingHitCount_);
    ImGui::Text("対象喪失後直進数: %zu", lockedWingTargetLostStraightCount_);
    ImGui::Text("別対象への乗り換え数: %zu（常に0）", lockedWingTargetSwitchCount_);
    ImGui::Text("通常弾への追尾適用数: %zu（常に0）", normalShotHomingApplyCount_);
#endif
}
