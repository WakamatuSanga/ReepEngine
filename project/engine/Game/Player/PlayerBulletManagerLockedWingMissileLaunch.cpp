#include "PlayerBulletManager.h"
#include "LockedWingMissileExhaustController.h"

void PlayerBulletManager::DrawLockedWingMissileLaunchImGui() {
    DrawLockedWingMissileIgnitionImGui();
    DrawLockedWingHomingImGui();
    if (lockedWingMissileExhaustController_) {
        lockedWingMissileExhaustController_->DrawImGui();
    }
}
