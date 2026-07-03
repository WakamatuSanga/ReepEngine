#include "EnemyDefeatEffectController.h"

#include "Engine/Core/DirectXCommon.h"
#include "Engine/Game/Player/PlayerJetExhaustBeamRenderer.h"
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

    Vector3 SubtractVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
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

    float Hash01(uint32_t seed) {
        seed ^= seed >> 16;
        seed *= 0x7feb352du;
        seed ^= seed >> 15;
        seed *= 0x846ca68bu;
        seed ^= seed >> 16;
        return static_cast<float>(seed & 0x00ffffffu) / static_cast<float>(0x01000000u);
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

    void AppendSparkLine(
        std::vector<PlayerJetExhaustBeamRenderer::Vertex>& vertices,
        const Vector3& start,
        const Vector3& end,
        const Vector3& cameraForward,
        float thickness) {
        const Vector3 direction = Normalize(SubtractVector3(end, start), { 1.0f, 0.0f, 0.0f });
        Vector3 side = Normalize(Cross(cameraForward, direction), { 0.0f, 1.0f, 0.0f });
        if (Length(side) <= kMinVectorLength) {
            side = { 0.0f, 1.0f, 0.0f };
        }
        side = ScaleVector3(side, (std::max)(thickness, 0.001f) * 0.5f);
        const Vector3 a = AddVector3(start, side);
        const Vector3 b = SubtractVector3(start, side);
        const Vector3 c = AddVector3(end, side);
        const Vector3 d = SubtractVector3(end, side);
        vertices.push_back({ a, { 0.0f, 0.0f } });
        vertices.push_back({ c, { 1.0f, 0.0f } });
        vertices.push_back({ d, { 1.0f, 1.0f } });
        vertices.push_back({ a, { 0.0f, 0.0f } });
        vertices.push_back({ d, { 1.0f, 1.0f } });
        vertices.push_back({ b, { 0.0f, 1.0f } });
    }
}

EnemyDefeatEffectController::EnemyDefeatEffectController() = default;
EnemyDefeatEffectController::~EnemyDefeatEffectController() = default;

