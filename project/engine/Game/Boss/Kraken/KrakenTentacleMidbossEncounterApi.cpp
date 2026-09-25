#include "KrakenTentacleMidbossController.h"

#include "KrakenTentacleMidbossControllerInternal.h"

#include "Engine/Game/KrakenTentacleFramingSnapshot.h"
#include "Engine/Game/Player/Player.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
void KrakenTentacleMidbossController::SetDebugHpOne(bool enabled) {
    if (!impl_ || impl_->debugHpOneEnabled == enabled) {
        return;
    }
    impl_->debugHpOneEnabled = enabled;
    impl_->health.SetDebugHpOne(enabled,
        impl_->initialized && impl_->IsVisible() && !impl_->IsDefeatState() &&
        !impl_->defeatStarted && !impl_->defeatCompleted);
}
#endif

bool KrakenTentacleMidbossController::ResetForWaveEncounter() {
    if (!impl_ || !impl_->initialized) {
        return false;
    }
    impl_->Reset();
    return impl_->state == KrakenTentacleMidbossState::Hidden &&
        !impl_->health.IsDefeatPending() && !impl_->defeatStarted &&
        !impl_->defeatCompleted &&
        std::fabs(impl_->health.GetCurrentHp() - impl_->health.GetMaxHp()) <=
            0.0001f;
}

bool KrakenTentacleMidbossController::PlaceInFrontOfCameraForWaveEncounter() {
    return impl_ && impl_->PlaceInFrontOfCamera();
}

bool KrakenTentacleMidbossController::ShowForWaveEncounter() {
    return impl_ && impl_->Show();
}

void KrakenTentacleMidbossController::HideForWaveEncounter() {
    if (impl_) {
        impl_->Hide();
    }
}

bool KrakenTentacleMidbossController::SetSelectedAttackChainForWaveEncounter(
    std::size_t chainIndex) {
    if (!impl_ || chainIndex >= impl_->chains.size()) {
        return false;
    }
    impl_->selectedAttackChainIndex = chainIndex;
    return true;
}

bool KrakenTentacleMidbossController::TryStartAttackForWaveEncounter() {
    return impl_ && impl_->StartAttack();
}

std::size_t KrakenTentacleMidbossController::GetDetectedChainCount() const {
    return impl_ ? impl_->chains.size() : 0;
}

std::size_t KrakenTentacleMidbossController::GetSelectedAttackChain() const {
    return impl_ ? impl_->selectedAttackChainIndex : 0;
}

Vector3 KrakenTentacleMidbossController::GetWorldPosition() const {
    return impl_ ? impl_->worldPosition : Vector3{};
}

float KrakenTentacleMidbossController::GetCameraForwardOffset() const {
    return impl_ ? impl_->placementSettings.forwardOffset : 0.0f;
}

float KrakenTentacleMidbossController::GetCameraRightOffset() const {
    return impl_ ? impl_->placementSettings.rightOffset : 0.0f;
}

float KrakenTentacleMidbossController::GetCameraUpOffset() const {
    return impl_ ? impl_->placementSettings.upOffset : 0.0f;
}

void KrakenTentacleMidbossController::Impl::ApplyRecommendedPlacementSettings() {
    placementSettings = {};
    worldScale = {
        placementSettings.uniformScale,
        placementSettings.uniformScale,
        placementSettings.uniformScale };
}

bool KrakenTentacleMidbossController::Impl::FacePlayer() {
    if (!collisionPlayer) {
        lastError = "Playerが未接続のため正面を向けません。";
        return false;
    }
    const Vector3 playerPosition = collisionPlayer->GetWorldPosition();
    const Vector3 toPlayer = {
        playerPosition.x - worldPosition.x,
        0.0f,
        playerPosition.z - worldPosition.z };
    const float length = std::sqrt(
        toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
    if (!std::isfinite(length) || length <= 0.00001f) {
        lastError = "Player方向をXZ平面で計算できません。";
        return false;
    }
    const float facingYaw = std::atan2(toPlayer.x, toPlayer.z);
    if (!std::isfinite(facingYaw) ||
        !std::isfinite(placementSettings.modelFacingYawOffset)) {
        lastError = "Player方向のYawが有限値ではありません。";
        return false;
    }
    worldRotation = {
        0.0f,
        facingYaw + placementSettings.modelFacingYawOffset,
        0.0f };
    placementDiagnostics.lastFacingYaw = facingYaw;
    ++placementDiagnostics.facingApplyCount;
    lastError.clear();
    return true;
}

void KrakenTentacleMidbossController::Impl::ResetDefeatVisibilityDiagnostics() {
    defeatDiagnostics.retreatVisibleUpdateCount = 0;
    defeatDiagnostics.instantHideDetectionCount = 0;
    defeatDiagnostics.currentFallDistance = 0.0f;
}

void KrakenTentacleMidbossController::SetWaveEncounterControlActive(
    bool active) {
    if (impl_) {
        impl_->waveEncounterControlActive = active;
    }
}

bool KrakenTentacleMidbossController::IsWaveEncounterControlActive() const {
    return impl_ && impl_->waveEncounterControlActive;
}

bool KrakenTentacleMidbossController::TryGetFramingSnapshot(
    KrakenTentacleFramingSnapshot& outSnapshot) const {
    outSnapshot = {};
    if (!impl_ || !impl_->initialized) {
        return false;
    }
    outSnapshot.screenMinimum =
        impl_->placementDiagnostics.screenBoundsMinimum;
    outSnapshot.screenMaximum =
        impl_->placementDiagnostics.screenBoundsMaximum;
    outSnapshot.screenHeightOccupancy =
        impl_->placementDiagnostics.screenHeightOccupancy;
    outSnapshot.screenBoundsValid =
        impl_->placementDiagnostics.screenBoundsValid;
    outSnapshot.rootSideHidden =
        impl_->placementDiagnostics.rootSideHidden;
    outSnapshot.nearPlaneWarning =
        impl_->placementDiagnostics.nearPlaneWarning;
    const std::size_t chainCount = (std::min)(
        impl_->chains.size(),
        KrakenTentacleFramingSnapshot::kChainCapacity);
    for (std::size_t chainIndex = 0; chainIndex < chainCount; ++chainIndex) {
        const KrakenTentacleChain& chain = impl_->chains[chainIndex];
        if (chain.joints.empty()) {
            continue;
        }
        const int tipJointIndex = chain.joints.back();
        const int midpointJointIndex = chain.joints[chain.joints.size() / 2];
        for (const KrakenTentacleMidbossBoneSnapshot& bone :
            impl_->boneSnapshots) {
            if (!bone.valid) {
                continue;
            }
            if (bone.jointIndex == tipJointIndex) {
                outSnapshot.tipWorldPositions[chainIndex] =
                    bone.worldPosition;
                outSnapshot.tipValid[chainIndex] = true;
            }
            if (bone.jointIndex == midpointJointIndex) {
                outSnapshot.upperMidpointWorldPositions[chainIndex] =
                    bone.worldPosition;
                outSnapshot.upperMidpointValid[chainIndex] = true;
            }
        }
    }
    return true;
}
