#include "ProjectileRailMotionAdapter.h"

#include "Engine/Game/Camera/RailShooterCameraRig.h"
#include "Engine/Game/Enemy/EnemyBullet.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float kMinVectorLength = 0.00001f;
    constexpr float kRailDistanceRewindTolerance = 0.001f;
    constexpr float kRadiansToDegrees = 57.29577951308232f;

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
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    Vector3 Normalize(const Vector3& value, const Vector3& fallback) {
        const float length = Length(value);
        if (!std::isfinite(length) || length <= kMinVectorLength) {
            return fallback;
        }
        return Scale(value, 1.0f / length);
    }

    float AngleDegrees(const Vector3& lhs, const Vector3& rhs) {
        const Vector3 normalizedLhs = Normalize(lhs, { 0.0f, 0.0f, 0.0f });
        const Vector3 normalizedRhs = Normalize(rhs, { 0.0f, 0.0f, 0.0f });
        if (Length(normalizedLhs) <= kMinVectorLength || Length(normalizedRhs) <= kMinVectorLength) {
            return 180.0f;
        }
        return std::acos(std::clamp(Dot(normalizedLhs, normalizedRhs), -1.0f, 1.0f)) * kRadiansToDegrees;
    }

    float FrameRotationDegrees(
        const Vector3& previousRight,
        const Vector3& previousUp,
        const Vector3& previousForward,
        const Vector3& currentRight,
        const Vector3& currentUp,
        const Vector3& currentForward) {
        return (std::max)({
            AngleDegrees(previousRight, currentRight),
            AngleDegrees(previousUp, currentUp),
            AngleDegrees(previousForward, currentForward),
        });
    }

    Vector3 InverseTransformVector(
        const Vector3& worldVector,
        const Vector3& right,
        const Vector3& up,
        const Vector3& forward) {
        return {
            Dot(worldVector, right),
            Dot(worldVector, up),
            Dot(worldVector, forward),
        };
    }

    Vector3 TransformVector(
        const Vector3& localVector,
        const Vector3& right,
        const Vector3& up,
        const Vector3& forward) {
        return Add(
            Add(Scale(right, localVector.x), Scale(up, localVector.y)),
            Scale(forward, localVector.z));
    }
}

void ProjectileRailMotionAdapter::Initialize(const RailShooterCameraRig* railCameraRig) {
    railCameraRig_ = railCameraRig;
    previousFrame_ = {};
    currentFrame_ = {};
    frameSequence_ = 0;
    initialized_ = true;
    hasCurrentFrame_ = false;
    hasPlayerAliveState_ = false;
    previousPlayerAlive_ = false;
    currentPlayerAlive_ = false;
    frameDeltaReady_ = false;
    forceDisableTracking_ = false;
    forceDisablePlayerTracking_ = false;
    forceDisableEnemyTracking_ = false;
    forceResyncRequested_ = false;
    ResetDiagnostics();
    lastFrameStatus_ = railCameraRig_ ? "初期化済み" : "Camera Rigが未設定";
}

void ProjectileRailMotionAdapter::Finalize() {
    railCameraRig_ = nullptr;
    previousFrame_ = {};
    currentFrame_ = {};
    frameSequence_ = 0;
    initialized_ = false;
    hasCurrentFrame_ = false;
    hasPlayerAliveState_ = false;
    previousPlayerAlive_ = false;
    currentPlayerAlive_ = false;
    frameDeltaReady_ = false;
    forceDisableTracking_ = false;
    forceDisablePlayerTracking_ = false;
    forceDisableEnemyTracking_ = false;
    forceResyncRequested_ = false;
    ResetDiagnostics();
    lastFrameStatus_ = "終了済み";
}

