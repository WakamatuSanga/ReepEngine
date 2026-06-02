#include "Engine/Graphics/Effect/PrimitiveEffectSystem.h"
#include "Engine/Graphics/Effect/CylinderEffect.h"
#include "Engine/Graphics/Effect/RingEffect.h"
#include "Engine/Graphics/Effect/RotatingPlaneHitEffect.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

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

void PrimitiveEffectSystem::SetVisible(bool isVisible) {
    isVisible_ = isVisible;
}
