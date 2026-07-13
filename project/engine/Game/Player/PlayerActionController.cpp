#include "PlayerActionController.h"
#include "Engine/Game/Effect/CombatEffectController.h"
#include "Engine/Game/Effect/CombatSlowMotionController.h"
#include "Engine/Game/Enemy/EnemyBulletManager.h"
#include "PlayerBarrelRollRingController.h"
#include "PlayerBulletCancelEffectController.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include "Engine/Input/Input.h"
#include "MyGame.h"
#include <algorithm>
#include <cmath>
#include <vector>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kMinDuration = 0.001f;

    float Saturate(float value) {
        return std::clamp(value, 0.0f, 1.0f);
    }

    float SmoothStep(float value) {
        const float t = Saturate(value);
        return t * t * (3.0f - 2.0f * t);
    }
}

PlayerActionController::PlayerActionController() = default;

PlayerActionController::~PlayerActionController() = default;

void PlayerActionController::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    object3dCommon_ = object3dCommon;
    camera_ = camera;
    if (object3dCommon_ && camera_) {
        clearRadiusObject_ = std::make_unique<Object3d>();
        clearRadiusObject_->Initialize(object3dCommon_);
        clearRadiusObject_->SetCamera(camera_);
        clearRadiusObject_->SetEnvironmentMapEnabled(false);
        clearRadiusModel_ = ModelManager::GetInstance()->CreateSphere("PlayerBarrelRollClearRadiusSphere", 24);
        clearRadiusObject_->SetModel(clearRadiusModel_);
        clearRadiusObject_->SetScale({
            GetEffectiveBarrelRollClearBulletRadius(),
            GetEffectiveBarrelRollClearBulletRadius(),
            GetEffectiveBarrelRollClearBulletRadius()
            });
        clearRadiusObject_->Update();
    }
}

void PlayerActionController::Finalize() {
    clearRadiusObject_.reset();
    clearRadiusModel_ = nullptr;
    enemyBulletManager_ = nullptr;
    combatEffectController_ = nullptr;
    rollRingController_ = nullptr;
    bulletCancelEffectController_ = nullptr;
    slowMotionController_ = nullptr;
    object3dCommon_ = nullptr;
    camera_ = nullptr;
}

void PlayerActionController::SetDependencies(
    EnemyBulletManager* enemyBulletManager,
    CombatEffectController* combatEffectController) {
    enemyBulletManager_ = enemyBulletManager;
    combatEffectController_ = combatEffectController;
}

void PlayerActionController::SetEffectControllers(
    PlayerBarrelRollRingController* rollRingController,
    PlayerBulletCancelEffectController* bulletCancelEffectController) {
    rollRingController_ = rollRingController;
    bulletCancelEffectController_ = bulletCancelEffectController;
}

void PlayerActionController::SetSlowMotionController(CombatSlowMotionController* slowMotionController) {
    slowMotionController_ = slowMotionController;
}

void PlayerActionController::SetDebugVisualsEnabled(bool isEnabled) {
    debugVisualsEnabled_ = isEnabled;
}

void PlayerActionController::Update(float deltaTime, bool canUseInput, const Vector3& playerPosition) {
    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    lastLeftTrigger_ = false;
    lastRightTrigger_ = false;
    lastInputAllowed_ = canUseInput;
    lastBlockedReason_ = "None";

    if (barrelRollCooldownTimer_ > 0.0f) {
        barrelRollCooldownTimer_ = (std::max)(0.0f, barrelRollCooldownTimer_ - safeDeltaTime);
    }

    if (isBarrelRolling_) {
        barrelRollTimer_ += safeDeltaTime;
        if (barrelRollTimer_ >= (std::max)(barrelRollDuration_, kMinDuration)) {
            isBarrelRolling_ = false;
            barrelRollDirection_ = BarrelRollDirection::None;
            barrelRollTimer_ = 0.0f;
        }
    }

    UpdateTapTimers(safeDeltaTime);

    Input* input = MyGame::GetInstance()->GetInput();
    if (!enableBarrelRoll_) {
        lastBlockedReason_ = "Barrel Roll disabled";
    } else if (!canUseInput) {
        lastBlockedReason_ = "Input not active";
    } else if (!input) {
        lastBlockedReason_ = "Input missing";
    } else {
        lastLeftTrigger_ = input->TriggerKey(DIK_A);
        lastRightTrigger_ = input->TriggerKey(DIK_D);
        if (lastLeftTrigger_) {
            if (leftTapTimer_ <= barrelRollInputDoubleTapTime_) {
                StartBarrelRoll(BarrelRollDirection::Left, playerPosition);
            }
            leftTapTimer_ = 0.0f;
        }
        if (lastRightTrigger_) {
            if (rightTapTimer_ <= barrelRollInputDoubleTapTime_) {
                StartBarrelRoll(BarrelRollDirection::Right, playerPosition);
            }
            rightTapTimer_ = 0.0f;
        }
    }

    UpdateDebugRadiusObject(playerPosition);
}

