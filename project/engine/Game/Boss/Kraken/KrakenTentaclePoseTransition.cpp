#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"
#include "Engine/Animation/AnimationClip.h"
#include "Engine/Animation/Skeleton.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace {
Quaternion EulerRotation(const Vector3& r) {
    const float sx = std::sin(r.x * 0.5f), cx = std::cos(r.x * 0.5f);
    const float sy = std::sin(r.y * 0.5f), cy = std::cos(r.y * 0.5f);
    const float sz = std::sin(r.z * 0.5f), cz = std::cos(r.z * 0.5f);
    // Current Pose stores row-vector Rx * Ry * Rz (quaternion qz * qy * qx).
    // This is the inverse of ConvertQuaternionToEulerXYZ, including compound bends.
    return { sx * cy * cz - cx * sy * sz, cx * sy * cz + sx * cy * sz,
        cx * cy * sz - sx * sy * cz, cx * cy * cz + sx * sy * sz };
}

Vector3 BlendRotation(const Vector3& from, const Vector3& to, float t) {
    if (t <= 0.0f) return from;
    if (t >= 1.0f) return to;
    // Reuse animation sampling's shortest-path quaternion SLERP.
    return ConvertQuaternionToEulerXYZ(CalculateValue(
        std::vector<KeyframeQuaternion>{ {0.0f, EulerRotation(from)},
            {1.0f, EulerRotation(to)} }, t));
}

Vector3 TransformPosition(const Vector3& p, const Matrix4x4& m) {
    return { p.x * m.m[0][0] + p.y * m.m[1][0] + p.z * m.m[2][0] + m.m[3][0],
        p.x * m.m[0][1] + p.y * m.m[1][1] + p.z * m.m[2][1] + m.m[3][1],
        p.x * m.m[0][2] + p.y * m.m[1][2] + p.z * m.m[2][2] + m.m[3][2] };
}

Vector3 TransformDirection(const Vector3& v, const Matrix4x4& m) {
    return {v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0],
        v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1],
        v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2]};
}

Vector3 Scale(const Vector3& v, float factor) {
    return {v.x * factor, v.y * factor, v.z * factor};
}

