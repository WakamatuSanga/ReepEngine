#include "PlayerChargeFeedbackController.h"

#include "Player.h"
#include "PlayerBulletManager.h"
#include "PlayerJetExhaustBeamRenderer.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Graphics/Camera/Camera.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
    constexpr float kMinVectorLength = 0.00001f;
    constexpr int kRingSegments = 48;

    Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }


    Vector3 ScaleVector3(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    Vector3 Cross(const Vector3& lhs, const Vector3& rhs) {
        return {
            lhs.y * rhs.z - lhs.z * rhs.y,
            lhs.z * rhs.x - lhs.x * rhs.z,
            lhs.x * rhs.y - lhs.y * rhs.x,
        };
    }

    float Length(const Vector3& value) {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    }

    float LengthSquared(const Vector3& value) {
        return value.x * value.x + value.y * value.y + value.z * value.z;
    }

    Vector3 Normalize(const Vector3& value, const Vector3& fallback) {
        const float length = Length(value);
        if (length <= kMinVectorLength || !std::isfinite(length)) {
            return fallback;
        }
        return { value.x / length, value.y / length, value.z / length };
    }

    float Saturate(float value) {
        return std::clamp(value, 0.0f, 1.0f);
    }

    float Lerp(float start, float end, float t) {
        return start + (end - start) * t;
    }

    float EaseOutCubic(float value) {
        const float t = Saturate(value);
        const float inv = 1.0f - t;
        return 1.0f - inv * inv * inv;
    }

    void AppendRingVertices(
        std::vector<PlayerJetExhaustBeamRenderer::Vertex>& vertices,
        const Vector3& center,
        const Vector3& normal,
        const Vector3& preferredUp,
        float radius,
        float thickness) {
        vertices.clear();
        const Vector3 safeNormal = Normalize(normal, { 0.0f, 0.0f, 1.0f });
        Vector3 right = Normalize(Cross(preferredUp, safeNormal), { 1.0f, 0.0f, 0.0f });
        Vector3 up = Normalize(Cross(safeNormal, right), { 0.0f, 1.0f, 0.0f });
        if (Length(right) <= kMinVectorLength || Length(up) <= kMinVectorLength) {
            right = Normalize(Cross({ 0.0f, 1.0f, 0.0f }, safeNormal), { 1.0f, 0.0f, 0.0f });
            up = Normalize(Cross(safeNormal, right), { 0.0f, 1.0f, 0.0f });
        }

        const float halfThickness = (std::max)(thickness * 0.5f, 0.001f);
        const float innerRadius = (std::max)(radius - halfThickness, 0.001f);
        const float outerRadius = (std::max)(radius + halfThickness, innerRadius + 0.001f);
        vertices.reserve(kRingSegments * 6);
        for (int segment = 0; segment < kRingSegments; ++segment) {
            const float u0 = static_cast<float>(segment) / static_cast<float>(kRingSegments);
            const float u1 = static_cast<float>(segment + 1) / static_cast<float>(kRingSegments);
            const float angle0 = u0 * std::numbers::pi_v<float> * 2.0f;
            const float angle1 = u1 * std::numbers::pi_v<float> * 2.0f;
            const Vector3 dir0 = AddVector3(ScaleVector3(right, std::cos(angle0)), ScaleVector3(up, std::sin(angle0)));
            const Vector3 dir1 = AddVector3(ScaleVector3(right, std::cos(angle1)), ScaleVector3(up, std::sin(angle1)));
            const Vector3 inner0 = AddVector3(center, ScaleVector3(dir0, innerRadius));
            const Vector3 outer0 = AddVector3(center, ScaleVector3(dir0, outerRadius));
            const Vector3 inner1 = AddVector3(center, ScaleVector3(dir1, innerRadius));
            const Vector3 outer1 = AddVector3(center, ScaleVector3(dir1, outerRadius));
            vertices.push_back({ inner0, { u0, 0.0f } });
            vertices.push_back({ outer0, { u0, 1.0f } });
            vertices.push_back({ outer1, { u1, 1.0f } });
            vertices.push_back({ inner0, { u0, 0.0f } });
            vertices.push_back({ outer1, { u1, 1.0f } });
            vertices.push_back({ inner1, { u1, 0.0f } });
        }
    }
}

