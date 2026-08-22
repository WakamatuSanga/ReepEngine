#include "KrakenTentacleMidbossController.h"

#include "KrakenTentacleMidbossControllerInternal.h"

#include <cmath>

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
    return impl_ ? impl_->cameraForwardOffset : 0.0f;
}

float KrakenTentacleMidbossController::GetCameraRightOffset() const {
    return impl_ ? impl_->cameraRightOffset : 0.0f;
}

float KrakenTentacleMidbossController::GetCameraUpOffset() const {
    return impl_ ? impl_->cameraUpOffset : 0.0f;
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