void ProjectileRailMotionAdapter::BeginFrame(bool playerAlive) {
    if (!initialized_) {
        return;
    }

    ++frameSequence_;
    ++beginFrameCount_;
    currentPlayerAlive_ = playerAlive;

    const bool hadPreviousFrame = hasCurrentFrame_;
    const FrameSnapshot sampledFrame = CaptureFrame();
    const FrameSnapshot oldCurrentFrame = currentFrame_;
    previousFrame_ = hadPreviousFrame ? oldCurrentFrame : sampledFrame;
    currentFrame_ = sampledFrame;
    hasCurrentFrame_ = true;

    frameTranslationDistance_ = 0.0f;
    frameRotationDegrees_ = 0.0f;
    if (hadPreviousFrame && previousFrame_.valid && currentFrame_.valid) {
        frameTranslationDistance_ = Length(Subtract(currentFrame_.position, previousFrame_.position));
        frameRotationDegrees_ = FrameRotationDegrees(
            previousFrame_.right,
            previousFrame_.up,
            previousFrame_.forward,
            currentFrame_.right,
            currentFrame_.up,
            currentFrame_.forward);
    }

    const std::string resyncReason = ResolveResyncReason(
        previousFrame_, currentFrame_, hadPreviousFrame, playerAlive);
    forceResyncRequested_ = false;
    if (!resyncReason.empty()) {
        previousFrame_ = currentFrame_;
        frameDeltaReady_ = false;
        RecordFrameSkip(resyncReason, true);
    } else if (!IsBaseTrackingActive()) {
        frameDeltaReady_ = false;
        RecordFrameSkip(ResolveInactiveReason(playerAlive), false);
    } else {
        frameDeltaReady_ = true;
        lastSkipReason_ = "なし";
        lastFrameStatus_ = "Rail Frame差分を適用可能";
    }

    previousPlayerAlive_ = playerAlive;
    hasPlayerAliveState_ = true;
}

void ProjectileRailMotionAdapter::RegisterProjectile(EnemyBullet& projectile) {
    if (!initialized_) {
        return;
    }

    projectile.SetProjectileSpawnSequence(frameSequence_);
    projectile.SetProjectileRailFrameSequence(frameSequence_ > 0 ? frameSequence_ - 1 : 0);
    ++registeredProjectileCount_;
}

void ProjectileRailMotionAdapter::ApplyToProjectile(EnemyBullet& projectile, ProjectileKind kind) {
    if (!initialized_ || frameSequence_ == 0 || !projectile.IsInitialized() ||
        !projectile.IsActive() || projectile.IsDead()) {
        return;
    }

    KindDiagnostics& diagnostics = GetDiagnostics(kind);
    const bool spawnedThisFrame = projectile.GetProjectileSpawnSequence() == frameSequence_;
    const bool processedThisFrame = projectile.GetProjectileRailFrameSequence() == frameSequence_;
    if (processedThisFrame) {
        ++diagnostics.doubleApplyCount;
        ++doubleApplicationCount_;
        doubleApplicationDetected_ = true;
        lastFrameStatus_ = "同一フレームの二重適用を検出";
        return;
    }

    projectile.SetProjectileRailFrameSequence(frameSequence_);
    if (spawnedThisFrame) {
        ++diagnostics.spawnFrameSkipCount;
        ++diagnostics.skipCount;
        return;
    }
    if (!IsTrackingActive(kind) || !frameDeltaReady_) {
        ++diagnostics.skipCount;
        return;
    }

    const Vector3 previousRelativePosition = Subtract(projectile.GetPosition(), previousFrame_.position);
    const Vector3 localPosition = InverseTransformVector(
        previousRelativePosition,
        previousFrame_.right,
        previousFrame_.up,
        previousFrame_.forward);
    const Vector3 transportedPosition = Add(
        currentFrame_.position,
        TransformVector(localPosition, currentFrame_.right, currentFrame_.up, currentFrame_.forward));

    const Vector3 localVelocity = InverseTransformVector(
        projectile.GetVelocity(),
        previousFrame_.right,
        previousFrame_.up,
        previousFrame_.forward);
    const Vector3 transportedVelocity = TransformVector(
        localVelocity,
        currentFrame_.right,
        currentFrame_.up,
        currentFrame_.forward);

    Vector3 transportedVisualForward{};
    const bool hasVisualForwardOverride = projectile.HasVisualForwardOverride();
    if (hasVisualForwardOverride) {
        const Vector3 localVisualForward = InverseTransformVector(
            projectile.GetVisualForwardOverride(),
            previousFrame_.right,
            previousFrame_.up,
            previousFrame_.forward);
        transportedVisualForward = TransformVector(
            localVisualForward,
            currentFrame_.right,
            currentFrame_.up,
            currentFrame_.forward);
    }

    if (!IsFinite(transportedPosition) || !IsFinite(transportedVelocity) ||
        (hasVisualForwardOverride && !IsFinite(transportedVisualForward))) {
        ++diagnostics.skipCount;
        lastSkipReason_ = "変換結果が非有限値のためスキップ";
        lastFrameStatus_ = lastSkipReason_;
        return;
    }

    projectile.SetPosition(transportedPosition);
    projectile.SetVelocity(transportedVelocity);
    if (hasVisualForwardOverride) {
        projectile.SetVisualForwardOverride(transportedVisualForward);
    }

    ++diagnostics.applyCount;
    ++frameDeltaApplyCount_;
}

