#include "InfluenceFieldManager.h"

#include "Engine/Game/Enemy/Enemy.h"
#include "Engine/Game/Enemy/EnemyManager.h"
#include "Engine/Game/Player/BoostController.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Graphics/Cloud/VolumetricCloudPass.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include "Engine/Graphics/Particle/GpuParticleSystem.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
constexpr float kMinDebugLineLength = 0.0001f;

struct DebugLineTransform {
    Vector3 translation{ 0.0f, 0.0f, 0.0f };
    Vector3 rotationRadians{ 0.0f, 0.0f, 0.0f };
    Vector3 scaling{ 1.0f, 1.0f, 1.0f };
};

float Lerp(float a, float b, float t) { return a + (b - a) * std::clamp(t, 0.0f, 1.0f); }
float ClampPositive(float value, float fallback) { return value > 0.0001f ? value : fallback; }

Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
    return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

Vector3 SubtractVector3(const Vector3& lhs, const Vector3& rhs) {
    return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
}

Vector3 ScaleVector3(const Vector3& value, float scale) {
    return { value.x * scale, value.y * scale, value.z * scale };
}

float Length(const Vector3& value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

Vector3 Normalize(const Vector3& value, const Vector3& fallback = { 0.0f, 0.0f, 1.0f }) {
    const float length = Length(value);
    if (length <= kMinDebugLineLength) {
        return fallback;
    }
    return { value.x / length, value.y / length, value.z / length };
}

Vector3 GetCameraForward(const Camera& camera) {
    const Matrix4x4& matrix = camera.GetWorldMatrix();
    return Normalize({ matrix.m[2][0], matrix.m[2][1], matrix.m[2][2] });
}

DebugLineTransform MakeLineTransform(const Vector3& from, const Vector3& to, float radius) {
    const Vector3 diff = SubtractVector3(to, from);
    const float length = Length(diff);
    const Vector3 direction = Normalize(diff);
    const float yaw = std::atan2(direction.x, direction.z);
    const float horizontal = std::sqrt(direction.x * direction.x + direction.z * direction.z);
    const float pitch = std::atan2(-direction.y, horizontal);
    const float thickness = (std::max)(radius, 0.05f);

    return {
        ScaleVector3(AddVector3(from, to), 0.5f),
        { pitch, yaw, 0.0f },
        { thickness, thickness, length * 0.5f },
    };
}

void ApplyModelMaterial(Model* model, const Vector4& color) {
    if (!model) {
        return;
    }
    if (Model::Material* material = model->GetMaterialData()) {
        material->color = color;
        material->enableLighting = 0;
        material->alphaReference = 0.0f;
    }
}
}

InfluenceFieldManager::InfluenceFieldManager() = default;
InfluenceFieldManager::~InfluenceFieldManager() = default;

void InfluenceFieldManager::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    object3dCommon_ = object3dCommon;
    camera_ = camera;
}

void InfluenceFieldManager::Finalize() {
    debugSphereObjects_.clear();
    debugTunnelObject_.reset();
    debugPlayerSphereModel_ = nullptr;
    debugEnemySphereModel_ = nullptr;
    debugTunnelModel_ = nullptr;
    object3dCommon_ = nullptr;
    camera_ = nullptr;
    player_ = nullptr;
    enemyManager_ = nullptr;
    boostController_ = nullptr;
    gpuParticleSystem_ = nullptr;
    cloudPass_ = nullptr;
    fields_.clear();
    fieldCount_ = 0;
}

void InfluenceFieldManager::SetTargets(Player* player, EnemyManager* enemyManager, BoostController* boostController) {
    player_ = player;
    enemyManager_ = enemyManager;
    boostController_ = boostController;
}

void InfluenceFieldManager::SetConsumers(GpuParticleSystem* gpuParticleSystem, VolumetricCloudPass* cloudPass) {
    gpuParticleSystem_ = gpuParticleSystem;
    cloudPass_ = cloudPass;
}

void InfluenceFieldManager::SetDebugVisualsEnabled(bool enabled) { debugVisualsEnabled_ = enabled; }

