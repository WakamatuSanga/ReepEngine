#include "PlayerBarrelRollRingController.h"

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

PlayerBarrelRollRingController::PlayerBarrelRollRingController() = default;
PlayerBarrelRollRingController::~PlayerBarrelRollRingController() = default;

bool PlayerBarrelRollRingController::Initialize(DirectXCommon* dxCommon, Camera* camera) {
    initialized_ = false;
    camera_ = camera;
    rings_.clear();
    renderer_ = std::make_unique<PlayerJetExhaustBeamRenderer>();
    initialized_ = dxCommon && camera_ && renderer_ && renderer_->Initialize(dxCommon);
    if (!initialized_) {
        renderer_.reset();
    }
    return initialized_;
}

void PlayerBarrelRollRingController::Finalize() {
    initialized_ = false;
    rings_.clear();
    renderer_.reset();
    camera_ = nullptr;
}

void PlayerBarrelRollRingController::Update(float deltaTime) {
    if (!initialized_) {
        return;
    }
    ClampSettings();
    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    time_ += safeDeltaTime;
    for (RollRing& ring : rings_) {
        ring.age += safeDeltaTime;
        if (ring.age >= ring.lifetime) {
            ring.active = false;
        }
    }
    rings_.erase(std::remove_if(rings_.begin(), rings_.end(), [](const RollRing& ring) {
        return !ring.active;
    }), rings_.end());
    activeRingCount_ = CountVisibleRings();
}

void PlayerBarrelRollRingController::Draw() {
    DrawLayer(false);
}

void PlayerBarrelRollRingController::DrawAfterCloud() {
    DrawLayer(true);
}

void PlayerBarrelRollRingController::SpawnRollRings(const Vector3& playerPosition, const Vector3& forward, int directionSign) {
    if (!initialized_ || !enableBarrelRollRings_) {
        return;
    }
    ClampSettings();
    const Vector3 normal = Normalize(forward, ResolveCameraForward());
    const float sideBias = static_cast<float>(std::clamp(directionSign, -1, 1)) * 0.03f;
    const int count = std::clamp(ringCount_, 1, 3);
    const float offsets[3] = { depthOffsets_.x, depthOffsets_.y, depthOffsets_.z };
    const float delays[3] = { spawnDelays_.x, spawnDelays_.y, spawnDelays_.z };
    for (int index = 0; index < count; ++index) {
        if (static_cast<int>(rings_.size()) >= maxActiveRings_) {
            ++droppedRingCount_;
            break;
        }
        RollRing ring;
        ring.center = AddVector3(playerPosition, ScaleVector3(normal, offsets[index] + sideBias));
        ring.normal = normal;
        ring.age = -delays[index];
        ring.lifetime = ringLifetime_;
        ring.startRadius = ringStartRadius_;
        ring.endRadius = ringEndRadius_;
        ring.thickness = ringThickness_;
        ring.alpha = ringAlpha_;
        ring.brightness = ringBrightness_;
        ring.active = true;
        rings_.push_back(ring);
        lastSpawnCenter_ = ring.center;
        lastSpawnNormal_ = ring.normal;
    }
}

void PlayerBarrelRollRingController::DrawLayer(bool afterCloudLayer) {
    if (!initialized_ || !enableBarrelRollRings_ || !renderer_ || !renderer_->IsInitialized() || !camera_ || afterCloudLayer != drawAfterCloud_) {
        return;
    }

    std::vector<PlayerJetExhaustBeamRenderer::Vertex> vertices;
    const Vector3 preferredUp = ResolveCameraUp();
    for (const RollRing& ring : rings_) {
        if (!ring.active || ring.age < 0.0f || ring.lifetime <= 0.0f) {
            continue;
        }
        const float t = Saturate(ring.age / ring.lifetime);
        const float radius = Lerp(ring.startRadius, ring.endRadius, EaseOutCubic(t));
        const float alpha = ring.alpha * std::pow(1.0f - t, (std::max)(ringFadePower_, 0.1f));
        if (alpha <= 0.01f) {
            continue;
        }
        AppendRingVertices(vertices, ring.center, ring.normal, preferredUp, radius, ring.thickness);
        renderer_->Draw(vertices, camera_, ring.brightness, alpha, 0.02f, 1.35f, 1.0f, time_, 2u);
    }
}

