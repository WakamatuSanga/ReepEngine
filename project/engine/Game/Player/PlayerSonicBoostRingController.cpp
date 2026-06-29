#include "PlayerSonicBoostRingController.h"

#include "BoostController.h"
#include "Player.h"
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

    Vector3 NegateVector3(const Vector3& value) {
        return { -value.x, -value.y, -value.z };
    }

    Vector3 Cross(const Vector3& lhs, const Vector3& rhs) {
        return {
            lhs.y * rhs.z - lhs.z * rhs.y,
            lhs.z * rhs.x - lhs.x * rhs.z,
            lhs.x * rhs.y - lhs.y * rhs.x,
        };
    }

    float Dot(const Vector3& lhs, const Vector3& rhs) {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
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

    Matrix4x4 MakeRotationMatrix(const Vector3& rotation) {
        return MatrixMath::MakeAffine({ 1.0f, 1.0f, 1.0f }, rotation, { 0.0f, 0.0f, 0.0f });
    }

    Vector3 ExtractForwardFromRotation(const Vector3& rotation, const Vector3& fallback) {
        const Matrix4x4 matrix = MakeRotationMatrix(rotation);
        return Normalize({ matrix.m[2][0], matrix.m[2][1], matrix.m[2][2] }, fallback);
    }

    Vector3 ExtractUpFromRotation(const Vector3& rotation, const Vector3& fallback) {
        const Matrix4x4 matrix = MakeRotationMatrix(rotation);
        return Normalize({ matrix.m[1][0], matrix.m[1][1], matrix.m[1][2] }, fallback);
    }
}

PlayerSonicBoostRingController::PlayerSonicBoostRingController() = default;
PlayerSonicBoostRingController::~PlayerSonicBoostRingController() = default;

bool PlayerSonicBoostRingController::Initialize(
    DirectXCommon* dxCommon,
    Camera* camera,
    Player* player,
    BoostController* boostController) {
    initialized_ = false;
    camera_ = camera;
    player_ = player;
    boostController_ = boostController;
    rings_.clear();
    activeRingCount_ = 0;
    ringSpawnTimer_ = 0.0f;
    wasBoosting_ = false;
    if (!dxCommon || !camera_ || !player_ || !boostController_) {
        renderer_.reset();
        return false;
    }

    renderer_ = std::make_unique<PlayerJetExhaustBeamRenderer>();
    initialized_ = renderer_ && renderer_->Initialize(dxCommon);
    if (!initialized_) {
        renderer_.reset();
    }
    return initialized_;
}

void PlayerSonicBoostRingController::Finalize() {
    initialized_ = false;
    rings_.clear();
    renderer_.reset();
    camera_ = nullptr;
    player_ = nullptr;
    boostController_ = nullptr;
}

void PlayerSonicBoostRingController::Update(float deltaTime) {
    if (!initialized_) {
        return;
    }
    ClampSettings();
    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    time_ += safeDeltaTime;
    currentBoostPower_ = boostController_ ? Saturate(boostController_->GetCurrentBoostPower()) : 0.0f;

    if (!enableSonicBoostRing_) {
        rings_.clear();
        activeRingCount_ = 0;
        ringSpawnTimer_ = 0.0f;
        wasBoosting_ = false;
        return;
    }

    lastRingFlowDirection_ = ResolveRingFlowDirection();
    currentRingBackwardSpeed_ = ResolveRingBackwardSpeed(currentBoostPower_);
    for (SonicBoostRing& ring : rings_) {
        if (enableRingBackwardFlow_ && ring.age >= 0.0f) {
            if (!lockRingVelocityOnSpawn_) {
                ring.velocity = ScaleVector3(lastRingFlowDirection_, currentRingBackwardSpeed_);
            }
            ring.center = AddVector3(ring.center, ScaleVector3(ring.velocity, safeDeltaTime));
        }
        ring.age += safeDeltaTime;
        if (ring.age >= ring.lifetime) {
            ring.active = false;
        }
    }
    rings_.erase(std::remove_if(rings_.begin(), rings_.end(), [](const SonicBoostRing& ring) {
        return !ring.active;
    }), rings_.end());

    maxActiveRings_ = std::clamp(maxActiveRings_, 1, 64);
    while (static_cast<int>(rings_.size()) > maxActiveRings_) {
        rings_.erase(rings_.begin());
    }

    const bool isBoostingNow = currentBoostPower_ > boostRingStartThreshold_;
    const bool boostStarted = isBoostingNow && !wasBoosting_;
    const float baseInterval = (std::max)(ringSpawnInterval_, 0.03f);
    const float interval = Lerp(baseInterval * (0.18f / 0.14f), baseInterval * (0.10f / 0.14f), currentBoostPower_);
    const float safeInterval = (std::max)(0.03f, interval);
    if (boostStarted) {
        ringSpawnTimer_ = spawnBurstOnBoostStart_ ? safeInterval : 0.0f;
        if (spawnBurstOnBoostStart_) {
            SpawnBurst();
        }
    }

    if (isBoostingNow) {
        ringSpawnTimer_ -= safeDeltaTime;
        if (ringSpawnTimer_ <= 0.0f) {
            SpawnRing(0.0f);
            ringSpawnTimer_ += safeInterval;
        }
    } else {
        ringSpawnTimer_ = 0.0f;
    }

    wasBoosting_ = isBoostingNow;
    activeRingCount_ = CountVisibleRings();
}