void InfluenceFieldManager::Update(float) {
    BuildFields();
    PackFields();
    PushToConsumers();
    UpdateDebugObjects();
}

void InfluenceFieldManager::BuildFields() {
    fields_.clear();
    if (!enabled_) { return; }

    const float boostPower = GetBoostPower();
    if (enablePlayerInfluence_ && player_) {
        InfluenceField field{};
        field.type = InfluenceField::Type::Player;
        field.center = player_->GetWorldPosition();
        field.radius = playerRadius_ * Lerp(1.0f, boostRadiusMultiplier_, boostPower);
        field.particleRepulsionStrength = playerParticleRepulsionStrength_ * Lerp(1.0f, boostStrengthMultiplier_, boostPower);
        field.cloudClearStrength = playerCloudClearStrength_ * Lerp(1.0f, boostStrengthMultiplier_, boostPower);
        field.strength = field.particleRepulsionStrength;
        field.falloffPower = playerFalloffPower_;
        field.affectCloud = enableCloudInfluence_;
        field.affectGpuParticle = enableParticleInfluence_;
        AddField(field);
    }

    if (!enableEnemyInfluence_ || !enemyManager_) { return; }
    const std::vector<Enemy*> enemies = enemyManager_->GetActiveEnemies();
    for (Enemy* enemy : enemies) {
        if (!enemy || enemy->IsDead()) { continue; }
        if (!includeSpawningEnemies_ && enemy->GetState() != Enemy::State::Active) { continue; }
        InfluenceField field{};
        field.type = InfluenceField::Type::Enemy;
        field.center = enemy->GetPosition();
        field.radius = enemyRadius_;
        field.particleRepulsionStrength = enemyParticleRepulsionStrength_;
        field.cloudClearStrength = enemyCloudClearStrength_;
        field.strength = field.particleRepulsionStrength;
        field.falloffPower = enemyFalloffPower_;
        field.affectCloud = enableCloudInfluence_;
        field.affectGpuParticle = enableParticleInfluence_;
        AddField(field);
    }
}

void InfluenceFieldManager::AddField(const InfluenceField& field) {
    if (!field.enabled || fields_.size() >= kMaxInfluenceFields || field.radius <= 0.0001f) { return; }
    fields_.push_back(field);
}

void InfluenceFieldManager::PackFields() {
    centersAndRadius_.fill({ 0.0f, 0.0f, 0.0f, 0.0f });
    params_.fill({ 0.0f, 0.0f, 1.0f, 0.0f });
    fieldCount_ = static_cast<uint32_t>((std::min)(fields_.size(), static_cast<size_t>(kMaxInfluenceFields)));
    for (uint32_t index = 0; index < fieldCount_; ++index) {
        const InfluenceField& field = fields_[index];
        centersAndRadius_[index] = { field.center.x, field.center.y, field.center.z, ClampPositive(field.radius, 1.0f) };
        params_[index] = {
            (std::max)(field.particleRepulsionStrength, 0.0f),
            std::clamp(field.cloudClearStrength, 0.0f, 1.0f),
            ClampPositive(field.falloffPower, 1.0f),
            static_cast<float>(BuildFlags(field))
        };
    }
}

