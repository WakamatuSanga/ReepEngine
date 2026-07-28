#include "PlayerBulletManager.h"
#include "LockedWingMissileExhaustController.h"
#include "MyGame.h"
#include "Engine/Core/GameViewport.h"
#include "Engine/Core/SrvManager.h"
#include "Engine/Game/Enemy/EnemyBullet.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Game/RailShooter/ProjectileRailMotionAdapter.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include "Engine/Input/Input.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kMinVectorLength = 0.00001f;
    constexpr float kPi = 3.14159265358979323846f;

    Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 SubtractVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
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

    Vector3 GetCameraForward(const Camera& camera) {
        const Matrix4x4& matrix = camera.GetWorldMatrix();
        return Normalize({ matrix.m[2][0], matrix.m[2][1], matrix.m[2][2] }, { 0.0f, 0.0f, 1.0f });
    }

    bool IsEditingImGuiText() {
#ifdef USE_IMGUI
        const ImGuiIO& io = ImGui::GetIO();
        return io.WantTextInput;
#else
        return false;
#endif
    }

    const char* ToVisualDirectionSourceLabel(PlayerBulletManager::VisualDirectionSource source) {
        switch (source) {
        case PlayerBulletManager::VisualDirectionSource::AimDirection:
            return "Aim Direction";
        case PlayerBulletManager::VisualDirectionSource::FinalVelocity:
        default:
            return "Final Velocity";
        }
    }
}

PlayerBulletManager::PlayerBulletManager() = default;

PlayerBulletManager::~PlayerBulletManager() = default;

void PlayerBulletManager::Initialize(Object3dCommon* object3dCommon, Camera* camera, Player* player) {
    ClearAimCorridorContext();
    object3dCommon_ = object3dCommon;
    camera_ = camera;
    player_ = player;
    fireTimer_ = fireInterval_;
    inputBlockedReason_ = "Initialized";
    if (camera_) {
        previousCameraPosition_ = camera_->GetTranslate();
        hasPreviousCameraPosition_ = true;
    }
    SyncModelPathBuffer();
    lockedWingMissileExhaustController_ =
        std::make_unique<LockedWingMissileExhaustController>();
    if (object3dCommon_) {
        lockedWingMissileExhaustController_->Initialize(
            object3dCommon_->GetDxCommon(),
            SrvManager::GetInstance(),
            camera_);
    }
}

void PlayerBulletManager::Finalize() {
    DeleteAllBullets();
    ClearAimCorridorContext();
    if (lockedWingMissileExhaustController_) {
        lockedWingMissileExhaustController_->Finalize();
    }
    lockedWingMissileExhaustController_.reset();
    object3dCommon_ = nullptr;
    camera_ = nullptr;
    player_ = nullptr;
    gameViewport_ = nullptr;
    projectileRailMotionAdapter_ = nullptr;
}

