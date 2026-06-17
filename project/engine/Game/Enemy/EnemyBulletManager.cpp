#include "EnemyBulletManager.h"
#include "EnemyBullet.h"
#include <algorithm>
#include <cmath>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kPi = 3.14159265358979323846f;

    float DistanceSquared(const Vector3& lhs, const Vector3& rhs) {
        const float x = lhs.x - rhs.x;
        const float y = lhs.y - rhs.y;
        const float z = lhs.z - rhs.z;
        return x * x + y * y + z * z;
    }
}

EnemyBulletManager::EnemyBulletManager() = default;

EnemyBulletManager::~EnemyBulletManager() = default;

void EnemyBulletManager::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    object3dCommon_ = object3dCommon;
    camera_ = camera;
}

void EnemyBulletManager::Finalize() {
    DeleteAllBullets();
    object3dCommon_ = nullptr;
    camera_ = nullptr;
}

void EnemyBulletManager::Update(float deltaTime) {
    for (std::unique_ptr<EnemyBullet>& bullet : bullets_) {
        if (bullet) {
            bullet->Update(deltaTime);
        }
    }

    if (autoRemoveDeadBullets_) {
        RemoveDeadBullets();
    }
}

void EnemyBulletManager::Draw() {
    for (std::unique_ptr<EnemyBullet>& bullet : bullets_) {
        if (bullet) {
            bullet->Draw();
        }
    }

    if (showEnemyBulletRadius_) {
        for (std::unique_ptr<EnemyBullet>& bullet : bullets_) {
            if (bullet) {
                bullet->DrawRadius();
            }
        }
    }
}

