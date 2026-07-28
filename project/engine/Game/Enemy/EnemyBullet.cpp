#include "EnemyBullet.h"
#include "Engine/Graphics/Model/Model.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Utility/Logger.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>

#ifdef USE_IMGUI
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
        if (length <= kMinVectorLength || !std::isfinite(length)) {
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

bool EnemyBullet::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    initialized_ = false;
    object3dCommon_ = object3dCommon;
    camera_ = camera;
    currentTime_ = 0.0f;
    isActive_ = false;
    isDead_ = true;
    deathReason_ = "初期化中";
    projectileRailFrameSequence_ = 0;
    projectileSpawnSequence_ = 0;

    if (!object3dCommon_ || !camera_) {
        loadStatus_ = "Initialize failed: Object3dCommon or Camera is null.";
        Logger::Log("[EnemyBullet] Initialize failed: Object3dCommon or Camera is null");
        return false;
    }

    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon_);
    if (!object_->IsValid()) {
        loadStatus_ = "Initialize failed: Object3d resource creation failed.";
        Logger::Log("[EnemyBullet] Initialize failed: Object3d is invalid");
        object_.reset();
        return false;
    }
    object_->SetCamera(camera_);
    object_->SetEnvironmentMapEnabled(false);

    radiusObject_ = std::make_unique<Object3d>();
    radiusObject_->Initialize(object3dCommon_);
    if (radiusObject_->IsValid()) {
        radiusObject_->SetCamera(camera_);
        radiusObject_->SetEnvironmentMapEnabled(false);
        radiusModel_ = ModelManager::GetInstance()->CreateSphere("EnemyBulletRadiusSphere", 12);
        radiusObject_->SetModel(radiusModel_);
    } else {
        Logger::Log("[EnemyBullet] Radius Object3d initialize failed; radius debug disabled");
        radiusObject_.reset();
        radiusModel_ = nullptr;
    }

    LoadModel();
    initialized_ = true;
    isActive_ = true;
    isDead_ = false;
    deathReason_ = "生存中";
    UpdateObjectTransform();
    object_->Update();
    if (radiusObject_ && radiusObject_->IsValid()) {
        radiusObject_->Update();
    }
    return true;
}
void EnemyBullet::Finalize() {
    initialized_ = false;
    isActive_ = false;
    isDead_ = true;
    radiusObject_.reset();
    radiusModel_ = nullptr;
    object_.reset();
    model_ = nullptr;
    deathReason_ = "終了済み";
    projectileRailFrameSequence_ = 0;
    projectileSpawnSequence_ = 0;
}

void EnemyBullet::Update(float deltaTime) {
    if (!initialized_ || !object_ || !object_->IsValid() || !isActive_ || isDead_) {
        return;
    }

    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    currentTime_ += safeDeltaTime;
    if (currentTime_ >= lifeTime_) {
        Kill("生存時間終了");
        return;
    }

    position_ = AddVector3(position_, ScaleVector3(velocity_, safeDeltaTime));
    UpdateObjectTransform();
    object_->Update();
    if (radiusObject_ && radiusObject_->IsValid()) {
        radiusObject_->Update();
    }
}

void EnemyBullet::Draw() {
    if (!initialized_ || !object3dCommon_ || !object_ || !object_->IsValid() || !model_ || !isActive_ || isDead_) {
        return;
    }

    object3dCommon_->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    object_->Draw();
}

void EnemyBullet::DrawRadius() {
    if (!initialized_ || !object3dCommon_ || !radiusObject_ || !radiusObject_->IsValid() || !radiusModel_ || !isActive_ || isDead_) {
        return;
    }

    object3dCommon_->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    radiusObject_->Draw();
}

