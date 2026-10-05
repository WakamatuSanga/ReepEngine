#include "TitleSortieEffects.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Game/Player/PlayerSonicBoostRingController.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr float kMaxBlurStrength = 0.34f; // Title-only; gameplay boost remains unchanged.
    constexpr float kEndBlurStrength = 0.17f;
    constexpr float kBlurHoldFraction = 0.50f;
    constexpr float kBlurReleaseSeconds = 0.30f;
    constexpr float kRingInterval = 0.10f;
    constexpr float kRingLifetime = 0.30f;
    constexpr float kRingStartRadius = 0.8f;
    constexpr float kRingEndRadius = 2.4f;
    constexpr float kRingThickness = 0.065f;
    constexpr float kRingBrightness = 1.0f;
    constexpr float kRingAlpha = 0.25f;
    constexpr float kRingBackwardSpeed = 2.0f;

    bool Project(const Vector3& local, const Matrix4x4& wvp, Vector2& uv) {
        const float w = local.x * wvp.m[0][3] + local.y * wvp.m[1][3] +
            local.z * wvp.m[2][3] + wvp.m[3][3];
        const float z = local.x * wvp.m[0][2] + local.y * wvp.m[1][2] +
            local.z * wvp.m[2][2] + wvp.m[3][2];
        if (!std::isfinite(w) || w <= 0.0001f || z < 0.0f || z > w) return false;
        uv.x = (local.x * wvp.m[0][0] + local.y * wvp.m[1][0] +
            local.z * wvp.m[2][0] + wvp.m[3][0]) / w * 0.5f + 0.5f;
        uv.y = 0.5f - (local.x * wvp.m[0][1] + local.y * wvp.m[1][1] +
            local.z * wvp.m[2][1] + wvp.m[3][1]) / w * 0.5f;
        return std::isfinite(uv.x) && std::isfinite(uv.y);
    }

    bool ProjectHull(const Transform& aircraft, const Camera& camera, Vector2& center, float& radius) {
        const auto world = MatrixMath::MakeAffine(aircraft.scale, aircraft.rotate, aircraft.translate);
        const auto wvp = MatrixMath::Multipty(world, camera.GetViewProjectionMatrix());
        if (!Project({ 0, 0, 0 }, wvp, center) || center.x < 0 || center.x > 1 ||
            center.y < 0 || center.y > 1) return false;
        radius = 0.0f;
        // resources/Player/player.obj local AABB. Project all eight corners,
        // using the shader's UV metric (not world radius or pixel width alone).
        for (int i = 0; i < 8; ++i) {
            Vector2 uv{};
            if (!Project({ (i & 1) ? 0.790639f : -0.790591f,
                (i & 2) ? 0.233814f : -0.232374f,
                (i & 4) ? 0.946203f : -0.947587f }, wvp, uv)) return false;
            const float x = uv.x - center.x, y = uv.y - center.y;
            radius = (std::max)(radius, std::sqrt(x * x + y * y));
        }
        radius = radius * 1.10f + 0.015f;
        return true;
    }
}

TitleSortieEffects::TitleSortieEffects() = default;
TitleSortieEffects::~TitleSortieEffects() { Finalize(); }

void TitleSortieEffects::Initialize(DirectXCommon* dx, Camera* camera) {
    Finalize();
    if (!dx || !camera) return;
    dx_ = dx;
    camera_ = camera;
    auto& p = dx_->GetPostEffectParameters();
    saved_ = { p.radialBlurEnabled, p.radialBlurStrength, p.radialBlurCenter,
        p.radialBlurSampleCount, p.radialBlurCenterClearRadius, p.radialBlurOuterEffectRadius };
    p.radialBlurEnabled = 0;
    p.radialBlurStrength = 0.0f;
    for (auto& ring : rings_) {
        ring = std::make_unique<PlayerSonicBoostRingController>();
        if (!ring->InitializeDisplay(dx_, camera_)) ring.reset();
    }
}