void EnemyBulletManager::DrawImGui() {
#ifdef _DEBUG
    ImGui::SetNextWindowSize(ImVec2(380.0f, 360.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("敵弾確認 (Enemy Bullet Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Bullet Count: %zu", GetBulletCount());
    ImGui::Text("Active Count: %zu", GetActiveCount());
    ImGui::Checkbox("Auto Remove Dead Bullets", &autoRemoveDeadBullets_);
    ImGui::DragFloat3("Default Scale", &defaultScale_.x, 0.01f, 0.001f, 20.0f, "%.3f");
    ImGui::DragFloat3("Default Rotation", &defaultRotation_.x, 0.01f, -6.28318f, 6.28318f, "%.3f");
    ImGui::SeparatorText("敵弾モデル向き補正 (Enemy Bullet Model Rotation Offset)");
    bool rotationOffsetChanged = false;
    if (ImGui::Button("Yaw +90##EnemyBulletDefault")) {
        defaultModelRotationOffset_.y += kPi * 0.5f;
        rotationOffsetChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Yaw -90##EnemyBulletDefault")) {
        defaultModelRotationOffset_.y -= kPi * 0.5f;
        rotationOffsetChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Yaw 180##EnemyBulletDefault")) {
        defaultModelRotationOffset_.y += kPi;
        rotationOffsetChanged = true;
    }
    if (ImGui::Button("Pitch +90##EnemyBulletDefault")) {
        defaultModelRotationOffset_.x += kPi * 0.5f;
        rotationOffsetChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Pitch -90##EnemyBulletDefault")) {
        defaultModelRotationOffset_.x -= kPi * 0.5f;
        rotationOffsetChanged = true;
    }
    if (ImGui::Button("向き補正リセット##EnemyBulletDefault")) {
        defaultModelRotationOffset_ = { 0.0f, 0.0f, 0.0f };
        rotationOffsetChanged = true;
    }
    rotationOffsetChanged |= ImGui::DragFloat3(
        "Enemy Bullet Model Rotation Offset",
        &defaultModelRotationOffset_.x,
        0.01f,
        -6.28318f,
        6.28318f,
        "%.3f");
    if (rotationOffsetChanged) {
        ApplyModelRotationOffsetToAllBullets();
    }
    ImGui::Text("Enemy Bullet Forward Axis: +Z + offset");
    ImGui::Checkbox("Show EnemyBullet Radius", &showEnemyBulletRadius_);
    if (ImGui::Checkbox("Use Lightweight Bullet Visual", &useLightweightBulletVisual_)) {
        SetUseLightweightBulletVisual(useLightweightBulletVisual_);
    }
    ImGui::DragFloat("EnemyBullet Radius", &defaultRadius_, 0.01f, 0.001f, 20.0f, "%.3f");
    if (ImGui::Button("Bullet Radius Small")) {
        defaultRadius_ = 0.10f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Bullet Radius Normal")) {
        defaultRadius_ = 0.15f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Bullet Radius Large")) {
        defaultRadius_ = 0.25f;
    }
    if (ImGui::Button("Delete All Bullets")) {
        DeleteAllBullets();
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove Dead Bullets")) {
        RemoveDeadBullets();
    }

    ImGui::SeparatorText("弾一覧 (Bullet List)");
    if (bullets_.empty()) {
        ImGui::TextDisabled("No bullets.");
    } else {
        if (selectedBulletIndex_ < 0 || selectedBulletIndex_ >= static_cast<int>(bullets_.size())) {
            selectedBulletIndex_ = 0;
        }
        for (int index = 0; index < static_cast<int>(bullets_.size()); ++index) {
            const std::string label = "Bullet " + std::to_string(index);
            if (ImGui::Selectable(label.c_str(), selectedBulletIndex_ == index)) {
                selectedBulletIndex_ = index;
            }
        }
    }

    if (selectedBulletIndex_ >= 0 && selectedBulletIndex_ < static_cast<int>(bullets_.size())) {
        ImGui::SeparatorText("選択中Bullet情報 (Selected Bullet Info)");
        if (EnemyBullet* bullet = bullets_[selectedBulletIndex_].get()) {
            bullet->DrawImGui();
        }
    }

    ImGui::End();
#endif
}

EnemyBullet* EnemyBulletManager::SpawnBullet(const Vector3& position, const Vector3& velocity) {
    if (!object3dCommon_ || !camera_) {
        return nullptr;
    }

    auto bullet = std::make_unique<EnemyBullet>();
    bullet->Initialize(object3dCommon_, camera_);
    bullet->SetPosition(position);
    bullet->SetVelocity(velocity);
    bullet->SetScale(defaultScale_);
    bullet->SetRotation(defaultRotation_);
    bullet->SetModelRotationOffset(defaultModelRotationOffset_);
    bullet->SetRadius(defaultRadius_);
    bullet->SetUseLightweightVisual(useLightweightBulletVisual_);

    EnemyBullet* bulletPtr = bullet.get();
    bullets_.push_back(std::move(bullet));
    return bulletPtr;
}

void EnemyBulletManager::DeleteAllBullets() {
    for (std::unique_ptr<EnemyBullet>& bullet : bullets_) {
        if (bullet) {
            bullet->Finalize();
        }
    }
    bullets_.clear();
    selectedBulletIndex_ = -1;
}

void EnemyBulletManager::SetUseLightweightBulletVisual(bool useLightweightVisual) {
    useLightweightBulletVisual_ = useLightweightVisual;
    for (std::unique_ptr<EnemyBullet>& bullet : bullets_) {
        if (bullet) {
            bullet->SetUseLightweightVisual(useLightweightBulletVisual_);
        }
    }
}

void EnemyBulletManager::ApplyModelRotationOffsetToAllBullets() {
    for (std::unique_ptr<EnemyBullet>& bullet : bullets_) {
        if (bullet) {
            bullet->SetModelRotationOffset(defaultModelRotationOffset_);
        }
    }
}

bool EnemyBulletManager::CheckHitAndKillFirstSphere(const Vector3& center, float radius, Vector3* hitPosition) {
    return CheckHitAndKillFirstSphere(center, radius, hitPosition, nullptr, nullptr, nullptr);
}

bool EnemyBulletManager::CheckHitAndKillFirstSphere(
    const Vector3& center,
    float radius,
    Vector3* hitPosition,
    float* lastDistance,
    float* lastRadiusSum,
    float* lastBulletRadius) {
    const float safeRadius = (std::max)(0.0f, radius);
    if (lastDistance) {
        *lastDistance = -1.0f;
    }
    if (lastRadiusSum) {
        *lastRadiusSum = safeRadius;
    }
    if (lastBulletRadius) {
        *lastBulletRadius = 0.0f;
    }
    for (std::unique_ptr<EnemyBullet>& bullet : bullets_) {
        if (!bullet || !bullet->IsActive() || bullet->IsDead()) {
            continue;
        }

        const float combinedRadius = safeRadius + bullet->GetRadius();
        const float distanceSquared = DistanceSquared(center, bullet->GetPosition());
        if (lastDistance) {
            *lastDistance = std::sqrt(distanceSquared);
        }
        if (lastRadiusSum) {
            *lastRadiusSum = combinedRadius;
        }
        if (lastBulletRadius) {
            *lastBulletRadius = bullet->GetRadius();
        }
        if (distanceSquared <= combinedRadius * combinedRadius) {
            if (hitPosition) {
                *hitPosition = bullet->GetPosition();
            }
            bullet->Kill();
            return true;
        }
    }

    return false;
}

size_t EnemyBulletManager::GetBulletCount() const {
    return bullets_.size();
}

size_t EnemyBulletManager::GetActiveCount() const {
    size_t activeCount = 0;
    for (const std::unique_ptr<EnemyBullet>& bullet : bullets_) {
        if (bullet && bullet->IsActive() && !bullet->IsDead()) {
            ++activeCount;
        }
    }
    return activeCount;
}

void EnemyBulletManager::RemoveDeadBullets() {
    bullets_.erase(
        std::remove_if(
            bullets_.begin(),
            bullets_.end(),
            [](const std::unique_ptr<EnemyBullet>& bullet) {
                return !bullet || bullet->IsDead();
            }),
        bullets_.end());

    if (selectedBulletIndex_ >= static_cast<int>(bullets_.size())) {
        selectedBulletIndex_ = bullets_.empty() ? -1 : static_cast<int>(bullets_.size()) - 1;
    }
}
