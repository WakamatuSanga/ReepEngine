#include "PlayerJetExhaustController.h"
#include "BoostController.h"
#include "Player.h"
#include "PlayerJetExhaustBeamCore.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Particle/GpuParticleSystem.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr float kMinVectorLength = 0.00001f;
    struct VisualBasis {
        Vector3 right{ 1.0f, 0.0f, 0.0f };
        Vector3 up{ 0.0f, 1.0f, 0.0f };
        Vector3 forward{ 0.0f, 0.0f, 1.0f };
    };

    Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 ScaleVector3(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    Vector3 NegateVector3(const Vector3& value) {
        return { -value.x, -value.y, -value.z };
    }

    float Length(const Vector3& value) {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    }

    Vector3 Normalize(const Vector3& value, const Vector3& fallback) {
        const float length = Length(value);
        if (length <= kMinVectorLength || !std::isfinite(length)) {
            return fallback;
        }
        return { value.x / length, value.y / length, value.z / length };
    }

    float LerpFloat(float start, float end, float t) {
        return start + (end - start) * t;
    }

    VisualBasis MakeBasisFromRotation(const Vector3& rotation, const Vector3& fallbackForward) {
        const Matrix4x4 matrix = MatrixMath::MakeAffine({ 1.0f, 1.0f, 1.0f }, rotation, { 0.0f, 0.0f, 0.0f });
        VisualBasis basis;
        basis.right = Normalize({ matrix.m[0][0], matrix.m[0][1], matrix.m[0][2] }, { 1.0f, 0.0f, 0.0f });
        basis.up = Normalize({ matrix.m[1][0], matrix.m[1][1], matrix.m[1][2] }, { 0.0f, 1.0f, 0.0f });
        basis.forward = Normalize({ matrix.m[2][0], matrix.m[2][1], matrix.m[2][2] }, fallbackForward);
        return basis;
    }

}

void PlayerJetExhaustController::Update(float deltaTime) {
    if (!player_) return;
    displayOpacity_ = 1.0f;
    displayLocalParticles_ = false;
    UpdateVisualPose(deltaTime, player_->GetWorldPosition(), player_->GetVisualModelRotation(),
        player_->GetBaseForward(), { 1.0f, 1.0f, 1.0f },
        boostController_ ? boostController_->GetCurrentBoostPower() : 0.0f);
}

void PlayerJetExhaustController::UpdateDisplay(float deltaTime, const Transform& transform,
    float referenceModelScale, float displayBoost, float opacity, bool nozzleLocalParticles) {
    if (referenceModelScale <= 0.0f) return;
    displayOpacity_ = std::clamp(opacity, 0.0f, 1.0f);
    displayLocalParticles_ = nozzleLocalParticles && !player_;
    const Vector3 scale = { transform.scale.x / referenceModelScale,
        transform.scale.y / referenceModelScale, transform.scale.z / referenceModelScale };
    UpdateVisualPose(deltaTime, transform.translate, transform.rotate, { 0.0f, 0.0f, 1.0f }, scale, displayBoost);
}

void PlayerJetExhaustController::ResetDisplayState() {
    if (player_) return;
    // Reuse resources; clear waiting particles before relocating behind the camera.
    if (particleSystem_) particleSystem_->ResetParticlePool();
    smoothedBoostPower_ = 0.0f;
    displayOpacity_ = 1.0f;
    displayLocalParticles_ = false;
}

void PlayerJetExhaustController::UpdateVisualPose(float deltaTime, const Vector3& position,
    const Vector3& rotation, const Vector3& forward, const Vector3& nozzleScale, float boostPower) {
    ++updateCount_;
    if (!particleSystem_ || !camera_) {
        return;
    }

    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    const float targetBoostPower = std::clamp(boostPower, 0.0f, 1.0f);
    const float smoothT = std::clamp(safeDeltaTime * boostSmoothSpeed_, 0.0f, 1.0f);
    smoothedBoostPower_ = LerpFloat(smoothedBoostPower_, targetBoostPower, smoothT);

    currentLengthMultiplier_ = LerpFloat(1.0f, boostLengthMultiplier_, smoothedBoostPower_);
    currentSpeedMultiplier_ = LerpFloat(1.0f, boostSpeedMultiplier_, smoothedBoostPower_);
    currentSpawnRateMultiplier_ = LerpFloat(1.0f, boostSpawnRateMultiplier_, smoothedBoostPower_);
    currentBrightness_ = brightness_ * LerpFloat(1.0f, boostBrightnessMultiplier_, smoothedBoostPower_);

    const VisualBasis basis = MakeBasisFromRotation(rotation, forward);
    currentNozzlePosition_ = AddVector3(
        AddVector3(
            AddVector3(position, ScaleVector3(basis.forward, -nozzleBackOffset_ * nozzleScale.z)),
            ScaleVector3(basis.up, nozzleUpOffset_ * nozzleScale.y)),
        ScaleVector3(basis.right, nozzleSideOffset_ * nozzleScale.x));
    currentExhaustDirection_ = invertExhaustDirection_ ? basis.forward : NegateVector3(basis.forward);
    currentExhaustDirection_ = Normalize(currentExhaustDirection_, NegateVector3(forward));
    const bool shouldEmit = enableJetExhaust_ && displayOpacity_ > 0.0f && (!hideWhenPlayerDead_ || isPlayerAlive_);
    if (beamCore_) {
        beamCore_->Update(
            currentNozzlePosition_,
            currentExhaustDirection_,
            basis.right,
            camera_,
            smoothedBoostPower_,
            safeDeltaTime,
            shouldEmit);
    }

    ApplyRuntimeSettings(safeDeltaTime);
    particleSystem_->SetDeltaTime(safeDeltaTime);
    particleSystem_->SetParticleInfluenceEnabled(false);
    particleSystem_->SetRailParticleFlow(
        affectedByRailFlow_ && !displayLocalParticles_,
        camera_->GetTranslate(),
        currentExhaustDirection_,
        exhaustSpeed_ * currentSpeedMultiplier_,
        railFlowScale_,
        24.0f,
        8.0f);
    if (displayLocalParticles_) {
        // Only title waiting/departure opts in. All existing particles retain
        // their nozzle-relative position/velocity instead of the old camera yaw.
        const auto& emitters = particleSystem_->GetState().emitters;
        for (size_t i = 0; i < emitters.size(); ++i) {
            auto emitter = emitters[i];
            emitter.position = {};
            emitter.direction = { 0.0f, 0.0f, invertExhaustDirection_ ? 1.0f : -1.0f };
            particleSystem_->SetEmitterRuntime(i, emitter);
        }
        const auto particleToWorld = MatrixMath::MakeAffine({ 1, 1, 1 }, rotation, currentNozzlePosition_);
        particleSystem_->Update(camera_, &particleToWorld);
    } else {
        particleSystem_->Update(camera_);
    }
    UpdateDebugObjects();
}