void PlayerBulletManager::Update(float deltaTime) {
    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    UpdateCameraVelocity(safeDeltaTime);
    UpdateViewportDebugState();
    UpdateLockedWingShotDiagnostics();
    fireTimer_ = (std::min)(fireInterval_, fireTimer_ + safeDeltaTime);

    inputBlockedReason_ = "None";
    lastLeftClickPressed_ = false;
    lastLeftClickHeld_ = false;
    lastCanFire_ = false;
    lastImGuiTextInputActive_ = false;
    Input* input = MyGame::GetInstance()->GetInput();
    if (!input) {
        inputBlockedReason_ = "Input is missing";
    } else {
        lastLeftClickPressed_ = input->MouseTrigger(Input::MouseLeft);
        lastLeftClickHeld_ = input->MouseDown(Input::MouseLeft);
    }

    bool inputBlocked = true;
    if (input) {
        inputBlocked = ShouldBlockFireInput();
    }
    UpdateChargeState(safeDeltaTime, !input || inputBlocked);

    if (!inputBlocked && input) {
        lastCanFire_ = fireTimer_ >= fireInterval_;
        if (lastLeftClickPressed_ && lastCanFire_) {
            FireFromPlayer();
            fireTimer_ = 0.0f;
            lastCanFire_ = false;
        }
    }
    for (PlayerBulletInstance& instance : bullets_) {
        if (instance.bullet) {
            if (projectileRailMotionAdapter_) {
                projectileRailMotionAdapter_->ApplyToProjectile(
                    *instance.bullet, ProjectileRailMotionAdapter::ProjectileKind::Player);
            }
            UpdateLockedWingShot(instance, safeDeltaTime);
            instance.bullet->Update(safeDeltaTime);
            UpdateLockedWingMissileExhaust(instance);
            if (projectileRailMotionAdapter_ && instance.bullet->IsDead()) {
                projectileRailMotionAdapter_->RecordDespawn(
                    ProjectileRailMotionAdapter::ProjectileKind::Player, instance.bullet->GetDeathReason());
            }
        }
    }

    if (lockedWingMissileExhaustController_) {
        lockedWingMissileExhaustController_->Update(safeDeltaTime);
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
    if (lockedWingMissileExhaustController_) {
        lockedWingMissileExhaustController_->Draw();
    }
}

void PlayerBulletManager::DrawImGui() {
#ifdef USE_IMGUI
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
    ImGui::Text("ImGui Text Input Active: %s", lastImGuiTextInputActive_ ? "true" : "false");
    ImGui::Text("Left Click Pressed / Held: %s / %s",
        lastLeftClickPressed_ ? "true" : "false",
        lastLeftClickHeld_ ? "true" : "false");
    ImGui::Text("Can Fire Now: %s", lastCanFire_ ? "true" : "false");
    ImGui::SeparatorText("チャージ表示用入力 (Charge Feedback Source)");
    ImGui::Checkbox("Enable Charge Feedback Input", &enableChargeFeedbackInput_);
    ImGui::DragFloat("Max Charge Time", &maxChargeTime_, 0.01f, 0.05f, 5.0f, "%.2f");
    ImGui::Text("Charge Time / Rate: %.2f / %.2f", chargeTime_, chargeRate_);
    ImGui::Text("Is Charge Max: %s", isChargeMax_ ? "true" : "false");
    DrawAimImGui();
    DrawLockedWingShotImGui();
    ImGui::SeparatorText("プレイヤー弾モデル向き補正 (Player Bullet Model Rotation Offset)");
    int visualSourceIndex = static_cast<int>(visualDirectionSource_);
    const char* visualSourceItems[] = { "Aim Direction", "Final Velocity" };
    if (ImGui::Combo("Player Bullet Forward Source", &visualSourceIndex, visualSourceItems, 2)) {
        visualDirectionSource_ = static_cast<VisualDirectionSource>(visualSourceIndex);
    }
    ImGui::Text("Current Forward Source: %s", ToVisualDirectionSourceLabel(visualDirectionSource_));
    ImGui::Text("Player Bullet Forward Axis: +Z + offset");
    bool bulletRotationOffsetChanged = false;
    if (ImGui::Button("Yaw +90##PlayerBullet")) {
        playerBulletModelRotationOffset_.y += kPi * 0.5f;
        bulletRotationOffsetChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Yaw -90##PlayerBullet")) {
        playerBulletModelRotationOffset_.y -= kPi * 0.5f;
        bulletRotationOffsetChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Yaw 180##PlayerBullet")) {
        playerBulletModelRotationOffset_.y += kPi;
        bulletRotationOffsetChanged = true;
    }
    if (ImGui::Button("Pitch +90##PlayerBullet")) {
        playerBulletModelRotationOffset_.x += kPi * 0.5f;
        bulletRotationOffsetChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Pitch -90##PlayerBullet")) {
        playerBulletModelRotationOffset_.x -= kPi * 0.5f;
        bulletRotationOffsetChanged = true;
    }
    if (ImGui::Button("Reset Bullet Rotation Offset")) {
        playerBulletModelRotationOffset_ = { 0.0f, 0.0f, 0.0f };
        bulletRotationOffsetChanged = true;
    }
    bulletRotationOffsetChanged |= ImGui::DragFloat3(
        "Player Bullet Model Rotation Offset",
        &playerBulletModelRotationOffset_.x,
        0.01f,
        -6.28318f,
        6.28318f,
        "%.3f");
    if (bulletRotationOffsetChanged) {
        ApplyModelRotationOffsetToBullets();
    }
    ImGui::Text("Current Bullet Direction: %.3f, %.3f, %.3f", lastVisualDirection_.x, lastVisualDirection_.y, lastVisualDirection_.z);
    ImGui::Text("Current Bullet Rotation: %.3f, %.3f, %.3f",
        currentBulletRotation_.x,
        currentBulletRotation_.y,
        currentBulletRotation_.z);
    ImGui::SeparatorText("弾パラメータ (Bullet)");
    ImGui::DragFloat("Bullet Speed", &bulletSpeed_, 0.1f, 0.0f, 200.0f, "%.2f");
    ImGui::DragFloat("Bullet Radius", &bulletRadius_, 0.01f, 0.001f, 20.0f, "%.3f");
    ImGui::DragInt("Bullet Damage", &bulletDamage_, 1.0f, 1, 999);
    ImGui::DragFloat("Fire Interval", &fireInterval_, 0.01f, 0.01f, 5.0f, "%.2f");
    ImGui::DragFloat("Bullet Life Time", &bulletLifeTime_, 0.05f, 0.1f, 30.0f, "%.2f");
    ImGui::Checkbox("Show Bullet Collision Radius", &showBulletCollisionRadius_);
    if (ImGui::Checkbox("Use Lightweight Bullet Visual", &useLightweightBulletVisual_)) {
        SetUseLightweightBulletVisual(useLightweightBulletVisual_);
    }
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

void PlayerBulletManager::SetGameViewport(GameViewport* gameViewport) {
    gameViewport_ = gameViewport;
}

void PlayerBulletManager::SetUseLightweightBulletVisual(bool useLightweightVisual) {
    useLightweightBulletVisual_ = useLightweightVisual;
    for (PlayerBulletInstance& instance : bullets_) {
        if (instance.bullet) {
            instance.bullet->SetUseLightweightVisual(useLightweightBulletVisual_);
        }
    }
}

void PlayerBulletManager::DeleteAllBullets() {
    if (lockedWingMissileExhaustController_) {
        lockedWingMissileExhaustController_->Reset(false);
    }
    for (PlayerBulletInstance& instance : bullets_) {
        if (instance.bullet) {
            instance.bullet->Finalize();
        }
    }
    bullets_.clear();
    selectedBulletIndex_ = -1;
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
        inputBlockedReason_ = "プレイヤーまたはカメラがありません";
        return;
    }

    const LockedWingShotResult lockedWingResult = TrySpawnLockedWingShot();
    if (lockedWingResult == LockedWingShotResult::Spawned
        || lockedWingResult == LockedWingShotResult::SpawnFailed) {
        return;
    }

    const Vector3 cameraForward = GetCameraForward(*camera_);
    const Vector3 initialDirection = ResolveAimDirection(player_->GetWorldPosition(), cameraForward);
    const Vector3 startPosition = AddVector3(
        player_->GetWorldPosition(), ScaleVector3(initialDirection, muzzleOffset_));
    const Vector3 direction = ResolveFinalShotDirection(startPosition, initialDirection);
    if (!lastShotSpawnDataValid_) {
        inputBlockedReason_ = "発射口または目標が無効なため弾を生成しません";
        return;
    }

    Vector3 velocity = ScaleVector3(direction, bulletSpeed_);
    if (lastUsedAimMode_ != AimMode::AimCorridor && inheritCameraVelocity_
        && !(projectileRailMotionAdapter_
            && projectileRailMotionAdapter_->ShouldSuppressCameraVelocityInheritance())) {
        velocity = AddVector3(velocity, ScaleVector3(cameraVelocity_, inheritCameraVelocityFactor_));
    }

    lastFirePosition_ = startPosition;
    lastFireDirection_ = direction;
    lastMuzzlePosition_ = startPosition;
    lastShotVelocity_ = velocity;
    lastVisualDirection_ = visualDirectionSource_ == VisualDirectionSource::AimDirection
        ? direction
        : Normalize(velocity, direction);
    currentBulletRotation_ = AddVector3(
        AddVector3(MakeRotationFromForward(lastVisualDirection_), defaultRotation_),
        playerBulletModelRotationOffset_);
    if (EnemyBullet* bullet = SpawnBullet(startPosition, velocity, bulletDamage_)) {
        RecordAimShot();
        if (projectileRailMotionAdapter_) {
            projectileRailMotionAdapter_->RecordShot(
                ProjectileRailMotionAdapter::ProjectileKind::Player,
                startPosition, lastAimPoint_, direction, velocity);
        }
        if (visualDirectionSource_ == VisualDirectionSource::AimDirection) {
            bullet->SetVisualForwardOverride(direction);
        } else {
            bullet->ClearVisualForwardOverride();
        }
    }
}

void PlayerBulletManager::RemoveDeadBullets() {
    for (PlayerBulletInstance& instance : bullets_) {
        if (!instance.bullet || instance.bullet->IsDead()) {
            UpdateLockedWingMissileExhaust(instance);
        }
    }
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

void PlayerBulletManager::UpdateCameraVelocity(float deltaTime) {
    cameraVelocity_ = { 0.0f, 0.0f, 0.0f };
    if (!camera_) {
        hasPreviousCameraPosition_ = false;
        return;
    }

    const Vector3 currentCameraPosition = camera_->GetTranslate();
    if (hasPreviousCameraPosition_ && deltaTime > kMinVectorLength) {
        cameraVelocity_ = ScaleVector3(SubtractVector3(currentCameraPosition, previousCameraPosition_), 1.0f / deltaTime);
    }
    previousCameraPosition_ = currentCameraPosition;
    hasPreviousCameraPosition_ = true;
}

void PlayerBulletManager::UpdateViewportDebugState() {
    mouseInGameView_ = false;
    mouseNdc_ = { 0.0f, 0.0f };
    mouseNormalized_ = { 0.0f, 0.0f };
    if (!gameViewport_) {
        return;
    }

    mouseInGameView_ = gameViewport_->IsMouseInGameViewport();
    mouseNdc_ = gameViewport_->GetMouseNdcInGameViewport();
    mouseNormalized_ = gameViewport_->GetMouseNormalizedInGameViewport();
}

void PlayerBulletManager::ApplyModelRotationOffsetToBullets() {
    for (PlayerBulletInstance& instance : bullets_) {
        if (instance.bullet) {
            instance.bullet->SetModelRotationOffset(playerBulletModelRotationOffset_);
        }
    }
}

void PlayerBulletManager::UpdateChargeState(float deltaTime, bool inputBlocked) {
    maxChargeTime_ = (std::max)(maxChargeTime_, 0.05f);
    if (!enableChargeFeedbackInput_ || inputBlocked || !lastLeftClickHeld_) {
        chargeTime_ = 0.0f;
        chargeRate_ = 0.0f;
        isChargeMax_ = false;
        return;
    }

    chargeTime_ = (std::min)(maxChargeTime_, chargeTime_ + (std::max)(deltaTime, 0.0f));
    chargeRate_ = std::clamp(chargeTime_ / maxChargeTime_, 0.0f, 1.0f);
    isChargeMax_ = chargeRate_ >= 1.0f;
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
    lastImGuiTextInputActive_ = IsEditingImGuiText();
    if (lastImGuiTextInputActive_) {
        inputBlockedReason_ = "ImGui text input is active";
        return true;
    }
    if (aimMode_ == AimMode::MouseRay || aimMode_ == AimMode::MouseAimPlane) {
        if (!gameViewport_) {
            inputBlockedReason_ = "GameViewport is missing";
            return true;
        }
        if (!gameViewport_->IsMouseInGameViewport()) {
            inputBlockedReason_ = "Mouse is outside GameViewport";
            return true;
        }
    }
    if (!player_ || !camera_) {
        inputBlockedReason_ = "Player or Camera is missing";
        return true;
    }
    return false;
}

