#include "PlayerBulletCancelEffectController.h"

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
    constexpr int kRingSegments = 32;

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

    Vector3 AddNormal(const Vector3& lhs, const Vector3& rhs, const Vector3& fallback) {
        return Normalize(AddVector3(lhs, rhs), fallback);
    }

    const char* GetRingDirectionModeName(int mode) {
        switch (mode) {
        case 0:
            return "Camera Axis";
        case 1:
            return "Cross Diagonal";
        default:
            return "Mixed";
        }
    }

    void AppendRingVertices(
        std::vector<PlayerJetExhaustBeamRenderer::Vertex>& vertices,
        const Vector3& center,
        const Vector3& normal,
        const Vector3& preferredUp,
        float radius,
        float thickness) {
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

PlayerBulletCancelEffectController::PlayerBulletCancelEffectController() = default;
PlayerBulletCancelEffectController::~PlayerBulletCancelEffectController() = default;

bool PlayerBulletCancelEffectController::Initialize(DirectXCommon* dxCommon, Camera* camera) {
    initialized_ = false;
    camera_ = camera;
    effects_.clear();
    renderer_ = std::make_unique<PlayerJetExhaustBeamRenderer>();
    initialized_ = dxCommon && camera_ && renderer_ && renderer_->Initialize(dxCommon);
    if (!initialized_) {
        renderer_.reset();
        return false;
    }
    ClampSettings();
    EnsureEffectSlots();
    return true;
}

void PlayerBulletCancelEffectController::Finalize() {
    initialized_ = false;
    effects_.clear();
    renderer_.reset();
    camera_ = nullptr;
}

void PlayerBulletCancelEffectController::BeginFrame() {
    spawnedThisFrame_ = 0;
}

void PlayerBulletCancelEffectController::Update(float deltaTime) {
    if (!initialized_) {
        return;
    }
    ClampSettings();
    EnsureEffectSlots();
    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    time_ += safeDeltaTime;
    for (CancelEffect& effect : effects_) {
        if (!effect.active) {
            continue;
        }
        effect.age += safeDeltaTime;
        if (effect.age >= effect.lifetime) {
            effect.active = false;
        }
    }
    activeEffectCount_ = CountActiveEffects();
}

void PlayerBulletCancelEffectController::Draw() {
    DrawLayer(false);
}

void PlayerBulletCancelEffectController::DrawAfterCloud() {
    DrawLayer(true);
}

void PlayerBulletCancelEffectController::SpawnCancelEffect(const Vector3& position) {
    if (!initialized_ || !enableBulletCancelEffect_) {
        return;
    }
    ClampSettings();
    EnsureEffectSlots();
    if (spawnedThisFrame_ >= maxSpawnPerFrame_) {
        ++droppedEffectCount_;
        return;
    }

    CancelEffect* slot = nullptr;
    for (CancelEffect& effect : effects_) {
        if (!effect.active) {
            slot = &effect;
            break;
        }
    }
    if (!slot) {
        ++droppedEffectCount_;
        return;
    }

    slot->center = position;
    slot->age = 0.0f;
    slot->lifetime = effectLifetime_;
    slot->seed = nextSeed_++;
    slot->active = true;
    ++spawnedThisFrame_;
    activeEffectCount_ = CountActiveEffects();
    lastEffectPosition_ = position;
}

void PlayerBulletCancelEffectController::DrawLayer(bool afterCloudLayer) {
    if (!initialized_ || !enableBulletCancelEffect_ || !renderer_ || !renderer_->IsInitialized() || !camera_ || afterCloudLayer != drawAfterCloud_) {
        return;
    }

    const Vector3 forward = ResolveCameraForward();
    const Vector3 right = ResolveCameraRight();
    const Vector3 up = ResolveCameraUp();
    std::vector<PlayerJetExhaustBeamRenderer::Vertex> vertices;
    vertices.reserve(static_cast<size_t>(std::clamp(ringsPerEffect_, 1, 5) * kRingSegments * 6));

    for (const CancelEffect& effect : effects_) {
        if (!effect.active || effect.lifetime <= 0.0f) {
            continue;
        }

        const float t = Saturate(effect.age / effect.lifetime);
        const float radius = Lerp(startRadius_, endRadius_, EaseOutCubic(t));
        const float alpha = ringAlpha_ * std::pow(1.0f - t, (std::max)(ringFadePower_, 0.1f));
        if (alpha <= 0.005f) {
            continue;
        }

        vertices.clear();
        const int requestedCount = std::clamp(ringsPerEffect_, 1, 5);
        const int count = useDiagonalRings_ ? requestedCount : (std::min)(requestedCount, 3);
        Vector3 diagonalA = AddNormal(right, up, forward);
        Vector3 diagonalB = AddNormal(right, ScaleVector3(up, -1.0f), forward);
        if (ringDirectionMode_ == 0) {
            diagonalA = AddNormal(forward, right, forward);
            diagonalB = AddNormal(forward, up, forward);
        } else if (ringDirectionMode_ == 2) {
            diagonalA = AddNormal(forward, AddNormal(right, up, right), forward);
            diagonalB = AddNormal(forward, AddNormal(right, ScaleVector3(up, -1.0f), right), forward);
        }
        AppendRingVertices(vertices, effect.center, forward, up, radius, ringThickness_);
        if (count >= 2) {
            AppendRingVertices(vertices, effect.center, up, forward, radius * 0.90f, ringThickness_ * 0.78f);
        }
        if (count >= 3) {
            AppendRingVertices(vertices, effect.center, right, up, radius * 0.86f, ringThickness_ * 0.74f);
        }
        if (count >= 4) {
            AppendRingVertices(vertices, effect.center, diagonalA, forward, radius * 0.78f, ringThickness_ * 0.66f);
        }
        if (count >= 5) {
            AppendRingVertices(vertices, effect.center, diagonalB, up, radius * 0.72f, ringThickness_ * 0.60f);
        }
        renderer_->Draw(vertices, camera_, ringBrightness_, alpha, 0.01f, 1.35f, 1.0f, time_, 2u);
    }
}

void PlayerBulletCancelEffectController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(420.0f, 360.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Bullet Cancel Effect Debug")) {
        ImGui::End();
        return;
    }
    ImGui::Checkbox("Enable Bullet Cancel Effect", &enableBulletCancelEffect_);
    ImGui::Checkbox("Draw After Cloud", &drawAfterCloud_);
    if (ImGui::Button("Test Spawn Bullet Cancel Effect")) {
        const Vector3 position = camera_ ? AddVector3(camera_->GetTranslate(), ScaleVector3(ResolveCameraForward(), 4.0f)) : Vector3{};
        SpawnCancelEffect(position);
    }
    ImGui::DragInt("Max Spawn Per Frame", &maxSpawnPerFrame_, 1.0f, 0, 8);
    ImGui::DragInt("Max Active Effects", &maxActiveEffects_, 1.0f, 1, 64);
    ImGui::DragInt("Rings Per Effect", &ringsPerEffect_, 1.0f, 1, 5);
    const char* ringDirectionItems[] = { "Camera Axis", "Cross Diagonal", "Mixed" };
    ImGui::Combo("Ring Direction Mode", &ringDirectionMode_, ringDirectionItems, 3);
    ImGui::Checkbox("Use Diagonal Rings", &useDiagonalRings_);
    ImGui::Text("Current Rings Per Effect: %d", useDiagonalRings_ ? std::clamp(ringsPerEffect_, 1, 5) : (std::min)(std::clamp(ringsPerEffect_, 1, 5), 3));
    ImGui::Text("Ring Direction Mode: %s", GetRingDirectionModeName(ringDirectionMode_));
    ImGui::DragFloat("Effect Lifetime", &effectLifetime_, 0.01f, 0.03f, 0.6f, "%.2f");
    ImGui::DragFloat("Start Radius", &startRadius_, 0.005f, 0.01f, 1.0f, "%.3f");
    ImGui::DragFloat("End Radius", &endRadius_, 0.005f, 0.02f, 1.5f, "%.3f");
    ImGui::DragFloat("Thickness", &ringThickness_, 0.001f, 0.004f, 0.12f, "%.3f");
    ImGui::DragFloat("Alpha", &ringAlpha_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Brightness", &ringBrightness_, 0.01f, 0.0f, 3.0f, "%.2f");
    ImGui::Text("Active Effect Count: %d", activeEffectCount_);
    ImGui::Text("Dropped Effect Count: %llu", static_cast<unsigned long long>(droppedEffectCount_));
    ImGui::Text("Spawned This Frame: %d", spawnedThisFrame_);
    ImGui::Text("Last Position: %.2f, %.2f, %.2f", lastEffectPosition_.x, lastEffectPosition_.y, lastEffectPosition_.z);
    ImGui::End();
#endif
}

Vector3 PlayerBulletCancelEffectController::ResolveCameraForward() const {
    if (!camera_) {
        return { 0.0f, 0.0f, 1.0f };
    }
    const Matrix4x4& matrix = camera_->GetWorldMatrix();
    return Normalize({ matrix.m[2][0], matrix.m[2][1], matrix.m[2][2] }, { 0.0f, 0.0f, 1.0f });
}

Vector3 PlayerBulletCancelEffectController::ResolveCameraRight() const {
    if (!camera_) {
        return { 1.0f, 0.0f, 0.0f };
    }
    const Matrix4x4& matrix = camera_->GetWorldMatrix();
    return Normalize({ matrix.m[0][0], matrix.m[0][1], matrix.m[0][2] }, { 1.0f, 0.0f, 0.0f });
}

Vector3 PlayerBulletCancelEffectController::ResolveCameraUp() const {
    if (!camera_) {
        return { 0.0f, 1.0f, 0.0f };
    }
    const Matrix4x4& matrix = camera_->GetWorldMatrix();
    return Normalize({ matrix.m[1][0], matrix.m[1][1], matrix.m[1][2] }, { 0.0f, 1.0f, 0.0f });
}

void PlayerBulletCancelEffectController::ClampSettings() {
    maxSpawnPerFrame_ = std::clamp(maxSpawnPerFrame_, 0, 8);
    maxActiveEffects_ = std::clamp(maxActiveEffects_, 1, 64);
    ringsPerEffect_ = std::clamp(ringsPerEffect_, 1, 5);
    ringDirectionMode_ = std::clamp(ringDirectionMode_, 0, 2);
    effectLifetime_ = (std::max)(effectLifetime_, 0.001f);
    startRadius_ = (std::max)(startRadius_, 0.001f);
    endRadius_ = (std::max)(endRadius_, startRadius_ + 0.001f);
    ringThickness_ = (std::max)(ringThickness_, 0.001f);
    ringAlpha_ = Saturate(ringAlpha_);
    ringBrightness_ = (std::max)(ringBrightness_, 0.0f);
    ringFadePower_ = (std::max)(ringFadePower_, 0.1f);
}

void PlayerBulletCancelEffectController::EnsureEffectSlots() {
    const size_t targetSize = static_cast<size_t>(std::clamp(maxActiveEffects_, 1, 64));
    if (effects_.size() == targetSize) {
        return;
    }
    if (effects_.size() > targetSize) {
        for (size_t index = targetSize; index < effects_.size(); ++index) {
            if (effects_[index].active) {
                ++droppedEffectCount_;
            }
        }
    }
    effects_.resize(targetSize);
}

int PlayerBulletCancelEffectController::CountActiveEffects() const {
    int count = 0;
    for (const CancelEffect& effect : effects_) {
        if (effect.active) {
            ++count;
        }
    }
    return count;
}