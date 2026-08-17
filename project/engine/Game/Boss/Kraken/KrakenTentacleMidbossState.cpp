#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"

#include "Engine/Animation/Skeleton.h"
#include "Engine/Graphics/Camera/Camera.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float kMaximumDeltaTime = 0.1f;
    constexpr float kMinimumScale = 0.0001f;

    bool IsFinite(const Vector3& value) {
        return std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
    }

    bool IsFiniteTransform(
        const Vector3& position,
        const Vector3& rotation,
        const Vector3& scale) {
        return IsFinite(position) && IsFinite(rotation) && IsFinite(scale) &&
            std::fabs(scale.x) > kMinimumScale &&
            std::fabs(scale.y) > kMinimumScale &&
            std::fabs(scale.z) > kMinimumScale;
    }

    Vector3 Normalize(const Vector3& value, const Vector3& fallback) {
        const float length = std::sqrt(
            value.x * value.x + value.y * value.y + value.z * value.z);
        if (!std::isfinite(length) || length <= 0.00001f) {
            return fallback;
        }
        return { value.x / length, value.y / length, value.z / length };
    }

    Vector3 Add(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 Scale(const Vector3& value, float scalar) {
        return { value.x * scalar, value.y * scalar, value.z * scalar };
    }
}

bool KrakenTentacleMidbossController::Impl::IsVisible() const {
    return initialized && modelLoaded && skeletonValid && object && model &&
        skeleton && state != KrakenTentacleMidbossState::Hidden;
}

bool KrakenTentacleMidbossController::Impl::IsAttackState() const {
    switch (state) {
    case KrakenTentacleMidbossState::Windup:
    case KrakenTentacleMidbossState::WindupHold:
    case KrakenTentacleMidbossState::Slam:
    case KrakenTentacleMidbossState::ImpactHold:
    case KrakenTentacleMidbossState::Recovery:
        return true;
    case KrakenTentacleMidbossState::Hidden:
    case KrakenTentacleMidbossState::Idle:
    default:
        return false;
    }
}

KrakenTentacleAttackPreviewPhase
KrakenTentacleMidbossController::Impl::GetAttackPhase() const {
    switch (state) {
    case KrakenTentacleMidbossState::Windup:
        return KrakenTentacleAttackPreviewPhase::Windup;
    case KrakenTentacleMidbossState::WindupHold:
        return KrakenTentacleAttackPreviewPhase::WindupHold;
    case KrakenTentacleMidbossState::Slam:
        return KrakenTentacleAttackPreviewPhase::Slam;
    case KrakenTentacleMidbossState::ImpactHold:
        return KrakenTentacleAttackPreviewPhase::ImpactHold;
    case KrakenTentacleMidbossState::Recovery:
        return KrakenTentacleAttackPreviewPhase::Recovery;
    case KrakenTentacleMidbossState::Hidden:
    case KrakenTentacleMidbossState::Idle:
    default:
        return KrakenTentacleAttackPreviewPhase::Completed;
    }
}

KrakenTentacleColliderAttackPhase
KrakenTentacleMidbossController::Impl::GetColliderAttackPhase() const {
    switch (state) {
    case KrakenTentacleMidbossState::Windup:
        return KrakenTentacleColliderAttackPhase::Windup;
    case KrakenTentacleMidbossState::WindupHold:
        return KrakenTentacleColliderAttackPhase::WindupHold;
    case KrakenTentacleMidbossState::Slam:
        return KrakenTentacleColliderAttackPhase::Slam;
    case KrakenTentacleMidbossState::ImpactHold:
        return KrakenTentacleColliderAttackPhase::ImpactHold;
    case KrakenTentacleMidbossState::Recovery:
        return KrakenTentacleColliderAttackPhase::Recovery;
    case KrakenTentacleMidbossState::Idle:
        return KrakenTentacleColliderAttackPhase::Completed;
    case KrakenTentacleMidbossState::Hidden:
    default:
        return KrakenTentacleColliderAttackPhase::Invalid;
    }
}

