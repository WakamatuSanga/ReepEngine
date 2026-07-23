#include "PlayerBulletManager.h"
#include "LockedWingMissileExhaustController.h"

#include "Engine/Game/Enemy/Enemy.h"
#include "Engine/Game/Enemy/EnemyBullet.h"
#include "Engine/Game/Enemy/EnemyManager.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Game/RailShooter/ProjectileRailMotionAdapter.h"
#include "Engine/Game/Targeting/AimCorridorTargetingController.h"
#include "Engine/Graphics/Camera/Camera.h"

#include <algorithm>
#include <cmath>
#include <vector>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
constexpr float kMinimumVectorLength = 0.00001f;
constexpr Vector3 kRecommendedLeftWingOffset{ -1.0f, -0.35f, -0.33f };
constexpr Vector3 kRecommendedRightWingOffset{ 1.0f, -0.35f, -0.33f };

Vector3 Add(const Vector3& lhs, const Vector3& rhs) {
    return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

Vector3 Subtract(const Vector3& lhs, const Vector3& rhs) {
    return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
}

Vector3 Scale(const Vector3& value, float scale) {
    return { value.x * scale, value.y * scale, value.z * scale };
}

float Dot(const Vector3& lhs, const Vector3& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

float Length(const Vector3& value) {
    return std::sqrt(Dot(value, value));
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

Vector3 MakeRotationFromForward(const Vector3& forward) {
    Vector3 normalized{};
    if (!TryNormalize(forward, normalized)) {
        normalized = { 0.0f, 0.0f, 1.0f };
    }
    const float horizontal =
        std::sqrt(normalized.x * normalized.x + normalized.z * normalized.z);
    return {
        std::atan2(-normalized.y, horizontal),
        std::atan2(normalized.x, normalized.z),
        0.0f,
    };
}

PlayerBulletManager::WingSide OppositeWing(PlayerBulletManager::WingSide wing) {
    return wing == PlayerBulletManager::WingSide::Left
        ? PlayerBulletManager::WingSide::Right
        : PlayerBulletManager::WingSide::Left;
}

const char* ToJapaneseBool(bool value) {
    return value ? "はい" : "いいえ";
}

const char* ToJapaneseWing(PlayerBulletManager::WingSide wing) {
    return wing == PlayerBulletManager::WingSide::Left ? "左翼" : "右翼";
}

const char* ToJapaneseLockState(
    AimCorridorTargetingController::AimLockState state) {
    switch (state) {
    case AimCorridorTargetingController::AimLockState::Candidate:
        return "候補";
    case AimCorridorTargetingController::AimLockState::Acquiring:
        return "ロック中";
    case AimCorridorTargetingController::AimLockState::Locked:
        return "ロック完了";
    case AimCorridorTargetingController::AimLockState::None:
    default:
        return "なし";
    }
}
}

PlayerBulletManager::LockedTargetValidation
PlayerBulletManager::ValidateLockedTarget() const {
    LockedTargetValidation validation;
    validation.targetingControllerAvailable =
        aimCorridorTargetingController_ != nullptr;
    if (!aimCorridorTargetingController_) {
        return validation;
    }

    validation.lockStateLocked =
        aimCorridorTargetingController_->GetLockState()
        == AimCorridorTargetingController::AimLockState::Locked;
    validation.targetId = aimCorridorTargetingController_->GetLockedTargetId();
    validation.targetIdValid = !validation.targetId.empty();
    if (!validation.lockStateLocked || !validation.targetIdValid
        || !aimCorridorTargetingController_->HasLockedTarget()
        || !enemyManager_) {
        return validation;
    }

    for (Enemy* enemy : enemyManager_->GetActiveEnemies()) {
        if (enemy && enemy->GetEnemyId() == validation.targetId) {
            validation.alive = true;
            break;
        }
    }

    Vector3 targetPosition{};
    std::vector<EnemyTargetView> targets;
    enemyManager_->CollectTargetableEnemies(targets);
    for (const EnemyTargetView& target : targets) {
        if (target.runtimeId == validation.targetId) {
            validation.targetable = true;
            validation.alive = true;
            targetPosition = target.worldPosition;
            break;
        }
    }

    if (validation.targetable && camera_) {
        const Matrix4x4& cameraWorld = camera_->GetWorldMatrix();
        Vector3 cameraForward{};
        if (TryNormalize(
                { cameraWorld.m[2][0], cameraWorld.m[2][1], cameraWorld.m[2][2] },
                cameraForward)) {
            const float cameraDepth = Dot(
                Subtract(targetPosition, camera_->GetTranslate()), cameraForward);
            validation.cameraFront =
                std::isfinite(cameraDepth) && cameraDepth > 0.0f;
        }
    }

    validation.valid =
        validation.targetingControllerAvailable
        && validation.lockStateLocked
        && validation.targetIdValid
        && validation.alive
        && validation.targetable
        && validation.cameraFront;
    return validation;
}

Vector3 PlayerBulletManager::ResolveLockedWingLaunchDirection() const {
    Vector3 direction{};
    if (player_ && TryNormalize(player_->GetBaseForward(), direction)) {
        return direction;
    }
    if (camera_) {
        const Matrix4x4& cameraWorld = camera_->GetWorldMatrix();
        if (TryNormalize(
                { cameraWorld.m[2][0], cameraWorld.m[2][1], cameraWorld.m[2][2] },
                direction)) {
            return direction;
        }
    }
    return { 0.0f, 0.0f, 1.0f };
}

Vector3 PlayerBulletManager::ResolveLockedWingEjectionDownDirection() const {
    if (player_) {
        const Vector3 visualOrigin =
            player_->TransformVisualModelPointToWorld({ 0.0f, 0.0f, 0.0f });
        const Vector3 visualUpPoint =
            player_->TransformVisualModelPointToWorld({ 0.0f, 1.0f, 0.0f });
        Vector3 visualDown{};
        if (TryNormalize(Subtract(visualOrigin, visualUpPoint), visualDown)) {
            return visualDown;
        }
    }
    return { 0.0f, -1.0f, 0.0f };
}
void PlayerBulletManager::UpdateLockedWingShotDiagnostics() {
    if (player_) {
        leftLockedWingWorldPosition_ =
            player_->TransformVisualModelPointToWorld(leftLockedWingLocalOffset_);
        rightLockedWingWorldPosition_ =
            player_->TransformVisualModelPointToWorld(rightLockedWingLocalOffset_);
    } else {
        leftLockedWingWorldPosition_ = {};
        rightLockedWingWorldPosition_ = {};
    }
}

PlayerBulletManager::LockedWingShotResult
PlayerBulletManager::TrySpawnLockedWingShot() {
    if (!lockedWingShotEnabled_) {
        return LockedWingShotResult::NotRequested;
    }

    const bool forcedTest =
        lockedWingForceTestPending_ && !aimGameModeActive_;
    if (!forcedTest) {
        if (!aimGameModeActive_ || !aimPlayerAlive_
            || aimMode_ != AimMode::AimCorridor
            || !aimCorridorTargetingController_
            || aimCorridorTargetingController_->GetLockState()
                != AimCorridorTargetingController::AimLockState::Locked) {
            return LockedWingShotResult::NotRequested;
        }

        lockedTargetValidation_ = ValidateLockedTarget();
        if (!lockedTargetValidation_.valid) {
            ++lockedWingTargetInvalidFallbackCount_;
            ++lockedWingNormalFallbackCount_;
            lastLockedWingStatus_ =
                "ロック対象が発射時点で無効なため、通常弾へ戻しました";
            return LockedWingShotResult::FallbackToNormal;
        }
    } else {
        lockedTargetValidation_ = ValidateLockedTarget();
    }

    const WingSide launchWing =
        forcedTest ? forcedLockedShotWing_ : nextLockedShotWing_;
    const std::string targetId = lockedTargetValidation_.valid
        ? lockedTargetValidation_.targetId
        : std::string{};
    if (forcedTest) {
        lockedWingForceTestPending_ = false;
    }
    return SpawnLockedWingShot(launchWing, targetId, forcedTest);
}

PlayerBulletManager::LockedWingShotResult
PlayerBulletManager::SpawnLockedWingShot(
    WingSide wing, const std::string& targetId, bool forcedTest) {
    if (!player_ || !camera_ || !std::isfinite(bulletSpeed_)) {
        ++lockedWingSpawnFailureCount_;
        lastLockedWingStatus_ =
            "プレイヤー、カメラ、または弾速が無効で生成できませんでした";
        return LockedWingShotResult::SpawnFailed;
    }

    const Vector3 launchPosition = wing == WingSide::Left
        ? player_->TransformVisualModelPointToWorld(leftLockedWingLocalOffset_)
        : player_->TransformVisualModelPointToWorld(rightLockedWingLocalOffset_);
    const Vector3 launchDirection = ResolveLockedWingLaunchDirection();
    const Vector3 ejectionDownDirection =
        ResolveLockedWingEjectionDownDirection();
    const float baseBulletSpeed = (std::max)(bulletSpeed_, 0.0f);
    const float ejectionDropDuration =
        std::clamp(lockedWingEjectionDropDuration_, 0.01f, 0.30f);
    const float ejectionDropDistance =
        std::clamp(lockedWingEjectionDropDistance_, 0.01f, 2.0f);
    const float ejectionDropSpeed =
        ejectionDropDistance / (std::max)(ejectionDropDuration, 0.001f);
    const Vector3 relativeVelocity =
        Scale(ejectionDownDirection, ejectionDropSpeed);
    if (!IsFinite(launchPosition) || !IsFinite(launchDirection)
        || !IsFinite(ejectionDownDirection) || !IsFinite(relativeVelocity)) {
        ++lockedWingSpawnFailureCount_;
        lastLockedWingStatus_ = "翼下位置または発射方向が無効で生成できませんでした";
        return LockedWingShotResult::SpawnFailed;
    }

    EnemyBullet* bullet =
        SpawnBullet(launchPosition, relativeVelocity, bulletDamage_);
    if (!bullet || bullets_.empty() || bullets_.back().bullet.get() != bullet) {
        ++lockedWingSpawnFailureCount_;
        lastLockedWingStatus_ = "弾の生成に失敗しました";
        return LockedWingShotResult::SpawnFailed;
    }

    PlayerBulletInstance& instance = bullets_.back();
    instance.projectileType = PlayerProjectileType::LockedWingShot;
    instance.lockedWingLaunch = std::make_unique<LockedWingLaunchState>();
    LockedWingLaunchState& launchState = *instance.lockedWingLaunch;
    launchState.lockedTargetId = targetId;
    launchState.currentFlightDirection = launchDirection;
    launchState.currentEjectionDownDirection = ejectionDownDirection;
    launchState.sequence = ++lockedWingSequenceCounter_;
    launchState.exhaustHandle = 0;
    launchState.totalElapsed = 0.0f;
    launchState.ejectionDropDuration = ejectionDropDuration;
    launchState.ejectionDropDistance = ejectionDropDistance;
    launchState.preIgnitionHoldDuration =
        std::clamp(lockedWingPreIgnitionHoldDuration_, 0.15f, 0.50f);
    launchState.ignitionRampDuration =
        std::clamp(lockedWingIgnitionRampDuration_, 0.01f, 0.50f);
    launchState.baseBulletSpeed = baseBulletSpeed;
    launchState.currentSpeedRate = 0.0f;
    launchState.launchWing = wing;
    launchState.phase = LockedWingLaunchPhase::EjectionDrop;
    launchState.exhaustEnabled = false;
    launchState.ignitionStarted = false;
    launchState.homingReady = false;
    launchState.homingEnabled = false;

    if (!forcedTest && hasLastLockedShotWing_
        && lastLockedShotWing_ == wing) {
        ++lockedWingAlternationErrorCount_;
    }

    hasLastLockedShotWing_ = true;
    lastLockedShotWing_ = wing;
    nextLockedShotWing_ = OppositeWing(wing);
    lastLockedWingShotSequence_ = launchState.sequence;
    lastLockedWingTargetId_ = targetId;
    lastLockedWingLaunchPhase_ = LockedWingLaunchPhase::EjectionDrop;
    hasLastLockedWingLaunchPhase_ = true;
    lastLockedWingLaunchElapsed_ = 0.0f;
    lastLockedWingSpeedRate_ = 0.0f;
    lastLockedWingCurrentSpeed_ = Length(relativeVelocity);
    lastLockedWingExhaustEnabled_ = false;
    lastLockedWingIgnitionStarted_ = false;
    lastLockedWingHomingReady_ = false;
    lastLockedWingHomingEnabled_ = false;
    lastLockedWingLaunchDirection_ = launchDirection;
    lastLockedWingEjectionDownDirection_ = ejectionDownDirection;
    lastLockedWingRelativeVelocity_ = relativeVelocity;
    lastLockedWingStatus_ = forcedTest
        ? "デバッグテスト用ミサイルを分離しました"
        : "ロック対象を確認し、翼下ミサイルを分離しました";

    ++lockedWingShotCount_;
    ++lockedWingEjectionDropStartCount_;
    if (wing == WingSide::Left) {
        ++leftLockedWingShotCount_;
    } else {
        ++rightLockedWingShotCount_;
    }

    const float diagnosticDistance = std::isfinite(aimDistance_)
        ? (std::max)(aimDistance_, 1.0f)
        : 1.0f;
    lastUsedAimMode_ = AimMode::AimCorridor;
    lastAimFallbackUsed_ = false;
    lastShotFallbackReason_ = "なし";
    lastShotSpawnDataValid_ = true;
    lastFirePosition_ = launchPosition;
    lastFireDirection_ = launchDirection;
    lastMuzzlePosition_ = launchPosition;
    lastAimPoint_ =
        Add(launchPosition, Scale(launchDirection, diagnosticDistance));
    lastAimDirection_ = launchDirection;
    lastShotDirectionLength_ = Length(launchDirection);
    lastShotVelocity_ = relativeVelocity;
    lastVisualDirection_ = launchDirection;
    currentBulletRotation_ = Add(
        Add(MakeRotationFromForward(launchDirection), defaultRotation_),
        playerBulletModelRotationOffset_);

    RecordAimShot();
    if (projectileRailMotionAdapter_) {
        projectileRailMotionAdapter_->RecordShot(
            ProjectileRailMotionAdapter::ProjectileKind::Player,
            launchPosition,
            lastAimPoint_,
            launchDirection,
            relativeVelocity);
    }
    bullet->SetVisualForwardOverride(launchDirection);
    return LockedWingShotResult::Spawned;
}

void PlayerBulletManager::ResetLockedWingShotState(bool resetStatistics) {
    nextLockedShotWing_ = WingSide::Left;
    lastLockedShotWing_ = WingSide::Left;
    forcedLockedShotWing_ = WingSide::Left;
    hasLastLockedShotWing_ = false;
    lockedWingForceTestPending_ = false;
    lockedTargetValidation_ = {};
    leftLockedWingWorldPosition_ = {};
    rightLockedWingWorldPosition_ = {};
    lastLockedWingLaunchDirection_ = { 0.0f, 0.0f, 1.0f };
    lastLockedWingEjectionDownDirection_ = { 0.0f, -1.0f, 0.0f };
    lastLockedWingRelativeVelocity_ = {};
    lastLockedWingTargetId_.clear();
    lastLockedWingStatus_ = "未実行";
    lastLockedWingLaunchPhase_ = LockedWingLaunchPhase::EjectionDrop;
    hasLastLockedWingLaunchPhase_ = false;
    lastLockedWingLaunchElapsed_ = 0.0f;
    lastLockedWingSpeedRate_ = 0.0f;
    lastLockedWingCurrentSpeed_ = 0.0f;
    lastLockedWingExhaustEnabled_ = false;
    lastLockedWingIgnitionStarted_ = false;
    lastLockedWingHomingReady_ = false;
    lastLockedWingHomingEnabled_ = false;
    lastLockedWingShotSequence_ = 0;
    for (PlayerBulletInstance& instance : bullets_) {
        if (instance.lockedWingLaunch) {
            instance.lockedWingLaunch->exhaustHandle = 0;
            instance.lockedWingLaunch->exhaustEnabled = false;
            instance.lockedWingLaunch->ignitionStarted = true;
        }
    }
    if (lockedWingMissileExhaustController_) {
        lockedWingMissileExhaustController_->Reset(resetStatistics);
    }

    if (resetStatistics) {
        lockedWingShotCount_ = 0;
        leftLockedWingShotCount_ = 0;
        rightLockedWingShotCount_ = 0;
        lockedWingTargetInvalidFallbackCount_ = 0;
        lockedWingSpawnFailureCount_ = 0;
        lockedWingNormalFallbackCount_ = 0;
        lockedWingAlternationErrorCount_ = 0;
        lockedWingEjectionDropStartCount_ = 0;
        lockedWingPreIgnitionHoldStartCount_ = 0;
        lockedWingIgnitionStartCount_ = 0;
        lockedWingCruiseTransitionCount_ = 0;
        lockedWingDirectionFallbackCount_ = 0;
        lockedWingNonFiniteVelocityCount_ = 0;
    }
}

void PlayerBulletManager::ClearLockedWingShotForceState() {
    lockedWingForceTestPending_ = false;
    forcedLockedShotWing_ = WingSide::Left;
}

void PlayerBulletManager::DrawLockedWingShotImGui() {
#ifdef USE_IMGUI
    ImGui::SeparatorText("ロック時の翼下発射");
    ImGui::Checkbox(
        "翼下発射を有効化##LockedWingEnabled",
        &lockedWingShotEnabled_);

    const auto lockState = aimCorridorTargetingController_
        ? aimCorridorTargetingController_->GetLockState()
        : AimCorridorTargetingController::AimLockState::None;
    const std::string lockedTargetId = aimCorridorTargetingController_
        ? aimCorridorTargetingController_->GetLockedTargetId()
        : std::string{};
    const LockedTargetValidation uiTargetValidation = ValidateLockedTarget();

    ImGui::Text(
        "ターゲット判定管理が有効: %s",
        ToJapaneseBool(aimCorridorTargetingController_ != nullptr));
    ImGui::Text("現在のロック状態: %s", ToJapaneseLockState(lockState));
    ImGui::TextWrapped(
        "ロック対象のTarget ID: %s",
        lockedTargetId.empty() ? "なし" : lockedTargetId.c_str());
    ImGui::Text(
        "ロック対象が有効: %s",
        ToJapaneseBool(uiTargetValidation.valid));
    ImGui::Text(
        "ロック対象が生存中: %s",
        ToJapaneseBool(uiTargetValidation.alive));
    ImGui::Text(
        "ロック対象を射撃に使用可能: %s",
        ToJapaneseBool(uiTargetValidation.targetable));
    ImGui::Text(
        "ロック対象がカメラ前方: %s",
        ToJapaneseBool(uiTargetValidation.cameraFront));

    ImGui::DragFloat3(
        "左翼下ローカルオフセット##LeftWingLocalOffset",
        &leftLockedWingLocalOffset_.x,
        0.01f,
        -5.0f,
        5.0f,
        "%.3f");
    ImGui::DragFloat3(
        "右翼下ローカルオフセット##RightWingLocalOffset",
        &rightLockedWingLocalOffset_.x,
        0.01f,
        -5.0f,
        5.0f,
        "%.3f");
    ImGui::Text(
        "左翼下のワールド位置: %.3f, %.3f, %.3f",
        leftLockedWingWorldPosition_.x,
        leftLockedWingWorldPosition_.y,
        leftLockedWingWorldPosition_.z);
    ImGui::Text(
        "右翼下のワールド位置: %.3f, %.3f, %.3f",
        rightLockedWingWorldPosition_.x,
        rightLockedWingWorldPosition_.y,
        rightLockedWingWorldPosition_.z);
    ImGui::Text("次に使用する翼: %s", ToJapaneseWing(nextLockedShotWing_));
    ImGui::Text(
        "最後に使用した翼: %s",
        hasLastLockedShotWing_ ? ToJapaneseWing(lastLockedShotWing_) : "なし");
    ImGui::Text("見た目の変換を発射位置へ使用: はい");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "翼下の発射位置は機体の最終的な見た目の回転と拡縮へ追従します。\n"
            "弾の発射方向には見た目のバンクやバレルロールを使用しません。");
    }
    ImGui::Text("見た目のバンクを発射方向へ未使用: はい");
    ImGui::Text("バレルロールを発射方向へ未使用: はい");

    DrawLockedWingMissileLaunchImGui();
    ImGui::TextWrapped(
        "最後に保存したTarget ID: %s",
        lastLockedWingTargetId_.empty()
            ? "なし"
            : lastLockedWingTargetId_.c_str());
    ImGui::TextWrapped(
        "最後の状態: %s",
        lastLockedWingStatus_.c_str());

    ImGui::Text("ロック翼下弾の発射回数: %zu", lockedWingShotCount_);
    ImGui::Text("左翼発射回数: %zu", leftLockedWingShotCount_);
    ImGui::Text("右翼発射回数: %zu", rightLockedWingShotCount_);
    ImGui::Text(
        "対象無効フォールバック回数: %zu",
        lockedWingTargetInvalidFallbackCount_);
    ImGui::Text(
        "弾生成失敗回数: %zu",
        lockedWingSpawnFailureCount_);
    ImGui::Text(
        "通常弾フォールバック回数: %zu",
        lockedWingNormalFallbackCount_);
    ImGui::Text(
        "左右交互エラー数: %zu",
        lockedWingAlternationErrorCount_);

    ImGui::BeginDisabled(aimGameModeActive_ || !lockedWingShotEnabled_);
    if (ImGui::Button("左翼下発射をテスト##TestLeftLockedWing")) {
        forcedLockedShotWing_ = WingSide::Left;
        lockedWingForceTestPending_ = true;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip(
            "デバッグモードで、次の既存通常射撃入力1回だけ左翼を使用します。\n"
            "このボタン自体は弾を生成しません。");
    }
    ImGui::SameLine();
    if (ImGui::Button("右翼下発射をテスト##TestRightLockedWing")) {
        forcedLockedShotWing_ = WingSide::Right;
        lockedWingForceTestPending_ = true;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip(
            "デバッグモードで、次の既存通常射撃入力1回だけ右翼を使用します。\n"
            "このボタン自体は弾を生成しません。");
    }
    ImGui::EndDisabled();

    ImGui::Text(
        "翼下テスト待機中: %s",
        ToJapaneseBool(lockedWingForceTestPending_));
    if (ImGui::Button("次の翼を左へ設定##SetNextLockedWingLeft")) {
        nextLockedShotWing_ = WingSide::Left;
    }
    ImGui::SameLine();
    if (ImGui::Button("次の翼を右へ設定##SetNextLockedWingRight")) {
        nextLockedShotWing_ = WingSide::Right;
    }
    if (ImGui::Button("翼下オフセットを推奨値へ戻す##ResetWingOffsets")) {
        leftLockedWingLocalOffset_ = kRecommendedLeftWingOffset;
        rightLockedWingLocalOffset_ = kRecommendedRightWingOffset;
    }

    if (ImGui::Button("翼下発射の診断をリセット##ResetLockedWingDiagnostics")) {
        ResetLockedWingShotState(true);
        UpdateLockedWingShotDiagnostics();
    }
    if (ImGui::Button("翼下発射の強制状態をすべて解除##ClearLockedWingForce")) {
        ClearLockedWingShotForceState();
    }
#endif
}