void PlayerSonicBoostRingController::Draw() {
    DrawLayer(false);
}

void PlayerSonicBoostRingController::DrawAfterCloud() {
    DrawLayer(true);
}

void PlayerSonicBoostRingController::SpawnBurst() {
    if (!initialized_) {
        return;
    }
    const int count = std::clamp(burstRingCount_, 0, maxActiveRings_);
    if (count <= 0) {
        return;
    }
    const float interval = (std::max)(burstInterval_, 0.0f);
    for (int index = 0; index < count; ++index) {
        SpawnRing(-interval * static_cast<float>(index));
    }
}

void PlayerSonicBoostRingController::SpawnRing(float initialAge) {
    if (!initialized_ || !renderer_ || !renderer_->IsInitialized() || !player_) {
        return;
    }

    maxActiveRings_ = std::clamp(maxActiveRings_, 1, 64);
    while (static_cast<int>(rings_.size()) >= maxActiveRings_) {
        rings_.erase(rings_.begin());
    }

    const Vector3 normal = ResolveRingForward();
    SonicBoostRing ring;
    ring.center = ResolveRingCenter(normal);
    ring.normal = normal;
    lastRingFlowDirection_ = ResolveRingFlowDirection();
    currentRingBackwardSpeed_ = ResolveRingBackwardSpeed(currentBoostPower_);
    ring.velocity = enableRingBackwardFlow_ ? ScaleVector3(lastRingFlowDirection_, currentRingBackwardSpeed_) : Vector3{ 0.0f, 0.0f, 0.0f };
    ring.age = initialAge;
    ring.lifetime = (std::max)(ringLifetime_, 0.05f);
    ring.startRadius = (std::max)(ringStartRadius_, 0.01f);
    ring.endRadius = (std::max)(ringEndRadius_ * Lerp(0.9f, 1.15f, currentBoostPower_), ring.startRadius + 0.01f);
    ring.thickness = (std::max)(ringThickness_, 0.005f);
    ring.brightness = ringBrightness_ * Lerp(0.8f, 1.4f, currentBoostPower_);
    ring.alpha = Saturate(ringAlpha_);
    ring.active = true;
    rings_.push_back(ring);

    lastRingCenter_ = ring.center;
    lastRingNormal_ = ring.normal;
}

void PlayerSonicBoostRingController::DrawLayer(bool afterCloudLayer) {
    if (!initialized_ || !enableSonicBoostRing_ || !renderer_ || !renderer_->IsInitialized() || !camera_ || afterCloudLayer != drawSonicRingAfterCloud_) {
        return;
    }
    if (rings_.empty() && !(debugVisualsEnabled_ && showRingDebug_)) {
        return;
    }

    std::vector<PlayerJetExhaustBeamRenderer::Vertex> vertices;
    vertices.reserve(kRingSegments * 6);
    auto appendRing = [&](const Vector3& center, const Vector3& normal, float radius, float thickness) {
        vertices.clear();
        const Vector3 safeNormal = Normalize(normal, { 0.0f, 0.0f, 1.0f });
        const Vector3 preferredUp = ResolveRingUp();
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
    };

    for (const SonicBoostRing& ring : rings_) {
        if (!ring.active || ring.age < 0.0f || ring.lifetime <= 0.0f) {
            continue;
        }
        const float t = Saturate(ring.age / ring.lifetime);
        const float radius = Lerp(ring.startRadius, ring.endRadius, EaseOutCubic(t));
        const float alpha = ring.alpha * std::pow(1.0f - t, (std::max)(ringFadePower_, 0.1f));
        if (alpha <= 0.01f) {
            continue;
        }
        appendRing(ring.center, ring.normal, radius, ring.thickness);
        renderer_->Draw(vertices, camera_, ring.brightness, alpha, 0.02f, 1.35f, 1.0f, time_, 2u);
        lastRingCenter_ = ring.center;
        lastRingNormal_ = ring.normal;
        lastRingRadius_ = radius;
    }

    if (debugVisualsEnabled_ && showRingDebug_) {
        const Vector3 normal = ResolveRingForward();
        const Vector3 center = ResolveRingCenter(normal);
        appendRing(center, normal, (std::max)(ringStartRadius_, 0.01f), (std::max)(ringThickness_, 0.005f));
        renderer_->Draw(vertices, camera_, 0.55f, 0.24f, 0.0f, 1.2f, 1.0f, time_, 2u);
        lastRingCenter_ = center;
        lastRingNormal_ = normal;
        lastRingRadius_ = ringStartRadius_;
    }
}

