#include "Player.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Game/Effect/CombatSlowMotionController.h"
#include "Engine/Graphics/Model/Model.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Input/Input.h"
#include "PlayerActionController.h"
#include "MyGame.h"
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

    const char* ToModelForwardAxisLabel(Player::ModelForwardAxis axis) {
        switch (axis) {
        case Player::ModelForwardAxis::NegativeZ:
            return "-Z";
        case Player::ModelForwardAxis::PositiveX:
            return "+X";
        case Player::ModelForwardAxis::NegativeX:
            return "-X";
        case Player::ModelForwardAxis::PositiveZ:
        default:
            return "+Z";
        }
    }

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

    bool IsEditingImGuiText() {
#ifdef USE_IMGUI
        const ImGuiIO& io = ImGui::GetIO();
        return io.WantTextInput;
#else
        return false;
#endif
    }

}

Player::Player() = default;

Player::~Player() = default;

void Player::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    object3dCommon_ = object3dCommon;
    camera_ = camera;
    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon_);
    object_->SetCamera(camera_);
    object_->SetEnvironmentMapEnabled(false);
    hitRadiusObject_ = std::make_unique<Object3d>();
    hitRadiusObject_->Initialize(object3dCommon_);
    hitRadiusObject_->SetCamera(camera_);
    hitRadiusObject_->SetEnvironmentMapEnabled(false);
    hitRadiusModel_ = ModelManager::GetInstance()->CreateSphere("PlayerHitRadiusSphere", 16);
    hitRadiusObject_->SetModel(hitRadiusModel_);
    actionController_ = std::make_unique<PlayerActionController>();
    actionController_->Initialize(object3dCommon_, camera_);
    LoadModel();
    ResetPosition();
    UpdateWorldPosition();
    UpdateObjectTransform();
    object_->Update();
    hitRadiusObject_->Update();
}

void Player::Finalize() {
    if (actionController_) {
        actionController_->Finalize();
    }
    actionController_.reset();
    hitRadiusObject_.reset();
    hitRadiusModel_ = nullptr;
    object_.reset();
    model_ = nullptr;
}

void Player::Update(float deltaTime) {
    ++updateCount_;
    lastWPressed_ = false;
    lastAPressed_ = false;
    lastSPressed_ = false;
    lastDPressed_ = false;
    lastLeftMouseDown_ = false;
    lastRightMouseDown_ = false;
    lastImGuiTextInputActive_ = false;
    lastInputApplied_ = false;
    lastRawMoveInput_ = { 0.0f, 0.0f, 0.0f };
    inputBlockedReason_ = "None";

    if (!object_ || !camera_) {
        inputBlockedReason_ = "Object or Camera is missing";
        return;
    }

    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    Input* input = MyGame::GetInstance()->GetInput();
    if (input) {
        lastAPressed_ = input->PushKey(DIK_A);
        lastDPressed_ = input->PushKey(DIK_D);
        lastWPressed_ = input->PushKey(DIK_W);
        lastSPressed_ = input->PushKey(DIK_S);
        lastLeftMouseDown_ = input->MouseDown(Input::MouseLeft);
        lastRightMouseDown_ = input->MouseDown(Input::MouseRight);

        if (lastAPressed_) {
            lastRawMoveInput_.x -= 1.0f;
        }
        if (lastDPressed_) {
            lastRawMoveInput_.x += 1.0f;
        }
        if (lastWPressed_) {
            lastRawMoveInput_.y += 1.0f;
        }
        if (lastSPressed_) {
            lastRawMoveInput_.y -= 1.0f;
        }
    } else {
        inputBlockedReason_ = "Input is missing";
    }

    lastImGuiTextInputActive_ = IsEditingImGuiText();
    if (!enablePlayer_) {
        inputBlockedReason_ = "Player disabled";
    } else if (!gameViewInputActive_) {
        inputBlockedReason_ = "Game View is not hovered/focused";
    } else if (lastImGuiTextInputActive_) {
        inputBlockedReason_ = "ImGui text input is active";
    } else if (input) {
        Vector3 moveInput = lastRawMoveInput_;
        const float inputLength = std::sqrt(moveInput.x * moveInput.x + moveInput.y * moveInput.y);
        if (inputLength > kMinVectorLength) {
            if (moveSpeed_ <= 0.0f) {
                inputBlockedReason_ = "Move Speed is 0";
            } else {
                moveInput.x /= inputLength;
                moveInput.y /= inputLength;
                localOffsetX_ = std::clamp(localOffsetX_ + moveInput.x * moveSpeed_ * safeDeltaTime, -moveLimitX_, moveLimitX_);
                localOffsetY_ = std::clamp(localOffsetY_ + moveInput.y * moveSpeed_ * safeDeltaTime, -moveLimitY_, moveLimitY_);
                lastInputApplied_ = true;
            }
        }
    }

    UpdateWorldPosition();
    UpdateCenterVisibilityAssist(safeDeltaTime);
    if (actionController_) {
        actionController_->Update(
            safeDeltaTime,
            enablePlayer_ && gameViewInputActive_ && !lastImGuiTextInputActive_,
            worldPosition_);
    }
    UpdateVisualTilt(safeDeltaTime);
    UpdateObjectTransform();
    object_->Update();
    if (hitRadiusObject_) {
        hitRadiusObject_->Update();
    }
}