float KrakenTentacleMidbossController::Impl::GetCurrentStateDuration() const {
    if (!IsAttackState()) {
        return 0.0f;
    }
    return GetKrakenTentacleAttackPhaseDuration(
        attackSettings, GetAttackPhase());
}

float KrakenTentacleMidbossController::Impl::GetSlamProgress() const {
    if (state != KrakenTentacleMidbossState::Slam) {
        return state == KrakenTentacleMidbossState::ImpactHold ? 1.0f : 0.0f;
    }
    const float duration = GetKrakenTentacleAttackPhaseDuration(
        attackSettings, KrakenTentacleAttackPreviewPhase::Slam);
    return std::isfinite(duration) && duration > 0.001f
        ? std::clamp(stateElapsedTime / duration, 0.0f, 1.0f)
        : 0.0f;
}

void KrakenTentacleMidbossController::Impl::EnterState(
    KrakenTentacleMidbossState nextState) {
    state = nextState;
    stateElapsedTime = 0.0f;
    if (state == KrakenTentacleMidbossState::Idle) {
        idleTime = 0.0f;
        attackElapsedTime = 0.0f;
    } else if (state == KrakenTentacleMidbossState::Windup) {
        attackElapsedTime = 0.0f;
    }
}

void KrakenTentacleMidbossController::Impl::EnterHidden(
    const std::string& errorMessage,
    bool safetyRecovery) {
    state = KrakenTentacleMidbossState::Hidden;
    stateElapsedTime = 0.0f;
    idleTime = 0.0f;
    attackElapsedTime = 0.0f;
    diagnostics.computeDispatchCount = 0;
    diagnostics.drawCallCount = 0;
    diagnostics.materialBindingCount = 0;
    pendingCommand = KrakenTentacleMidbossPendingCommand::None;
    safetyStopped = safetyRecovery;
    if (safetyRecovery) {
        ++diagnostics.safetyRecoveryCount;
    }
    if (!errorMessage.empty()) {
        lastError = errorMessage;
    }
    RestoreBindPose();
    if (skeleton) {
        UpdateSkeletonWorldTransforms(*skeleton);
    }
    RefreshColliderSnapshots();
    RefreshBoneSnapshots();
}

bool KrakenTentacleMidbossController::Impl::Show() {
    if (!initialized || !modelLoaded || !skeletonValid || !camera) {
        lastError = !camera
            ? "Gameplay Cameraが無効なため中ボスを表示できません。"
            : "Runtimeの初期化が完了していないため表示できません。";
        return false;
    }
    if (chains.empty() || selectedAttackChainIndex >= chains.size()) {
        ++diagnostics.outOfRangeChainCount;
        lastError = "有効な触手Chainがないため表示できません。";
        return false;
    }
    safetyStopped = false;
    lastError.clear();
    idleSwayEnabled = true;
    EnterState(KrakenTentacleMidbossState::Idle);
    return true;
}

void KrakenTentacleMidbossController::Impl::Hide() {
    EnterHidden({}, false);
}

bool KrakenTentacleMidbossController::Impl::StartAttack() {
    if (!IsVisible() || state != KrakenTentacleMidbossState::Idle ||
        selectedAttackChainIndex >= chains.size()) {
        ++diagnostics.attackStartRejectedCount;
        if (selectedAttackChainIndex >= chains.size()) {
            ++diagnostics.outOfRangeChainCount;
            lastError = "攻撃Chainが範囲外のため攻撃を開始できません。";
        } else if (IsAttackState()) {
            lastWarning = "攻撃再生中のため再攻撃を拒否しました。";
        } else {
            lastWarning = "Idle表示中ではないため攻撃を開始できません。";
        }
        return false;
    }
    lastWarning.clear();
    idleSwayEnabled = true;
    EnterState(KrakenTentacleMidbossState::Windup);
    return true;
}

