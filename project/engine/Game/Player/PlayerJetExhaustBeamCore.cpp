#include "PlayerJetExhaustBeamCore.h"

#include "PlayerJetExhaustBeamRenderer.h"
#include "Engine/Graphics/Camera/Camera.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <cmath>

namespace {
    constexpr float kMinLength = 0.00001f;

    Vector3 Add(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 Subtract(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    }

    Vector3 Scale(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    float Length(const Vector3& value) {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    }

    Vector3 Normalize(const Vector3& value, const Vector3& fallback) {
        const float length = Length(value);
        if (length <= kMinLength || !std::isfinite(length)) {
            return fallback;
        }
        return { value.x / length, value.y / length, value.z / length };
    }

    Vector3 Cross(const Vector3& lhs, const Vector3& rhs) {
        return {
            lhs.y * rhs.z - lhs.z * rhs.y,
            lhs.z * rhs.x - lhs.x * rhs.z,
            lhs.x * rhs.y - lhs.y * rhs.x,
        };
    }

    float Lerp(float start, float end, float t) {
        return start + (end - start) * t;
    }

    Vector3 ExtractCameraRight(const Camera* camera) {
        if (!camera) {
            return { 1.0f, 0.0f, 0.0f };
        }
        const Matrix4x4 matrix = camera->GetWorldMatrix();
        return Normalize({ matrix.m[0][0], matrix.m[0][1], matrix.m[0][2] }, { 1.0f, 0.0f, 0.0f });
    }

    Vector3 ExtractCameraUp(const Camera* camera) {
        if (!camera) {
            return { 0.0f, 1.0f, 0.0f };
        }
        const Matrix4x4 matrix = camera->GetWorldMatrix();
        return Normalize({ matrix.m[1][0], matrix.m[1][1], matrix.m[1][2] }, { 0.0f, 1.0f, 0.0f });
    }
}

bool PlayerJetExhaustBeamCore::Initialize(DirectXCommon* dxCommon) {
    renderer_ = std::make_unique<PlayerJetExhaustBeamRenderer>();
    return renderer_->Initialize(dxCommon);
}

void PlayerJetExhaustBeamCore::Update(
    const Vector3& nozzlePosition,
    const Vector3& exhaustDirection,
    const Vector3& playerRight,
    const Camera* camera,
    float boostPower,
    float deltaTime,
    bool exhaustEnabled) {
    time_ += std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    exhaustEnabled_ = exhaustEnabled;
    const float boostT = std::clamp(boostPower, 0.0f, 1.0f);
    currentNozzlePosition_ = nozzlePosition;
    currentExhaustDirection_ = Normalize(exhaustDirection, { 0.0f, 0.0f, -1.0f });
    currentBeamLength_ = Lerp(baseBeamLength_, boostBeamLength_, boostT);
    currentBeamEndWidth_ = Lerp(beamEndWidth_, beamEndWidth_ * 1.25f, boostT);
    currentBeamBrightness_ = Lerp(baseBeamBrightness_, boostBeamBrightness_, boostT);
    currentGlowSize_ = Lerp(nozzleGlowSize_, boostNozzleGlowSize_, boostT);
    currentGlowBrightness_ = Lerp(nozzleGlowBrightness_, boostNozzleGlowBrightness_, boostT);
    currentBeamEndPosition_ = Add(currentNozzlePosition_, Scale(currentExhaustDirection_, currentBeamLength_));

    const Vector3 viewDirection = Normalize(Subtract(camera ? camera->GetTranslate() : currentNozzlePosition_, currentNozzlePosition_), { 0.0f, 0.0f, 1.0f });
    Vector3 side = Normalize(Cross(viewDirection, currentExhaustDirection_), playerRight);
    if (Length(side) <= kMinLength) {
        side = Normalize(playerRight, { 1.0f, 0.0f, 0.0f });
    }
    Vector3 upLike = Normalize(Cross(currentExhaustDirection_, side), ExtractCameraUp(camera));
    if (Length(upLike) <= kMinLength) {
        upLike = ExtractCameraUp(camera);
    }

    BuildBeamVertices(side, upLike);
    BuildGlowVertices(ExtractCameraRight(camera), ExtractCameraUp(camera));
}

void PlayerJetExhaustBeamCore::Draw(const Camera* camera) {
    if (!renderer_ || !camera || !exhaustEnabled_) {
        return;
    }
    if (enableBeamCore_ && !beamVertices_.empty()) {
        renderer_->Draw(beamVertices_, camera, currentBeamBrightness_, beamFlickerStrength_, time_, 0u);
    }
    if (enableNozzleGlow_ && !glowVertices_.empty()) {
        renderer_->Draw(glowVertices_, camera, currentGlowBrightness_, beamFlickerStrength_ * 0.5f, time_, 1u);
    }
}

void PlayerJetExhaustBeamCore::DrawImGui() {
#ifdef USE_IMGUI
    if (ImGui::TreeNodeEx("Beam Core / Glow", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Beam Coreを表示 (Enable Beam Core)", &enableBeamCore_);
        ImGui::Checkbox("外側Particleを表示 (Enable Outer Particles)", &enableOuterParticles_);
        ImGui::Checkbox("ノズルGlowを表示 (Enable Nozzle Glow)", &enableNozzleGlow_);
        ImGui::Checkbox("Cross Billboardを使う (Use Cross Billboard)", &useCrossBillboard_);
        ImGui::Checkbox("Beam Debug表示 (Show Beam Debug)", &showBeamDebug_);
        ImGui::DragFloat("Beam Base Length", &baseBeamLength_, 0.01f, 0.1f, 8.0f, "%.2f");
        ImGui::DragFloat("Beam Boost Length", &boostBeamLength_, 0.01f, 0.1f, 12.0f, "%.2f");
        ImGui::DragFloat("Beam Start Width", &beamStartWidth_, 0.005f, 0.01f, 2.0f, "%.3f");
        ImGui::DragFloat("Beam End Width", &beamEndWidth_, 0.005f, 0.01f, 3.0f, "%.3f");
        ImGui::DragFloat("Beam Brightness", &baseBeamBrightness_, 0.01f, 0.0f, 6.0f, "%.2f");
        ImGui::DragFloat("Boost Beam Brightness", &boostBeamBrightness_, 0.01f, 0.0f, 8.0f, "%.2f");
        ImGui::DragFloat("Beam Flicker Strength", &beamFlickerStrength_, 0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat("Nozzle Glow Size", &nozzleGlowSize_, 0.005f, 0.01f, 2.0f, "%.2f");
        ImGui::DragFloat("Nozzle Glow Brightness", &nozzleGlowBrightness_, 0.01f, 0.0f, 8.0f, "%.2f");
        if (showBeamDebug_) {
            ImGui::Text("Beam Start: %.2f, %.2f, %.2f", currentNozzlePosition_.x, currentNozzlePosition_.y, currentNozzlePosition_.z);
            ImGui::Text("Beam End: %.2f, %.2f, %.2f", currentBeamEndPosition_.x, currentBeamEndPosition_.y, currentBeamEndPosition_.z);
            ImGui::Text("Current Length / End Width / Brightness: %.2f / %.2f / %.2f",
                currentBeamLength_, currentBeamEndWidth_, currentBeamBrightness_);
        }
        ImGui::TreePop();
    }
#endif
}

void PlayerJetExhaustBeamCore::BuildBeamVertices(const Vector3& side, const Vector3& upLike) {
    beamVertices_.clear();
    if (!enableBeamCore_) {
        return;
    }

    AddQuad(
        beamVertices_,
        Add(currentNozzlePosition_, Scale(side, -beamStartWidth_)),
        Add(currentNozzlePosition_, Scale(side, beamStartWidth_)),
        Add(currentBeamEndPosition_, Scale(side, -currentBeamEndWidth_)),
        Add(currentBeamEndPosition_, Scale(side, currentBeamEndWidth_)));

    if (useCrossBillboard_) {
        AddQuad(
            beamVertices_,
            Add(currentNozzlePosition_, Scale(upLike, -beamStartWidth_)),
            Add(currentNozzlePosition_, Scale(upLike, beamStartWidth_)),
            Add(currentBeamEndPosition_, Scale(upLike, -currentBeamEndWidth_)),
            Add(currentBeamEndPosition_, Scale(upLike, currentBeamEndWidth_)));
    }
}

void PlayerJetExhaustBeamCore::BuildGlowVertices(const Vector3& cameraRight, const Vector3& cameraUp) {
    glowVertices_.clear();
    if (!enableNozzleGlow_) {
        return;
    }

    const float halfSize = currentGlowSize_ * 0.5f;
    const Vector3 right = Scale(cameraRight, halfSize);
    const Vector3 up = Scale(cameraUp, halfSize);
    const Vector3 a = Add(Add(currentNozzlePosition_, Scale(right, -1.0f)), Scale(up, -1.0f));
    const Vector3 b = Add(Add(currentNozzlePosition_, Scale(right, -1.0f)), up);
    const Vector3 c = Add(Add(currentNozzlePosition_, right), Scale(up, -1.0f));
    const Vector3 d = Add(Add(currentNozzlePosition_, right), up);

    glowVertices_.push_back({ a, { 0.0f, 1.0f } });
    glowVertices_.push_back({ b, { 0.0f, 0.0f } });
    glowVertices_.push_back({ c, { 1.0f, 1.0f } });
    glowVertices_.push_back({ c, { 1.0f, 1.0f } });
    glowVertices_.push_back({ b, { 0.0f, 0.0f } });
    glowVertices_.push_back({ d, { 1.0f, 0.0f } });
}

void PlayerJetExhaustBeamCore::AddQuad(
    std::vector<BeamVertex>& vertices,
    const Vector3& startA,
    const Vector3& startB,
    const Vector3& endA,
    const Vector3& endB) {
    vertices.push_back({ startA, { 0.0f, 1.0f } });
    vertices.push_back({ startB, { 0.0f, 0.0f } });
    vertices.push_back({ endA, { 1.0f, 1.0f } });
    vertices.push_back({ endA, { 1.0f, 1.0f } });
    vertices.push_back({ startB, { 0.0f, 0.0f } });
    vertices.push_back({ endB, { 1.0f, 0.0f } });
}