Vector3 Subtract(const Vector3& a, const Vector3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

float Dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float Length(const Vector3& v) {
    return std::sqrt(Dot(v, v));
}

Vector3 Normalize(const Vector3& v, const Vector3& fallback) {
    const float length = Length(v);
    return std::isfinite(length) && length > 0.00001f ? Scale(v, 1.0f / length) : fallback;
}

Matrix4x4 AimChild(const Vector3& from, const Vector3& to) {
    const float cosine = std::clamp(Dot(from, to), -1.0f, 1.0f);
    Vector3 axis = MatrixMath::Cross(from, to);
    if (cosine < -0.99999f) {
        const Vector3 reference = std::abs(from.y) < 0.9f ? Vector3{0, 1, 0} : Vector3{1, 0, 0};
        axis = Normalize(MatrixMath::Cross(from, reference), {1, 0, 0});
    }
    const float scalar = cosine < -0.99999f ? 0.0f : 1.0f + cosine;
    const float inverseLength = 1.0f / std::sqrt(Dot(axis, axis) + scalar * scalar);
    const Quaternion rotation{axis.x * inverseLength, axis.y * inverseLength,
        axis.z * inverseLength, scalar * inverseLength};
    return MatrixMath::MakeAffine({1, 1, 1}, ConvertQuaternionToEulerXYZ(rotation), {});
}

float SmoothProgress(float time, float duration) {
    const float t = std::clamp(time / duration, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
}

bool KrakenTentacleMidbossController::Impl::ApplySweepPose() {
    const auto& chain = chains[selectedAttackChainIndex].joints;
    constexpr size_t boneCount = 10;
    constexpr float hiddenDrop = 24.0f;
    if (chain.size() != boneCount || attackEntryPose.size() != chain.size()) {
        return false;
    }

    KrakenTentacleIdlePoseResult idle{};
    if (!BuildKrakenTentacleIdlePose(idleSettings, idleTime, chains, true,
            selectedAttackChainIndex, skeleton->joints.size(), skeleton->root, idle) || !idle.valid) {
        return false;
    }
    std::array<Vector3, boneCount> idleRotations{};
    for (size_t i = 0; i < chain.size(); ++i) idleRotations[i] = bindLocalEulerRadians[chain[i]];
    for (const auto& joint : idle.joints) {
        const auto& offset = joint.localEulerOffsetRadians;
        auto& rotation = joint.chainIndex == selectedAttackChainIndex
            ? idleRotations[joint.chainBoneIndex] : skeleton->joints[joint.jointIndex].localRotate;
        rotation = {rotation.x + offset.x, rotation.y + offset.y, rotation.z + offset.z};
    }

    UpdateSkeletonWorldTransforms(*skeleton);
    Joint& root = skeleton->joints[chain.front()];
    const auto& parent = skeleton->joints[root.parentIndex];
    const auto rootParentWorld = MatrixMath::Multipty(parent.worldMatrix, worldMatrix);
    const auto moveRoot = [&](const Vector3& worldOffset) {
        const auto offset = TransformDirection(worldOffset, MatrixMath::Inverse(rootParentWorld));
        root.localTranslate = {root.localTranslate.x + offset.x, root.localTranslate.y + offset.y,
            root.localTranslate.z + offset.z};
    };
    if (state == KrakenTentacleMidbossState::SweepPull) {
        for (size_t i = 0; i < chain.size(); ++i) {
            skeleton->joints[chain[i]].localRotate = attackEntryPose[i];
        }
        moveRoot({0.0f, -hiddenDrop * SmoothProgress(stateElapsedTime, sweep.settings.pull), 0.0f});
        return true;
    }
    if (state == KrakenTentacleMidbossState::SweepCounter || state == KrakenTentacleMidbossState::SweepReturn) {
        for (size_t i = 0; i < chain.size(); ++i) {
            skeleton->joints[chain[i]].localRotate = idleRotations[i];
        }
        const float rise = state == KrakenTentacleMidbossState::SweepReturn
            ? SmoothProgress(stateElapsedTime, sweep.settings.restore) : 0.0f;
        moveRoot({0.0f, -hiddenDrop * (1.0f - rise), 0.0f});
        return true;
    }

    float across = sweep.launchX;
    if (state == KrakenTentacleMidbossState::SweepStrike) {
        const float time = std::clamp(stateElapsedTime / sweep.settings.strike, 0.0f, 1.0f);
        across = std::lerp(sweep.launchX, sweep.strikeEndX, time * (2.0f - time));
    } else if (state == KrakenTentacleMidbossState::SweepWithdraw) {
        sweep.withdrawOffsetX = (sweep.launchX - sweep.strikeEndX) *
            SmoothProgress(stateElapsedTime, sweep.settings.withdraw);
        across = sweep.strikeEndX + sweep.withdrawOffsetX;
    }
    const Vector3 target = {sweep.bandCenter.x + sweep.right.x * across,
        sweep.bandCenter.y + sweep.right.y * across, sweep.bandCenter.z + sweep.right.z * across};
    // Pose the whole chain along the band. Its base recedes into depth on the
    // entry side while the distal section stays level with the player's plane.
    const Vector3 inward = Scale(sweep.right, -sweep.entrySide);
    const Vector3 forward = MatrixMath::Cross(sweep.right, sweep.up);
    Matrix4x4 parentWorld = rootParentWorld;
    for (size_t i = 0; i + 1 < chain.size(); ++i) {
        Joint& joint = skeleton->joints[chain[i]];
        const Joint& child = skeleton->joints[chain[i + 1]];
        const float along = static_cast<float>(i) / static_cast<float>(chain.size() - 2);
        const Vector3 direction = Subtract(inward, Scale(forward, std::lerp(0.55f, 0.04f, along)));
        const auto restRotation = MatrixMath::MakeAffine({1, 1, 1}, joint.localRotate, {});
        const Vector3 childOffset{child.localTranslate.x * joint.localScale.x,
            child.localTranslate.y * joint.localScale.y, child.localTranslate.z * joint.localScale.z};
        const auto from = Normalize(TransformDirection(childOffset, restRotation), {0, 1, 0});
        const auto to = Normalize(TransformDirection(direction, MatrixMath::Inverse(parentWorld)), from);
        joint.localRotate = MatrixMath::ExtractEulerXYZ(MatrixMath::Multipty(restRotation, AimChild(from, to)));
        parentWorld = MatrixMath::Multipty(
            MatrixMath::MakeAffine(joint.localScale, joint.localRotate, joint.localTranslate), parentWorld);
    }
    const Vector3 tip = TransformPosition(skeleton->joints[chain.back()].localTranslate, parentWorld);
    moveRoot(Subtract(target, tip));
    return true;
}

float EvaluateKrakenTentacleSlamFlex(
    KrakenTentacleAttackPreviewPhase phase, float progress, float upperPosition) {
    float time = 0.0f, amplitude = 0.0f;
    if (phase == KrakenTentacleAttackPreviewPhase::Slam) {
        time = std::cbrt(std::clamp(progress, 0.0f, 1.0f));
        amplitude = -4.0f;
    } else if (phase == KrakenTentacleAttackPreviewPhase::Recovery) {
        time = 1.0f - std::clamp(progress, 0.0f, 1.0f);
        amplitude = 0.6f;
    } else {
        return 0.0f;
    }
    const float delay = 0.08f + 0.16f * upperPosition;
    const float t = std::clamp((time - delay) / (1.0f - delay), 0.0f, 1.0f);
    if (t <= 0.0f || t >= 1.0f) return 0.0f;
    const float wave = std::sin(std::numbers::pi_v<float> * t);
    // Per-bone delay and a zero-slope envelope; no added bend at the hit endpoint.
    return amplitude * wave * wave;
}

bool KrakenTentacleMidbossController::Impl::ApplyWholeSlamPoseToSkeleton(
    Skeleton& targetSkeleton, std::size_t chainIndex,
    KrakenTentacleAttackPreviewPhase phase, float phaseTime, float poseIdleTime,
    const KrakenTentacleAttackPoseTotals& totals, const Vector3& attackTarget,
    KrakenTentacleWholeSlamPoseDiagnostics* poseDiagnostics) const {
    if (chainIndex >= chains.size()) return false;
    // The caller restores bind locals and placement first: aiming never uses the sway.
    UpdateSkeletonWorldTransforms(targetSkeleton);
    KrakenTentacleAttackPoseResult pose{};
    KrakenTentacleWholeSlamPoseDiagnostics evaluated{};
    if (!BuildKrakenTentacleWholeSlamPose(
            attackSettings, wholeSlamDiagnostics.targetSettings, phase,
            totals, chains[chainIndex].joints, bindLocalEulerRadians,
            targetSkeleton, worldMatrix, attackTarget, pose, evaluated) || !pose.valid) {
        return false;
    }

    auto idleRotations = bindLocalEulerRadians;
    if (idleSwayEnabled) {
        KrakenTentacleIdlePoseResult idle{};
        if (!BuildKrakenTentacleIdlePose(idleSettings, poseIdleTime, chains,
                true, chainIndex, targetSkeleton.joints.size(), targetSkeleton.root, idle)) {
            return false;
        }
        for (const auto& joint : idle.joints) {
            auto& rotation = idleRotations[joint.jointIndex];
            rotation.x += joint.localEulerOffsetRadians.x;
            rotation.y += joint.localEulerOffsetRadians.y;
            rotation.z += joint.localEulerOffsetRadians.z;
            if (joint.chainIndex != chainIndex) {
                targetSkeleton.joints[joint.jointIndex].localRotate = rotation;
            }
        }
    }
    const float duration = GetKrakenTentacleAttackPhaseDuration(attackSettings, phase);
    const float t = duration > 0.00001f ? std::clamp(phaseTime / duration, 0.0f, 1.0f) : 1.0f;
    const float blend = t * t * (3.0f - 2.0f * t);
    const bool hasEntry = poseDiagnostics && chainIndex == wholeSlamDiagnostics.chainIndex &&
        attackEntryPose.size() == chains[chainIndex].joints.size();
    for (const auto& joint : pose.joints) {
        Vector3 rotation = joint.absoluteLocalEulerRadians;
        if (phase == KrakenTentacleAttackPreviewPhase::Windup) {
            rotation = BlendRotation(hasEntry ? attackEntryPose[joint.chainBoneIndex] :
                idleRotations[joint.jointIndex], rotation, blend);
        } else if (phase == KrakenTentacleAttackPreviewPhase::Recovery) {
            // Blend directly to the running Idle clock, not the old entry snapshot.
            rotation = BlendRotation(rotation, idleRotations[joint.jointIndex], blend);
        }
        targetSkeleton.joints[joint.jointIndex].localRotate = rotation;
    }
    UpdateSkeletonWorldTransforms(targetSkeleton);
    const int hinge = chains[chainIndex].joints[(std::min)(
        static_cast<std::size_t>(attackSettings.fixedLeadingBoneCount),
        chains[chainIndex].joints.size() - 1)];
    evaluated.pivotWorldPosition = TransformPosition(targetSkeleton.joints[hinge].worldTranslate, worldMatrix);
    evaluated.tipWorldPosition = TransformPosition(
        targetSkeleton.joints[chains[chainIndex].joints.back()].worldTranslate, worldMatrix);
    Vector3 direction = { evaluated.tipWorldPosition.x - evaluated.pivotWorldPosition.x,
        evaluated.tipWorldPosition.y - evaluated.pivotWorldPosition.y,
        evaluated.tipWorldPosition.z - evaluated.pivotWorldPosition.z };
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    evaluated.tentacleWorldDirection = length > 0.00001f
        ? Vector3{ direction.x / length, direction.y / length, direction.z / length } : Vector3{};
    if (poseDiagnostics) *poseDiagnostics = evaluated;
    return true;
}