bool ProjectileRailMotionAdapter::TransportDirectionForProjectile(
    const EnemyBullet& projectile,
    ProjectileKind kind,
    const Vector3& direction,
    Vector3& transportedDirection) const {
    if (!initialized_ || frameSequence_ == 0 || !projectile.IsInitialized()
        || !projectile.IsActive() || projectile.IsDead()
        || projectile.GetProjectileSpawnSequence() == frameSequence_
        || projectile.GetProjectileRailFrameSequence() != frameSequence_
        || !IsTrackingActive(kind) || !frameDeltaReady_ || !IsFinite(direction)) {
        return false;
    }

    const Vector3 localDirection = InverseTransformVector(
        direction,
        previousFrame_.right,
        previousFrame_.up,
        previousFrame_.forward);
    const Vector3 result = TransformVector(
        localDirection,
        currentFrame_.right,
        currentFrame_.up,
        currentFrame_.forward);
    if (!IsFinite(result)) {
        return false;
    }

    transportedDirection = result;
    return true;
}
bool ProjectileRailMotionAdapter::IsTrackingActive(ProjectileKind kind) const {
    if (!IsBaseTrackingActive() || forceDisableTracking_) {
        return false;
    }
    if (kind == ProjectileKind::Player) {
        return !forceDisablePlayerTracking_;
    }
    return !forceDisableEnemyTracking_;
}

bool ProjectileRailMotionAdapter::ShouldSuppressCameraVelocityInheritance() const {
    return IsBaseTrackingActive();
}

void ProjectileRailMotionAdapter::RecordShot(
    ProjectileKind kind,
    const Vector3& muzzleWorldPosition,
    const Vector3& targetWorldPosition,
    const Vector3& shotDirection,
    const Vector3& relativeVelocity) {
    KindDiagnostics& diagnostics = GetDiagnostics(kind);
    diagnostics.lastMuzzleWorldPosition = muzzleWorldPosition;
    diagnostics.lastTargetWorldPosition = targetWorldPosition;
    diagnostics.lastShotDirection = shotDirection;
    diagnostics.lastRelativeVelocity = relativeVelocity;
    ++diagnostics.shotCount;
}

void ProjectileRailMotionAdapter::RecordDespawn(ProjectileKind kind, const std::string& reason) {
    GetDiagnostics(kind).lastDespawnReason = reason.empty() ? "理由未指定" : reason;
}

void ProjectileRailMotionAdapter::ResetDiagnostics() {
    playerDiagnostics_ = {};
    enemyDiagnostics_ = {};
    beginFrameCount_ = 0;
    registeredProjectileCount_ = 0;
    frameDeltaApplyCount_ = 0;
    frameDeltaSkipCount_ = 0;
    frameResyncCount_ = 0;
    doubleApplicationCount_ = 0;
    doubleApplicationDetected_ = false;
    lastSkipReason_ = "未評価";
    lastFrameStatus_ = initialized_ ? "診断をリセットしました" : "未初期化";
}

void ProjectileRailMotionAdapter::ClearForcedStates() {
    forceDisableTracking_ = false;
    forceDisablePlayerTracking_ = false;
    forceDisableEnemyTracking_ = false;
    forceResyncRequested_ = initialized_;
}