void PlayerActionController::DrawDebugVisuals() {
    if (!debugVisualsEnabled_ || !showBarrelRollClearRadius_ || !clearRadiusObject_ || !clearRadiusModel_ || !object3dCommon_) {
        return;
    }

    object3dCommon_->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    clearRadiusObject_->Draw();
}

void PlayerActionController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SeparatorText("バレルロール / 回避 (Barrel Roll Action)");
    ImGui::Checkbox("Enable Barrel Roll", &enableBarrelRoll_);
    ImGui::Checkbox("Enable Barrel Roll Effect", &enableBarrelRollEffect_);
    ImGui::DragFloat("Barrel Roll Duration", &barrelRollDuration_, 0.01f, 0.05f, 3.0f, "%.2f");
    ImGui::DragFloat("Barrel Roll Cooldown", &barrelRollCooldown_, 0.01f, 0.0f, 5.0f, "%.2f");
    ImGui::DragFloat("Double Tap Time", &barrelRollInputDoubleTapTime_, 0.01f, 0.05f, 1.0f, "%.2f");
    ImGui::DragFloat("Invincible Time", &barrelRollInvincibleTime_, 0.01f, 0.0f, 3.0f, "%.2f");
    ImGui::DragFloat("Barrel Roll Bullet Clear Radius", &barrelRollClearBulletRadius_, 0.05f, 0.0f, 20.0f, "%.2f");
    ImGui::SliderFloat("Bullet Clear Radius Scale", &barrelRollClearBulletRadiusScale_, 1.0f, 1.6f, "%.2f");
    barrelRollClearBulletRadius_ = std::clamp(barrelRollClearBulletRadius_, 0.0f, 20.0f);
    barrelRollClearBulletRadiusScale_ = std::clamp(barrelRollClearBulletRadiusScale_, 1.0f, 1.6f);
    ImGui::Text("Effective Bullet Clear Radius: %.2f", GetEffectiveBarrelRollClearBulletRadius());
    ImGui::SliderFloat("Damage Reduction", &barrelRollDamageReduction_, 0.0f, 1.0f, "%.2f");
    ImGui::Checkbox("Show Barrel Roll Clear Radius", &showBarrelRollClearRadius_);
    ImGui::Text("Is Barrel Rolling: %s", IsBarrelRolling() ? "true" : "false");
    ImGui::Text("Is Invincible: %s", IsInvincible() ? "true" : "false");
    ImGui::Text("Barrel Roll Timer: %.3f", barrelRollTimer_);
    ImGui::Text("Barrel Roll Cooldown Timer: %.3f", barrelRollCooldownTimer_);
    ImGui::Text("Last Barrel Roll Direction: %s", GetDirectionName(lastBarrelRollDirection_));
    ImGui::Text("Cleared Bullet Count: %u", lastClearedBulletCount_);
    ImGui::Text("Input Allowed: %s", lastInputAllowed_ ? "true" : "false");
    ImGui::Text("A/D Trigger: %s / %s", lastLeftTrigger_ ? "true" : "false", lastRightTrigger_ ? "true" : "false");
    ImGui::TextWrapped("Blocked Reason: %s", lastBlockedReason_.c_str());
    if (ImGui::Button("Trigger Left Barrel Roll")) {
        StartBarrelRoll(BarrelRollDirection::Left, clearRadiusObject_ ? clearRadiusObject_->GetTransform().translate : Vector3{});
    }
    ImGui::SameLine();
    if (ImGui::Button("Trigger Right Barrel Roll")) {
        StartBarrelRoll(BarrelRollDirection::Right, clearRadiusObject_ ? clearRadiusObject_->GetTransform().translate : Vector3{});
    }
#endif
}


float PlayerActionController::GetBarrelRollClearBulletRadius() const {
    return GetEffectiveBarrelRollClearBulletRadius();
}

float PlayerActionController::GetEffectiveBarrelRollClearBulletRadius() const {
    return (std::max)(0.0f, barrelRollClearBulletRadius_) * std::clamp(barrelRollClearBulletRadiusScale_, 1.0f, 1.6f);
}
bool PlayerActionController::IsBarrelRolling() const {
    return isBarrelRolling_;
}

bool PlayerActionController::IsInvincible() const {
    return isBarrelRolling_ &&
        barrelRollTimer_ <= barrelRollInvincibleTime_ &&
        barrelRollDamageReduction_ > 0.0f;
}