void Player::Draw() {
    if (!enablePlayer_ || !showPlayer_ || !object3dCommon_ || !object_ || !model_) {
        return;
    }

    object3dCommon_->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    object_->Draw();
    if (showHitRadius_ && hitRadiusObject_ && hitRadiusModel_) {
        hitRadiusObject_->Draw();
    }
    if (actionController_) {
        actionController_->DrawDebugVisuals();
    }
}

void Player::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(360.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("プレイヤー確認 (Player Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Player有効 (Enable Player)", &enablePlayer_);
    ImGui::Checkbox("Player表示 (Show Player)", &showPlayer_);
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
    if (ImGui::Checkbox("Use Lightweight Player Visual", &useLightweightVisual_)) {
        LoadModel();
        UpdateObjectTransform();
        if (object_) {
            object_->Update();
        }
    }
    ImGui::SeparatorText("入力診断 (Input Diagnostics)");
    ImGui::Text("Update Called: yes (%llu)", static_cast<unsigned long long>(updateCount_));
    ImGui::TextWrapped("Input Blocked Reason: %s", inputBlockedReason_.c_str());
    ImGui::Text("Game View Input Active: %s", gameViewInputActive_ ? "true" : "false");
    ImGui::Text("ImGui Text Input Active: %s", lastImGuiTextInputActive_ ? "true" : "false");
    ImGui::Text("Left / Right Mouse Down: %s / %s",
        lastLeftMouseDown_ ? "true" : "false",
        lastRightMouseDown_ ? "true" : "false");
    ImGui::Text("W/A/S/D Pressed: %s / %s / %s / %s",
        lastWPressed_ ? "true" : "false",
        lastAPressed_ ? "true" : "false",
        lastSPressed_ ? "true" : "false",
        lastDPressed_ ? "true" : "false");
    ImGui::Text("Raw Move Input: %.1f, %.1f", lastRawMoveInput_.x, lastRawMoveInput_.y);
    ImGui::Text("Input Applied: %s", lastInputApplied_ ? "true" : "false");
    ImGui::Text("Base Mode: %s", baseMode_ == BaseMode::Rail ? "Rail" : "CameraFront");
    ImGui::Text("External Base Valid: %s", hasExternalBase_ ? "true" : "false");
    if (ImGui::Button("Load / Reload Model")) {
        LoadModel();
        UpdateObjectTransform();
        if (object_) {
            object_->Update();
        }
    }

    ImGui::SeparatorText("Rail Shooter Preview");
    ImGui::TextDisabled("CameraRig確認用の見た目プリセットです。PlayerはCameraFrontのままです。");
    ImGui::Text("推奨Scale: 0.45 / 推奨Distance From Camera: 2.8〜3.0");
    if (ImGui::Button("近め (Near)")) {
        distanceFromCamera_ = 2.5f;
    }
    ImGui::SameLine();
    if (ImGui::Button("標準 (Default)")) {
        distanceFromCamera_ = 3.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("遠め (Far)")) {
        distanceFromCamera_ = 6.0f;
    }
    if (ImGui::Button("レールシューティング用 (Rail Shooter Preset)")) {
        distanceFromCamera_ = 2.8f;
        modelScale_ = { 0.45f, 0.45f, 0.45f };
        ResetPosition();
        UpdateWorldPosition();
        UpdateObjectTransform();
        if (object_) {
            object_->Update();
        }
    }

    ImGui::SeparatorText("モデル向き補正 (Model Rotation Offset)");
    int forwardAxisIndex = static_cast<int>(modelForwardAxis_);
    const char* forwardAxisItems[] = { "+Z", "-Z", "+X", "-X" };
    if (ImGui::Combo("モデル正面軸 (Model Forward Axis)", &forwardAxisIndex, forwardAxisItems, 4)) {
        modelForwardAxis_ = static_cast<ModelForwardAxis>(forwardAxisIndex);
    }
    if (ImGui::Button("Forward +Z")) {
        modelForwardAxis_ = ModelForwardAxis::PositiveZ;
        modelRotationOffset_ = { 0.0f, 0.0f, 0.0f };
    }
    ImGui::SameLine();
    if (ImGui::Button("Forward -Z")) {
        modelForwardAxis_ = ModelForwardAxis::NegativeZ;
        modelRotationOffset_ = { 0.0f, 0.0f, 0.0f };
    }
    if (ImGui::Button("Forward +X")) {
        modelForwardAxis_ = ModelForwardAxis::PositiveX;
        modelRotationOffset_ = { 0.0f, 0.0f, 0.0f };
    }
    ImGui::SameLine();
    if (ImGui::Button("Forward -X")) {
        modelForwardAxis_ = ModelForwardAxis::NegativeX;
        modelRotationOffset_ = { 0.0f, 0.0f, 0.0f };
    }
    if (ImGui::Button("Yaw 180")) {
        modelRotationOffset_.y += kPi;
    }
    ImGui::SameLine();
    if (ImGui::Button("Yaw 90")) {
        modelRotationOffset_.y += kPi * 0.5f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Yaw -90")) {
        modelRotationOffset_.y -= kPi * 0.5f;
    }
    if (ImGui::Button("向き補正リセット (Reset Rotation Offset)")) {
        modelRotationOffset_ = { 0.0f, 0.0f, 0.0f };
    }
    ImGui::DragFloat3("Model Rotation Offset", &modelRotationOffset_.x, 0.01f, -6.28318f, 6.28318f, "%.3f");
    ImGui::Text("Model Forward Axis: %s", ToModelForwardAxisLabel(modelForwardAxis_));
    ImGui::Text("Current Forward: %.3f, %.3f, %.3f", baseForward_.x, baseForward_.y, baseForward_.z);
    ImGui::Text("Current Model Rotation: %.3f, %.3f, %.3f",
        visualFinalRotation_.x,
        visualFinalRotation_.y,
        visualFinalRotation_.z);

    ImGui::SeparatorText("プレイヤー見た目傾き (Player Visual Tilt)");
    ImGui::Checkbox("見た目傾き有効 (Enable Visual Tilt)", &enableVisualTilt_);
    ImGui::DragFloat("W/S Pitch Tilt Amount", &visualPitchTiltAmount_, 0.01f, 0.0f, 1.5f, "%.3f");
    ImGui::DragFloat("A/D Roll Tilt Amount", &visualRollTiltAmount_, 0.01f, 0.0f, 1.5f, "%.3f");
    ImGui::DragFloat("Tilt Smooth Speed", &visualTiltSmoothSpeed_, 0.1f, 0.0f, 40.0f, "%.2f");
    if (ImGui::Button("傾きリセット (Reset Visual Tilt)")) {
        currentVisualTilt_ = { 0.0f, 0.0f, 0.0f };
    }
    ImGui::Text("Current Visual Tilt: %.3f, %.3f, %.3f",
        currentVisualTilt_.x,
        currentVisualTilt_.y,
        currentVisualTilt_.z);
    if (actionController_) {
        actionController_->DrawImGui();
    }

    ImGui::SeparatorText("Player Visibility Assist");
    ImGui::Checkbox("中心付近でPlayerを薄くする (Enable Player Center Fade)", &enableCenterFade_);
    ImGui::DragFloat("Center Fade Inner Radius", &centerFadeInnerRadius_, 0.005f, 0.0f, 1.0f, "%.3f");
    ImGui::DragFloat("Center Fade Outer Radius", &centerFadeOuterRadius_, 0.005f, 0.0f, 1.0f, "%.3f");
    centerFadeOuterRadius_ = (std::max)(centerFadeOuterRadius_, centerFadeInnerRadius_ + 0.001f);
    ImGui::DragFloat("Center Min Alpha", &centerFadeMinAlpha_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Center Fade Speed", &centerFadeSpeed_, 0.1f, 0.0f, 60.0f, "%.1f");
    ImGui::Text("Current Screen Distance: %.3f", currentScreenDistance_);
    ImGui::Text("Current Player Alpha: %.3f", currentPlayerAlpha_);
    ImGui::Text("Projection Valid: %s", centerProjectionValid_ ? "true" : "false");
    ImGui::Checkbox("FakeShadowにも反映 (Affect FakeShadow)", &affectCenterFadeToFakeShadow_);
    ImGui::Checkbox("Jet Exhaustにも反映 (Affect Jet Exhaust)", &affectCenterFadeToJetExhaust_);

    ImGui::DragFloat("Move Speed", &moveSpeed_, 0.05f, 0.0f, 30.0f, "%.2f");
    ImGui::DragFloat("Move Limit X", &moveLimitX_, 0.05f, 0.0f, 20.0f, "%.2f");
    ImGui::DragFloat("Move Limit Y", &moveLimitY_, 0.05f, 0.0f, 20.0f, "%.2f");
    ImGui::DragFloat("Distance From Camera", &distanceFromCamera_, 0.05f, 0.1f, 50.0f, "%.2f");
    ImGui::DragFloat("Event Trigger Radius", &eventTriggerRadius_, 0.01f, 0.0f, 10.0f, "%.2f");
    ImGui::SeparatorText("当たり判定 (Collision Radius)");
    ImGui::Checkbox("Show Player Hit Radius", &showHitRadius_);
    ImGui::DragFloat("Player Hit Radius", &hitRadius_, 0.01f, 0.0f, 10.0f, "%.2f");
    if (ImGui::Button("Hit Radius Small")) {
        hitRadius_ = 0.20f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Hit Radius Normal")) {
        hitRadius_ = 0.30f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Hit Radius Large")) {
        hitRadius_ = 0.45f;
    }
    ImGui::DragFloat2("Local Offset", &localOffsetX_, 0.03f, -50.0f, 50.0f, "%.2f");
    localOffsetX_ = std::clamp(localOffsetX_, -moveLimitX_, moveLimitX_);
    localOffsetY_ = std::clamp(localOffsetY_, -moveLimitY_, moveLimitY_);
    ImGui::DragFloat3("Model Scale", &modelScale_.x, 0.01f, 0.001f, 20.0f, "%.3f");
    if (ImGui::Button("Reset Position")) {
        ResetPosition();
    }

    UpdateWorldPosition();
    UpdateObjectTransform();
    if (object_) {
        object_->Update();
    }
    ImGui::Text("Base Position: %.3f, %.3f, %.3f", basePosition_.x, basePosition_.y, basePosition_.z);
    ImGui::Text("Base Forward: %.3f, %.3f, %.3f", baseForward_.x, baseForward_.y, baseForward_.z);
    ImGui::Text("World Position: %.3f, %.3f, %.3f", worldPosition_.x, worldPosition_.y, worldPosition_.z);
    ImGui::Text("Visual Base Rotation: %.3f, %.3f, %.3f",
        visualBaseRotation_.x,
        visualBaseRotation_.y,
        visualBaseRotation_.z);
    ImGui::Text("Visual Model Rotation: %.3f, %.3f, %.3f",
        visualFinalRotation_.x,
        visualFinalRotation_.y,
        visualFinalRotation_.z);
    ImGui::End();
#endif
}

void Player::SetGameViewInputActive(bool isActive) {
    gameViewInputActive_ = isActive;
}

void Player::SetActionDebugVisualsEnabled(bool isEnabled) {
    if (actionController_) {
        actionController_->SetDebugVisualsEnabled(isEnabled);
    }
}

void Player::SetBarrelRollDependencies(
    EnemyBulletManager* enemyBulletManager,
    CombatEffectController* combatEffectController) {
    if (actionController_) {
        actionController_->SetDependencies(enemyBulletManager, combatEffectController);
    }
}

void Player::SetBarrelRollEffectControllers(
    PlayerBarrelRollRingController* rollRingController,
    PlayerBulletCancelEffectController* bulletCancelEffectController) {
    if (actionController_) {
        actionController_->SetEffectControllers(rollRingController, bulletCancelEffectController);
    }
}

void Player::SetBarrelRollSlowMotionController(CombatSlowMotionController* slowMotionController) {
    if (actionController_) {
        actionController_->SetSlowMotionController(slowMotionController);
    }
}

void Player::SetDamageFeedbackAlpha(float alpha) {
    damageFeedbackAlpha_ = std::clamp(alpha, 0.0f, 1.0f);
    ApplyModelAlpha(currentPlayerAlpha_ * damageFeedbackAlpha_);
}

void Player::SetBaseMode(BaseMode baseMode) {
    baseMode_ = baseMode;
}

void Player::SetExternalBasePosition(const Vector3& position) {
    externalBasePosition_ = position;
    hasExternalBase_ = true;
}

void Player::SetExternalBaseForward(const Vector3& forward) {
    externalBaseForward_ = forward;
}

void Player::SetExternalBaseUp(const Vector3& up) {
    externalBaseUp_ = up;
}

bool Player::UsesWASDInput() const {
    return enablePlayer_ && gameViewInputActive_;
}

bool Player::IsBarrelRolling() const {
    return actionController_ && actionController_->IsBarrelRolling();
}

bool Player::IsInvincible() const {
    return actionController_ && actionController_->IsInvincible();
}

bool Player::IsBarrelRollEffectEnabled() const {
    return actionController_ && actionController_->IsBarrelRollEffectEnabled();
}

float Player::GetDamageReduction() const {
    return actionController_ ? actionController_->GetDamageReduction() : 0.0f;
}

float Player::GetBarrelRollClearBulletRadius() const {
    return actionController_ ? actionController_->GetBarrelRollClearBulletRadius() : 0.0f;
}

void Player::LoadModel() {
    useFallbackModel_ = false;
    resolvedModelPath_ = ResolveResourcePath(modelPath_);
    texturePath_.clear();

    ModelManager* modelManager = ModelManager::GetInstance();
    if (useLightweightVisual_) {
        model_ = modelManager->CreateBox("PlayerLightweightBox");
        useFallbackModel_ = true;
        loadStatus_ = "Using lightweight player primitive.";
    } else if (!resolvedModelPath_.empty()) {
        modelManager->LoadModel(resolvedModelPath_);
        model_ = modelManager->FindModel(resolvedModelPath_);
        if (model_) {
            const std::filesystem::path modelPath(resolvedModelPath_);
            texturePath_ = ToGenericString(modelPath.parent_path() / "textures" / "material_0_baseColor.png");
            if (!std::filesystem::exists(std::filesystem::path(texturePath_))) {
                texturePath_ = ResolveResourcePath("resources/obj/axis/uvChecker.png");
            }
            if (!texturePath_.empty()) {
                TextureManager::GetInstance()->LoadTexture(texturePath_);
                model_->SetTextureIndex(TextureManager::GetInstance()->GetTextureIndexByFilePath(texturePath_));
            }
            loadStatus_ =
                !texturePath_.empty() && std::filesystem::path(texturePath_).filename().string() == "material_0_baseColor.png"
                ? "Player model loaded."
                : "Player model loaded. Texture missing, using fallback texture.";
        }
    }

    if (!model_) {
        model_ = modelManager->CreateBox("PlayerFallbackBox");
        useFallbackModel_ = true;
        loadStatus_ = "Player model missing. Using fallback box.";
    }

    if (object_) {
        object_->SetModel(model_);
    }
    ApplyModelAlpha(currentPlayerAlpha_ * damageFeedbackAlpha_);
}