void KrakenTentacleMidbossController::Impl::StopAttack() {
    if (!IsAttackState()) {
        return;
    }
    EnterState(KrakenTentacleMidbossState::Idle);
}

void KrakenTentacleMidbossController::Impl::ReturnToIdle() {
    if (state == KrakenTentacleMidbossState::Hidden) {
        Show();
        return;
    }
    idleSwayEnabled = true;
    EnterState(KrakenTentacleMidbossState::Idle);
}

void KrakenTentacleMidbossController::Impl::ReturnToBindPose() {
    idleSwayEnabled = false;
    stateElapsedTime = 0.0f;
    idleTime = 0.0f;
    attackElapsedTime = 0.0f;
    if (IsVisible()) {
        state = KrakenTentacleMidbossState::Idle;
    }
}

void KrakenTentacleMidbossController::Impl::ResetStateOnly() {
    ResetCollisionQueryState(true);
    selectedAttackChainIndex = 0;
    totalActiveTime = 0.0f;
    safetyStopped = false;
    lastError.clear();
    lastWarning.clear();
    Hide();
}

void KrakenTentacleMidbossController::Impl::ProcessPendingCommand() {
    const KrakenTentacleMidbossPendingCommand command = pendingCommand;
    pendingCommand = KrakenTentacleMidbossPendingCommand::None;
    switch (command) {
    case KrakenTentacleMidbossPendingCommand::Show:
        Show();
        break;
    case KrakenTentacleMidbossPendingCommand::Hide:
        Hide();
        break;
    case KrakenTentacleMidbossPendingCommand::ReturnToIdle:
        ReturnToIdle();
        break;
    case KrakenTentacleMidbossPendingCommand::StartAttack:
        StartAttack();
        break;
    case KrakenTentacleMidbossPendingCommand::StopAttack:
        StopAttack();
        break;
    case KrakenTentacleMidbossPendingCommand::ReturnToBindPose:
        ReturnToBindPose();
        break;
    case KrakenTentacleMidbossPendingCommand::ResetState:
        ResetStateOnly();
        break;
    case KrakenTentacleMidbossPendingCommand::ResetRuntime:
        Reset();
        break;
    case KrakenTentacleMidbossPendingCommand::None:
    default:
        break;
    }
}

void KrakenTentacleMidbossController::Impl::AdvanceState(float deltaTime) {
    float remaining = deltaTime;
    for (int transitionGuard = 0; transitionGuard < 8; ++transitionGuard) {
        if (state == KrakenTentacleMidbossState::Idle) {
            stateElapsedTime += remaining;
            idleTime += remaining;
            return;
        }
        if (!IsAttackState()) {
            return;
        }

        const float duration = GetCurrentStateDuration();
        if (!std::isfinite(duration) || duration < 0.0f) {
            EnterHidden("攻撃State時間が不正なため表示を停止しました。", true);
            return;
        }
        const float available = (std::max)(duration - stateElapsedTime, 0.0f);
        const float consumed = (std::min)(remaining, available);
        stateElapsedTime += consumed;
        attackElapsedTime += consumed;
        remaining -= consumed;
        if (stateElapsedTime + 0.000001f < duration) {
            return;
        }

        switch (state) {
        case KrakenTentacleMidbossState::Windup:
            EnterState(KrakenTentacleMidbossState::WindupHold);
            break;
        case KrakenTentacleMidbossState::WindupHold:
            EnterState(KrakenTentacleMidbossState::Slam);
            break;
        case KrakenTentacleMidbossState::Slam:
            EnterState(KrakenTentacleMidbossState::ImpactHold);
            break;
        case KrakenTentacleMidbossState::ImpactHold:
            EnterState(KrakenTentacleMidbossState::Recovery);
            break;
        case KrakenTentacleMidbossState::Recovery:
            EnterState(KrakenTentacleMidbossState::Idle);
            break;
        default:
            return;
        }
        if (remaining <= 0.0f) {
            return;
        }
    }
    EnterHidden("攻撃State遷移回数が上限を超えました。", true);
}

