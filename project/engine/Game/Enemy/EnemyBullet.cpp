#include "EnemyBullet.h"
#include "Engine/Graphics/Model/Model.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kMinVectorLength = 0.00001f;
    constexpr float kPi = 3.14159265358979323846f;

    std::string ToGenericString(const std::filesystem::path& path) {
        return path.lexically_normal().generic_string();
    }

    std::string ResolveResourcePath(const std::string& path) {
        const std::array<std::filesystem::path, 6> basePaths = {
            std::filesystem::path{},
            std::filesystem::path{ "project" },
            std::filesystem::path{ ".." } / "project",
            std::filesystem::path{ ".." } / ".." / "project",
            std::filesystem::path{ ".." } / ".." / ".." / "project",
            std::filesystem::path{ ".." } / ".." / ".." / ".." / "project",
        };

        for (const std::filesystem::path& basePath : basePaths) {
            const std::filesystem::path candidate = basePath.empty()
                ? std::filesystem::path(path)
                : basePath / path;
            if (std::filesystem::exists(candidate)) {
                return ToGenericString(candidate);
            }
        }
        return {};
    }

    Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 ScaleVector3(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    float Length(const Vector3& value) {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    }

    Vector3 Normalize(const Vector3& value, const Vector3& fallback) {
        const float length = Length(value);
        if (length <= kMinVectorLength) {
            return fallback;
        }
        return { value.x / length, value.y / length, value.z / length };
    }

    Vector3 MakeRotationFromForward(const Vector3& forward) {
        const Vector3 normalized = Normalize(forward, { 0.0f, 0.0f, 1.0f });
        const float horizontal = std::sqrt(normalized.x * normalized.x + normalized.z * normalized.z);
        const float yaw = std::atan2(normalized.x, normalized.z);
        const float pitch = std::atan2(-normalized.y, horizontal);
        return { pitch, yaw, 0.0f };
    }
}

EnemyBullet::EnemyBullet() = default;

EnemyBullet::~EnemyBullet() = default;

void EnemyBullet::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    object3dCommon_ = object3dCommon;
    camera_ = camera;
    currentTime_ = 0.0f;
    isActive_ = true;
    isDead_ = false;

    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon_);
    object_->SetCamera(camera_);
    object_->SetEnvironmentMapEnabled(false);
    radiusObject_ = std::make_unique<Object3d>();
    radiusObject_->Initialize(object3dCommon_);
    radiusObject_->SetCamera(camera_);
    radiusObject_->SetEnvironmentMapEnabled(false);
    radiusModel_ = ModelManager::GetInstance()->CreateSphere("EnemyBulletRadiusSphere", 12);
    radiusObject_->SetModel(radiusModel_);

    LoadModel();
    UpdateObjectTransform();
    object_->Update();
    radiusObject_->Update();
}

void EnemyBullet::Finalize() {
    radiusObject_.reset();
    radiusModel_ = nullptr;
    object_.reset();
    model_ = nullptr;
}

void EnemyBullet::Update(float deltaTime) {
    if (!object_ || !isActive_ || isDead_) {
        return;
    }

    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    currentTime_ += safeDeltaTime;
    if (currentTime_ >= lifeTime_) {
        Kill();
        return;
    }

    position_ = AddVector3(position_, ScaleVector3(velocity_, safeDeltaTime));
    UpdateObjectTransform();
    object_->Update();
    if (radiusObject_) {
        radiusObject_->Update();
    }
}

void EnemyBullet::Draw() {
    if (!object3dCommon_ || !object_ || !model_ || !isActive_ || isDead_) {
        return;
    }

    object3dCommon_->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    object_->Draw();
}

void EnemyBullet::DrawRadius() {
    if (!object3dCommon_ || !radiusObject_ || !radiusModel_ || !isActive_ || isDead_) {
        return;
    }

    object3dCommon_->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    radiusObject_->Draw();
}

