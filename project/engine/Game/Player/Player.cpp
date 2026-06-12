#include "Player.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Input/Input.h"
#include "MyGame.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>

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

    Vector3 GetCameraRight(const Camera& camera) {
        const Matrix4x4& matrix = camera.GetWorldMatrix();
        return Normalize({ matrix.m[0][0], matrix.m[0][1], matrix.m[0][2] }, { 1.0f, 0.0f, 0.0f });
    }

    Vector3 GetCameraUp(const Camera& camera) {
        const Matrix4x4& matrix = camera.GetWorldMatrix();
        return Normalize({ matrix.m[1][0], matrix.m[1][1], matrix.m[1][2] }, { 0.0f, 1.0f, 0.0f });
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

Player::Player() = default;

Player::~Player() = default;

void Player::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    object3dCommon_ = object3dCommon;
    camera_ = camera;
    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon_);
    object_->SetCamera(camera_);
    object_->SetEnvironmentMapEnabled(false);
    LoadModel();
    ResetPosition();
    UpdateWorldPosition();
    UpdateObjectTransform();
    object_->Update();
}

void Player::Finalize() {
    object_.reset();
    model_ = nullptr;
}

void Player::Update(float deltaTime) {
    ++updateCount_;
    lastWPressed_ = false;
    lastAPressed_ = false;
    lastSPressed_ = false;
    lastDPressed_ = false;
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

    const bool isEditingImGuiInput = IsEditingImGuiInput();
    if (!enablePlayer_) {
        inputBlockedReason_ = "Player disabled";
    } else if (!gameViewInputActive_) {
        inputBlockedReason_ = "Game View is not hovered/focused";
    } else if (isEditingImGuiInput) {
        inputBlockedReason_ = "ImGui item or text input is active";
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
    UpdateObjectTransform();
    object_->Update();
}

void Player::Draw() {
    if (!enablePlayer_ || !showPlayer_ || !object3dCommon_ || !object_ || !model_) {
        return;
    }

    object3dCommon_->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    object_->Draw();
}

void Player::DrawImGui() {
#ifdef _DEBUG
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
    ImGui::SeparatorText("入力診断 (Input Diagnostics)");
    ImGui::Text("Update Called: yes (%llu)", static_cast<unsigned long long>(updateCount_));
    ImGui::TextWrapped("Input Blocked Reason: %s", inputBlockedReason_.c_str());
    ImGui::Text("Game View Input Active: %s", gameViewInputActive_ ? "true" : "false");
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
    ImGui::Text("推奨Scale: 0.45 / 推奨Distance From Camera: 4.0");
    if (ImGui::Button("Rail Shooter Preset")) {
        distanceFromCamera_ = 4.0f;
        modelScale_ = { 0.45f, 0.45f, 0.45f };
        modelRotation_ = { 0.0f, 3.14159265f, 0.0f };
        ResetPosition();
        UpdateWorldPosition();
        UpdateObjectTransform();
        if (object_) {
            object_->Update();
        }
    }

    ImGui::DragFloat("Move Speed", &moveSpeed_, 0.05f, 0.0f, 30.0f, "%.2f");
    ImGui::DragFloat("Move Limit X", &moveLimitX_, 0.05f, 0.0f, 20.0f, "%.2f");
    ImGui::DragFloat("Move Limit Y", &moveLimitY_, 0.05f, 0.0f, 20.0f, "%.2f");
    ImGui::DragFloat("Distance From Camera", &distanceFromCamera_, 0.05f, 0.1f, 50.0f, "%.2f");
    ImGui::DragFloat("Event Trigger Radius", &eventTriggerRadius_, 0.01f, 0.0f, 10.0f, "%.2f");
    ImGui::DragFloat2("Local Offset", &localOffsetX_, 0.03f, -50.0f, 50.0f, "%.2f");
    localOffsetX_ = std::clamp(localOffsetX_, -moveLimitX_, moveLimitX_);
    localOffsetY_ = std::clamp(localOffsetY_, -moveLimitY_, moveLimitY_);
    ImGui::DragFloat3("Model Scale", &modelScale_.x, 0.01f, 0.001f, 20.0f, "%.3f");
    ImGui::DragFloat3("Model Rotation", &modelRotation_.x, 0.01f, -6.28318f, 6.28318f, "%.3f");
    if (ImGui::Button("Reset Position")) {
        ResetPosition();
    }

    UpdateWorldPosition();
    ImGui::Text("Base Position: %.3f, %.3f, %.3f", basePosition_.x, basePosition_.y, basePosition_.z);
    ImGui::Text("Base Forward: %.3f, %.3f, %.3f", baseForward_.x, baseForward_.y, baseForward_.z);
    ImGui::Text("World Position: %.3f, %.3f, %.3f", worldPosition_.x, worldPosition_.y, worldPosition_.z);
    ImGui::End();
#endif
}

void Player::SetGameViewInputActive(bool isActive) {
    gameViewInputActive_ = isActive;
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

void Player::LoadModel() {
    useFallbackModel_ = false;
    resolvedModelPath_ = ResolveResourcePath(modelPath_);
    texturePath_.clear();

    ModelManager* modelManager = ModelManager::GetInstance();
    if (!resolvedModelPath_.empty()) {
        modelManager->LoadModel(resolvedModelPath_);
        model_ = modelManager->FindModel(resolvedModelPath_);
        if (model_) {
            const std::filesystem::path modelPath(resolvedModelPath_);
            texturePath_ = ToGenericString(modelPath.parent_path() / "player.png");
            if (!std::filesystem::exists(std::filesystem::path(texturePath_))) {
                texturePath_ = ResolveResourcePath("resources/obj/axis/uvChecker.png");
            }
            if (!texturePath_.empty()) {
                TextureManager::GetInstance()->LoadTexture(texturePath_);
                model_->SetTextureIndex(TextureManager::GetInstance()->GetTextureIndexByFilePath(texturePath_));
            }
            loadStatus_ =
                !texturePath_.empty() && std::filesystem::path(texturePath_).filename().string() == "player.png"
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
}

void Player::ResetPosition() {
    localOffsetX_ = 0.0f;
    localOffsetY_ = 0.0f;
}

void Player::UpdateWorldPosition() {
    if (!camera_) {
        return;
    }

    const Vector3 cameraPosition = camera_->GetTranslate();
    const Vector3 cameraRight = GetCameraRight(*camera_);
    const Vector3 cameraUp = GetCameraUp(*camera_);
    const Vector3 cameraForward = GetCameraForward(*camera_);

    if (baseMode_ == BaseMode::Rail && hasExternalBase_) {
        basePosition_ = externalBasePosition_;
        baseForward_ = Normalize(externalBaseForward_, cameraForward);
    } else {
        basePosition_ = AddVector3(cameraPosition, ScaleVector3(cameraForward, distanceFromCamera_));
        baseForward_ = cameraForward;
    }

    worldPosition_ = AddVector3(
        AddVector3(basePosition_, ScaleVector3(cameraRight, localOffsetX_)),
        ScaleVector3(cameraUp, localOffsetY_));
}

void Player::UpdateObjectTransform() {
    if (!object_) {
        return;
    }

    object_->SetTranslate(worldPosition_);
    object_->SetRotate(modelRotation_);
    object_->SetScale(modelScale_);
}