void TitleSortieEffects::Finalize() {
    if (dx_) {
        auto& p = dx_->GetPostEffectParameters();
        p.radialBlurEnabled = saved_.enabled;
        p.radialBlurStrength = saved_.strength;
        p.radialBlurCenter = saved_.center;
        p.radialBlurSampleCount = saved_.samples;
        p.radialBlurCenterClearRadius = saved_.clearRadius;
        p.radialBlurOuterEffectRadius = saved_.outerRadius;
    }
    for (auto& ring : rings_) ring.reset();
    dx_ = nullptr;
    camera_ = nullptr;
    nextRing_ = 0;
    ringTimer_ = 0.0f;
    blurStartProgress_ = -1.0f;
    blurReleaseElapsed_ = -1.0f;
}

void TitleSortieEffects::Update(float dt, const Transform* aircraft, bool flying, float visibility, float flightProgress) {
    if (!dx_ || !camera_) return;
    const float blurDeltaTime = std::isfinite(dt) ? (std::max)(dt, 0.0f) : 0.0f;
    // Match the reused ring controller's bounded effect timestep.
    dt = std::clamp(dt, 0.0f, 1.0f / 15.0f);
    for (auto& ring : rings_) if (ring) ring->Update(dt);
    Vector2 center{};
    float radius = 0.0f;
    const bool visible = aircraft && visibility > 0.0f &&
        ProjectHull(*aircraft, *camera_, center, radius);
    const bool emitting = flying && visible;
    if (emitting) {
        ringTimer_ -= dt;
        if (ringTimer_ <= 0.0f) {
            const auto rotation = MatrixMath::MakeAffine({ 1, 1, 1 }, aircraft->rotate, { 0, 0, 0 });
            const Vector3 forward{ rotation.m[2][0], rotation.m[2][1], rotation.m[2][2] };
            if (auto& ring = rings_[nextRing_]) {
                ring->EmitDisplayRing(aircraft->translate, forward,
                    { -forward.x * kRingBackwardSpeed, -forward.y * kRingBackwardSpeed, -forward.z * kRingBackwardSpeed },
                    kRingLifetime, kRingStartRadius, kRingEndRadius, kRingThickness, kRingBrightness, kRingAlpha);
            }
            nextRing_ = (nextRing_ + 1) % rings_.size();
            ringTimer_ += kRingInterval;
        }
    } else {
        ringTimer_ = 0.0f;
    }
    // Start at the first actually visible frame, not at the behind-camera launch.
    // Follow sortie progress rather than a separate (ring-clamped) effect clock.
    if (emitting && blurStartProgress_ < 0.0f) blurStartProgress_ = flightProgress;
    float strength = 0.0f;
    if (flying && blurStartProgress_ >= 0.0f) {
        const float t = std::clamp((flightProgress - blurStartProgress_) /
            (std::max)(1.0f - blurStartProgress_, 0.0001f), 0.0f, 1.0f);
        const float fade = std::clamp((t - kBlurHoldFraction) / (1.0f - kBlurHoldFraction), 0.0f, 1.0f);
        strength = kMaxBlurStrength + (kEndBlurStrength - kMaxBlurStrength) *
            fade * fade * (3.0f - 2.0f * fade);
    } else if (blurStartProgress_ >= 0.0f && flightProgress >= 1.0f) {
        // The first completed-sortie frame starts at 0.17. Continue during
        // cloud cover using elapsed time, independently of the ring timestep.
        blurReleaseElapsed_ = blurReleaseElapsed_ < 0.0f ? 0.0f : blurReleaseElapsed_ + blurDeltaTime;
        const float fade = std::clamp(blurReleaseElapsed_ / kBlurReleaseSeconds, 0.0f, 1.0f);
        strength = kEndBlurStrength * (1.0f - fade * fade * (3.0f - 2.0f * fade));
    }
    auto& p = dx_->GetPostEffectParameters();
    if (flying && visible) {
        p.radialBlurCenter = { center.x, center.y };
        p.radialBlurCenterClearRadius = radius;
        p.radialBlurOuterEffectRadius = (std::max)(0.72f, radius + 0.10f);
    } // Hold the last valid center/protection throughout the post-sortie release.
    p.radialBlurSampleCount = 6;
    p.radialBlurStrength = strength;
    p.radialBlurEnabled = strength > 0.0f ? 1u : 0u;
}

void TitleSortieEffects::Draw() {
    for (auto& ring : rings_) if (ring) ring->DrawAfterCloud();
}