PlayerChargeFeedbackController::PlayerChargeFeedbackController() = default;
PlayerChargeFeedbackController::~PlayerChargeFeedbackController() = default;

bool PlayerChargeFeedbackController::Initialize(
    DirectXCommon* dxCommon,
    Camera* camera,
    Player* player,
    PlayerBulletManager* bulletManager) {
    initialized_ = false;
    camera_ = camera;
    player_ = player;
    bulletManager_ = bulletManager;
    pulses_.clear();
    renderer_ = std::make_unique<PlayerJetExhaustBeamRenderer>();
    initialized_ = dxCommon && camera_ && player_ && bulletManager_ && renderer_ && renderer_->Initialize(dxCommon);
    if (!initialized_) {
        renderer_.reset();
    }
    return initialized_;
}

void PlayerChargeFeedbackController::Finalize() {
    initialized_ = false;
    pulses_.clear();
    renderer_.reset();
    camera_ = nullptr;
    player_ = nullptr;
    bulletManager_ = nullptr;
}

void PlayerChargeFeedbackController::Update(float deltaTime) {
    if (!initialized_) {
        return;
    }

    ClampSettings();
    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    time_ += safeDeltaTime;

    rawChargeTimer_ = bulletManager_ ? bulletManager_->GetChargeTime() : 0.0f;
    const float maxChargeTime = (std::max)(bulletManager_ ? bulletManager_->GetMaxChargeTime() : 1.2f, 0.01f);
    rawChargeRate_ = Saturate(rawChargeTimer_ / maxChargeTime);
    const bool actualChargeMax = bulletManager_ && bulletManager_->IsChargeMax();
    chargeVisualActive_ = forceChargeMax_ || actualChargeMax || rawChargeTimer_ >= chargeGatherStartDelay_;
    const float visualChargeDuration = (std::max)(maxChargeTime - chargeGatherStartDelay_, 0.01f);
    visualChargeRate_ = chargeVisualActive_ ? Saturate((rawChargeTimer_ - chargeGatherStartDelay_) / visualChargeDuration) : 0.0f;
    chargeRate_ = (forceChargeMax_ || actualChargeMax) ? 1.0f : visualChargeRate_;
    isChargeMax_ = enableChargeFeedback_ && (forceChargeMax_ || actualChargeMax);
    lastReticleCenter_ = ResolveReticleCenter();
    lastFrontGlowCenter_ = ResolveFrontGlowCenter();

    if (!wasChargeMax_ && isChargeMax_) {
        SpawnMaxChargePulse();
    }
    wasChargeMax_ = isChargeMax_;

    for (ChargePulse& pulse : pulses_) {
        pulse.age += safeDeltaTime;
        if (pulse.age >= pulse.lifetime) {
            pulse.active = false;
        }
    }
    pulses_.erase(
        std::remove_if(pulses_.begin(), pulses_.end(), [](const ChargePulse& pulse) { return !pulse.active; }),
        pulses_.end());
    activePulseCount_ = static_cast<int>(pulses_.size());
}

void PlayerChargeFeedbackController::Draw() {
    DrawLayer(false);
}

void PlayerChargeFeedbackController::DrawAfterCloud() {
    DrawLayer(true);
}