Vector3 PlayerSonicBoostRingController::ResolveRingForward() const {
    const Vector3 fallback = player_ ? player_->GetBaseForward() : Vector3{ 0.0f, 0.0f, 1.0f };
    if (usePlayerForward_ && player_) {
        return Normalize(player_->GetBaseForward(), { 0.0f, 0.0f, 1.0f });
    }
    if (useCameraForward_ && camera_) {
        return ExtractForwardFromRotation(camera_->GetRotate(), fallback);
    }
    return Normalize(fallback, { 0.0f, 0.0f, 1.0f });
}

Vector3 PlayerSonicBoostRingController::ResolveRingUp() const {
    if (camera_) {
        return ExtractUpFromRotation(camera_->GetRotate(), { 0.0f, 1.0f, 0.0f });
    }
    return { 0.0f, 1.0f, 0.0f };
}

Vector3 PlayerSonicBoostRingController::ResolveRingCenter(const Vector3& normal) const {
    if (!player_) {
        return { 0.0f, 0.0f, 0.0f };
    }
    return AddVector3(
        AddVector3(player_->GetWorldPosition(), ScaleVector3(normal, ringForwardOffset_)),
        ScaleVector3(ResolveRingUp(), ringVerticalOffset_));
}

Vector3 PlayerSonicBoostRingController::ResolveRingFlowDirection() const {
    Vector3 forward = ResolveRingForward();
    if (useCameraForwardForRingFlow_ && camera_) {
        forward = ExtractForwardFromRotation(camera_->GetRotate(), forward);
    } else if (useRailFlowDirectionForRing_) {
        forward = ResolveRingForward();
    }
    return Normalize(NegateVector3(forward), { 0.0f, 0.0f, -1.0f });
}

float PlayerSonicBoostRingController::ResolveRingBackwardSpeed(float boostPower) const {
    return (std::max)(ringBackwardSpeed_, 0.0f) * Lerp(1.0f, (std::max)(boostRingBackwardSpeedMultiplier_, 0.0f), Saturate(boostPower));
}

void PlayerSonicBoostRingController::ClampSettings() {
    ringLifetime_ = (std::max)(ringLifetime_, 0.01f);
    ringStartRadius_ = (std::max)(ringStartRadius_, 0.0f);
    ringEndRadius_ = (std::max)(ringEndRadius_, ringStartRadius_ + 0.001f);
    ringThickness_ = (std::max)(ringThickness_, 0.001f);
    ringBrightness_ = (std::max)(ringBrightness_, 0.0f);
    ringAlpha_ = Saturate(ringAlpha_);
    ringFadePower_ = (std::max)(ringFadePower_, 0.1f);
    ringSpawnInterval_ = (std::max)(ringSpawnInterval_, 0.01f);
    boostRingStartThreshold_ = Saturate(boostRingStartThreshold_);
    maxActiveRings_ = std::clamp(maxActiveRings_, 1, 64);
    burstRingCount_ = std::clamp(burstRingCount_, 0, maxActiveRings_);
    burstInterval_ = (std::max)(burstInterval_, 0.0f);
    ringBackwardSpeed_ = (std::max)(ringBackwardSpeed_, 0.0f);
    boostRingBackwardSpeedMultiplier_ = (std::max)(boostRingBackwardSpeedMultiplier_, 0.0f);
}