ProjectileRailMotionAdapter::FrameSnapshot ProjectileRailMotionAdapter::CaptureFrame() const {
    FrameSnapshot result;
    if (!railCameraRig_) {
        return result;
    }

    const RailShooterCameraRig::ProjectileRailFrame source = railCameraRig_->GetProjectileRailFrame();
    result.position = source.position;
    result.right = source.right;
    result.up = source.up;
    result.forward = source.forward;
    result.railDistance = source.railDistance;
    result.revision = source.railRevision;
    result.continuityRevision = source.continuityRevision;
    result.railIndex = source.railIndex;
    result.railActive = source.railActive;
    result.running = source.running;
    result.runtimeV2Active = source.runtimeV2Active;
    result.gameModeActive = source.gameModeActive;
    result.valid = source.valid &&
        IsFinite(source.position) && IsFinite(source.right) && IsFinite(source.up) && IsFinite(source.forward) &&
        std::isfinite(source.railDistance) &&
        Length(source.right) > kMinVectorLength &&
        Length(source.up) > kMinVectorLength &&
        Length(source.forward) > kMinVectorLength;
    if (result.valid) {
        result.right = Normalize(source.right, { 1.0f, 0.0f, 0.0f });
        result.up = Normalize(source.up, { 0.0f, 1.0f, 0.0f });
        result.forward = Normalize(source.forward, { 0.0f, 0.0f, 1.0f });
    }
    return result;
}

std::string ProjectileRailMotionAdapter::ResolveResyncReason(
    const FrameSnapshot& previous,
    const FrameSnapshot& current,
    bool hadPreviousFrame,
    bool playerAlive) const {
    if (forceResyncRequested_) {
        return "強制再同期";
    }
    if (!hadPreviousFrame) {
        return "初回Frame同期";
    }
    if (previous.valid != current.valid) {
        return "Rail Frame有効状態が変化";
    }
    if (!current.valid) {
        return {};
    }
    if (previous.revision != current.revision) {
        return "Rail Revisionが変化";
    }
    if (previous.continuityRevision != current.continuityRevision) {
        return "Rail連続性Revisionが変化";
    }
    if (previous.railIndex != current.railIndex) {
        return "Rail Indexが変化";
    }
    if (previous.runtimeV2Active != current.runtimeV2Active) {
        return "Runtime V2 / Legacyが切り替わった";
    }
    if (previous.gameModeActive != current.gameModeActive) {
        return "GameMode状態が変化";
    }
    if (previous.railActive != current.railActive) {
        return "Rail有効状態が変化";
    }
    if (previous.running != current.running) {
        return "Rail停止 / 再開状態が変化";
    }
    if (hasPlayerAliveState_ && previousPlayerAlive_ != playerAlive) {
        return "Player生存状態が変化";
    }
    if (current.railDistance + kRailDistanceRewindTolerance < previous.railDistance) {
        return "Rail Distanceが巻き戻った";
    }
    if (frameTranslationDistance_ > maxFrameTranslation_) {
        return "Frame移動量が上限を超えた";
    }
    if (frameRotationDegrees_ > maxFrameRotationDegrees_) {
        return "Frame回転量が上限を超えた";
    }
    return {};
}

std::string ProjectileRailMotionAdapter::ResolveInactiveReason(bool playerAlive) const {
    if (!currentFrame_.valid) {
        return railCameraRig_ ? "Rail Frameが無効" : "Camera Rigが未設定";
    }
    if (!currentFrame_.gameModeActive) {
        return "GameMode外のため追従停止";
    }
    if (!currentFrame_.railActive) {
        return "Rail走行が無効";
    }
    if (!currentFrame_.running) {
        return "Railが停止中";
    }
    if (!playerAlive) {
        return "Player非生存のため追従停止";
    }
    return "Rail Frame差分を適用できない";
}

bool ProjectileRailMotionAdapter::IsBaseTrackingActive() const {
    return initialized_ && hasCurrentFrame_ && currentFrame_.valid &&
        currentFrame_.gameModeActive && currentFrame_.railActive && currentFrame_.running && currentPlayerAlive_;
}

ProjectileRailMotionAdapter::KindDiagnostics& ProjectileRailMotionAdapter::GetDiagnostics(ProjectileKind kind) {
    return kind == ProjectileKind::Player ? playerDiagnostics_ : enemyDiagnostics_;
}

const ProjectileRailMotionAdapter::KindDiagnostics& ProjectileRailMotionAdapter::GetDiagnostics(ProjectileKind kind) const {
    return kind == ProjectileKind::Player ? playerDiagnostics_ : enemyDiagnostics_;
}

void ProjectileRailMotionAdapter::RecordFrameSkip(const std::string& reason, bool resync) {
    ++frameDeltaSkipCount_;
    if (resync) {
        ++frameResyncCount_;
    }
    lastSkipReason_ = reason.empty() ? "理由未指定" : reason;
    lastFrameStatus_ = resync ? "Rail Frameを再同期" : "Rail Frame差分をスキップ";
}
