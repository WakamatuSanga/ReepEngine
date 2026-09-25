#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"

#include "Engine/Game/KrakenTentacleWeakPointAnchorSnapshot.h"

#include <algorithm>
#include <cmath>
#include <limits>

bool KrakenTentacleMidbossController::TryGetWeakPointLockOnAnchorSnapshot(
    std::size_t chainIndex,
    KrakenTentacleWeakPointAnchorSnapshot& outSnapshot) const {
    outSnapshot = {};
    if (!impl_ || !impl_->initialized) {
        return false;
    }

    bool found = false;
    for (const KrakenTentacleMidbossTipSnapshot& snapshot :
         impl_->tipSnapshots) {
        if (static_cast<std::size_t>(snapshot.chainIndex) != chainIndex ||
            snapshot.role != KrakenColliderPreviewRole::WeakPoint) {
            continue;
        }
        if (found) {
            outSnapshot = {};
            return false;
        }

        outSnapshot.colliderId = snapshot.colliderId;
        outSnapshot.chainIndex = snapshot.chainIndex;
        outSnapshot.worldCenter = snapshot.worldPosition;
        outSnapshot.worldRadius = snapshot.worldRadius;
        outSnapshot.enabled = snapshot.enabled && !impl_->entranceActive;
        outSnapshot.valid = snapshot.valid;
        found = true;
    }
    return found;
}

bool KrakenTentacleMidbossController::TryGetChainLockOnGeometrySnapshot(
    std::size_t chainIndex,
    KrakenTentacleLockOnGeometrySnapshot& outSnapshot) const {
    outSnapshot = {};
    if (!impl_ || !impl_->IsVisible() || impl_->entranceActive ||
        impl_->IsDefeatState() || impl_->health.IsDefeatPending() ||
        chainIndex >= impl_->chains.size() ||
        chainIndex >= impl_->colliderDefinitions.size()) {
        return false;
    }
    const auto& joints = impl_->chains[chainIndex].joints;
    if (joints.size() < 2) {
        return false;
    }
    const auto bonePosition = [this](int jointIndex, Vector3& position) {
        for (const auto& bone : impl_->boneSnapshots) {
            if (bone.jointIndex == jointIndex && bone.valid) {
                position = bone.worldPosition;
                return true;
            }
        }
        return false;
    };
    outSnapshot.segments.reserve(joints.size());
    for (std::size_t index = 0; index + 1 < joints.size(); ++index) {
        KrakenTentacleLockOnSegment segment{};
        if (!bonePosition(joints[index], segment.worldStart) ||
            !bonePosition(joints[index + 1], segment.worldEnd)) {
            outSnapshot = {};
            return false;
        }
        // Subdivide along every current-pose bone, using the existing local section's radius.
        const float midpoint = (static_cast<float>(index) + 0.5f) /
            static_cast<float>(joints.size() - 1);
        float bestDistance = (std::numeric_limits<float>::max)();
        for (const auto& definition : impl_->colliderDefinitions[chainIndex].capsules) {
            const float distance = std::fabs(midpoint - std::clamp(midpoint,
                definition.normalizedStart, definition.normalizedEnd));
            if (distance >= bestDistance) {
                continue;
            }
            for (const auto& capsule : impl_->capsuleSnapshots) {
                if (capsule.chainIndex == chainIndex && capsule.valid &&
                    capsule.colliderIndex == definition.colliderIndex) {
                    segment.worldRadius = capsule.worldRadius;
                    bestDistance = distance;
                    break;
                }
            }
        }
        if (!std::isfinite(segment.worldRadius) || segment.worldRadius <= 0.0f) {
            outSnapshot = {};
            return false;
        }
        outSnapshot.segments.push_back(segment);
    }
    KrakenTentacleWeakPointAnchorSnapshot weakPoint{};
    if (!TryGetWeakPointLockOnAnchorSnapshot(chainIndex, weakPoint) ||
        !weakPoint.valid || !weakPoint.enabled) {
        outSnapshot = {};
        return false;
    }
    // Marker and homing both consume the evaluated collision snapshot.
    outSnapshot.markerWorldPosition = weakPoint.worldCenter;
    // Preserve the existing tip selection coverage; this is not a damage collider.
    const Vector3 tipPosition = outSnapshot.segments.back().worldEnd;
    outSnapshot.segments.push_back({ tipPosition, tipPosition, weakPoint.worldRadius });
    return true;
}
