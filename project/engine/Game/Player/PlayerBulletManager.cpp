#include "PlayerBulletManager.h"
#include "MyGame.h"
#include "Engine/Game/Enemy/EnemyBullet.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include "Engine/Input/Input.h"
#include <algorithm>
#include <cmath>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kMinVectorLength = 0.00001f;

    Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 ScaleVector3(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    float DistanceSquared(const Vector3& lhs, const Vector3& rhs) {
        const float x = lhs.x - rhs.x;
        const float y = lhs.y - rhs.y;
        const float z = lhs.z - rhs.z;
        return x * x + y * y + z * z;
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

    Vector3 GetCameraForward(const Camera& camera) {
        const Matrix4x4& matrix = camera.GetWorldMatrix();
        return Normalize({ matrix.m[2][0], matrix.m[2][1], matrix.m[2][2] }, { 0.0f, 0.0f, 1.0f });
    }

    bool IsEditingImGuiInput() {
#ifdef _DEBUG
        const ImGuiIO& io = ImGui::GetIO();
        return io.WantTextInput || ImGui::IsAnyItemActive();
#else
        return false;
#endif
    }
}

PlayerBulletManager::PlayerBulletManager() = default;

PlayerBulletManager::~PlayerBulletManager() = default;

void PlayerBulletManager::Initialize(Object3dCommon* object3dCommon, Camera* camera, Player* player) {
    object3dCommon_ = object3dCommon;
    camera_ = camera;
    player_ = player;
    fireTimer_ = fireInterval_;
    inputBlockedReason_ = "Initialized";
    SyncModelPathBuffer();
}

void PlayerBulletManager::Finalize() {
    DeleteAllBullets();
    object3dCommon_ = nullptr;
    camera_ = nullptr;
    player_ = nullptr;
}

void PlayerBulletManager::Update(float deltaTime) {
    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    fireTimer_ = (std::min)(fireInterval_, fireTimer_ + safeDeltaTime);

    inputBlockedReason_ = "None";
    Input* input = MyGame::GetInstance()->GetInput();
    if (!input) {
        inputBlockedReason_ = "Input is missing";
    } else if (ShouldBlockFireInput()) {
        // ShouldBlockFireInput writes inputBlockedReason_.
    } else if (input->MouseTrigger(Input::MouseLeft) && fireTimer_ >= fireInterval_) {
        FireFromPlayer();
        fireTimer_ = 0.0f;
    }

    for (PlayerBulletInstance& instance : bullets_) {
        if (instance.bullet) {
            instance.bullet->Update(safeDeltaTime);
        }
    }

    if (autoRemoveDeadBullets_) {
        RemoveDeadBullets();
    }
}

void PlayerBulletManager::Draw() {
    for (PlayerBulletInstance& instance : bullets_) {
        if (instance.bullet) {
            instance.bullet->Draw();
        }
    }
    if (showBulletCollisionRadius_) {
        for (PlayerBulletInstance& instance : bullets_) {
            if (instance.bullet) {
                instance.bullet->DrawRadius();
            }
        }
    }
}

void PlayerBulletManager::DrawImGui() {
#ifdef _DEBUG
    ImGui::SetNextWindowSize(ImVec2(420.0f, 430.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("プレイヤーショット確認 (Player Shot Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Enable Player Shot", &enablePlayerShot_);
    ImGui::Text("Bullet Count: %zu", GetBulletCount());
    ImGui::Text("Active Count: %zu", GetActiveCount());
    ImGui::Text("Fired Bullet Count: %zu", firedBulletCount_);
    ImGui::TextWrapped("Input Blocked Reason: %s", inputBlockedReason_.c_str());
    ImGui::Text("Game View Input Active: %s", gameViewInputActive_ ? "true" : "false");
    ImGui::DragFloat("Bullet Speed", &bulletSpeed_, 0.1f, 0.0f, 200.0f, "%.2f");
    ImGui::DragFloat("Bullet Radius", &bulletRadius_, 0.01f, 0.001f, 20.0f, "%.3f");
    ImGui::DragInt("Bullet Damage", &bulletDamage_, 1.0f, 1, 999);
    ImGui::DragFloat("Fire Interval", &fireInterval_, 0.01f, 0.01f, 5.0f, "%.2f");
    ImGui::DragFloat("Bullet Life Time", &bulletLifeTime_, 0.05f, 0.1f, 30.0f, "%.2f");
    ImGui::DragFloat("Muzzle Offset", &muzzleOffset_, 0.01f, 0.0f, 10.0f, "%.2f");
    ImGui::Checkbox("Show Bullet Collision Radius", &showBulletCollisionRadius_);
    ImGui::Checkbox("Auto Remove Dead Bullets", &autoRemoveDeadBullets_);
    if (ImGui::InputText("Bullet Model Path", modelPathBuffer_.data(), modelPathBuffer_.size())) {
        modelPath_ = modelPathBuffer_.data();
    }
    ImGui::DragFloat3("Bullet Model Scale", &defaultScale_.x, 0.01f, 0.001f, 20.0f, "%.3f");
    ImGui::DragFloat3("Bullet Model Rotation", &defaultRotation_.x, 0.01f, -6.28318f, 6.28318f, "%.3f");
    ImGui::Text("Last Fire Position: %.2f, %.2f, %.2f", lastFirePosition_.x, lastFirePosition_.y, lastFirePosition_.z);
    ImGui::Text("Last Fire Direction: %.2f, %.2f, %.2f", lastFireDirection_.x, lastFireDirection_.y, lastFireDirection_.z);
    if (ImGui::Button("Delete All Player Bullets")) {
        DeleteAllBullets();
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove Dead Player Bullets")) {
        RemoveDeadBullets();
    }

    ImGui::SeparatorText("プレイヤー弾一覧 (Player Bullet List)");
    if (bullets_.empty()) {
        ImGui::TextDisabled("No player bullets.");
    } else {
        if (selectedBulletIndex_ < 0 || selectedBulletIndex_ >= static_cast<int>(bullets_.size())) {
            selectedBulletIndex_ = 0;
        }
        for (int index = 0; index < static_cast<int>(bullets_.size()); ++index) {
            const std::string label = "Player Bullet " + std::to_string(index);
            if (ImGui::Selectable(label.c_str(), selectedBulletIndex_ == index)) {
                selectedBulletIndex_ = index;
            }
        }
    }
    if (selectedBulletIndex_ >= 0 && selectedBulletIndex_ < static_cast<int>(bullets_.size())) {
        ImGui::SeparatorText("選択中PlayerBullet情報 (Selected Player Bullet Info)");
        ImGui::Text("Damage: %d", bullets_[selectedBulletIndex_].damage);
        if (EnemyBullet* bullet = bullets_[selectedBulletIndex_].bullet.get()) {
            bullet->DrawImGui();
        }
    }
    ImGui::End();
#endif
}

void PlayerBulletManager::SetGameViewInputActive(bool isActive) {
    gameViewInputActive_ = isActive;
}

EnemyBullet* PlayerBulletManager::SpawnBullet(const Vector3& position, const Vector3& velocity, int damage) {
    if (!object3dCommon_ || !camera_) {
        return nullptr;
    }

    PlayerBulletInstance instance;
    instance.bullet = std::make_unique<EnemyBullet>();
    instance.bullet->Initialize(object3dCommon_, camera_);
    instance.bullet->SetModelPath(modelPath_);
    instance.bullet->SetPosition(position);
    instance.bullet->SetVelocity(velocity);
    instance.bullet->SetScale(defaultScale_);
    instance.bullet->SetRotation(defaultRotation_);
    instance.bullet->SetRadius(bulletRadius_);
    instance.bullet->SetLifeTime(bulletLifeTime_);
    instance.damage = (std::max)(1, damage);

    EnemyBullet* bulletPtr = instance.bullet.get();
    bullets_.push_back(std::move(instance));
    ++firedBulletCount_;
    return bulletPtr;
}

void PlayerBulletManager::DeleteAllBullets() {
    for (PlayerBulletInstance& instance : bullets_) {
        if (instance.bullet) {
            instance.bullet->Finalize();
        }
    }
    bullets_.clear();
    selectedBulletIndex_ = -1;
}

bool PlayerBulletManager::CheckHitAndKillFirstSphere(
    const Vector3& center,
    float radius,
    Vector3* hitPosition,
    int* damage,
    float* lastDistance,
    float* lastRadiusSum,
    float* lastBulletRadius) {
    const float safeRadius = (std::max)(0.0f, radius);
    float closestDistance = -1.0f;
    float closestRadiusSum = safeRadius;
    float closestBulletRadius = 0.0f;
    const float closestInitial = -1.0f;

    for (PlayerBulletInstance& instance : bullets_) {
        EnemyBullet* bullet = instance.bullet.get();
        if (!bullet || !bullet->IsActive() || bullet->IsDead()) {
            continue;
        }

        const float bulletRadius = bullet->GetRadius();
        const float combinedRadius = safeRadius + bulletRadius;
        const float distanceSquared = DistanceSquared(center, bullet->GetPosition());
        const float distance = std::sqrt(distanceSquared);
        if (closestDistance == closestInitial || distance < closestDistance) {
            closestDistance = distance;
            closestRadiusSum = combinedRadius;
            closestBulletRadius = bulletRadius;
        }
        if (distanceSquared <= combinedRadius * combinedRadius) {
            if (hitPosition) {
                *hitPosition = bullet->GetPosition();
            }
            if (damage) {
                *damage = instance.damage;
            }
            if (lastDistance) {
                *lastDistance = distance;
            }
            if (lastRadiusSum) {
                *lastRadiusSum = combinedRadius;
            }
            if (lastBulletRadius) {
                *lastBulletRadius = bulletRadius;
            }
            bullet->Kill();
            return true;
        }
    }

    if (lastDistance) {
        *lastDistance = closestDistance;
    }
    if (lastRadiusSum) {
        *lastRadiusSum = closestRadiusSum;
    }
    if (lastBulletRadius) {
        *lastBulletRadius = closestBulletRadius;
    }
    return false;
}

size_t PlayerBulletManager::GetBulletCount() const {
    return bullets_.size();
}

size_t PlayerBulletManager::GetActiveCount() const {
    size_t activeCount = 0;
    for (const PlayerBulletInstance& instance : bullets_) {
        if (instance.bullet && instance.bullet->IsActive() && !instance.bullet->IsDead()) {
            ++activeCount;
        }
    }
    return activeCount;
}

void PlayerBulletManager::FireFromPlayer() {
    if (!player_ || !camera_) {
        inputBlockedReason_ = "Player or Camera is missing";
        return;
    }

    const Vector3 direction = GetCameraForward(*camera_);
    const Vector3 startPosition = AddVector3(player_->GetWorldPosition(), ScaleVector3(direction, muzzleOffset_));
    lastFirePosition_ = startPosition;
    lastFireDirection_ = direction;
    SpawnBullet(startPosition, ScaleVector3(direction, bulletSpeed_), bulletDamage_);
}

void PlayerBulletManager::RemoveDeadBullets() {
    bullets_.erase(
        std::remove_if(
            bullets_.begin(),
            bullets_.end(),
            [](const PlayerBulletInstance& instance) {
                return !instance.bullet || instance.bullet->IsDead();
            }),
        bullets_.end());

    if (selectedBulletIndex_ >= static_cast<int>(bullets_.size())) {
        selectedBulletIndex_ = bullets_.empty() ? -1 : static_cast<int>(bullets_.size()) - 1;
    }
}

void PlayerBulletManager::SyncModelPathBuffer() {
    std::fill(modelPathBuffer_.begin(), modelPathBuffer_.end(), '\0');
    const size_t copyLength = (std::min)(modelPath_.size(), modelPathBuffer_.size() - 1);
    std::copy_n(modelPath_.data(), copyLength, modelPathBuffer_.data());
}

bool PlayerBulletManager::ShouldBlockFireInput() {
    if (!enablePlayerShot_) {
        inputBlockedReason_ = "Player shot disabled";
        return true;
    }
    if (!gameViewInputActive_) {
        inputBlockedReason_ = "Game View is not hovered/focused";
        return true;
    }
    if (IsEditingImGuiInput()) {
        inputBlockedReason_ = "ImGui item or text input is active";
        return true;
    }
    if (!player_ || !camera_) {
        inputBlockedReason_ = "Player or Camera is missing";
        return true;
    }
    return false;
}
