#include "Engine/Core/DebugUiWorkspace.h"
#include "Engine/Graphics/Effect/PrimitiveEffectSystem.h"
#include "Engine/Graphics/Effect/CylinderEffect.h"
#include "Engine/Graphics/Effect/IPrimitiveEffect.h"
#include "Engine/Graphics/Effect/RingEffect.h"
#include "Engine/Graphics/Effect/RotatingPlaneHitEffect.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

#ifdef USE_IMGUI
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
    auto rotatingPlane = std::make_unique<RotatingPlaneHitEffect>();
    rotatingPlane->Initialize(object3dCommon, camera);

    auto ring = std::make_unique<RingEffect>();
    ring->Initialize(object3dCommon, camera);

    auto cylinder = std::make_unique<CylinderEffect>();
    cylinder->Initialize(object3dCommon, camera);

    auto* rotatingPlaneEffect = rotatingPlane.get();
    auto* ringEffect = ring.get();
    auto* cylinderEffect = cylinder.get();

    std::vector<std::unique_ptr<IPrimitiveEffect>> normalEffects;
    normalEffects.reserve(2);
    normalEffects.push_back(std::move(rotatingPlane));
    normalEffects.push_back(std::move(ring));
    std::vector<std::unique_ptr<IPrimitiveEffect>> additiveEffects;
    additiveEffects.push_back(std::move(cylinder));

    object3dCommon_ = object3dCommon;
    camera_ = camera;
    normalEffects_ = std::move(normalEffects);
    additiveEffects_ = std::move(additiveEffects);
    rotatingPlaneHitEffect_ = rotatingPlaneEffect;
    ringEffect_ = ringEffect;
    cylinderEffect_ = cylinderEffect;
}

void PrimitiveEffectSystem::Update(float deltaTime) {
    for (const auto* effects : { &normalEffects_, &additiveEffects_ }) {
        for (const auto& effect : *effects) {
            effect->Update(deltaTime, camera_);
        }
    }
}

void PrimitiveEffectSystem::Draw() {
    if (!isVisible_ || diagnosticSuppressed_ || !object3dCommon_) {
        return;
    }

    using BlendMode = Object3dCommon::BlendMode;
    const auto layers = {
        std::pair{ &normalEffects_, BlendMode::kNormal },
        std::pair{ &additiveEffects_, BlendMode::kAdd },
    };
    for (const auto& [effects, blendMode] : layers) {
        object3dCommon_->CommonDrawSetting(blendMode);
        for (const auto& effect : *effects) {
            effect->Draw();
        }
    }
    object3dCommon_->CommonDrawSetting(BlendMode::kNormal);
}

void PrimitiveEffectSystem::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(420, 520), ImGuiCond_Once);
    DebugUiWorkspace::BeginPanel("Primitive Effect Debug / プリミティブエフェクト調整");
    ImGui::Checkbox("全体表示 (Show All)", &isVisible_);
    if (diagnosticSuppressed_) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "影診断でPrimitive Effectが一時非表示です。");
    }

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

    DebugUiWorkspace::EndPanel();
#endif
}

size_t PrimitiveEffectSystem::GetEffectCount() const {
    return normalEffects_.size() + additiveEffects_.size();
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

void PrimitiveEffectSystem::PlayRingEffectAt(const Vector3& position, float scaleMultiplier, float alphaMultiplier) {
    if (ringEffect_) {
        ringEffect_->PlayAt(position, scaleMultiplier, alphaMultiplier);
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