void EnemyBullet::DrawImGui() {
#ifdef _DEBUG
    ImGui::Text("Active: %s / Dead: %s", isActive_ ? "true" : "false", isDead_ ? "true" : "false");
    ImGui::TextWrapped("Model Path: %s", modelPath_.c_str());
    ImGui::TextWrapped("Resolved Model Path: %s", resolvedModelPath_.empty() ? "(none)" : resolvedModelPath_.c_str());
    ImGui::TextWrapped("Texture Path: %s", texturePath_.empty() ? "(none)" : texturePath_.c_str());
    ImGui::TextWrapped("Load Status: %s", loadStatus_.c_str());
    ImGui::Text("Fallback: %s", useFallbackModel_ ? "true" : "false");
    ImGui::DragFloat3("Position", &position_.x, 0.05f, -100.0f, 100.0f, "%.2f");
    ImGui::DragFloat3("Velocity", &velocity_.x, 0.05f, -100.0f, 100.0f, "%.2f");
    ImGui::DragFloat3("Rotation", &rotation_.x, 0.01f, -6.28318f, 6.28318f, "%.3f");
    ImGui::SeparatorText("敵弾モデル向き補正 (Enemy Bullet Model Rotation Offset)");
    if (ImGui::Button("Yaw +90")) {
        modelRotationOffset_.y += kPi * 0.5f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Yaw -90")) {
        modelRotationOffset_.y -= kPi * 0.5f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Yaw 180")) {
        modelRotationOffset_.y += kPi;
    }
    if (ImGui::Button("向き補正リセット")) {
        modelRotationOffset_ = { 0.0f, 0.0f, 0.0f };
    }
    ImGui::DragFloat3("Enemy Bullet Model Rotation Offset", &modelRotationOffset_.x, 0.01f, -6.28318f, 6.28318f, "%.3f");
    ImGui::Text("Velocity Forward: %.3f, %.3f, %.3f", lastVisualForward_.x, lastVisualForward_.y, lastVisualForward_.z);
    ImGui::Text("Base Rotation: %.3f, %.3f, %.3f",
        visualBaseRotation_.x,
        visualBaseRotation_.y,
        visualBaseRotation_.z);
    ImGui::Text("Current Model Rotation: %.3f, %.3f, %.3f",
        visualModelRotation_.x,
        visualModelRotation_.y,
        visualModelRotation_.z);
    ImGui::DragFloat3("Scale", &scale_.x, 0.01f, 0.001f, 20.0f, "%.3f");
    ImGui::SeparatorText("敵弾当たり判定 (EnemyBullet Radius)");
    ImGui::DragFloat("EnemyBullet Radius", &radius_, 0.01f, 0.001f, 20.0f, "%.3f");
    if (ImGui::Button("Bullet Radius Small")) {
        radius_ = 0.10f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Bullet Radius Normal")) {
        radius_ = 0.15f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Bullet Radius Large")) {
        radius_ = 0.25f;
    }
    ImGui::DragFloat("Life Time", &lifeTime_, 0.05f, 0.1f, 60.0f, "%.2f");
    ImGui::Text("Current Time: %.2f", currentTime_);
    if (ImGui::Button("Kill Bullet")) {
        Kill();
    }
    UpdateObjectTransform();
    if (object_) {
        object_->Update();
    }
    if (radiusObject_) {
        radiusObject_->Update();
    }
#endif
}

void EnemyBullet::SetPosition(const Vector3& position) {
    position_ = position;
    UpdateObjectTransform();
}

void EnemyBullet::SetVelocity(const Vector3& velocity) {
    velocity_ = velocity;
    UpdateObjectTransform();
}

void EnemyBullet::SetRotation(const Vector3& rotation) {
    rotation_ = rotation;
    UpdateObjectTransform();
}

void EnemyBullet::SetScale(const Vector3& scale) {
    scale_ = scale;
    UpdateObjectTransform();
}

void EnemyBullet::SetRadius(float radius) {
    radius_ = (std::max)(0.001f, radius);
}

void EnemyBullet::Kill() {
    isDead_ = true;
    isActive_ = false;
}

void EnemyBullet::LoadModel() {
    useFallbackModel_ = false;
    resolvedModelPath_ = ResolveResourcePath(modelPath_);
    texturePath_.clear();
    model_ = nullptr;

    ModelManager* modelManager = ModelManager::GetInstance();
    if (!resolvedModelPath_.empty()) {
        modelManager->LoadModel(resolvedModelPath_);
        model_ = modelManager->FindModel(resolvedModelPath_);
        if (model_) {
            const std::filesystem::path resolvedPath(resolvedModelPath_);
            texturePath_ = ToGenericString(resolvedPath.parent_path() / "EnemyBullet.png");
            if (!std::filesystem::exists(std::filesystem::path(texturePath_))) {
                texturePath_ = ResolveResourcePath("resources/obj/axis/uvChecker.png");
            }
            if (!texturePath_.empty()) {
                TextureManager::GetInstance()->LoadTexture(texturePath_);
                model_->SetTextureIndex(TextureManager::GetInstance()->GetTextureIndexByFilePath(texturePath_));
            }
            loadStatus_ =
                !texturePath_.empty() && std::filesystem::path(texturePath_).filename().string() == "EnemyBullet.png"
                ? "Enemy bullet model loaded."
                : "Enemy bullet model loaded. Texture missing, using fallback texture.";
        }
    }

    if (!model_) {
        model_ = modelManager->CreateSphere("EnemyBulletFallbackSphere", 12);
        useFallbackModel_ = true;
        loadStatus_ = "Enemy bullet model missing. Using fallback sphere.";
    }

    if (object_) {
        object_->SetModel(model_);
    }
}

void EnemyBullet::UpdateObjectTransform() {
    if (!object_) {
        return;
    }

    object_->SetTranslate(position_);
    if (Length(velocity_) > kMinVectorLength) {
        lastVisualForward_ = Normalize(velocity_, lastVisualForward_);
    }
    visualBaseRotation_ = MakeRotationFromForward(lastVisualForward_);
    visualModelRotation_ = AddVector3(
        AddVector3(visualBaseRotation_, rotation_),
        modelRotationOffset_);
    object_->SetRotate(visualModelRotation_);
    object_->SetScale(scale_);
    if (radiusObject_) {
        radiusObject_->SetTranslate(position_);
        radiusObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
        radiusObject_->SetScale({ radius_, radius_, radius_ });
    }
}
