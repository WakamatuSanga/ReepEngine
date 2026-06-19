#include "Enemy.h"
#include "Engine/Graphics/Camera/Camera.h"
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

    Vector3 SubtractVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    }

    float Length(const Vector3& value) {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    }

    Vector3 Normalize(const Vector3& value, const Vector3& fallback) {
        const float length = Length(value);
        if (length <= 0.00001f || !std::isfinite(length)) {
            return fallback;
        }
        return { value.x / length, value.y / length, value.z / length };
    }

    Vector3 MakeYawRotationFromForward(const Vector3& forward) {
        const Vector3 normalized = Normalize({ forward.x, 0.0f, forward.z }, { 0.0f, 0.0f, -1.0f });
        return { 0.0f, std::atan2(normalized.x, normalized.z), 0.0f };
    }

    Vector3 GetCameraForward(const Camera& camera) {
        const Matrix4x4& matrix = camera.GetWorldMatrix();
        return Normalize({ matrix.m[2][0], matrix.m[2][1], matrix.m[2][2] }, { 0.0f, 0.0f, 1.0f });
    }

    Vector3 LerpVector3(const Vector3& start, const Vector3& end, float t) {
        return { start.x + (end.x - start.x) * t, start.y + (end.y - start.y) * t, start.z + (end.z - start.z) * t };
    }

    float EaseOutCubic(float t) {
        const float clamped = std::clamp(t, 0.0f, 1.0f);
        const float inv = 1.0f - clamped;
        return 1.0f - inv * inv * inv;
    }

    float SmoothStep(float t) {
        const float clamped = std::clamp(t, 0.0f, 1.0f);
        return clamped * clamped * (3.0f - 2.0f * clamped);
    }

    float Remap01(float value, float start, float end) {
        if (end <= start + 0.0001f) {
            return value >= end ? 1.0f : 0.0f;
        }
        return std::clamp((value - start) / (end - start), 0.0f, 1.0f);
    }

    float NormalizeAngle(float angle) {
        while (angle > kPi) {
            angle -= kPi * 2.0f;
        }
        while (angle < -kPi) {
            angle += kPi * 2.0f;
        }
        return angle;
    }

    float LerpAngle(float start, float end, float t) {
        return start + NormalizeAngle(end - start) * t;
    }

    Vector3 LerpEulerShortest(const Vector3& start, const Vector3& end, float t) {
        return { LerpAngle(start.x, end.x, t), LerpAngle(start.y, end.y, t), LerpAngle(start.z, end.z, t) };
    }

    const char* ToStateLabel(Enemy::State state) {
        switch (state) {
        case Enemy::State::Spawning:
            return "Spawning";
        case Enemy::State::AligningToPlayer:
            return "AligningToPlayer";
        case Enemy::State::Active:
            return "Active";
        case Enemy::State::Dead:
            return "Dead";
        default:
            return "Unknown";
        }
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
    state_ = State::Active;
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
    if (state_ == State::Spawning) {
        UpdateSpawnAnimation(safeDeltaTime);
    } else if (state_ == State::AligningToPlayer) {
        UpdateAlignToPlayer(safeDeltaTime);
    } else {
        position_ = AddVector3(position_, ScaleVector3(velocity_, safeDeltaTime));
    }
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
    ImGui::Text("State: %s", ToStateLabel(state_));
    ImGui::Text("Can Attack: %s", CanAttack() ? "true" : "false");
    ImGui::Text("Can Receive Player Bullet: %s", CanReceivePlayerBullet() ? "true" : "false");
    ImGui::Text("Aligning To Player: %s", state_ == State::AligningToPlayer ? "true" : "false");
    ImGui::TextWrapped("Model Path: %s", modelPath_.c_str());
    ImGui::TextWrapped("Resolved Model Path: %s", resolvedModelPath_.empty() ? "(none)" : resolvedModelPath_.c_str());
    ImGui::TextWrapped("Texture Path: %s", texturePath_.empty() ? "(none)" : texturePath_.c_str());
    ImGui::TextWrapped("Load Status: %s", loadStatus_.c_str());
    ImGui::Text("Fallback: %s", useFallbackModel_ ? "true" : "false");
    ImGui::Text("Model Stats: vertices=%zu indices=%zu materials=%zu",
        model_ ? model_->GetVertexCount() : 0,
        model_ ? model_->GetIndexCount() : 0,
        model_ ? model_->GetMaterialCount() : 0);
    if (model_) {
        model_->DrawPbrMaterialImGui();
    }
    ImGui::Checkbox("Active", &isActive_);
    if (ImGui::Checkbox("Use Lightweight Enemy Visual", &useLightweightVisual_)) {
        LoadModel();
    }
    ImGui::Text("Dead: %s", isDead_ ? "true" : "false");
    ImGui::DragFloat3("Position", &position_.x, 0.05f, -100.0f, 100.0f, "%.2f");
    ImGui::DragFloat3("Rotation", &rotation_.x, 0.01f, -6.28318f, 6.28318f, "%.3f");
    ImGui::SeparatorText("敵出現状態 (Enemy Spawn State)");
    ImGui::Text("Current Spawn T: %.3f / Align T: %.3f", currentSpawnT_, currentAlignT_);
    ImGui::Text("Spawn Facing Blend / Current Spawn Facing Weight: %.3f", currentSpawnFacingWeight_);
    ImGui::Text("Current Spin Weight: %.3f", currentSpinWeight_);
    ImGui::Text("Spawn Start Position: %.2f, %.2f, %.2f", spawnStartPosition_.x, spawnStartPosition_.y, spawnStartPosition_.z);
    ImGui::Text("Spawn Target Position: %.2f, %.2f, %.2f", spawnTargetPosition_.x, spawnTargetPosition_.y, spawnTargetPosition_.z);
    ImGui::Text("Spawn Look Target: %.2f, %.2f, %.2f", spawnLookTarget_.x, spawnLookTarget_.y, spawnLookTarget_.z);
    ImGui::Text("Spawn Spin Axis: %s",
        spawnSpinAxisMode_ == SpawnSpinAxisMode::AroundForward ? "AroundForward" : "AroundWorldY");
    ImGui::Text("Align Smooth Type: %s",
        alignSmoothType_ == AlignSmoothType::Linear ? "Linear" :
        alignSmoothType_ == AlignSmoothType::EaseOut ? "EaseOut" : "SmoothStep");
    ImGui::Text("Spawn Visual Rotation: %.3f, %.3f, %.3f", spawnVisualRotation_.x, spawnVisualRotation_.y, spawnVisualRotation_.z);
    ImGui::Text("Align Start Rotation: %.3f, %.3f, %.3f", alignStartRotation_.x, alignStartRotation_.y, alignStartRotation_.z);
    ImGui::Text("Align Target Rotation: %.3f, %.3f, %.3f", alignTargetRotation_.x, alignTargetRotation_.y, alignTargetRotation_.z);
    ImGui::Text("Active Final Rotation: %.3f, %.3f, %.3f", activeFinalRotation_.x, activeFinalRotation_.y, activeFinalRotation_.z);
    ImGui::Text("Current Visual Rotation: %.3f, %.3f, %.3f", visualModelRotation_.x, visualModelRotation_.y, visualModelRotation_.z);
    ImGui::SeparatorText("敵モデル向き補正 (Enemy Model Rotation Offset)");
    ImGui::Text("Enemy Forward: %.3f, %.3f, %.3f", desiredForward_.x, desiredForward_.y, desiredForward_.z);
    ImGui::Text("Final Spawn Rotation: %.3f, %.3f, %.3f", finalSpawnRotation_.x, finalSpawnRotation_.y, finalSpawnRotation_.z);
    if (ImGui::Button("Face Camera Opposite")) {
        if (camera_) {
            const Vector3 cameraForward = GetCameraForward(*camera_);
            SetForward(ScaleVector3(cameraForward, -1.0f));
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Look At Camera")) {
        if (camera_) {
            LookAt(camera_->GetTranslate());
        }
    }
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
    ImGui::Text("Current Model Rotation: %.3f, %.3f, %.3f", visualModelRotation_.x, visualModelRotation_.y, visualModelRotation_.z);
    ImGui::DragFloat3("Scale", &scale_.x, 0.01f, 0.001f, 20.0f, "%.3f");
    ImGui::DragFloat3("Velocity", &velocity_.x, 0.01f, -100.0f, 100.0f, "%.2f");
    ImGui::DragFloat("Enemy Hit Radius", &hitRadius_, 0.01f, 0.001f, 20.0f, "%.3f");
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
    finalSpawnRotation_ = rotation_;
    UpdateObjectTransform();
}

void Enemy::SetScale(const Vector3& scale) {
    scale_ = scale;
    UpdateObjectTransform();
}

void Enemy::SetVelocity(const Vector3& velocity) {
    velocity_ = velocity;
}

void Enemy::SetForward(const Vector3& forward) {
    desiredForward_ = Normalize(forward, desiredForward_);
    rotation_ = MakeYawRotationFromForward(desiredForward_);
    finalSpawnRotation_ = rotation_;
    UpdateObjectTransform();
}

void Enemy::LookAt(const Vector3& targetPosition) {
    SetForward(SubtractVector3(targetPosition, position_));
}

void Enemy::SetHitRadius(float hitRadius) {
    hitRadius_ = (std::max)(0.001f, hitRadius);
}

void Enemy::SetUseLightweightVisual(bool useLightweightVisual) {
    if (useLightweightVisual_ == useLightweightVisual) {
        return;
    }
    useLightweightVisual_ = useLightweightVisual;
    LoadModel();
}

void Enemy::SetModelPath(const std::string& modelPath) {
    modelPath_ = modelPath;
    LoadModel();
}

void Enemy::SetSpawnPresentationOptions(
    bool faceDownDuringSpawn, bool facePlayerOnComplete, bool resetRollOnActive, bool resetPitchOnActive,
    bool enableCollisionDuringSpawn, SpawnSpinAxisMode spinAxisMode, bool facePlayerDuringSpawn,
    float facePlayerStartT, float facePlayerEndT, float spinFadeStartT, float spinFadeEndT,
    bool alignAfterSpawn, float alignDuration, AlignSmoothType alignSmoothType, const Vector3& lookTarget) {
    spawnFaceDownDuringSpawn_ = faceDownDuringSpawn;
    spawnFacePlayerOnComplete_ = facePlayerOnComplete;
    spawnResetRollOnActive_ = resetRollOnActive;
    spawnResetPitchOnActive_ = resetPitchOnActive;
    enableCollisionDuringSpawn_ = enableCollisionDuringSpawn;
    spawnSpinAxisMode_ = spinAxisMode;
    facePlayerDuringSpawn_ = facePlayerDuringSpawn;
    spawnFacePlayerStartT_ = std::clamp(facePlayerStartT, 0.0f, 1.0f);
    spawnFacePlayerEndT_ = std::clamp((std::max)(facePlayerEndT, spawnFacePlayerStartT_ + 0.01f), 0.0f, 1.0f);
    spawnSpinFadeStartT_ = std::clamp(spinFadeStartT, 0.0f, 1.0f);
    spawnSpinFadeEndT_ = std::clamp((std::max)(spinFadeEndT, spawnSpinFadeStartT_ + 0.01f), 0.0f, 1.0f);
    alignAfterSpawn_ = alignAfterSpawn;
    alignDuration_ = (std::max)(0.01f, alignDuration);
    alignSmoothType_ = alignSmoothType;
    spawnLookTarget_ = lookTarget;
}

void Enemy::StartSpawnAnimation(
    const Vector3& targetPosition,
    float spawnHeight,
    float spawnDuration,
    float spawnSpinSpeedDegrees,
    float spawnAttackDelay) {
    StartSpawnAnimationFrom(
        AddVector3(targetPosition, { 0.0f, (std::max)(0.0f, spawnHeight), 0.0f }),
        targetPosition,
        spawnDuration,
        spawnSpinSpeedDegrees,
        spawnAttackDelay);
}

void Enemy::StartSpawnAnimationFrom(
    const Vector3& startPosition,
    const Vector3& targetPosition,
    float spawnDuration,
    float spawnSpinSpeedDegrees,
    float spawnAttackDelay) {
    spawnTargetPosition_ = targetPosition;
    spawnStartPosition_ = startPosition;
    spawnDuration_ = (std::max)(0.01f, spawnDuration);
    spawnAttackDelay_ = (std::max)(0.0f, spawnAttackDelay);
    spawnSpinSpeedRadians_ = spawnSpinSpeedDegrees * kPi / 180.0f;
    spawnElapsed_ = 0.0f;
    currentSpawnT_ = 0.0f;
    currentSpawnFacingWeight_ = 0.0f;
    currentSpinWeight_ = 1.0f;
    spawnSpinAngle_ = 0.0f;
    alignElapsed_ = 0.0f;
    currentAlignT_ = 0.0f;
    finalSpawnRotation_ = rotation_;
    activeFinalRotation_ = rotation_;
    alignStartRotation_ = rotation_;
    alignTargetRotation_ = AddVector3(rotation_, modelRotationOffset_);
    position_ = spawnStartPosition_;
    state_ = State::Spawning;
    isActive_ = true;
    isDead_ = false;
    UpdateObjectTransform();
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
    state_ = State::Dead;
}

void Enemy::Revive(int hp) {
    hp_ = (std::max)(1, hp);
    isDead_ = false;
    isActive_ = true;
    state_ = State::Active;
}

void Enemy::LoadModel() {
    useFallbackModel_ = false;
    resolvedModelPath_ = ResolveResourcePath(modelPath_);
    texturePath_.clear();
    model_ = nullptr;

    ModelManager* modelManager = ModelManager::GetInstance();
    if (useLightweightVisual_) {
        model_ = modelManager->CreateBox("EnemyLightweightBox");
        useFallbackModel_ = true;
        loadStatus_ = "Using lightweight enemy primitive.";
    } else if (!resolvedModelPath_.empty()) {
        modelManager->LoadModel(resolvedModelPath_);
        model_ = modelManager->FindModel(resolvedModelPath_);
        if (model_) {
            const std::filesystem::path resolvedPath(resolvedModelPath_);
            texturePath_ = ToGenericString(resolvedPath.parent_path() / "textures" / "Material.003_baseColor.png");
            if (!std::filesystem::exists(std::filesystem::path(texturePath_))) {
                texturePath_ = ResolveResourcePath("resources/obj/axis/uvChecker.png");
            }
            if (!texturePath_.empty()) {
                TextureManager::GetInstance()->LoadTexture(texturePath_);
                model_->SetTextureIndex(TextureManager::GetInstance()->GetTextureIndexByFilePath(texturePath_));
            }
            loadStatus_ =
                !texturePath_.empty() && std::filesystem::path(texturePath_).filename().string() == "Material.003_baseColor.png"
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
    if ((state_ == State::Spawning || state_ == State::AligningToPlayer) && spawnFaceDownDuringSpawn_) {
        visualModelRotation_ = rotation_;
    } else {
        visualModelRotation_ = AddVector3(rotation_, modelRotationOffset_);
    }
    object_->SetRotate(visualModelRotation_);
    object_->SetScale(scale_);
}

void Enemy::UpdateSpawnAnimation(float deltaTime) {
    spawnElapsed_ += deltaTime;
    currentSpawnT_ = std::clamp(spawnElapsed_ / spawnDuration_, 0.0f, 1.0f);
    const float moveT = EaseOutCubic(currentSpawnT_);
    position_ = LerpVector3(spawnStartPosition_, spawnTargetPosition_, moveT);
    const float arc = std::sin(SmoothStep(currentSpawnT_) * kPi) * spawnGlideArcHeight_;
    position_.y += arc;
    currentSpawnFacingWeight_ = facePlayerDuringSpawn_ ? SmoothStep(Remap01(currentSpawnT_, spawnFacePlayerStartT_, spawnFacePlayerEndT_)) : 0.0f;
    currentSpinWeight_ = 1.0f - SmoothStep(Remap01(currentSpawnT_, spawnSpinFadeStartT_, spawnSpinFadeEndT_));
    spawnSpinAngle_ += spawnSpinSpeedRadians_ * currentSpinWeight_ * deltaTime;

    if (currentSpawnT_ < 1.0f) {
        if (spawnFaceDownDuringSpawn_) {
            const Vector3 downRotation = MakeSpawnFacingDownVisualRotation(spawnSpinAngle_);
            const Vector3 targetRotation = AddVector3(ComputePlayerFacingRotation(), modelRotationOffset_);
            rotation_ = LerpEulerShortest(downRotation, targetRotation, currentSpawnFacingWeight_);
            spawnVisualRotation_ = rotation_;
        }
        return;
    }

    position_ = spawnTargetPosition_;
    BeginAlignToPlayer();
}

void Enemy::BeginAlignToPlayer() {
    position_ = spawnTargetPosition_;
    activeFinalRotation_ = ComputePlayerFacingRotation();
    finalSpawnRotation_ = activeFinalRotation_;
    alignStartRotation_ = rotation_;
    alignTargetRotation_ = spawnFaceDownDuringSpawn_ ? AddVector3(activeFinalRotation_, modelRotationOffset_) : activeFinalRotation_;
    alignElapsed_ = 0.0f;
    currentAlignT_ = 0.0f;

    if (!alignAfterSpawn_ || alignDuration_ <= 0.0001f) {
        ApplySpawnCompleteFacing();
        state_ = State::Active;
        return;
    }

    state_ = State::AligningToPlayer;
}

void Enemy::UpdateAlignToPlayer(float deltaTime) {
    alignElapsed_ += deltaTime;
    const float rawT = std::clamp(alignElapsed_ / alignDuration_, 0.0f, 1.0f);
    currentAlignT_ = rawT;
    const float easedT = ApplyAlignCurve(rawT);
    rotation_ = LerpEulerShortest(alignStartRotation_, alignTargetRotation_, easedT);
    position_ = spawnTargetPosition_;

    if (alignElapsed_ >= alignDuration_ + spawnAttackDelay_) {
        ApplySpawnCompleteFacing();
        state_ = State::Active;
    }
}

void Enemy::ApplySpawnCompleteFacing() {
    activeFinalRotation_ = ComputePlayerFacingRotation();
    rotation_ = activeFinalRotation_;
    desiredForward_ = Normalize({ std::sin(rotation_.y), 0.0f, std::cos(rotation_.y) }, desiredForward_);
    if (spawnResetPitchOnActive_) {
        rotation_.x = 0.0f;
    }
    if (spawnResetRollOnActive_) {
        rotation_.z = 0.0f;
    }
    finalSpawnRotation_ = rotation_;
    activeFinalRotation_ = rotation_;
    alignTargetRotation_ = spawnFaceDownDuringSpawn_ ? AddVector3(activeFinalRotation_, modelRotationOffset_) : activeFinalRotation_;
}

Vector3 Enemy::MakeSpawnFacingDownVisualRotation(float spinAngle) const {
    if (spawnSpinAxisMode_ == SpawnSpinAxisMode::AroundWorldY) {
        return { kPi * 0.5f, spinAngle, 0.0f };
    }

    // Enemy.obj is corrected for gameplay with a yaw offset. During entry we
    // use a direct visual rotation so the corrected model points downward and
    // spins around its nose instead of orbiting sideways.
    return { spinAngle, 0.0f, -kPi * 0.5f };
}

Vector3 Enemy::ComputePlayerFacingRotation() const {
    Vector3 forward = desiredForward_;
    if (spawnFacePlayerOnComplete_) {
        forward = SubtractVector3(spawnLookTarget_, position_);
    }
    forward.y = 0.0f;
    return MakeYawRotationFromForward(forward);
}

float Enemy::ApplyAlignCurve(float t) const {
    switch (alignSmoothType_) {
    case AlignSmoothType::Linear:
        return std::clamp(t, 0.0f, 1.0f);
    case AlignSmoothType::EaseOut:
        return EaseOutCubic(t);
    case AlignSmoothType::SmoothStep:
    default:
        return SmoothStep(t);
    }
}