void PlayerChargeFeedbackController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(420.0f, 360.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Player Charge Feedback Debug")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Enable Charge Feedback", &enableChargeFeedback_);
    ImGui::Checkbox("Force Charge Max", &forceChargeMax_);
    ImGui::Checkbox("Draw After Cloud", &drawAfterCloud_);
    ImGui::DragFloat("Charge Gather Start Delay", &chargeGatherStartDelay_, 0.01f, 0.0f, 0.5f, "%.2f");
    ImGui::Text("Raw Charge Timer: %.2f", rawChargeTimer_);
    ImGui::Text("Raw Charge Rate: %.2f", rawChargeRate_);
    ImGui::Text("Visual Charge Rate: %.2f", visualChargeRate_);
    ImGui::Text("Charge Visual Active: %s", chargeVisualActive_ ? "true" : "false");
    ImGui::Text("Charge Rate: %.2f", chargeRate_);
    ImGui::Text("Is Charge Max: %s", isChargeMax_ ? "true" : "false");
    if (ImGui::Button("Test Max Charge Pulse")) {
        SpawnMaxChargePulse();
    }

    ImGui::SeparatorText("Reticle Glow");
    ImGui::DragFloat("Reticle Glow Alpha", &reticleGlowAlpha_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Reticle Glow Pulse", &maxChargeGlowPulse_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Reticle Pulse Lifetime", &pulseLifetime_, 0.01f, 0.05f, 2.0f, "%.2f");
    ImGui::DragFloat("Reticle Pulse Start Radius", &pulseStartRadius_, 0.005f, 0.01f, 2.0f, "%.3f");
    ImGui::DragFloat("Reticle Pulse End Radius", &pulseEndRadius_, 0.005f, 0.01f, 2.0f, "%.3f");
    ImGui::DragFloat("Reticle Pulse Thickness", &pulseThickness_, 0.001f, 0.001f, 0.2f, "%.3f");
    ImGui::DragFloat("Reticle Pulse Brightness", &pulseBrightness_, 0.01f, 0.0f, 4.0f, "%.2f");

    ImGui::SeparatorText("Player Front Glow");
    ImGui::Checkbox("Front Glow Enabled", &frontGlowEnabled_);
    ImGui::DragFloat("Front Glow Size", &frontGlowSize_, 0.005f, 0.01f, 2.0f, "%.3f");
    ImGui::DragFloat("Front Glow Alpha", &frontGlowAlpha_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Front Glow Brightness", &frontGlowBrightness_, 0.01f, 0.0f, 4.0f, "%.2f");
    ImGui::DragFloat("Front Glow Pulse Rate", &frontGlowPulseRate_, 0.1f, 0.0f, 30.0f, "%.1f");
    ImGui::DragFloat("Front Offset", &frontOffset_, 0.01f, 0.0f, 5.0f, "%.2f");

    ImGui::SeparatorText("Runtime Info");
    ImGui::Text("Active Pulse Count: %d", activePulseCount_);
    ImGui::Text("Reticle Center: %.2f, %.2f, %.2f", lastReticleCenter_.x, lastReticleCenter_.y, lastReticleCenter_.z);
    ImGui::Text("Front Glow Center: %.2f, %.2f, %.2f", lastFrontGlowCenter_.x, lastFrontGlowCenter_.y, lastFrontGlowCenter_.z);
    ImGui::End();
#endif
}

void PlayerChargeFeedbackController::SpawnMaxChargePulse() {
    if (!initialized_ || !enableChargeFeedback_) {
        return;
    }
    ClampSettings();
    ChargePulse pulse;
    pulse.center = ResolveReticleCenter();
    pulse.normal = ResolveCameraForward();
    pulse.lifetime = pulseLifetime_;
    pulse.startRadius = pulseStartRadius_;
    pulse.endRadius = pulseEndRadius_;
    pulse.thickness = pulseThickness_;
    pulse.brightness = pulseBrightness_;
    pulse.alpha = reticleGlowAlpha_;
    pulses_.push_back(pulse);
    activePulseCount_ = static_cast<int>(pulses_.size());
}

void PlayerChargeFeedbackController::DrawLayer(bool afterCloudLayer) {
    if (!initialized_ || !enableChargeFeedback_ || !renderer_ || !renderer_->IsInitialized() || !camera_ || afterCloudLayer != drawAfterCloud_) {
        return;
    }

    const Vector3 cameraForward = ResolveCameraForward();
    if (isChargeMax_) {
        const float pulse = 0.5f + std::sin(time_ * 8.0f) * 0.5f;
        const float alpha = reticleGlowAlpha_ * (1.0f + maxChargeGlowPulse_ * pulse);
        const float radius = Lerp(pulseStartRadius_, pulseStartRadius_ * 1.35f, pulse);
        DrawRing(lastReticleCenter_, cameraForward, radius, pulseThickness_, pulseBrightness_, alpha);

        if (frontGlowEnabled_) {
            const float frontPulse = 0.5f + std::sin(time_ * frontGlowPulseRate_) * 0.5f;
            const float frontAlpha = frontGlowAlpha_ * Lerp(0.75f, 1.0f, frontPulse);
            const float frontRadius = frontGlowSize_ * Lerp(0.92f, 1.08f, frontPulse);
            DrawRing(lastFrontGlowCenter_, cameraForward, frontRadius, pulseThickness_ * 0.85f, frontGlowBrightness_, frontAlpha);
        }
    }

    for (const ChargePulse& pulse : pulses_) {
        if (!pulse.active || pulse.lifetime <= 0.0f) {
            continue;
        }
        const float t = Saturate(pulse.age / pulse.lifetime);
        const float radius = Lerp(pulse.startRadius, pulse.endRadius, EaseOutCubic(t));
        const float alpha = pulse.alpha * std::pow(1.0f - t, 1.65f);
        if (alpha <= 0.01f) {
            continue;
        }
        DrawRing(pulse.center, pulse.normal, radius, pulse.thickness, pulse.brightness, alpha);
    }
}

