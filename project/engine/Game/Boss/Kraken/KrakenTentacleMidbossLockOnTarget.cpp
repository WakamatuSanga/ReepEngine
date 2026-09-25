#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"

#include "Engine/Game/KrakenTentacleWeakPointAnchorSnapshot.h"

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
        outSnapshot.enabled = snapshot.enabled;
        outSnapshot.valid = snapshot.valid;
        found = true;
    }
    return found;
}
