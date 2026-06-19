#include "Engine/Graphics/Effect/RingEffect.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Model/Model.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr uint32_t kRingSubdivision = 32u;
    constexpr const char* kRingModelKey = "PrimitiveEffect_Ring";
    constexpr const char* kRingTexturePath = "resources/particle/gradationLine.png";
    constexpr const char* kDefaultPrimitiveTexturePath = "resources/obj/axis/uvChecker.png";

    float Clamp01(float value) {
        return std::clamp(value, 0.0f, 1.0f);
    }
}

RingEffect::RingEffect() = default;

RingEffect::~RingEffect() = default;

void RingEffect::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    TextureManager::GetInstance()->LoadTexture(kRingTexturePath);
    RebuildModelIfNeeded();

    ownedObject_ = std::make_unique<Object3d>();
    object3d_ = ownedObject_.get();
    object3d_->Initialize(object3dCommon);
    object3d_->SetModel(ringModel_);
    object3d_->SetCamera(camera);
    object3d_->SetEnvironmentMapEnabled(false);
    object3d_->SetRingAppearanceEnabled(false);
    object3d_->SetRingUVDirection(0);
    object3d_->SetRingStartFadeRange(0.15f);
    object3d_->SetRingEndFadeRange(0.15f);
    object3d_->SetRingInnerColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    object3d_->SetRingOuterColor(settings_.color);

    Play();
}

void RingEffect::Play() {
    elapsedTime_ = 0.0f;
    isPlaying_ = true;
}

void RingEffect::PlayAt(const Vector3& position) {
    settings_.position = position;
    Play();
}

void RingEffect::Stop() {
    isPlaying_ = false;
}

void RingEffect::Update(float deltaTime, const Camera* camera) {
    RebuildModelIfNeeded();

    if (!object3d_ || !isPlaying_) {
        return;
    }

    elapsedTime_ += (std::max)(0.0f, deltaTime);
    const float lifetime = (std::max)(0.0001f, settings_.lifetime);
    const float t = Clamp01(elapsedTime_ / lifetime);
    const float scale = std::lerp(settings_.startScale, settings_.endScale, t);
    const float alpha = std::lerp(settings_.startAlpha, settings_.endAlpha, t);

    Vector3 rotation = settings_.rotation;
    if (settings_.useBillboard && camera) {
        const Vector3 cameraRotation = camera->GetRotate();
        rotation.x += cameraRotation.x;
        rotation.y += cameraRotation.y;
        rotation.z += cameraRotation.z;
    }

    ApplyMaterial(alpha);
    object3d_->SetTranslate(settings_.position);
    object3d_->SetRotate(rotation);
    object3d_->SetScale({ scale, scale, 1.0f });
    object3d_->SetRingInnerRadiusRatio(GetInnerRadius());
    object3d_->SetRingStartAlpha(alpha);
    object3d_->SetRingEndAlpha(alpha);
    object3d_->SetRingOuterColor({
        settings_.color.x,
        settings_.color.y,
        settings_.color.z,
        alpha
    });
    object3d_->Update();

    if (elapsedTime_ >= lifetime) {
        isPlaying_ = false;
    }
}

void RingEffect::Draw() {
    if (!isVisible_ || !isPlaying_ || !object3d_) {
        return;
    }

    object3d_->Draw();
}

void RingEffect::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::Checkbox("表示 (Show)##RingEffect", &isVisible_);
    ImGui::SameLine();
    if (ImGui::Button("再生 (Play)##RingEffect")) {
        Play();
    }
    ImGui::SameLine();
    if (ImGui::Button("停止 (Stop)##RingEffect")) {
        Stop();
    }
    ImGui::Text("再生状態 (Playing): %s", isPlaying_ ? "ON" : "OFF");
    ImGui::SeparatorText("Texture Debug");
    ImGui::Text("Texture Path: %s", kRingTexturePath);
    ImGui::Text("Texture Index: %u", textureIndex_);
    ImGui::Text("Texture Handle: 0x%llX",
        static_cast<unsigned long long>(TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_).ptr));
    ImGui::Text("Using Default Texture: %s", isUsingDefaultTexture_ ? "true" : "false");
    ImGui::TextDisabled("Primitive default: %s", kDefaultPrimitiveTexturePath);
    ImGui::DragFloat3("位置 (Position)##RingEffect", &settings_.position.x, 0.05f);
    ImGui::DragFloat3("回転 (Rotation)##RingEffect", &settings_.rotation.x, 0.01f);
    ImGui::DragFloat("寿命 (Lifetime)##RingEffect", &settings_.lifetime, 0.01f, 0.05f, 5.0f, "%.2f");
    ImGui::DragFloat("開始スケール (Start Scale)##RingEffect", &settings_.startScale, 0.02f, 0.01f, 5.0f, "%.2f");
    ImGui::DragFloat("終了スケール (End Scale)##RingEffect", &settings_.endScale, 0.02f, 0.01f, 8.0f, "%.2f");
    if (ImGui::DragFloat("太さ (Thickness)##RingEffect", &settings_.thickness, 0.01f, 0.05f, 0.95f, "%.2f")) {
        settings_.thickness = std::clamp(settings_.thickness, 0.05f, 0.95f);
        isModelDirty_ = true;
    }
    ImGui::DragFloat("開始透明度 (Start Alpha)##RingEffect", &settings_.startAlpha, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("終了透明度 (End Alpha)##RingEffect", &settings_.endAlpha, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::Checkbox("ビルボード (Billboard)##RingEffect", &settings_.useBillboard);
    ImGui::ColorEdit4("色 (Color)##RingEffect", &settings_.color.x);
#endif
}

void RingEffect::RebuildModelIfNeeded() {
    if (!isModelDirty_) {
        return;
    }

    ringModel_ = ModelManager::GetInstance()->CreateRing(
        kRingModelKey,
        kRingSubdivision,
        GetInnerRadius(),
        1.0f);
    if (ringModel_) {
        textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(kRingTexturePath);
        isUsingDefaultTexture_ = false;
        ringModel_->SetTextureIndex(textureIndex_);
        if (auto* material = ringModel_->GetMaterialData()) {
            material->enableLighting = 0;
            material->alphaReference = 0.5f;
        }
        if (object3d_) {
            object3d_->SetModel(ringModel_);
        }
    }

    isModelDirty_ = false;
}

void RingEffect::ApplyMaterial(float alpha) {
    if (!ringModel_) {
        return;
    }

    if (auto* material = ringModel_->GetMaterialData()) {
        material->color = {
            settings_.color.x,
            settings_.color.y,
            settings_.color.z,
            alpha
        };
    }
}

float RingEffect::GetInnerRadius() const {
    return std::clamp(1.0f - settings_.thickness, 0.05f, 0.95f);
}