int PlayerSonicBoostRingController::CountVisibleRings() const {
    int count = 0;
    for (const SonicBoostRing& ring : rings_) {
        if (ring.active && ring.age >= 0.0f && ring.age < ring.lifetime) {
            ++count;
        }
    }
    return count;
}

void PlayerSonicBoostRingController::ApplySubtlePreset() {
    ringLifetime_ = 0.28f;
    ringStartRadius_ = 0.30f;
    ringEndRadius_ = 1.45f;
    ringThickness_ = 0.030f;
    ringBrightness_ = 0.8f;
    ringAlpha_ = 0.45f;
    ringSpawnInterval_ = 0.18f;
}

void PlayerSonicBoostRingController::ApplyStandardPreset() {
    ringLifetime_ = 0.28f;
    ringStartRadius_ = 0.30f;
    ringEndRadius_ = 0.82f;
    ringThickness_ = 0.030f;
    ringBrightness_ = 0.8f;
    ringAlpha_ = 0.15f;
    ringFadePower_ = 1.60f;
    ringSpawnInterval_ = 0.18f;
    maxActiveRings_ = 16;
    boostRingStartThreshold_ = 0.15f;
    spawnBurstOnBoostStart_ = false;
    burstRingCount_ = 8;
    burstInterval_ = 0.040f;
    ringForwardOffset_ = 0.40f;
    ringVerticalOffset_ = 0.00f;
    usePlayerForward_ = true;
    useCameraForward_ = false;
}

void PlayerSonicBoostRingController::ApplyStrongPreset() {
    ringLifetime_ = 0.42f;
    ringStartRadius_ = 0.45f;
    ringEndRadius_ = 2.35f;
    ringThickness_ = 0.045f;
    ringBrightness_ = 1.5f;
    ringAlpha_ = 0.70f;
    ringSpawnInterval_ = 0.10f;
}

