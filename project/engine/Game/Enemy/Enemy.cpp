#include "Enemy.h"
#include "Engine/Graphics/Model/Model.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include <algorithm>
#include <array>
#include <filesystem>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
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
}

Enemy::Enemy() = default;

Enemy::~Enemy() = default;

void Enemy::Initialize(Object3dCommon* object3dCommon, Camera* camera, const std::string& enemyId) {
    object3dCommon_ = object3dCommon;
    camera_ = camera;
    enemyId_ = enemyId;
    isActive_ = true;
    isDead_ = false;
    hp_ = 10;

    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon_);
    object_->SetCamera(camera_);
    object_->SetEnvironmentMapEnabled(false);

    LoadModel();
    UpdateObjectTransform();
    object_->Update();
}

void Enemy::Finalize() {
    object_.reset();
    model_ = nullptr;
}

void Enemy::Update(float deltaTime) {
    if (!object_ || !isActive_ || isDead_) {
        return;
    }

    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    position_ = AddVector3(position_, ScaleVector3(velocity_, safeDeltaTime));
    UpdateObjectTransform();
    object_->Update();
}

void Enemy::Draw() {
    if (!object3dCommon_ || !object_ || !model_ || !isActive_ || isDead_) {
        return;
    }

    object3dCommon_->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    object_->Draw();
}

void Enemy::DrawImGui() {
#ifdef _DEBUG
    ImGui::Text("Enemy ID: %s", enemyId_.c_str());
    ImGui::Text("Enemy Type: %s", enemyType_.c_str());
    ImGui::TextWrapped("Model Path: %s", modelPath_.c_str());
    ImGui::TextWrapped("Resolved Model Path: %s", resolvedModelPath_.empty() ? "(none)" : resolvedModelPath_.c_str());
    ImGui::TextWrapped("Texture Path: %s", texturePath_.empty() ? "(none)" : texturePath_.c_str());
    ImGui::TextWrapped("Load Status: %s", loadStatus_.c_str());
    ImGui::Text("Fallback: %s", useFallbackModel_ ? "true" : "false");
    ImGui::Checkbox("Active", &isActive_);
    ImGui::Text("Dead: %s", isDead_ ? "true" : "false");
    ImGui::DragFloat3("Position", &position_.x, 0.05f, -100.0f, 100.0f, "%.2f");
    ImGui::DragFloat3("Rotation", &rotation_.x, 0.01f, -6.28318f, 6.28318f, "%.3f");
    ImGui::SeparatorText("敵モデル向き補正 (Enemy Model Rotation Offset)");
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
    ImGui::DragFloat3("Enemy Model Rotation Offset", &modelRotationOffset_.x, 0.01f, -6.28318f, 6.28318f, "%.3f");
    ImGui::Text("Current Model Rotation: %.3f, %.3f, %.3f",
        visualModelRotation_.x,
        visualModelRotation_.y,
        visualModelRotation_.z);
    ImGui::DragFloat3("Scale", &scale_.x, 0.01f, 0.001f, 20.0f, "%.3f");
    ImGui::DragFloat3("Velocity", &velocity_.x, 0.01f, -100.0f, 100.0f, "%.2f");
    ImGui::DragInt("HP", &hp_, 1.0f, 0, 999);
    if (ImGui::Button("Reload Model")) {
        LoadModel();
    }
    ImGui::SameLine();
    if (ImGui::Button("Kill")) {
        Kill();
    }
    ImGui::SameLine();
    if (ImGui::Button("Revive")) {
        Revive(10);
    }
    UpdateObjectTransform();
    if (object_) {
        object_->Update();
    }
#endif
}

void Enemy::SetEnemyId(const std::string& enemyId) {
    enemyId_ = enemyId;
}

void Enemy::SetEnemyType(const std::string& enemyType) {
    enemyType_ = enemyType;
}

void Enemy::SetPosition(const Vector3& position) {
    position_ = position;
    UpdateObjectTransform();
}

void Enemy::SetRotation(const Vector3& rotation) {
    rotation_ = rotation;
    UpdateObjectTransform();
}

void Enemy::SetScale(const Vector3& scale) {
    scale_ = scale;
    UpdateObjectTransform();
}

void Enemy::SetVelocity(const Vector3& velocity) {
    velocity_ = velocity;
}

void Enemy::SetModelPath(const std::string& modelPath) {
    modelPath_ = modelPath;
    LoadModel();
}

void Enemy::Damage(int amount) {
    if (amount <= 0 || isDead_) {
        return;
    }

    hp_ = (std::max)(0, hp_ - amount);
    if (hp_ <= 0) {
        Kill();
    }
}

void Enemy::Kill() {
    isDead_ = true;
    isActive_ = false;
}

void Enemy::Revive(int hp) {
    hp_ = (std::max)(1, hp);
    isDead_ = false;
    isActive_ = true;
}

void Enemy::LoadModel() {
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
            texturePath_ = ToGenericString(resolvedPath.parent_path() / "Enemy.png");
            if (!std::filesystem::exists(std::filesystem::path(texturePath_))) {
                texturePath_ = ResolveResourcePath("resources/obj/axis/uvChecker.png");
            }
            if (!texturePath_.empty()) {
                TextureManager::GetInstance()->LoadTexture(texturePath_);
                model_->SetTextureIndex(TextureManager::GetInstance()->GetTextureIndexByFilePath(texturePath_));
            }
            loadStatus_ =
                !texturePath_.empty() && std::filesystem::path(texturePath_).filename().string() == "Enemy.png"
                ? "Enemy model loaded."
                : "Enemy model loaded. Texture missing, using fallback texture.";
        }
    }

    if (!model_) {
        model_ = modelManager->CreateBox("EnemyFallbackBox");
        useFallbackModel_ = true;
        loadStatus_ = "Enemy model missing. Using fallback box.";
    }

    if (object_) {
        object_->SetModel(model_);
    }
}

void Enemy::UpdateObjectTransform() {
    if (!object_) {
        return;
    }

    object_->SetTranslate(position_);
    visualModelRotation_ = AddVector3(rotation_, modelRotationOffset_);
    object_->SetRotate(visualModelRotation_);
    object_->SetScale(scale_);
}