bool KrakenTentacleMidbossController::Impl::PlaceInFrontOfCamera() {
    if (!camera) {
        lastError = "Gameplay Cameraが無効なため前方へ配置できません。";
        return false;
    }
    const Matrix4x4& cameraWorld = camera->GetWorldMatrix();
    const Vector3 cameraPosition = camera->GetTranslate();
    const Vector3 cameraRight = {
        cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2] };
    const Vector3 cameraUp = {
        cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2] };
    const Vector3 cameraForward = {
        cameraWorld.m[2][0], cameraWorld.m[2][1], cameraWorld.m[2][2] };
    if (!IsFinite(cameraPosition) || !IsFinite(cameraRight) ||
        !IsFinite(cameraUp) || !IsFinite(cameraForward) ||
        !std::isfinite(cameraForwardOffset) ||
        !std::isfinite(cameraRightOffset) ||
        !std::isfinite(cameraUpOffset)) {
        lastError = "Gameplay Cameraの行列または配置距離が無効です。";
        return false;
    }
    const Vector3 right = Normalize(
        cameraRight,
        { 1.0f, 0.0f, 0.0f });
    const Vector3 up = Normalize(
        cameraUp,
        { 0.0f, 1.0f, 0.0f });
    const Vector3 forward = Normalize(
        cameraForward,
        { 0.0f, 0.0f, 1.0f });
    worldPosition = Add(
        Add(
            Add(cameraPosition, Scale(right, cameraRightOffset)),
            Scale(up, cameraUpOffset)),
        Scale(forward, cameraForwardOffset));
    if (!IsFinite(worldPosition)) {
        lastError = "カメラ前方の配置座標が有限値ではありません。";
        return false;
    }
    lastError.clear();
    // ImGui操作中にはSnapshotを更新せず、次回のRuntime Updateで
    // 描画・Debug Draw・Collision Queryへ同時に反映する。
    return true;
}

void KrakenTentacleMidbossController::Impl::Update(float scaledDeltaTime) {
    diagnostics.cpuSkinningUpdateCount = 0;
    if (!initialized) {
        lastScaledDeltaTime = 0.0f;
        return;
    }
    if (!std::isfinite(scaledDeltaTime)) {
        lastScaledDeltaTime = 0.0f;
        EnterHidden("scaled delta timeが有限値ではありません。", true);
        return;
    }
    lastScaledDeltaTime = std::clamp(
        scaledDeltaTime, 0.0f, kMaximumDeltaTime);
    ProcessPendingCommand();
    if (state == KrakenTentacleMidbossState::Hidden) {
        return;
    }
    if (!camera) {
        EnterHidden("Gameplay Cameraが無効なため表示を停止しました。", true);
        return;
    }
    if (selectedAttackChainIndex >= chains.size()) {
        ++diagnostics.outOfRangeChainCount;
        EnterHidden("攻撃Chainが範囲外のため表示を停止しました。", true);
        return;
    }
    if (!IsFiniteTransform(worldPosition, worldRotation, worldScale)) {
        EnterHidden("RuntimeのWorld Transformが不正です。", true);
        return;
    }

    totalActiveTime += lastScaledDeltaTime;
    AdvanceState(lastScaledDeltaTime);
    if (!IsVisible()) {
        return;
    }
    if (!std::isfinite(stateElapsedTime) ||
        !std::isfinite(totalActiveTime) ||
        !std::isfinite(idleTime) ||
        !std::isfinite(attackElapsedTime)) {
        EnterHidden("RuntimeのState時間が有限値ではありません。", true);
        return;
    }
    UpdateCurrentPoseAndSkinning();
}
