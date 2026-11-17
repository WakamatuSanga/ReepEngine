#include "KrakenTentacleMidbossControllerInternal.h"

#include "Engine/Game/Player/Player.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include "Engine/Graphics/Primitive/BeamBatchRenderer.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
Vector3 Add(const Vector3& a, const Vector3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Subtract(const Vector3& a, const Vector3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 Scale(const Vector3& v, float factor) {
    return {v.x * factor, v.y * factor, v.z * factor};
}

float Dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 Normalize(const Vector3& v, const Vector3& fallback) {
    const float length = std::sqrt(Dot(v, v));
    return std::isfinite(length) && length > 0.00001f ? Scale(v, 1.0f / length) : fallback;
}

bool TryGetBandXAtNdc(const Matrix4x4& viewProjection, const Vector3& center,
    const Vector3& right, float ndc, float& bandX) {
    const Vector3 clipX{viewProjection.m[0][0], viewProjection.m[1][0], viewProjection.m[2][0]};
    const Vector3 clipW{viewProjection.m[0][3], viewProjection.m[1][3], viewProjection.m[2][3]};
    const float centerClipX = Dot(center, clipX) + viewProjection.m[3][0];
    const float centerClipW = Dot(center, clipW) + viewProjection.m[3][3];
    const float rightClipW = Dot(right, clipW);
    const float divisor = Dot(right, clipX) - ndc * rightClipW;
    if (std::abs(divisor) <= 0.00001f) return false;
    bandX = (ndc * centerClipW - centerClipX) / divisor;
    return std::isfinite(bandX) && centerClipW + bandX * rightClipW > 0.0f;
}

bool TryProjectBandEdge(const Matrix4x4& viewProjection, const Vector3& world,
    float& ndcY, float& ndcDepth) {
    const auto clip = [&](int column) {
        return Dot(world, {viewProjection.m[0][column], viewProjection.m[1][column],
            viewProjection.m[2][column]}) + viewProjection.m[3][column];
    };
    const float w = clip(3);
    if (!std::isfinite(w) || w <= 0.00001f) return false;
    ndcY = clip(1) / w;
    ndcDepth = clip(2) / w;
    return std::isfinite(ndcY) && ndcDepth >= 0.0f && ndcDepth <= 1.0f;
}
}

KrakenTentacleMidbossController::Impl::SweepSettings
    KrakenTentacleMidbossController::Impl::sweepSettings{};

bool KrakenTentacleMidbossController::Impl::IsSweepState() const {
    return state == KrakenTentacleMidbossState::SweepPull ||
        state == KrakenTentacleMidbossState::SweepWarning ||
        state == KrakenTentacleMidbossState::SweepStrike ||
        state == KrakenTentacleMidbossState::SweepWithdraw ||
        state == KrakenTentacleMidbossState::SweepCounter ||
        state == KrakenTentacleMidbossState::SweepReturn;
}

bool KrakenTentacleMidbossController::Impl::SuppressSweepTarget(std::size_t chainIndex) const {
    return chainIndex == selectedAttackChainIndex && IsSweepState();
}

bool KrakenTentacleMidbossController::Impl::StartSweep() {
    if (state != KrakenTentacleMidbossState::Idle || !collisionPlayer ||
        !collisionPlayerAlive || chains.empty() || !camera) {
        return false;
    }

    static std::minstd_rand random{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> chooseChain(0, chains.size() - 1);
    const auto previousChain = selectedAttackChainIndex;
    selectedAttackChainIndex = chooseChain(random);
    if (!StartAttack(KrakenTentacleMidbossState::SweepPull)) {
        selectedAttackChainIndex = previousChain;
        return false;
    }
    sweep = {};
    sweep.settings = sweepSettings;
    // The authored idle order across the screen is 04, 03, 02, 01.
    sweep.entrySide = selectedAttackChainIndex < chains.size() / 2 ? 1.0f : -1.0f;
    return true;
}

bool KrakenTentacleMidbossController::Impl::CaptureSweepBand() {
    if (!collisionPlayer || !collisionPlayerAlive || !camera) {
        return false;
    }
    const Player& player = *collisionPlayer;
    const Matrix4x4& cameraWorld = camera->GetWorldMatrix();
    const Vector3 cameraUp{cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2]};
    const Vector3 forward = player.GetBaseForward();
    const float depth = player.GetGameplayReferenceDistance();
    const bool viewport = player.UsesGameplayViewport();
    const Vector3 center = viewport ? player.GetGameplayViewportPoint({}, depth) : player.GetBasePosition();
    const Vector3 vertical = viewport
        ? Subtract(player.GetGameplayViewportPoint({0.0f, 1.0f}, depth),
            player.GetGameplayViewportPoint({0.0f, -1.0f}, depth))
        : cameraUp;
    const Vector3 right = Normalize(MatrixMath::Cross(vertical, forward), {1.0f, 0.0f, 0.0f});
    const Vector3 up = Normalize(MatrixMath::Cross(forward, right), {0.0f, 1.0f, 0.0f});

    float left = -player.GetMoveLimitX(), rightEdge = player.GetMoveLimitX();
    float bottom = -player.GetMoveLimitY(), top = player.GetMoveLimitY();
    if (viewport) {
        left = bottom = 0.0f;
        rightEdge = top = 0.0f;
        for (float x : {-1.0f, 1.0f}) {
            for (float y : {-1.0f, 1.0f}) {
                const Vector3 offset = Subtract(player.GetGameplayViewportPoint({x, y}, depth), center);
                left = (std::min)(left, Dot(offset, right));
                rightEdge = (std::max)(rightEdge, Dot(offset, right));
                bottom = (std::min)(bottom, Dot(offset, up));
                top = (std::max)(top, Dot(offset, up));
            }
        }
    }
    const float height = Dot(Subtract(player.GetWorldPosition(), center), up);
    if (!std::isfinite(height) || rightEdge - left < 0.8f || top - bottom < 0.6f) {
        return false;
    }

    sweep.settings.bandHalfHeight = (std::min)(sweep.settings.bandHalfHeight, (top - bottom) * 0.26f);
    sweep.bandCenter = Add(center, Scale(up, std::clamp(height, bottom + 0.05f, top - 0.05f)));
    sweep.right = right;
    sweep.up = up;
    // Project the attack capsule's vertical bounds once, at the start of the warning.
    const auto& viewProjection = camera->GetViewProjectionMatrix();
    const Vector3 halfHeight = Scale(up, sweep.settings.bandHalfHeight);
    if (!TryProjectBandEdge(viewProjection, Add(sweep.bandCenter, halfHeight),
            sweep.warningTop.ndcY, sweep.warningTop.ndcDepth) ||
        !TryProjectBandEdge(viewProjection, Subtract(sweep.bandCenter, halfHeight),
            sweep.warningBottom.ndcY, sweep.warningBottom.ndcDepth) ||
        sweep.warningTop.ndcY <= sweep.warningBottom.ndcY) {
        return false;
    }
    // Keep the launch beyond the view edge, independently of the drawn band.
    const bool fromRight = sweep.entrySide > 0.0f;
    float launchEdge = fromRight ? rightEdge : left;
    float projectedEdge = 0.0f;
    if (TryGetBandXAtNdc(viewProjection, sweep.bandCenter,
            right, sweep.entrySide * 1.12f, projectedEdge)) {
        launchEdge = fromRight ? (std::max)(launchEdge, projectedEdge)
            : (std::min)(launchEdge, projectedEdge);
    }
    constexpr float launchClearance = 0.65f;
    sweep.launchX = launchEdge + sweep.entrySide * launchClearance;
    sweep.strikeEndX = fromRight ? left : rightEdge;
    return true;
}

void KrakenTentacleMidbossController::DrawAfterCloud() {
    if (impl_) impl_->DrawSweepWarning();
}

void KrakenTentacleMidbossController::Impl::DrawSweepWarning() {
    if (state != KrakenTentacleMidbossState::SweepWarning || !IsVisible() ||
        health.IsDefeatPending() || !camera || !object3dCommon) {
        return;
    }
    if (!sweepWarningRenderer) {
        sweepWarningRenderer = std::make_unique<BeamBatchRenderer>();
        sweepWarningRenderer->Initialize(object3dCommon->GetDxCommon(), true, true, 6);
    }

    const auto& top = sweep.warningTop;
    const auto& bottom = sweep.warningBottom;
    const Vector3 a{-1.0f, top.ndcY, top.ndcDepth};
    const Vector3 b{-1.0f, bottom.ndcY, bottom.ndcDepth};
    const Vector3 c{1.0f, top.ndcY, top.ndcDepth};
    const Vector3 d{1.0f, bottom.ndcY, bottom.ndcDepth};
    const std::vector<BeamBatchRenderer::Vertex> band{
        {a, {0, 0}}, {b, {0, 1}}, {c, {1, 0}},
        {b, {0, 1}}, {d, {1, 1}}, {c, {1, 0}},
    };
    const float t = std::clamp(stateElapsedTime / sweep.settings.warning, 0.0f, 1.0f);
    const float pulse = 0.5f - 0.5f * std::cos(t * 4.0f * std::numbers::pi_v<float>);
    const Vector4 warningRed{1.0f, 0.045f, 0.055f, 1.0f};
    sweepWarningRenderer->Draw(band, camera, 1.0f, 0.18f + pulse * 0.035f,
        0.0f, 1.0f, 1.0f, stateElapsedTime, BeamBatchRenderer::kWarningRectangleMode,
        0.0f, &warningRed);
}

void KrakenTentacleMidbossController::Impl::DrawSweepImGui() {
#ifdef USE_IMGUI
    if (ImGui::CollapsingHeader("横薙ぎ")) {
        ImGui::SliderFloat("引き込み時間", &sweepSettings.pull, 0.15f, 0.50f, "%.2f 秒");
        ImGui::SliderFloat("予告時間", &sweepSettings.warning, 0.65f, 1.5f, "%.2f 秒");
        ImGui::SliderFloat("薙ぎ時間", &sweepSettings.strike, 0.22f, 0.65f, "%.2f 秒");
        ImGui::SliderFloat("薙ぎ後に隠れる時間", &sweepSettings.withdraw, 0.15f, 0.50f, "%.2f 秒");
        ImGui::SliderFloat("反撃時間", &sweepSettings.counter, 0.70f, 2.50f, "%.2f 秒");
        ImGui::SliderFloat("待機姿勢への復帰時間", &sweepSettings.restore, 0.15f, 0.55f, "%.2f 秒");
        ImGui::SliderFloat("予告帯の半幅", &sweepSettings.bandHalfHeight, 0.34f, 0.52f, "%.2f");
        ImGui::TextUnformatted("調整は次の横薙ぎから反映されます。");
    }
#endif
}
