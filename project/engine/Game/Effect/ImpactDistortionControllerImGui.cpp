#include "ImpactDistortionController.h"

#include "Engine/Graphics/Camera/Camera.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
    Vector3 NormalizeOr(const Vector3& value, const Vector3& fallback) {
        const float lengthSq = value.x * value.x + value.y * value.y + value.z * value.z;
        if (lengthSq <= 0.000001f || !std::isfinite(lengthSq)) {
            return fallback;
        }
        const float invLength = 1.0f / std::sqrt(lengthSq);
        return { value.x * invLength, value.y * invLength, value.z * invLength };
    }

    Vector3 GetCameraForward(const Camera* camera) {
        if (!camera) {
            return { 0.0f, 0.0f, 1.0f };
        }
        const Matrix4x4& matrix = camera->GetWorldMatrix();
        return NormalizeOr({ matrix.m[2][0], matrix.m[2][1], matrix.m[2][2] }, { 0.0f, 0.0f, 1.0f });
    }

    Vector3 Add(const Vector3& a, const Vector3& b) {
        return { a.x + b.x, a.y + b.y, a.z + b.z };
    }

    Vector3 Scale(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }
}
const char* ImpactDistortionController::GetQualityModeName() const {
    switch (qualityMode_) {
    case QualityMode::Visual:
        return "Visual";
    case QualityMode::Balanced:
    default:
        return "Balanced";
    }
}
void ImpactDistortionController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(430.0f, 560.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Impact Distortion Debug")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Enable Impact Distortion", &enabled_);
    int qualityIndex = static_cast<int>(qualityMode_);
    const char* qualityItems[] = { "Balanced", "Visual" };
    if (ImGui::Combo("Quality Mode", &qualityIndex, qualityItems, IM_ARRAYSIZE(qualityItems))) {
        qualityMode_ = static_cast<QualityMode>(qualityIndex);
    }
    ImGui::SliderInt("Max Active Instances", &maxActiveInstances_, 1, static_cast<int>(kMaxGpuInstances));
    auto triggerEnemyDefeatAtDepth = [this](float depth) {
        const Vector3 cameraPosition = camera_ ? camera_->GetTranslate() : Vector3{};
        TriggerEnemyDefeat(Add(cameraPosition, Scale(GetCameraForward(camera_), (std::max)(0.1f, depth))));
    };
    if (ImGui::Button("Test Bullet Cancel Distortion")) {
        TriggerBulletCancel(Add(camera_ ? camera_->GetTranslate() : Vector3{}, Scale(GetCameraForward(camera_), 8.0f)));
    }
    ImGui::SameLine();
    if (ImGui::Button("Test Enemy Defeat Distortion")) {
        triggerEnemyDefeatAtDepth(10.0f);
    }
    if (ImGui::Button("Test Enemy Defeat Near")) {
        triggerEnemyDefeatAtDepth(distanceScaleSettings_.nearDepth);
    }
    ImGui::SameLine();
    if (ImGui::Button("Test Enemy Defeat Mid")) {
        triggerEnemyDefeatAtDepth((distanceScaleSettings_.nearDepth + distanceScaleSettings_.farDepth) * 0.5f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Test Enemy Defeat Far")) {
        triggerEnemyDefeatAtDepth(distanceScaleSettings_.farDepth);
    }
    ImGui::Text("Active Instance Count: %d", activeInstanceCount_);
    ImGui::Text("Dropped Instance Count: %llu", static_cast<unsigned long long>(droppedInstanceCount_));
    ImGui::Text("Last Trigger Type: %s", GetTriggerTypeName(lastTriggerType_));
    ImGui::Text("Last World Position: %.2f, %.2f, %.2f", lastWorldPosition_.x, lastWorldPosition_.y, lastWorldPosition_.z);
    ImGui::Text("Last Screen UV: %.3f, %.3f", lastScreenUv_.x, lastScreenUv_.y);
    ImGui::Text("Last Impact Depth: %.2f", lastImpactDepth_);
    ImGui::Text("Computed Radius Scale: %.2f", lastComputedRadiusScale_);
    ImGui::Text("Computed Strength Scale: %.2f", lastComputedStrengthScale_);
    ImGui::Text("Computed Chromatic Scale: %.2f", lastComputedChromaticScale_);
    ImGui::Text("Computed Ring Scale: %.2f", lastComputedRingScale_);
    ImGui::TextWrapped("Last Result: %s", lastResult_.c_str());
    ImGui::Checkbox("Show Screen Position Marker", &showScreenPositionMarker_);
    ImGui::Checkbox("Force Strong Distortion", &forceStrongDistortion_);
    ImGui::Checkbox("Disable Chromatic", &disableChromatic_);
    ImGui::Checkbox("Disable Ring Highlight", &disableRingHighlight_);
    ImGui::Checkbox("Disable Flash", &disableFlash_);

    ImGui::SeparatorText("Distance Based Scale");
    ImGui::Checkbox("Enable Distance Based Scale", &enableDistanceBasedScale_);
    ImGui::DragFloat("Near Depth", &distanceScaleSettings_.nearDepth, 0.5f, 0.1f, 500.0f, "%.1f");
    ImGui::DragFloat("Far Depth", &distanceScaleSettings_.farDepth, 0.5f, 0.1f, 1000.0f, "%.1f");
    ImGui::DragFloat("Near Radius Scale", &distanceScaleSettings_.nearRadiusScale, 0.01f, 0.1f, 3.0f, "%.2f");
    ImGui::DragFloat("Far Radius Scale", &distanceScaleSettings_.farRadiusScale, 0.01f, 0.1f, 3.0f, "%.2f");
    ImGui::DragFloat("Near Strength Scale", &distanceScaleSettings_.nearStrengthScale, 0.01f, 0.1f, 3.0f, "%.2f");
    ImGui::DragFloat("Far Strength Scale", &distanceScaleSettings_.farStrengthScale, 0.01f, 0.1f, 3.0f, "%.2f");
    ImGui::DragFloat("Near Chromatic Scale", &distanceScaleSettings_.nearChromaticScale, 0.01f, 0.1f, 3.0f, "%.2f");
    ImGui::DragFloat("Far Chromatic Scale", &distanceScaleSettings_.farChromaticScale, 0.01f, 0.1f, 3.0f, "%.2f");
    ImGui::DragFloat("Near Ring Scale", &distanceScaleSettings_.nearRingScale, 0.01f, 0.1f, 3.0f, "%.2f");
    ImGui::DragFloat("Far Ring Scale", &distanceScaleSettings_.farRingScale, 0.01f, 0.1f, 3.0f, "%.2f");
    ImGui::Checkbox("Apply Distance Scale To Bullet Cancel", &applyDistanceScaleToBulletCancel_);
    ImGui::DragFloat("Bullet Cancel Distance Scale Strength", &bulletCancelDistanceScaleStrength_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::TextDisabled("Clamp: radius 0.60-1.30, strength 0.60-1.20, chromatic 0.50-1.20, ring 0.60-1.20");

    auto drawPreset = [](const char* label, EffectPreset& preset) {
        ImGui::SeparatorText(label);
        ImGui::DragFloat("Lifetime", &preset.lifetime, 0.01f, 0.01f, 3.0f, "%.3f");
        ImGui::DragFloat("Start Radius", &preset.startRadius, 0.001f, 0.001f, 1.0f, "%.3f");
        ImGui::DragFloat("End Radius", &preset.endRadius, 0.001f, 0.001f, 1.0f, "%.3f");
        ImGui::DragFloat("Distortion Strength", &preset.distortionStrength, 0.001f, 0.0f, 0.3f, "%.3f");
        ImGui::DragFloat("Ring Thickness", &preset.ringThickness, 0.001f, 0.001f, 0.2f, "%.3f");
        ImGui::DragFloat("Ring Strength", &preset.ringStrength, 0.01f, 0.0f, 2.0f, "%.2f");
        ImGui::DragFloat("Chromatic Strength", &preset.chromaticStrength, 0.001f, 0.0f, 0.1f, "%.3f");
        ImGui::DragFloat("Flash Strength", &preset.flashStrength, 0.01f, 0.0f, 2.0f, "%.2f");
        ImGui::SliderInt("Max Spawn Per Frame", &preset.maxSpawnPerFrame, 0, 16);
    };
    drawPreset("BulletCancel Params", bulletCancelPreset_);
    drawPreset("EnemyDefeat Params", enemyDefeatPreset_);
    ImGui::Text("Quality: %s", GetQualityModeName());
    ImGui::End();
#endif
}