void PlayerBarrelRollRingController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(420.0f, 360.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Player Roll Effect Debug")) {
        ImGui::End();
        return;
    }
    ImGui::Checkbox("Enable Barrel Roll Rings", &enableBarrelRollRings_);
    ImGui::Checkbox("Draw After Cloud", &drawAfterCloud_);
    if (ImGui::Button("Test Spawn Roll Rings")) {
        const Vector3 forward = ResolveCameraForward();
        const Vector3 position = camera_ ? AddVector3(camera_->GetTranslate(), ScaleVector3(forward, 3.0f)) : Vector3{};
        SpawnRollRings(position, forward, 1);
    }
    ImGui::DragInt("Ring Count", &ringCount_, 1.0f, 1, 3);
    ImGui::DragFloat("Ring Lifetime", &ringLifetime_, 0.01f, 0.05f, 1.5f, "%.2f");
    ImGui::DragFloat("Ring Start Radius", &ringStartRadius_, 0.01f, 0.01f, 4.0f, "%.2f");
    ImGui::DragFloat("Ring End Radius", &ringEndRadius_, 0.01f, 0.02f, 5.0f, "%.2f");
    ImGui::DragFloat("Ring Thickness", &ringThickness_, 0.001f, 0.005f, 0.2f, "%.3f");
    ImGui::DragFloat("Ring Alpha", &ringAlpha_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Ring Brightness", &ringBrightness_, 0.01f, 0.0f, 4.0f, "%.2f");
    ImGui::DragInt("Max Active Roll Rings", &maxActiveRings_, 1.0f, 1, 32);
    ImGui::Text("Active Roll Rings: %d", activeRingCount_);
    ImGui::Text("Dropped Roll Rings: %llu", static_cast<unsigned long long>(droppedRingCount_));
    ImGui::Text("Last Center: %.2f, %.2f, %.2f", lastSpawnCenter_.x, lastSpawnCenter_.y, lastSpawnCenter_.z);
    ImGui::End();
#endif
}

Vector3 PlayerBarrelRollRingController::ResolveCameraForward() const {
    if (!camera_) {
        return { 0.0f, 0.0f, 1.0f };
    }
    const Matrix4x4& matrix = camera_->GetWorldMatrix();
    return Normalize({ matrix.m[2][0], matrix.m[2][1], matrix.m[2][2] }, { 0.0f, 0.0f, 1.0f });
}

Vector3 PlayerBarrelRollRingController::ResolveCameraUp() const {
    if (!camera_) {
        return { 0.0f, 1.0f, 0.0f };
    }
    const Matrix4x4& matrix = camera_->GetWorldMatrix();
    return Normalize({ matrix.m[1][0], matrix.m[1][1], matrix.m[1][2] }, { 0.0f, 1.0f, 0.0f });
}

void PlayerBarrelRollRingController::ClampSettings() {
    ringCount_ = std::clamp(ringCount_, 1, 3);
    maxActiveRings_ = std::clamp(maxActiveRings_, 1, 32);
    ringLifetime_ = (std::max)(ringLifetime_, 0.01f);
    ringStartRadius_ = (std::max)(ringStartRadius_, 0.01f);
    ringEndRadius_ = (std::max)(ringEndRadius_, ringStartRadius_ + 0.001f);
    ringThickness_ = (std::max)(ringThickness_, 0.001f);
    ringAlpha_ = Saturate(ringAlpha_);
    ringBrightness_ = (std::max)(ringBrightness_, 0.0f);
    ringFadePower_ = (std::max)(ringFadePower_, 0.1f);
}

int PlayerBarrelRollRingController::CountVisibleRings() const {
    int count = 0;
    for (const RollRing& ring : rings_) {
        if (ring.active && ring.age >= 0.0f && ring.age < ring.lifetime) {
            ++count;
        }
    }
    return count;
}