void EnemyBullet::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::Text("Active: %s / Dead: %s", isActive_ ? "true" : "false", isDead_ ? "true" : "false");
    ImGui::TextWrapped("Model Path: %s", modelPath_.c_str());
    ImGui::TextWrapped("Resolved Model Path: %s", resolvedModelPath_.empty() ? "(none)" : resolvedModelPath_.c_str());
    ImGui::TextWrapped("Texture Path: %s", texturePath_.empty() ? "(none)" : texturePath_.c_str());
    ImGui::TextWrapped("Load Status: %s", loadStatus_.c_str());
    ImGui::Text("Fallback: %s", useFallbackModel_ ? "true" : "false");
    ImGui::Text("Model Stats: vertices=%zu indices=%zu materials=%zu",
        model_ ? model_->GetVertexCount() : 0,
        model_ ? model_->GetIndexCount() : 0,
        model_ ? model_->GetMaterialCount() : 0);
    if (ImGui::Checkbox("Use Lightweight Bullet Visual", &useLightweightVisual_)) {
        LoadModel();
    }
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
    if (ImGui::Button("Pitch +90")) {
        modelRotationOffset_.x += kPi * 0.5f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Pitch -90")) {
        modelRotationOffset_.x -= kPi * 0.5f;
    }
    if (ImGui::Button("向き補正リセット")) {
        modelRotationOffset_ = { 0.0f, 0.0f, 0.0f };
    }
    ImGui::DragFloat3("Enemy Bullet Model Rotation Offset", &modelRotationOffset_.x, 0.01f, -6.28318f, 6.28318f, "%.3f");
    ImGui::Text("Visual Forward Source: %s", hasVisualForwardOverride_ ? "Override" : "Velocity");
    ImGui::Text("Enemy Bullet Forward Axis: +Z + offset");
    ImGui::Text("Velocity/Visual Forward: %.3f, %.3f, %.3f", lastVisualForward_.x, lastVisualForward_.y, lastVisualForward_.z);
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
    if (object_ && object_->IsValid()) {
        object_->Update();
    }
    if (radiusObject_ && radiusObject_->IsValid()) {
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

void EnemyBullet::SetModelRotationOffset(const Vector3& rotationOffset) {
    modelRotationOffset_ = rotationOffset;
    UpdateObjectTransform();
}

void EnemyBullet::SetVisualForwardOverride(const Vector3& forward) {
    visualForwardOverride_ = Normalize(forward, lastVisualForward_);
    hasVisualForwardOverride_ = true;
    UpdateObjectTransform();
}

void EnemyBullet::ClearVisualForwardOverride() {
    hasVisualForwardOverride_ = false;
    UpdateObjectTransform();
}

void EnemyBullet::SetScale(const Vector3& scale) {
    scale_ = scale;
    UpdateObjectTransform();
}

void EnemyBullet::SetRadius(float radius) {
    radius_ = (std::max)(0.001f, radius);
}

void EnemyBullet::SetLifeTime(float lifeTime) {
    lifeTime_ = (std::max)(0.1f, lifeTime);
}

void EnemyBullet::SetUseLightweightVisual(bool useLightweightVisual) {
    if (useLightweightVisual_ == useLightweightVisual) {
        return;
    }
    useLightweightVisual_ = useLightweightVisual;
    LoadModel();
    UpdateObjectTransform();
}

void EnemyBullet::SetModelPath(const std::string& modelPath) {
    modelPath_ = modelPath;
    LoadModel();
    UpdateObjectTransform();
}

void EnemyBullet::Kill(const std::string& reason) {
    isDead_ = true;
    isActive_ = false;
    deathReason_ = reason.empty() ? "外部処理" : reason;
}

void EnemyBullet::LoadModel() {
    useFallbackModel_ = false;
    resolvedModelPath_ = ResolveResourcePath(modelPath_);
    texturePath_.clear();
    model_ = nullptr;

    ModelManager* modelManager = ModelManager::GetInstance();
    if (useLightweightVisual_) {
        model_ = modelManager->CreateSphere("EnemyBulletLightweightSphere", 8);
        useFallbackModel_ = true;
        loadStatus_ = "Using lightweight bullet primitive.";
    } else if (!resolvedModelPath_.empty()) {
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
    if (!initialized_ || !object_ || !object_->IsValid()) {
        return;
    }

    object_->SetTranslate(position_);
    const Vector3 visualForwardSource = hasVisualForwardOverride_ ? visualForwardOverride_ : velocity_;
    if (Length(visualForwardSource) > kMinVectorLength) {
        lastVisualForward_ = Normalize(visualForwardSource, lastVisualForward_);
    }
    visualBaseRotation_ = MakeRotationFromForward(lastVisualForward_);
    visualModelRotation_ = AddVector3(
        AddVector3(visualBaseRotation_, rotation_),
        modelRotationOffset_);
    object_->SetRotate(visualModelRotation_);
    object_->SetScale(scale_);
    if (radiusObject_ && radiusObject_->IsValid()) {
        radiusObject_->SetTranslate(position_);
        radiusObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
        radiusObject_->SetScale({ radius_, radius_, radius_ });
    }
}