void PlayerSonicBoostRingController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(460.0f, 620.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("ソニックブーストリング確認 (Sonic Boost Ring Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("ソニックリングを表示 (Enable Sonic Boost Ring)", &enableSonicBoostRing_);
    ImGui::Checkbox("雲の後に描画 (Draw Ring After Cloud)", &drawSonicRingAfterCloud_);
    ImGui::Checkbox("リングデバッグ表示 (Show Ring Debug)", &showRingDebug_);
    ImGui::TextWrapped("Boost中だけPlayer周辺へ青白い円環を発生させます。PostEffect / Jet Exhaustとは独立した軽量メッシュ描画です。");

    if (ImGui::Button("控えめプリセット (Use Subtle Sonic Ring Preset)")) {
        ApplySubtlePreset();
    }
    if (ImGui::Button("標準プリセット (Use Standard Sonic Ring Preset)")) {
        ApplyStandardPreset();
    }
    if (ImGui::Button("強めプリセット (Use Strong Sonic Ring Preset)")) {
        ApplyStrongPreset();
    }
    if (ImGui::Button("テストリング発生 (Spawn Test Ring)")) {
        SpawnRing(0.0f);
    }

    ClampSettings();
    ImGui::SeparatorText("リング形状 (Ring Shape)");
    ImGui::DragFloat("リング寿命 (Ring Lifetime)", &ringLifetime_, 0.01f, 0.05f, 2.0f, "%.2f");
    ImGui::DragFloat("開始半径 (Ring Start Radius)", &ringStartRadius_, 0.01f, 0.01f, 6.0f, "%.2f");
    ImGui::DragFloat("終了半径 (Ring End Radius)", &ringEndRadius_, 0.01f, 0.02f, 8.0f, "%.2f");
    ImGui::DragFloat("太さ (Ring Thickness)", &ringThickness_, 0.001f, 0.005f, 0.5f, "%.3f");
    ImGui::DragFloat("明るさ (Ring Brightness)", &ringBrightness_, 0.01f, 0.0f, 4.0f, "%.2f");
    ImGui::DragFloat("透明度 (Ring Alpha)", &ringAlpha_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("フェード強度 (Ring Fade Power)", &ringFadePower_, 0.01f, 0.1f, 5.0f, "%.2f");

    ImGui::SeparatorText("発生設定 (Spawn Settings)");
    ImGui::DragFloat("発生間隔 (Ring Spawn Interval)", &ringSpawnInterval_, 0.01f, 0.03f, 1.0f, "%.2f");
    ImGui::DragInt("最大リング数 (Max Active Rings)", &maxActiveRings_, 1.0f, 1, 64);
    ImGui::DragFloat("Boost開始しきい値 (Boost Ring Start Threshold)", &boostRingStartThreshold_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::Checkbox("Boost開始時Burst (Spawn Burst On Boost Start)", &spawnBurstOnBoostStart_);
    ImGui::DragInt("Burstリング数 (Burst Ring Count)", &burstRingCount_, 1.0f, 0, maxActiveRings_);
    ImGui::DragFloat("Burst間隔 (Burst Interval)", &burstInterval_, 0.005f, 0.0f, 0.2f, "%.3f");
    ImGui::DragFloat("前方オフセット (Ring Forward Offset)", &ringForwardOffset_, 0.01f, -2.0f, 3.0f, "%.2f");
    ImGui::DragFloat("上下オフセット (Ring Vertical Offset)", &ringVerticalOffset_, 0.01f, -2.0f, 2.0f, "%.2f");

    ImGui::SeparatorText("向き (Direction)");
    if (ImGui::Checkbox("Player Forwardを使う (Use Player Forward)", &usePlayerForward_) && usePlayerForward_) {
        useCameraForward_ = false;
    }
    if (ImGui::Checkbox("Camera Forwardを使う (Use Camera Forward)", &useCameraForward_) && useCameraForward_) {
        usePlayerForward_ = false;
    }
    if (!usePlayerForward_ && !useCameraForward_) {
        usePlayerForward_ = true;
    }

    ImGui::SeparatorText("後方フロー (Ring Backward Flow)");
    ImGui::Checkbox("リングを後方へ流す (Enable Ring Backward Flow)", &enableRingBackwardFlow_);
    if (ImGui::Checkbox("Rail Flow方向を使う (Use Rail Flow Direction For Ring)", &useRailFlowDirectionForRing_) && useRailFlowDirectionForRing_) {
        useCameraForwardForRingFlow_ = false;
    }
    if (ImGui::Checkbox("Camera Forwardから後方へ流す (Use Camera Forward For Ring Flow)", &useCameraForwardForRingFlow_) && useCameraForwardForRingFlow_) {
        useRailFlowDirectionForRing_ = false;
    }
    if (!useRailFlowDirectionForRing_ && !useCameraForwardForRingFlow_) {
        useRailFlowDirectionForRing_ = true;
    }
    ImGui::Checkbox("発生時の速度を固定 (Lock Ring Velocity On Spawn)", &lockRingVelocityOnSpawn_);
    ImGui::DragFloat("後方流速 (Ring Backward Speed)", &ringBackwardSpeed_, 0.1f, 0.0f, 32.0f, "%.1f");
    ImGui::DragFloat("Boost流速倍率 (Boost Ring Backward Speed Multiplier)", &boostRingBackwardSpeedMultiplier_, 0.01f, 0.0f, 4.0f, "%.2f");
    const Vector3 cameraForward = camera_ ? ExtractForwardFromRotation(camera_->GetRotate(), { 0.0f, 0.0f, 1.0f }) : Vector3{ 0.0f, 0.0f, 1.0f };
    ImGui::Text("Camera Forward: %.2f, %.2f, %.2f", cameraForward.x, cameraForward.y, cameraForward.z);
    ImGui::Text("Current Ring Flow Direction: %.2f, %.2f, %.2f", lastRingFlowDirection_.x, lastRingFlowDirection_.y, lastRingFlowDirection_.z);
    ImGui::Text("Current Ring Backward Speed: %.2f", currentRingBackwardSpeed_);
    ImGui::Text("dot(cameraForward, ringFlowDirection): %.2f", Dot(cameraForward, lastRingFlowDirection_));

    ImGui::SeparatorText("状態 (Status)");
    ImGui::Text("Current Active Ring Count: %d", activeRingCount_);
    ImGui::Text("Current Boost Power: %.2f", currentBoostPower_);
    ImGui::Text("Last Ring Center: %.2f, %.2f, %.2f", lastRingCenter_.x, lastRingCenter_.y, lastRingCenter_.z);
    ImGui::Text("Last Ring Normal: %.2f, %.2f, %.2f", lastRingNormal_.x, lastRingNormal_.y, lastRingNormal_.z);
    ImGui::Text("Current Ring Radius: %.2f", lastRingRadius_);
    ImGui::Text("Draw Layer: %s", drawSonicRingAfterCloud_ ? "After Cloud" : "Before Cloud");
    ImGui::End();
#endif
}