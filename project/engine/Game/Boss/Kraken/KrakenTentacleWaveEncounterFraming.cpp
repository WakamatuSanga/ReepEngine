#include "KrakenTentacleWaveEncounterController.h"

#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossController.h"
#include "Engine/Game/KrakenTentacleFramingSnapshot.h"
#include "Engine/Game/KrakenTentacleWeakPointAnchorSnapshot.h"
#include "Engine/Graphics/Camera/Camera.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
constexpr float kMinimumFov = 0.01f;
constexpr float kMaximumFov = 3.0f;
constexpr float kMaximumAdditionDegrees = 20.0f;
constexpr float kFovEpsilon = 0.0001f;

float SmoothStep(float value) {
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

bool IsFinite(const Vector3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

Vector3 Add(const Vector3& lhs, const Vector3& rhs) {
    return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

Vector3 Scale(const Vector3& value, float scale) {
    return { value.x * scale, value.y * scale, value.z * scale };
}

float Dot(const Vector3& lhs, const Vector3& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vector3 Normalize(const Vector3& value) {
    const float length = std::sqrt(Dot(value, value));
    return std::isfinite(length) && length > kFovEpsilon
        ? Scale(value, 1.0f / length)
        : Vector3{};
}

Vector3 Lerp(const Vector3& begin, const Vector3& end, float t) {
    return {
        begin.x + (end.x - begin.x) * t,
        begin.y + (end.y - begin.y) * t,
        begin.z + (end.z - begin.z) * t };
}

bool NearlyEqual(const Vector3& lhs, const Vector3& rhs) {
    return std::fabs(lhs.x - rhs.x) <= kFovEpsilon &&
        std::fabs(lhs.y - rhs.y) <= kFovEpsilon &&
        std::fabs(lhs.z - rhs.z) <= kFovEpsilon;
}

bool IsVisibleInCamera(
    const Vector3& worldPosition,
    const Matrix4x4& viewProjection) {
    if (!IsFinite(worldPosition)) {
        return false;
    }
    const float clipX = worldPosition.x * viewProjection.m[0][0] +
        worldPosition.y * viewProjection.m[1][0] +
        worldPosition.z * viewProjection.m[2][0] +
        viewProjection.m[3][0];
    const float clipY = worldPosition.x * viewProjection.m[0][1] +
        worldPosition.y * viewProjection.m[1][1] +
        worldPosition.z * viewProjection.m[2][1] +
        viewProjection.m[3][1];
    const float clipZ = worldPosition.x * viewProjection.m[0][2] +
        worldPosition.y * viewProjection.m[1][2] +
        worldPosition.z * viewProjection.m[2][2] +
        viewProjection.m[3][2];
    const float clipW = worldPosition.x * viewProjection.m[0][3] +
        worldPosition.y * viewProjection.m[1][3] +
        worldPosition.z * viewProjection.m[2][3] +
        viewProjection.m[3][3];
    if (!std::isfinite(clipX) || !std::isfinite(clipY) ||
        !std::isfinite(clipZ) || !std::isfinite(clipW) ||
        clipW <= kFovEpsilon) {
        return false;
    }
    const float x = clipX / clipW;
    const float y = clipY / clipW;
    const float z = clipZ / clipW;
    return x >= -1.0f && x <= 1.0f &&
        y >= -1.0f && y <= 1.0f && z >= 0.0f && z <= 1.0f;
}
}

void KrakenTentacleWaveEncounterController::BeginFovOverride() {
    if (!camera_) {
        return;
    }
    const float currentFov = camera_->GetFovY();
    const Vector3 currentViewOffset = camera_->GetViewTranslationOffset();
    const Matrix4x4& cameraWorld = camera_->GetWorldMatrix();
    const Vector3 cameraForward = Normalize({
        cameraWorld.m[2][0], cameraWorld.m[2][1], cameraWorld.m[2][2] });
    if (!std::isfinite(currentFov) || currentFov < kMinimumFov ||
        currentFov > kMaximumFov || !IsFinite(currentViewOffset) ||
        !IsFinite(cameraForward) || Dot(cameraForward, cameraForward) <= 0.0f) {
        ++cameraMissingCount_;
        lastWarning_ =
            "通常カメラ構図が不正なためウェーブ4構図補正を開始できません。";
        return;
    }
    if (!baseFovCaptured_) {
        baseFovY_ = currentFov;
        baseViewTranslationOffset_ = currentViewOffset;
        baseFovCaptured_ = true;
    }
    compositionCameraForward_ = cameraForward;
    const float additionDegrees = std::clamp(
        wave4FovAdditionDegrees_, 0.0f, kMaximumAdditionDegrees);
    wave4FovY_ = std::clamp(
        baseFovY_ + additionDegrees *
            std::numbers::pi_v<float> / 180.0f,
        kMinimumFov, kMaximumFov);
    wave4ViewTranslationOffset_ = Add(
        Add(baseViewTranslationOffset_,
            { 0.0f, wave4VerticalViewOffset_, 0.0f }),
        Scale(compositionCameraForward_, -wave4BackwardViewOffset_));
    fovBlendStartY_ = currentFov;
    viewBlendStartTranslationOffset_ = currentViewOffset;
    fovBlendElapsed_ = 0.0f;
    fovBlendProgress_ = 0.0f;
    fovOverrideRequested_ = true;
    fovOverrideActive_ = true;
    fovBlendActive_ = true;
    fovRestoreVerified_ = false;
    viewTranslationRestoreVerified_ = false;
}

void KrakenTentacleWaveEncounterController::EndFovOverride(bool immediate) {
    if (!baseFovCaptured_ || !camera_) {
        fovOverrideRequested_ = false;
        fovOverrideActive_ = false;
        fovBlendActive_ = false;
        return;
    }
    const bool wasRequested = fovOverrideRequested_;
    fovOverrideRequested_ = false;
    if (immediate) {
        RestoreFovImmediately();
        return;
    }
    if (!wasRequested && fovBlendActive_) {
        return;
    }
    fovBlendStartY_ = camera_->GetFovY();
    viewBlendStartTranslationOffset_ =
        camera_->GetViewTranslationOffset();
    fovBlendElapsed_ = 0.0f;
    fovBlendProgress_ = 0.0f;
    fovBlendActive_ = true;
    fovBlendDuration_ = 0.45f; // Preserve the existing return-to-normal composition timing.
}

void KrakenTentacleWaveEncounterController::UpdateFovOverride(
    float gameplayDeltaTime) {
    if (!camera_ || !baseFovCaptured_) {
        return;
    }
    const float target = fovOverrideRequested_ ? wave4FovY_ : baseFovY_;
    const Vector3 targetViewOffset = fovOverrideRequested_
        ? wave4ViewTranslationOffset_
        : baseViewTranslationOffset_;
    if (fovBlendActive_) {
        fovBlendElapsed_ += gameplayDeltaTime;
        fovBlendProgress_ = fovBlendDuration_ > kFovEpsilon
            ? std::clamp(fovBlendElapsed_ / fovBlendDuration_, 0.0f, 1.0f)
            : 1.0f;
        const float t = SmoothStep(fovBlendProgress_);
        camera_->SetFovY(
            fovBlendStartY_ + (target - fovBlendStartY_) * t);
        camera_->SetViewTranslationOffset(Lerp(
            viewBlendStartTranslationOffset_, targetViewOffset, t));
        if (fovBlendProgress_ >= 1.0f) {
            camera_->SetFovY(target);
            camera_->SetViewTranslationOffset(targetViewOffset);
            fovBlendActive_ = false;
            if (!fovOverrideRequested_) {
                fovOverrideActive_ = false;
                fovRestoreVerified_ = std::fabs(
                    camera_->GetFovY() - baseFovY_) <= kFovEpsilon;
                viewTranslationRestoreVerified_ = NearlyEqual(
                    camera_->GetViewTranslationOffset(),
                    baseViewTranslationOffset_);
                baseFovCaptured_ = false;
            }
        }
    } else if (fovOverrideRequested_) {
        camera_->SetFovY(wave4FovY_);
        camera_->SetViewTranslationOffset(wave4ViewTranslationOffset_);
        fovOverrideActive_ = true;
        fovBlendProgress_ = 1.0f;
    }
}

void KrakenTentacleWaveEncounterController::RestoreFovImmediately() {
    if (camera_ && baseFovCaptured_ && std::isfinite(baseFovY_)) {
        camera_->SetFovY(baseFovY_);
        camera_->SetViewTranslationOffset(baseViewTranslationOffset_);
        fovRestoreVerified_ = std::fabs(
            camera_->GetFovY() - baseFovY_) <= kFovEpsilon;
        viewTranslationRestoreVerified_ = NearlyEqual(
            camera_->GetViewTranslationOffset(),
            baseViewTranslationOffset_);
    }
    baseFovCaptured_ = false;
    fovOverrideRequested_ = false;
    fovOverrideActive_ = false;
    fovBlendActive_ = false;
    fovBlendElapsed_ = 0.0f;
    fovBlendProgress_ = 0.0f;
}

void KrakenTentacleWaveEncounterController::RefreshFramingDiagnostics() {
    visibleWeakPointCount_ = 0;
    visibleTentacleCount_ = 0;
    visibleTentacleChains_.fill(false);
    framingScreenMinimum_ = {};
    framingScreenMaximum_ = {};
    framingScreenHeightOccupancy_ = 0.0f;
    framingScreenBoundsValid_ = false;
    framingRootSideHidden_ = false;
    framingNearPlaneWarning_ = false;
    if (!camera_ || !kraken_) {
        return;
    }
    const Matrix4x4& viewProjection = camera_->GetViewProjectionMatrix();
    const std::size_t chainCount = kraken_->GetDetectedChainCount();
    for (std::size_t chainIndex = 0;
        chainIndex < chainCount; ++chainIndex) {
        KrakenTentacleWeakPointAnchorSnapshot anchor{};
        if (!kraken_->TryGetWeakPointLockOnAnchorSnapshot(
                chainIndex, anchor) || !anchor.valid || !anchor.enabled) {
            continue;
        }
        if (IsVisibleInCamera(anchor.worldCenter, viewProjection)) {
            ++visibleWeakPointCount_;
            if (chainIndex < visibleTentacleChains_.size()) {
                visibleTentacleChains_[chainIndex] = true;
            }
        }
    }
    KrakenTentacleFramingSnapshot framing{};
    if (kraken_->TryGetFramingSnapshot(framing)) {
        for (std::size_t chainIndex = 0;
            chainIndex < visibleTentacleChains_.size(); ++chainIndex) {
            const bool tipVisible = framing.tipValid[chainIndex] &&
                IsVisibleInCamera(
                    framing.tipWorldPositions[chainIndex], viewProjection);
            const bool midpointVisible =
                framing.upperMidpointValid[chainIndex] &&
                IsVisibleInCamera(
                    framing.upperMidpointWorldPositions[chainIndex],
                    viewProjection);
            visibleTentacleChains_[chainIndex] =
                visibleTentacleChains_[chainIndex] ||
                tipVisible || midpointVisible;
        }
        framingScreenMinimum_ = framing.screenMinimum;
        framingScreenMaximum_ = framing.screenMaximum;
        framingScreenHeightOccupancy_ = framing.screenHeightOccupancy;
        framingScreenBoundsValid_ = framing.screenBoundsValid;
        framingRootSideHidden_ = framing.rootSideHidden;
        framingNearPlaneWarning_ = framing.nearPlaneWarning;
    }
    visibleTentacleCount_ = static_cast<std::size_t>(std::count(
        visibleTentacleChains_.begin(), visibleTentacleChains_.end(), true));
    const Matrix4x4& cameraWorld = camera_->GetWorldMatrix();
    const Vector3 currentForward = Normalize({
        cameraWorld.m[2][0], cameraWorld.m[2][1], cameraWorld.m[2][2] });
    framingCameraForwardDot_ = Dot(
        currentForward, compositionCameraForward_);
    framingCameraRotationUnchanged_ =
        std::isfinite(framingCameraForwardDot_) &&
        framingCameraForwardDot_ >= 0.99999f;
}
