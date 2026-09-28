#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"
#include "Engine/Animation/AnimationClip.h"
#include "Engine/Animation/Skeleton.h"

#include <algorithm>
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
