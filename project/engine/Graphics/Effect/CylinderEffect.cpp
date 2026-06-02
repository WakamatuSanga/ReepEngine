#include "Engine/Graphics/Effect/CylinderEffect.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Model/Model.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include <algorithm>
#include <cmath>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr const char* kCylinderModelKey = "PrimitiveEffect_Cylinder";
    constexpr const char* kCylinderTexturePath = "resources/particle/gradationLine.png";
    constexpr const char* kDefaultPrimitiveTexturePath = "resources/obj/axis/uvChecker.png";

    float Clamp01(float value) {
        return std::clamp(value, 0.0f, 1.0f);
    }
}

CylinderEffect::CylinderEffect() = default;

CylinderEffect::~CylinderEffect() = default;

void CylinderEffect::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    auto* textureManager = TextureManager::GetInstance();
    textureManager->LoadTexture(kCylinderTexturePath);

    cylinderModel_ = ModelManager::GetInstance()->CreateEffectCylinder(kCylinderModelKey, 32);
    if (cylinderModel_) {
        textureIndex_ = textureManager->GetTextureIndexByFilePath(kCylinderTexturePath);
        isUsingDefaultTexture_ = false;
        cylinderModel_->SetTextureIndex(textureIndex_);
        if (auto* material = cylinderModel_->GetMaterialData()) {
            material->enableLighting = 0;
            material->alphaReference = 0.0f;
        }
    }

    cylinder_ = std::make_unique<Object3d>();
    cylinder_->Initialize(object3dCommon);
    cylinder_->SetModel(cylinderModel_);
    cylinder_->SetCamera(camera);
    cylinder_->SetEnvironmentMapEnabled(false);

    Play();
}

void CylinderEffect::Play() {
    elapsedTime_ = 0.0f;
    uvTime_ = 0.0f;
    isPlaying_ = true;
}

void CylinderEffect::Stop() {
    isPlaying_ = false;
}

void CylinderEffect::Update(float deltaTime) {
    if (!cylinder_ || !isPlaying_) {
        return;
    }

    const float safeDeltaTime = (std::max)(0.0f, deltaTime);
    elapsedTime_ += safeDeltaTime;
    uvTime_ += safeDeltaTime;

    const float lifetime = (std::max)(0.0001f, settings_.lifetime);
    const float t = Clamp01(elapsedTime_ / lifetime);
    const float radius = std::lerp(settings_.startRadius, settings_.endRadius, t);
    const float height = std::lerp(settings_.startHeight, settings_.endHeight, t);
    const float alpha = settings_.color.w * (1.0f - t);

    ApplyMaterial(alpha);
    cylinder_->SetTranslate(settings_.position);
    cylinder_->SetRotate(settings_.rotation);
    cylinder_->SetScale({ radius, height, radius });
    cylinder_->Update();

    if (elapsedTime_ >= lifetime) {
        isPlaying_ = false;
    }
}

void CylinderEffect::Draw() {
    if (!isVisible_ || !isPlaying_ || !cylinder_) {
        return;
    }

    cylinder_->Draw();
}

void CylinderEffect::DrawImGui() {
#ifdef _DEBUG
    ImGui::Checkbox("表示 (Show)##CylinderEffect", &isVisible_);
    ImGui::SameLine();
    if (ImGui::Button("再生 (Play)##CylinderEffect")) {
        Play();
    }
    ImGui::SameLine();
    if (ImGui::Button("停止 (Stop)##CylinderEffect")) {
        Stop();
    }
    ImGui::Text("再生状態 (Playing): %s", isPlaying_ ? "ON" : "OFF");
    ImGui::SeparatorText("Texture Debug");
    ImGui::Text("Texture Path: %s", kCylinderTexturePath);
    ImGui::Text("Texture Index: %u", textureIndex_);
    ImGui::Text("Texture Handle: 0x%llX",
        static_cast<unsigned long long>(TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_).ptr));
    ImGui::Text("Using Default Texture: %s", isUsingDefaultTexture_ ? "true" : "false");
    ImGui::TextDisabled("Primitive default: %s", kDefaultPrimitiveTexturePath);
    ImGui::DragFloat3("位置 (Position)##CylinderEffect", &settings_.position.x, 0.05f);
    ImGui::DragFloat3("回転 (Rotation)##CylinderEffect", &settings_.rotation.x, 0.01f);
    ImGui::DragFloat("寿命 (Lifetime)##CylinderEffect", &settings_.lifetime, 0.01f, 0.05f, 5.0f, "%.2f");
    ImGui::DragFloat("開始半径 (Start Radius)##CylinderEffect", &settings_.startRadius, 0.02f, 0.01f, 5.0f, "%.2f");
    ImGui::DragFloat("終了半径 (End Radius)##CylinderEffect", &settings_.endRadius, 0.02f, 0.01f, 8.0f, "%.2f");
    ImGui::DragFloat("開始高さ (Start Height)##CylinderEffect", &settings_.startHeight, 0.02f, 0.01f, 5.0f, "%.2f");
    ImGui::DragFloat("終了高さ (End Height)##CylinderEffect", &settings_.endHeight, 0.02f, 0.01f, 8.0f, "%.2f");
    ImGui::Checkbox("UVスクロール (UV Scroll)##CylinderEffect", &settings_.uvScrollEnabled);
    ImGui::DragFloat("UV速度X (UV Speed X)##CylinderEffect", &settings_.uvScrollSpeedX, 0.02f, -4.0f, 4.0f, "%.2f");
    ImGui::DragFloat("UV速度Y (UV Speed Y)##CylinderEffect", &settings_.uvScrollSpeedY, 0.02f, -4.0f, 4.0f, "%.2f");
    ImGui::ColorEdit4("色 (Color)##CylinderEffect", &settings_.color.x);
#endif
}

void CylinderEffect::ApplyMaterial(float alpha) {
    if (!cylinderModel_) {
        return;
    }

    if (auto* material = cylinderModel_->GetMaterialData()) {
        material->color = {
            settings_.color.x,
            settings_.color.y,
            settings_.color.z,
            alpha
        };

        if (settings_.uvScrollEnabled) {
            material->uvTransform = MatrixMath::MakeAffine(
                { 1.0f, 1.0f, 1.0f },
                { 0.0f, 0.0f, 0.0f },
                {
                    uvTime_ * settings_.uvScrollSpeedX,
                    uvTime_ * settings_.uvScrollSpeedY,
                    0.0f
                });
        } else {
            material->uvTransform = MatrixMath::MakeIdentity4x4();
        }
    }
}