bool EnemyDefeatEffectController::Initialize(DirectXCommon* dxCommon, Camera* camera) {
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

void EnemyDefeatEffectController::Finalize() {
    initialized_ = false;
    effects_.clear();
    renderer_.reset();
    camera_ = nullptr;
}

void EnemyDefeatEffectController::BeginFrame() {
    spawnedThisFrame_ = 0;
}

void EnemyDefeatEffectController::Update(float deltaTime) {
    if (!initialized_) {
        return;
    }
    ClampSettings();
    EnsureEffectSlots();
    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    time_ += safeDeltaTime;
    for (DefeatEffect& effect : effects_) {
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

void EnemyDefeatEffectController::Draw() {
    DrawLayer(false);
}

void EnemyDefeatEffectController::DrawAfterCloud() {
    DrawLayer(true);
}

void EnemyDefeatEffectController::SpawnDefeatEffect(const Vector3& position, float scale) {
    if (!initialized_ || !enableDefeatEffect_) {
        return;
    }
    ClampSettings();
    EnsureEffectSlots();
    if (spawnedThisFrame_ >= maxSpawnPerFrame_) {
        ++droppedEffectCount_;
        return;
    }

    DefeatEffect* slot = nullptr;
    for (DefeatEffect& effect : effects_) {
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
    slot->scale = std::clamp(scale, 0.6f, 2.5f);
    slot->seed = nextSeed_++;
    slot->active = true;
    ++spawnedThisFrame_;
    activeEffectCount_ = CountActiveEffects();
    lastEffectPosition_ = position;
}

void EnemyDefeatEffectController::DrawLayer(bool afterCloudLayer) {
    if (!initialized_ || !enableDefeatEffect_ || !renderer_ || !renderer_->IsInitialized() || !camera_ || afterCloudLayer != drawAfterCloud_) {
        return;
    }

    const Vector3 forward = ResolveCameraForward();
    const Vector3 right = ResolveCameraRight();
    const Vector3 up = ResolveCameraUp();
    std::vector<PlayerJetExhaustBeamRenderer::Vertex> vertices;
    vertices.reserve(static_cast<size_t>(kRingSegments * 12 + std::clamp(sparkCount_, 0, 24) * 6));

    for (const DefeatEffect& effect : effects_) {
        if (!effect.active || effect.lifetime <= 0.0f) {
            continue;
        }

        const float t = Saturate(effect.age / effect.lifetime);
        const float fade = std::pow(1.0f - t, 1.45f);
        const float alpha = effectAlpha_ * fade;
        if (alpha <= 0.005f) {
            continue;
        }

        vertices.clear();
        const float scale = (std::max)(effect.scale, 0.001f);
        if (effect.age <= flashLifetime_) {
            const float flashT = Saturate(effect.age / (std::max)(flashLifetime_, 0.001f));
            const float flashRadius = Lerp(0.10f, 0.36f, EaseOutCubic(flashT)) * scale;
            AppendRingVertices(vertices, effect.center, forward, up, flashRadius, ringThickness_ * scale * 2.2f);
        }

        const float ringRadius = Lerp(ringStartRadius_, ringEndRadius_, EaseOutCubic(t)) * scale;
        AppendRingVertices(vertices, effect.center, forward, up, ringRadius, ringThickness_ * scale);

        const float sparkT = Saturate(effect.age / (std::max)(sparkLifetime_, 0.001f));
        if (sparkT < 1.0f) {
            const int sparkCount = std::clamp(sparkCount_, 0, 24);
            for (int index = 0; index < sparkCount; ++index) {
                const uint32_t seed = effect.seed * 1664525u + static_cast<uint32_t>(index) * 1013904223u;
                const float angle = Hash01(seed) * std::numbers::pi_v<float> * 2.0f;
                const float lift = Lerp(-0.28f, 0.42f, Hash01(seed ^ 0xa511e9b3u));
                const float length = Lerp(sparkMinLength_, sparkMaxLength_, Hash01(seed ^ 0x63d83595u)) * scale;
                const Vector3 planeDirection = Normalize(
                    AddVector3(ScaleVector3(right, std::cos(angle)), ScaleVector3(up, std::sin(angle))),
                    right);
                const Vector3 sparkDirection = Normalize(
                    AddVector3(planeDirection, ScaleVector3(forward, lift)),
                    planeDirection);
                const float travel = EaseOutCubic(sparkT) * length;
                const Vector3 start = AddVector3(effect.center, ScaleVector3(sparkDirection, travel * 0.22f));
                const Vector3 end = AddVector3(effect.center, ScaleVector3(sparkDirection, travel + 0.08f * scale));
                AppendSparkLine(vertices, start, end, forward, ringThickness_ * scale * 0.55f);
            }
        }

        renderer_->Draw(vertices, camera_, effectBrightness_, alpha, 0.02f, 2.6f, 0.75f, time_, 2u);
    }
}

void EnemyDefeatEffectController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(430.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Enemy Defeat Effect Debug")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Enable Enemy Defeat Effect", &enableDefeatEffect_);
    ImGui::Checkbox("Draw After Cloud", &drawAfterCloud_);
    if (ImGui::Button("Test Spawn Defeat Effect")) {
        const Vector3 position = camera_ ? AddVector3(camera_->GetTranslate(), ScaleVector3(ResolveCameraForward(), 7.0f)) : Vector3{};
        SpawnDefeatEffect(position, 1.0f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Spawn At Last Defeated")) {
        SpawnDefeatEffect(lastEffectPosition_, 1.0f);
    }
    ImGui::DragInt("Max Active Effects", &maxActiveEffects_, 1.0f, 1, 64);
    ImGui::DragInt("Max Spawn Per Frame", &maxSpawnPerFrame_, 1.0f, 0, 8);
    ImGui::DragFloat("Effect Lifetime", &effectLifetime_, 0.01f, 0.05f, 1.0f, "%.2f");
    ImGui::DragFloat("Flash Lifetime", &flashLifetime_, 0.01f, 0.01f, 0.5f, "%.2f");
    ImGui::DragFloat("Ring Start Radius", &ringStartRadius_, 0.005f, 0.01f, 2.0f, "%.3f");
    ImGui::DragFloat("Ring End Radius", &ringEndRadius_, 0.005f, 0.02f, 3.0f, "%.3f");
    ImGui::DragFloat("Ring Thickness", &ringThickness_, 0.001f, 0.004f, 0.16f, "%.3f");
    ImGui::DragInt("Spark Count", &sparkCount_, 1.0f, 0, 24);
    ImGui::DragFloat("Spark Lifetime", &sparkLifetime_, 0.01f, 0.03f, 0.8f, "%.2f");
    ImGui::DragFloat("Spark Min Length", &sparkMinLength_, 0.01f, 0.02f, 2.0f, "%.2f");
    ImGui::DragFloat("Spark Max Length", &sparkMaxLength_, 0.01f, 0.02f, 3.0f, "%.2f");
    ImGui::DragFloat("Alpha", &effectAlpha_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Brightness", &effectBrightness_, 0.01f, 0.0f, 4.0f, "%.2f");
    ImGui::Separator();
    ImGui::Text("Active Count: %d", activeEffectCount_);
    ImGui::Text("Dropped Count: %llu", static_cast<unsigned long long>(droppedEffectCount_));
    ImGui::Text("Spawned This Frame: %d", spawnedThisFrame_);
    ImGui::Text("Last Defeated Position: %.2f, %.2f, %.2f", lastEffectPosition_.x, lastEffectPosition_.y, lastEffectPosition_.z);
    ImGui::End();
#endif
}

Vector3 EnemyDefeatEffectController::ResolveCameraForward() const {
    if (!camera_) {
        return { 0.0f, 0.0f, 1.0f };
    }
    const Matrix4x4& matrix = camera_->GetWorldMatrix();
    return Normalize({ matrix.m[2][0], matrix.m[2][1], matrix.m[2][2] }, { 0.0f, 0.0f, 1.0f });
}

Vector3 EnemyDefeatEffectController::ResolveCameraRight() const {
    if (!camera_) {
        return { 1.0f, 0.0f, 0.0f };
    }
    const Matrix4x4& matrix = camera_->GetWorldMatrix();
    return Normalize({ matrix.m[0][0], matrix.m[0][1], matrix.m[0][2] }, { 1.0f, 0.0f, 0.0f });
}

Vector3 EnemyDefeatEffectController::ResolveCameraUp() const {
    if (!camera_) {
        return { 0.0f, 1.0f, 0.0f };
    }
    const Matrix4x4& matrix = camera_->GetWorldMatrix();
    return Normalize({ matrix.m[1][0], matrix.m[1][1], matrix.m[1][2] }, { 0.0f, 1.0f, 0.0f });
}

void EnemyDefeatEffectController::ClampSettings() {
    maxActiveEffects_ = std::clamp(maxActiveEffects_, 1, 64);
    maxSpawnPerFrame_ = std::clamp(maxSpawnPerFrame_, 0, 8);
    sparkCount_ = std::clamp(sparkCount_, 0, 24);
    effectLifetime_ = (std::max)(effectLifetime_, 0.001f);
    flashLifetime_ = std::clamp(flashLifetime_, 0.001f, effectLifetime_);
    ringStartRadius_ = (std::max)(ringStartRadius_, 0.001f);
    ringEndRadius_ = (std::max)(ringEndRadius_, ringStartRadius_ + 0.001f);
    ringThickness_ = (std::max)(ringThickness_, 0.001f);
    sparkLifetime_ = std::clamp(sparkLifetime_, 0.001f, effectLifetime_);
    sparkMinLength_ = (std::max)(sparkMinLength_, 0.001f);
    sparkMaxLength_ = (std::max)(sparkMaxLength_, sparkMinLength_);
    effectAlpha_ = Saturate(effectAlpha_);
    effectBrightness_ = (std::max)(effectBrightness_, 0.0f);
}

void EnemyDefeatEffectController::EnsureEffectSlots() {
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

int EnemyDefeatEffectController::CountActiveEffects() const {
    int count = 0;
    for (const DefeatEffect& effect : effects_) {
        if (effect.active) {
            ++count;
        }
    }
    return count;
}
