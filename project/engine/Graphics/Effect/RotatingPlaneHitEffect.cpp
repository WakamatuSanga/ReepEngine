#include "Engine/Graphics/Effect/RotatingPlaneHitEffect.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Model/Model.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include <algorithm>
#include <cmath>
#include <numbers>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr int kMaxPlaneCount = 8;
    constexpr const char* kPlaneModelKey = "PrimitiveEffect_RotatingPlane";
    constexpr const char* kPlaneTexturePath = "resources/particle/circle2.png";
    constexpr const char* kDefaultPrimitiveTexturePath = "resources/obj/axis/uvChecker.png";

    float Clamp01(float value) {
        return std::clamp(value, 0.0f, 1.0f);
    }

    Vector3 Lerp(const Vector3& a, const Vector3& b, float t) {
        return {
            std::lerp(a.x, b.x, t),
            std::lerp(a.y, b.y, t),
            std::lerp(a.z, b.z, t)
        };
    }
}

RotatingPlaneHitEffect::RotatingPlaneHitEffect() = default;

RotatingPlaneHitEffect::~RotatingPlaneHitEffect() = default;

void RotatingPlaneHitEffect::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    auto* textureManager = TextureManager::GetInstance();
    textureManager->LoadTexture(kPlaneTexturePath);

    planeModel_ = ModelManager::GetInstance()->CreatePlane(kPlaneModelKey);
    if (planeModel_) {
        textureIndex_ = textureManager->GetTextureIndexByFilePath(kPlaneTexturePath);
        isUsingDefaultTexture_ = false;
        planeModel_->SetTextureIndex(textureIndex_);
        if (auto* material = planeModel_->GetMaterialData()) {
            material->enableLighting = 0;
            material->alphaReference = 0.5f;
        }
    }

    planes_.clear();
    planes_.reserve(kMaxPlaneCount);
    for (int index = 0; index < kMaxPlaneCount; ++index) {
        auto plane = std::make_unique<Object3d>();
        plane->Initialize(object3dCommon);
        plane->SetModel(planeModel_);
        plane->SetCamera(camera);
        plane->SetEnvironmentMapEnabled(false);
        planes_.push_back(std::move(plane));
    }

    Play();
}

void RotatingPlaneHitEffect::Play() {
    elapsedTime_ = 0.0f;
    isPlaying_ = true;
}

void RotatingPlaneHitEffect::Stop() {
    isPlaying_ = false;
}

void RotatingPlaneHitEffect::Update(float deltaTime) {
    if (!isPlaying_) {
        return;
    }

    elapsedTime_ += (std::max)(0.0f, deltaTime);
    const float lifetime = (std::max)(0.0001f, settings_.lifetime);
    const float t = Clamp01(elapsedTime_ / lifetime);
    const float alpha = settings_.color.w * (1.0f - t);
    const Vector3 scale = Lerp(settings_.startScale, settings_.endScale, t);

    ApplyMaterial(alpha);

    const int planeCount = GetActivePlaneCount();
    for (int index = 0; index < planeCount; ++index) {
        const float zRotation =
            settings_.baseRotation.z +
            settings_.rotationStep * static_cast<float>(index) +
            settings_.rotationSpeed * elapsedTime_;
        Object3d* plane = planes_[static_cast<size_t>(index)].get();
        plane->SetTranslate(settings_.position);
        plane->SetRotate({
            settings_.baseRotation.x,
            settings_.baseRotation.y,
            zRotation
        });
        plane->SetScale(scale);
        plane->Update();
    }

    if (elapsedTime_ >= lifetime) {
        isPlaying_ = false;
    }
}

void RotatingPlaneHitEffect::Draw() {
    if (!isVisible_ || !isPlaying_) {
        return;
    }

    const int planeCount = GetActivePlaneCount();
    for (int index = 0; index < planeCount; ++index) {
        planes_[static_cast<size_t>(index)]->Draw();
    }
}

void RotatingPlaneHitEffect::DrawImGui() {
#ifdef _DEBUG
    ImGui::Checkbox("表示 (Show)##RotatingPlaneHit", &isVisible_);
    ImGui::SameLine();
    if (ImGui::Button("再生 (Play)##RotatingPlaneHit")) {
        Play();
    }
    ImGui::SameLine();
    if (ImGui::Button("停止 (Stop)##RotatingPlaneHit")) {
        Stop();
    }
    ImGui::Text("再生状態 (Playing): %s", isPlaying_ ? "ON" : "OFF");
    ImGui::SeparatorText("Texture Debug");
    ImGui::Text("Texture Path: %s", kPlaneTexturePath);
    ImGui::Text("Texture Index: %u", textureIndex_);
    ImGui::Text("Texture Handle: 0x%llX",
        static_cast<unsigned long long>(TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_).ptr));
    ImGui::Text("Using Default Texture: %s", isUsingDefaultTexture_ ? "true" : "false");
    ImGui::TextDisabled("Primitive default: %s", kDefaultPrimitiveTexturePath);
    ImGui::DragFloat3("位置 (Position)##RotatingPlaneHit", &settings_.position.x, 0.05f);
    ImGui::DragFloat3("基準回転 (Base Rotation)##RotatingPlaneHit", &settings_.baseRotation.x, 0.01f);
    ImGui::SliderInt("枚数 (Plane Count)##RotatingPlaneHit", &settings_.planeCount, 1, kMaxPlaneCount);
    ImGui::DragFloat("寿命 (Lifetime)##RotatingPlaneHit", &settings_.lifetime, 0.01f, 0.05f, 3.0f, "%.2f");
    ImGui::DragFloat("回転間隔 (Rotation Step)##RotatingPlaneHit", &settings_.rotationStep, 0.01f, 0.0f, std::numbers::pi_v<float>, "%.2f");
    ImGui::DragFloat("回転速度 (Rotation Speed)##RotatingPlaneHit", &settings_.rotationSpeed, 0.05f, -20.0f, 20.0f, "%.2f");
    ImGui::DragFloat3("開始スケール (Start Scale)##RotatingPlaneHit", &settings_.startScale.x, 0.02f, 0.01f, 5.0f, "%.2f");
    ImGui::DragFloat3("終了スケール (End Scale)##RotatingPlaneHit", &settings_.endScale.x, 0.02f, 0.01f, 5.0f, "%.2f");
    ImGui::ColorEdit4("色 (Color)##RotatingPlaneHit", &settings_.color.x);
#endif
}

void RotatingPlaneHitEffect::ApplyMaterial(float alpha) {
    if (!planeModel_) {
        return;
    }

    if (auto* material = planeModel_->GetMaterialData()) {
        material->color = {
            settings_.color.x,
            settings_.color.y,
            settings_.color.z,
            alpha
        };
    }
}

int RotatingPlaneHitEffect::GetActivePlaneCount() const {
    return std::clamp(settings_.planeCount, 1, kMaxPlaneCount);
}