void PlayerChargeFeedbackController::DrawRing(
    const Vector3& center,
    const Vector3& normal,
    float radius,
    float thickness,
    float brightness,
    float alpha) {
    std::vector<PlayerJetExhaustBeamRenderer::Vertex> vertices;
    AppendRingVertices(vertices, center, normal, ResolveCameraUp(), radius, thickness);
    renderer_->Draw(vertices, camera_, brightness, alpha, 0.04f, 1.35f, 1.0f, time_, 2u);
}

Vector3 PlayerChargeFeedbackController::ResolveCameraForward() const {
    if (!camera_) {
        return { 0.0f, 0.0f, 1.0f };
    }
    const Matrix4x4& matrix = camera_->GetWorldMatrix();
    return Normalize({ matrix.m[2][0], matrix.m[2][1], matrix.m[2][2] }, { 0.0f, 0.0f, 1.0f });
}

Vector3 PlayerChargeFeedbackController::ResolveCameraUp() const {
    if (!camera_) {
        return { 0.0f, 1.0f, 0.0f };
    }
    const Matrix4x4& matrix = camera_->GetWorldMatrix();
    return Normalize({ matrix.m[1][0], matrix.m[1][1], matrix.m[1][2] }, { 0.0f, 1.0f, 0.0f });
}

Vector3 PlayerChargeFeedbackController::ResolvePlayerForward() const {
    if (!player_) {
        return ResolveCameraForward();
    }
    return Normalize(player_->GetBaseForward(), ResolveCameraForward());
}

Vector3 PlayerChargeFeedbackController::ResolveReticleCenter() const {
    if (bulletManager_) {
        const Vector3 aimPoint = bulletManager_->GetLastAimPoint();
        if (LengthSquared(aimPoint) > 0.0001f) {
            return aimPoint;
        }
    }
    const Vector3 origin = player_ ? player_->GetWorldPosition() : (camera_ ? camera_->GetTranslate() : Vector3{});
    return AddVector3(origin, ScaleVector3(ResolveCameraForward(), 8.0f));
}

Vector3 PlayerChargeFeedbackController::ResolveFrontGlowCenter() const {
    const Vector3 origin = player_ ? player_->GetWorldPosition() : (camera_ ? camera_->GetTranslate() : Vector3{});
    return AddVector3(origin, ScaleVector3(ResolvePlayerForward(), frontOffset_));
}

void PlayerChargeFeedbackController::ClampSettings() {
    chargeGatherStartDelay_ = std::clamp(chargeGatherStartDelay_, 0.0f, 0.5f);
    reticleGlowAlpha_ = Saturate(reticleGlowAlpha_);
    maxChargeGlowPulse_ = Saturate(maxChargeGlowPulse_);
    pulseLifetime_ = (std::max)(pulseLifetime_, 0.01f);
    pulseStartRadius_ = (std::max)(pulseStartRadius_, 0.001f);
    pulseEndRadius_ = (std::max)(pulseEndRadius_, pulseStartRadius_ + 0.001f);
    pulseThickness_ = (std::max)(pulseThickness_, 0.001f);
    pulseBrightness_ = (std::max)(pulseBrightness_, 0.0f);
    frontGlowSize_ = (std::max)(frontGlowSize_, 0.001f);
    frontGlowAlpha_ = Saturate(frontGlowAlpha_);
    frontGlowBrightness_ = (std::max)(frontGlowBrightness_, 0.0f);
    frontGlowPulseRate_ = (std::max)(frontGlowPulseRate_, 0.0f);
    frontOffset_ = (std::max)(frontOffset_, 0.0f);
}