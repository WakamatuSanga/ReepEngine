#include "PlayerBulletManager.h"
#include "LockedWingMissileExhaustController.h"

void PlayerBulletManager::DrawLockedWingMissileLaunchImGui() {
    DrawLockedWingMissileIgnitionImGui();
    if (lockedWingMissileExhaustController_) {
        lockedWingMissileExhaustController_->DrawImGui();
    }
}