void InfluenceFieldManager::PushToConsumers() {
    Vector3 railFlowDirection = Normalize(fixedRailFlowDirection_, {0.0f, 0.0f, -1.0f});
    if (useCameraForwardRailFlow_ && camera_) {
        railFlowDirection = Normalize(ScaleVector3(GetCameraForward(*camera_), -1.0f), {0.0f, 0.0f, -1.0f});
    }
    lastRailFlowDirection_ = railFlowDirection;

    if (gpuParticleSystem_) {
        gpuParticleSystem_->SetInfluenceFields(centersAndRadius_.data(), params_.data(), fieldCount_);
        gpuParticleSystem_->SetParticleInfluenceEnabled(enabled_ && enableParticleInfluence_);
        gpuParticleSystem_->SetParticleInfluenceResponseScale(particleInfluenceResponseScale_);
        gpuParticleSystem_->SetRailParticleFlow(enabled_ && enableRailParticleFlow_, camera_ ? camera_->GetTranslate() : Vector3{0.0f, 0.0f, 0.0f}, railFlowDirection, railFlowSpeed_, railFlowScale_, railSpawnAheadDistance_, railDespawnBehindDistance_);
    }
    if (cloudPass_) {
        cloudPass_->SetInfluenceFields(centersAndRadius_.data(), params_.data(), fieldCount_);
        cloudPass_->SetCloudInfluenceEnabled(enabled_ && enableCloudInfluence_);
        cloudPass_->SetCameraForwardTunnelSettings(enabled_ && enableCloudInfluence_ && useCameraForwardTunnel_, tunnelLength_, tunnelRadius_, tunnelClearStrength_);
    }
}

void InfluenceFieldManager::UpdateDebugObjects() {
    if (!debugVisualsEnabled_ || !showInfluenceDebug_) { return; }
    EnsureDebugObjects(fieldCount_);
    for (uint32_t index = 0; index < fieldCount_; ++index) {
        Object3d* object = debugSphereObjects_[index].get();
        if (!object) { continue; }
        const InfluenceField& field = fields_[index];
        object->SetCamera(camera_);
        object->SetModel(field.type == InfluenceField::Type::Player ? debugPlayerSphereModel_ : debugEnemySphereModel_);
        object->SetTranslate(field.center);
        object->SetScale({ field.radius, field.radius, field.radius });
        object->Update();
    }

    if (enabled_ && enableCloudInfluence_ && useCameraForwardTunnel_ && camera_) {
        EnsureTunnelDebugObject();
        if (debugTunnelObject_) {
            const Vector3 start = camera_->GetTranslate();
            const Vector3 forward = GetCameraForward(*camera_);
            const Vector3 end = AddVector3(start, ScaleVector3(forward, (std::max)(tunnelLength_, 0.0f)));
            const DebugLineTransform transform = MakeLineTransform(start, end, tunnelRadius_);
            debugTunnelObject_->SetCamera(camera_);
            debugTunnelObject_->SetTranslate(transform.translation);
            debugTunnelObject_->SetRotate(transform.rotationRadians);
            debugTunnelObject_->SetScale(transform.scaling);
            debugTunnelObject_->Update();
        }
    }
}

void InfluenceFieldManager::EnsureDebugObjects(size_t count) {
    if (!object3dCommon_ || !camera_) { return; }
    auto* modelManager = ModelManager::GetInstance();
    if (!debugPlayerSphereModel_) {
        debugPlayerSphereModel_ = modelManager->CreateSphere("InfluenceFieldDebugPlayerSphere", 24);
        ApplyModelMaterial(debugPlayerSphereModel_, { 0.2f, 0.75f, 1.0f, 0.28f });
    }
    if (!debugEnemySphereModel_) {
        debugEnemySphereModel_ = modelManager->CreateSphere("InfluenceFieldDebugEnemySphere", 24);
        ApplyModelMaterial(debugEnemySphereModel_, { 1.0f, 0.45f, 0.15f, 0.24f });
    }
    while (debugSphereObjects_.size() < count) {
        auto object = std::make_unique<Object3d>();
        object->Initialize(object3dCommon_);
        object->SetCamera(camera_);
        object->SetEnvironmentMapEnabled(false);
        object->SetModel(debugPlayerSphereModel_);
        object->Update();
        debugSphereObjects_.push_back(std::move(object));
    }
}

void InfluenceFieldManager::EnsureTunnelDebugObject() {
    if (!object3dCommon_ || !camera_) { return; }
    if (!debugTunnelModel_) {
        debugTunnelModel_ = ModelManager::GetInstance()->CreateBox("InfluenceFieldDebugCameraTunnel");
        ApplyModelMaterial(debugTunnelModel_, { 0.15f, 0.45f, 1.0f, 0.20f });
    }
    if (!debugTunnelObject_) {
        debugTunnelObject_ = std::make_unique<Object3d>();
        debugTunnelObject_->Initialize(object3dCommon_);
        debugTunnelObject_->SetCamera(camera_);
        debugTunnelObject_->SetEnvironmentMapEnabled(false);
        debugTunnelObject_->SetModel(debugTunnelModel_);
        debugTunnelObject_->Update();
    }
}