Vector3 PlayerActionController::GetVisualRotationOffset() const {
    if (!isBarrelRolling_) {
        return { 0.0f, 0.0f, 0.0f };
    }

    const float duration = (std::max)(barrelRollDuration_, kMinDuration);
    const float progress = Saturate(barrelRollTimer_ / duration);
    const float easedProgress = SmoothStep(progress);
    const float directionSign = barrelRollDirection_ == BarrelRollDirection::Left ? 1.0f : -1.0f;
    return { 0.0f, 0.0f, directionSign * kPi * 2.0f * easedProgress };
}

void PlayerActionController::StartBarrelRoll(BarrelRollDirection direction, const Vector3& playerPosition) {
    if (!enableBarrelRoll_) {
        lastBlockedReason_ = "Barrel Roll disabled";
        return;
    }
    if (direction == BarrelRollDirection::None) {
        lastBlockedReason_ = "No direction";
        return;
    }
    if (isBarrelRolling_) {
        lastBlockedReason_ = "Already rolling";
        return;
    }
    if (barrelRollCooldownTimer_ > 0.0f) {
        lastBlockedReason_ = "Cooldown active";
        return;
    }

    isBarrelRolling_ = true;
    barrelRollDirection_ = direction;
    lastBarrelRollDirection_ = direction;
    barrelRollTimer_ = 0.0f;
    barrelRollCooldownTimer_ = barrelRollCooldown_;
    ++barrelRollCount_;
    lastClearedBulletCount_ = 0;

    if (rollRingController_) {
        Vector3 forward{ 0.0f, 0.0f, 1.0f };
        if (camera_) {
            const Matrix4x4& matrix = camera_->GetWorldMatrix();
            const float length = std::sqrt(matrix.m[2][0] * matrix.m[2][0] + matrix.m[2][1] * matrix.m[2][1] + matrix.m[2][2] * matrix.m[2][2]);
            if (length > 0.00001f && std::isfinite(length)) {
                forward = { matrix.m[2][0] / length, matrix.m[2][1] / length, matrix.m[2][2] / length };
            }
        }
        rollRingController_->SpawnRollRings(playerPosition, forward, direction == BarrelRollDirection::Left ? -1 : 1);
    }

    const float effectiveClearRadius = GetEffectiveBarrelRollClearBulletRadius();
    if (enemyBulletManager_ && effectiveClearRadius > 0.0f) {
        std::vector<Vector3> clearedPositions;
        const size_t maxEffectPositions = bulletCancelEffectController_ ? static_cast<size_t>((std::max)(0, bulletCancelEffectController_->GetMaxEffectsPerFrame())) : 0;
        lastClearedBulletCount_ = static_cast<uint32_t>(enemyBulletManager_->ClearBulletsInRadius(
            playerPosition,
            effectiveClearRadius,
            bulletCancelEffectController_ ? &clearedPositions : nullptr,
            maxEffectPositions));
        if (bulletCancelEffectController_) {
            for (const Vector3& position : clearedPositions) {
                bulletCancelEffectController_->SpawnCancelEffect(position);
            }
        }
        if (slowMotionController_ && lastClearedBulletCount_ > 0) {
            slowMotionController_->TriggerBulletCancelSlowMotion(static_cast<int>(lastClearedBulletCount_));
        }
    }
    if (enableBarrelRollEffect_ && combatEffectController_) {
        combatEffectController_->PlayEnemyBulletHitPlayer(playerPosition);
    }
}

void PlayerActionController::UpdateTapTimers(float deltaTime) {
    leftTapTimer_ = (std::min)(leftTapTimer_ + deltaTime, 999.0f);
    rightTapTimer_ = (std::min)(rightTapTimer_ + deltaTime, 999.0f);
}

void PlayerActionController::UpdateDebugRadiusObject(const Vector3& playerPosition) {
    if (!clearRadiusObject_) {
        return;
    }
    clearRadiusObject_->SetTranslate(playerPosition);
    clearRadiusObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
    clearRadiusObject_->SetScale({
        GetEffectiveBarrelRollClearBulletRadius(),
        GetEffectiveBarrelRollClearBulletRadius(),
        GetEffectiveBarrelRollClearBulletRadius()
        });
    clearRadiusObject_->Update();
}

const char* PlayerActionController::GetDirectionName(BarrelRollDirection direction) const {
    switch (direction) {
    case BarrelRollDirection::Left:
        return "Left";
    case BarrelRollDirection::Right:
        return "Right";
    case BarrelRollDirection::None:
    default:
        return "None";
    }
}

