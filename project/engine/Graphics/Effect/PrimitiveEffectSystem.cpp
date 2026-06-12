#include "Engine/Graphics/Effect/PrimitiveEffectSystem.h"
#include "Engine/Graphics/Effect/CylinderEffect.h"
#include "Engine/Graphics/Effect/RingEffect.h"
#include "Engine/Graphics/Effect/RotatingPlaneHitEffect.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include <algorithm>
#include <cctype>
#include <string>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    std::string ToLowerString(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
            });
        return text;
    }
}

PrimitiveEffectSystem::PrimitiveEffectSystem() = default;

PrimitiveEffectSystem::~PrimitiveEffectSystem() = default;

void PrimitiveEffectSystem::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    object3dCommon_ = object3dCommon;
    camera_ = camera;

    rotatingPlaneHitEffect_ = std::make_unique<RotatingPlaneHitEffect>();
    rotatingPlaneHitEffect_->Initialize(object3dCommon_, camera_);

    ringEffect_ = std::make_unique<RingEffect>();
    ringEffect_->Initialize(object3dCommon_, camera_);

    cylinderEffect_ = std::make_unique<CylinderEffect>();
    cylinderEffect_->Initialize(object3dCommon_, camera_);
}

void PrimitiveEffectSystem::Update(float deltaTime) {
    if (rotatingPlaneHitEffect_) {
        rotatingPlaneHitEffect_->Update(deltaTime);
    }
    if (ringEffect_) {
        ringEffect_->Update(deltaTime, camera_);
    }
    if (cylinderEffect_) {
        cylinderEffect_->Update(deltaTime);
    }
}

void PrimitiveEffectSystem::Draw() {
    if (!isVisible_ || !object3dCommon_) {
        return;
    }

    object3dCommon_->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    if (rotatingPlaneHitEffect_) {
        rotatingPlaneHitEffect_->Draw();
    }
    if (ringEffect_) {
        ringEffect_->Draw();
    }
    object3dCommon_->CommonDrawSetting(Object3dCommon::BlendMode::kAdd);
    if (cylinderEffect_) {
        cylinderEffect_->Draw();
    }
    object3dCommon_->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
}

void PrimitiveEffectSystem::DrawImGui() {
#ifdef _DEBUG
    ImGui::SetNextWindowSize(ImVec2(420, 520), ImGuiCond_Once);
    ImGui::Begin("Primitive Effect Debug / プリミティブエフェクト調整");
    ImGui::Checkbox("全体表示 (Show All)", &isVisible_);

    if (ImGui::CollapsingHeader("Plane回転ヒット (Rotating Plane Hit)", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (rotatingPlaneHitEffect_) {
            rotatingPlaneHitEffect_->DrawImGui();
        }
    }
    if (ImGui::CollapsingHeader("Ringエフェクト (Ring Effect)", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ringEffect_) {
            ringEffect_->DrawImGui();
        }
    }
    if (ImGui::CollapsingHeader("Cylinderエフェクト (Cylinder Effect)", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (cylinderEffect_) {
            cylinderEffect_->DrawImGui();
        }
    }

    ImGui::End();
#endif
}

void PrimitiveEffectSystem::DrawVisibilityImGui() {
#ifdef _DEBUG
    ImGui::Checkbox("Primitive Effects", &isVisible_);
    if (!isVisible_) {
        return;
    }

    ImGui::Indent();
    if (rotatingPlaneHitEffect_) {
        bool isEffectVisible = rotatingPlaneHitEffect_->IsVisible();
        if (ImGui::Checkbox("Rotating Plane Hit", &isEffectVisible)) {
            rotatingPlaneHitEffect_->SetVisible(isEffectVisible);
        }
    }
    if (ringEffect_) {
        bool isEffectVisible = ringEffect_->IsVisible();
        if (ImGui::Checkbox("Ring Effect", &isEffectVisible)) {
            ringEffect_->SetVisible(isEffectVisible);
        }
    }
    if (cylinderEffect_) {
        bool isEffectVisible = cylinderEffect_->IsVisible();
        if (ImGui::Checkbox("Cylinder Effect", &isEffectVisible)) {
            cylinderEffect_->SetVisible(isEffectVisible);
        }
    }
    ImGui::Unindent();
#endif
}

void PrimitiveEffectSystem::PlayHitEffectAt(const Vector3& position) {
    if (rotatingPlaneHitEffect_) {
        rotatingPlaneHitEffect_->PlayAt(position);
    }
}

void PrimitiveEffectSystem::PlayRingEffectAt(const Vector3& position) {
    if (ringEffect_) {
        ringEffect_->PlayAt(position);
    }
}

void PrimitiveEffectSystem::PlayCylinderEffectAt(const Vector3& position) {
    if (cylinderEffect_) {
        cylinderEffect_->PlayAt(position);
    }
}

bool PrimitiveEffectSystem::PlayPresetAt(
    const std::string& effectType,
    const Vector3& position,
    std::string& resultMessage) {
    const std::string type = ToLowerString(effectType.empty() ? "hitring" : effectType);
    if (type == "hit") {
        PlayHitEffectAt(position);
        resultMessage = "Played Hit effect.";
        return true;
    }
    if (type == "ring") {
        PlayRingEffectAt(position);
        resultMessage = "Played Ring effect.";
        return true;
    }
    if (type == "cylinder") {
        PlayCylinderEffectAt(position);
        resultMessage = "Played Cylinder effect.";
        return true;
    }
    if (type == "hitring" || type == "hit_ring" || type == "hit+ring") {
        PlayHitEffectAt(position);
        PlayRingEffectAt(position);
        resultMessage = "Played HitRing effect.";
        return true;
    }

    PlayHitEffectAt(position);
    PlayRingEffectAt(position);
    resultMessage = "Unknown effectType, played HitRing fallback: " + effectType;
    return true;
}

void PrimitiveEffectSystem::SetVisible(bool isVisible) {
    isVisible_ = isVisible;
}