uint32_t InfluenceFieldManager::BuildFlags(const InfluenceField& field) const {
    uint32_t flags = field.type == InfluenceField::Type::Player ? kInfluenceFieldFlagPlayer : kInfluenceFieldFlagEnemy;
    if (field.affectCloud) { flags |= kInfluenceFieldFlagAffectCloud; }
    if (field.affectGpuParticle) { flags |= kInfluenceFieldFlagAffectGpuParticle; }
    return flags;
}

float InfluenceFieldManager::GetBoostPower() const {
    return boostController_ ? boostController_->GetCurrentBoostPower() : 0.0f;
}

void InfluenceFieldManager::DrawDebug() {
    if (!debugVisualsEnabled_ || !showInfluenceDebug_ || !object3dCommon_) { return; }
    object3dCommon_->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    const uint32_t drawCount = (std::min)(fieldCount_, static_cast<uint32_t>(debugSphereObjects_.size()));
    for (uint32_t index = 0; index < drawCount; ++index) {
        if (debugSphereObjects_[index]) { debugSphereObjects_[index]->Draw(); }
    }
    if (enabled_ && enableCloudInfluence_ && useCameraForwardTunnel_ && debugTunnelObject_) {
        debugTunnelObject_->Draw();
    }
}

void InfluenceFieldManager::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(430.0f, 520.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("風圧フィールド確認 (Influence Field Debug)")) { ImGui::End(); return; }
    ImGui::TextWrapped("PlayerやEnemyの近くにある雲とParticleを押しのけ、視界を確保します。Boost中はPlayer周辺の影響範囲と押し出しが強くなります。");
    ImGui::Checkbox("風圧フィールドを使う (Enable Influence Field)", &enabled_);
    ImGui::Checkbox("Player風圧を使う (Enable Player Influence)", &enablePlayerInfluence_);
    ImGui::Checkbox("Enemy風圧を使う (Enable Enemy Influence)", &enableEnemyInfluence_);
    ImGui::Checkbox("Particleへ風圧を反映 (Enable Particle Influence)", &enableParticleInfluence_);
    ImGui::DragFloat("Particle風圧反応倍率 (Particle Influence Response Scale)", &particleInfluenceResponseScale_, 0.01f, 0.0f, 10.0f, "%.2f");
    ImGui::SeparatorText("GPU Particleレール相対流れ (Rail Particle Flow)");
    ImGui::TextWrapped("Rail Shooter用に、ParticleへCamera前方の逆向きの一定移動を足します。velocityには蓄積せず、Influence Fieldの押し出しとは別に扱います。");
    ImGui::Checkbox("レールParticle流れを使う (Enable Rail Particle Flow)", &enableRailParticleFlow_);
    ImGui::Checkbox("Camera前方の逆へ流す (Use Camera Forward Direction)", &useCameraForwardRailFlow_);
    ImGui::DragFloat("レール流速 (Rail Flow Speed)", &railFlowSpeed_, 0.05f, 0.0f, 200.0f, "%.2f");
    ImGui::DragFloat("レール流れ全体倍率 (Rail Flow Scale)", &railFlowScale_, 0.01f, 0.0f, 10.0f, "%.2f");
    ImGui::DragFloat3("固定流れ方向 (Fixed Flow Direction)", &fixedRailFlowDirection_.x, 0.01f, -1.0f, 1.0f, "%.2f");
    ImGui::DragFloat("前方Spawn距離 (Spawn Ahead Distance)", &railSpawnAheadDistance_, 0.1f, 0.0f, 500.0f, "%.1f");
    ImGui::DragFloat("後方Despawn距離 (Despawn Behind Distance)", &railDespawnBehindDistance_, 0.1f, 0.0f, 500.0f, "%.1f");
    ImGui::Checkbox("レール流れデバッグ表示 (Show Rail Flow Debug)", &showRailFlowDebug_);
    if (showRailFlowDebug_) {
        ImGui::Text("現在の流れ方向 (Current Direction): %.2f, %.2f, %.2f", lastRailFlowDirection_.x, lastRailFlowDirection_.y, lastRailFlowDirection_.z);
        ImGui::Text("現在の流速 (Current railFlowSpeed): %.2f", railFlowSpeed_ * railFlowScale_);
        if (camera_) {
            const Vector3 cameraForward = GetCameraForward(*camera_);
            ImGui::Text("Camera Forward: %.2f, %.2f, %.2f", cameraForward.x, cameraForward.y, cameraForward.z);
        }
    }
    ImGui::Checkbox("雲に穴を作る (Enable Cloud Influence Clear)", &enableCloudInfluence_);
    ImGui::Checkbox("Spawn/Align中Enemyも対象 (Include Spawning Enemies)", &includeSpawningEnemies_);
    ImGui::Checkbox("風圧デバッグ表示 (Show Influence Debug)", &showInfluenceDebug_);
    ImGui::Text("現在のInfluence Field数 (Current Count): %u / %u", fieldCount_, kMaxInfluenceFields);
    ImGui::SeparatorText("Player風圧 (Player Influence)");
    ImGui::DragFloat("Player風圧半径 (Player Radius)", &playerRadius_, 0.05f, 0.1f, 50.0f, "%.2f");
    ImGui::DragFloat("Player Particle押し出し強さ (Player Particle Repulsion Strength)", &playerParticleRepulsionStrength_, 0.05f, 0.0f, 100.0f, "%.2f");
    ImGui::DragFloat("Player雲穴あけ強さ (Player Cloud Clear Strength)", &playerCloudClearStrength_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Player風圧減衰 (Player Influence Falloff)", &playerFalloffPower_, 0.05f, 0.1f, 8.0f, "%.2f");
    ImGui::DragFloat("Boost時半径倍率 (Boost Radius Multiplier)", &boostRadiusMultiplier_, 0.01f, 1.0f, 5.0f, "%.2f");
    ImGui::DragFloat("Boost時強さ倍率 (Boost Strength Multiplier)", &boostStrengthMultiplier_, 0.01f, 1.0f, 5.0f, "%.2f");
    ImGui::Text("現在のBoost強度 (Current Boost Power): %.2f", GetBoostPower());
    ImGui::SeparatorText("Enemy風圧 (Enemy Influence)");
    ImGui::DragFloat("Enemy風圧半径 (Enemy Radius)", &enemyRadius_, 0.05f, 0.1f, 50.0f, "%.2f");
    ImGui::DragFloat("Enemy Particle押し出し強さ (Enemy Particle Repulsion Strength)", &enemyParticleRepulsionStrength_, 0.05f, 0.0f, 100.0f, "%.2f");
    ImGui::DragFloat("Enemy雲穴あけ強さ (Enemy Cloud Clear Strength)", &enemyCloudClearStrength_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Enemy風圧減衰 (Enemy Influence Falloff)", &enemyFalloffPower_, 0.05f, 0.1f, 8.0f, "%.2f");
    ImGui::SeparatorText("カメラ前方視界トンネル (Camera Forward Visibility Tunnel)");
    ImGui::Checkbox("カメラ前方トンネルを使う (Camera Forward Tunnel)", &useCameraForwardTunnel_);
    ImGui::DragFloat("トンネル長さ (Tunnel Length)", &tunnelLength_, 0.1f, 0.1f, 200.0f, "%.1f");
    ImGui::DragFloat("トンネル半径 (Tunnel Radius)", &tunnelRadius_, 0.05f, 0.1f, 50.0f, "%.2f");
    ImGui::DragFloat("トンネル雲穴あけ強さ (Tunnel Clear Strength)", &tunnelClearStrength_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::End();
#endif
}
