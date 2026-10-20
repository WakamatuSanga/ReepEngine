#include "Engine/Core/DebugUiWorkspace.h"
#include "BoostController.h"
#include "FlightTuning.h"

#include "Engine/Game/Effect/PostEffectController.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Cloud/VolumetricCloudPass.h"
#include "Player.h"
#include "Engine/Input/Input.h"
#include "MyGame.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

BoostController::BoostController() = default;

BoostController::~BoostController() = default;

void BoostController::Initialize(VolumetricCloudPass* cloudPass) {
    cloudPass_ = cloudPass;
    currentCloudFlowMultiplier_ = 1.0f;
    targetCloudFlowMultiplier_ = 1.0f;
    currentCloudBoostExtraSpeed_ = 0.0f;
    targetCloudBoostExtraSpeed_ = 0.0f;
    PushMultiplierToCloud();
}

void BoostController::SetPostEffectContext(Player* player, Camera* camera, PostEffectController* postEffectController) {
    player_ = player;
    camera_ = camera;
    postEffectController_ = postEffectController;
}

void BoostController::Finalize() {
    ResetBoost();
    PushMultiplierToCloud();
    cloudPass_ = nullptr;
}

void BoostController::BeginFrame(float unscaledDeltaTime, bool ordinaryGameplay) {
    const float realDt = std::clamp(unscaledDeltaTime, 0.0f, 1.0f / 15.0f);
    lastInputTriggered_ = lastCanBoost_ = false;
    if (!ordinaryGameplay || !enableBoost_) { Cancel(); return; }
    Input* input = MyGame::GetInstance()->GetInput();
    if (gameViewInputActive_ && input) {
        lastInputTriggered_ = input->TriggerKey(ResolveInputKey());
        lastCanBoost_ = state_ == State::Idle && boostCooldownTimer_ <= 0;
        if (lastInputTriggered_ && lastCanBoost_) StartBoost(false);
    }
    const auto& tuning = FlightTuning::Get();
    const float target = IsBoosting() ? tuning.boostTimeScale : 1.0f;
    currentTimeScale_ += (target - currentTimeScale_) * (1.0f - std::exp(-realDt / tuning.timeResponse));
    if (!IsBoosting() && std::fabs(currentTimeScale_ - 1) < 0.0001f) currentTimeScale_ = 1;
}

void BoostController::Update(float deltaTime) {
    const float dt = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    const auto& tuning = FlightTuning::Get();
    boostDuration_ = tuning.boostDuration;
    if (player_) player_->SetBoostTrajectory(0, {}, 0, {});
    if (IsBoosting()) {
        boostTimer_ += dt;
        const float u = std::clamp(boostTimer_ / boostDuration_, 0.0f, 1.0f);
        constexpr float pi = 3.14159265358979323846f;
        const float sine = std::sin(pi * u);
        const float envelope = sine * sine;
        currentBoostPower_ = Saturate((std::min)(SmoothStep(boostTimer_ / .10f), SmoothStep((boostDuration_ - boostTimer_) / .30f)));
        const float wave = std::sin(2 * pi * u);
        if (player_) {
            const float distance = player_->GetGameplayReferenceDistance();
            const float depth = distance * tuning.depthShrink / (1 - tuning.depthShrink) * envelope;
            player_->SetBoostTrajectory(envelope,
                { -startSide_ * (std::fabs(startX_) + tuning.orbitExtra), -tuning.orbitVertical * wave }, depth,
                { .08f * wave * envelope, -startSide_ * .20f * envelope, startSide_ * .25f * envelope },
                startSide_ * 2.0f * pi * u);
        }
        if (u >= 1) {
            state_ = boostCooldown_ > 0 ? State::Cooldown : State::Idle;
            boostTimer_ = 0; boostCooldownTimer_ = boostCooldown_; currentBoostPower_ = 0;
            if (player_) player_->SetBoostTrajectory(0, {}, 0, {});
        }
    } else if (state_ == State::Cooldown) {
        boostCooldownTimer_ = (std::max)(0.0f, boostCooldownTimer_ - dt);
        if (boostCooldownTimer_ == 0) state_ = State::Idle;
    }
    UpdateCloudFlowMultiplier(dt);
    PushMultiplierToCloud();
}
void BoostController::RefreshPresentation() { UpdateBoostPostEffect(); }
void BoostController::Cancel() { ResetBoost(); PushMultiplierToCloud(); }

void BoostController::DrawImGui() {
#ifdef USE_IMGUI
    if (DebugUiWorkspace::BeginPanel("ブースト確認 (Boost Debug)")) {
        ImGui::Checkbox("ブーストを使う", &enableBoost_);
        int key = static_cast<int>(boostInputKey_);
        const char* keys[] = { "左Shift", "右Shift", "B" };
        if (ImGui::Combo("発動キー", &key, keys, 3)) boostInputKey_ = static_cast<InputKey>(key);
        ImGui::Text("状態: %s / ゲーム経過 %.2f / 共通時間倍率 %.3f", GetStateName(), boostTimer_, currentTimeScale_);
        ImGui::Text("開始側 %.0f / 開始横位置 %.2f / 強度 %.2f", startSide_, startX_, currentBoostPower_);
        ImGui::Text("雲の基準 %.1f / 追加 %.1f / 実流速 %.1f", cloudPass_ ? cloudPass_->GetCloudBaseFlowSpeed() : 0,
            currentCloudBoostExtraSpeed_, cloudPass_ ? cloudPass_->GetCurrentCloudFlowSpeed() : 0);
        ImGui::SliderFloat("再発動待ち(ゲーム秒)", &boostCooldown_, 0, 5);
        ImGui::SliderFloat("雲Boost追加速度", &maxCloudBoostExtraSpeed_, 0, 30);
        ImGui::Text("軌道・時間倍率の設定は飛行調整へ。");
        if (ImGui::Button("テスト発動")) StartBoost(true);
        ImGui::SameLine(); if (ImGui::Button("中断")) Cancel();
    }
    DebugUiWorkspace::EndPanel();
#endif
}

float BoostController::GetCloudFlowMultiplier() const {
    return currentCloudFlowMultiplier_;
}

float BoostController::GetBoostGaugeRate() const {
    if (state_ == State::Boosting) {
        const float safeDuration = (std::max)(boostDuration_, 0.001f);
        return 1.0f - Saturate(boostTimer_ / safeDuration);
    }
    if (state_ == State::Cooldown) {
        const float safeCooldown = (std::max)(boostCooldown_, 0.001f);
        return 1.0f - Saturate(boostCooldownTimer_ / safeCooldown);
    }
    return 1.0f;
}
float BoostController::GetVisualSpeedMultiplier() const {
    return 1.0f + (std::max)(0.0f, boostVisualSpeedMultiplier_ - 1.0f) * currentBoostPower_;
}

void BoostController::StartBoost(bool ignoreCooldown) {
    if (!enableBoost_ && !ignoreCooldown) {
        inputBlockedReason_ = "Boost disabled";
        return;
    }
    if (!ignoreCooldown && (state_ != State::Idle || boostCooldownTimer_ > 0.0f)) {
        inputBlockedReason_ = "Boost cooldown or already boosting";
        return;
    }

    boostDuration_ = FlightTuning::Get().boostDuration;
    startX_ = player_ ? std::clamp(player_->GetLocalOffsetX() / (std::max)(player_->GetMoveLimitX(), .001f), -1.0f, 1.0f) : 0;
    startSide_ = startX_ < 0 ? -1.0f : 1.0f;
    state_ = State::Boosting;
    boostTimer_ = 0.0f;
    boostCooldownTimer_ = 0.0f;
    currentBoostPower_ = 0.0f;
    ++boostCount_;
    inputBlockedReason_ = "Boost started";
}

void BoostController::ResetBoost() {
    currentTimeScale_ = 1;
    if (player_) player_->ClearFlightTransient();
    if (postEffectController_) postEffectController_->ClearBoostEffects();
    state_ = State::Idle;
    boostTimer_ = 0.0f;
    boostCooldownTimer_ = 0.0f;
    currentBoostPower_ = 0.0f;
    currentCloudFlowMultiplier_ = 1.0f;
    targetCloudFlowMultiplier_ = 1.0f;
    currentCloudBoostExtraSpeed_ = 0.0f;
    targetCloudBoostExtraSpeed_ = 0.0f;
    UpdateBoostPostEffect();
    inputBlockedReason_ = "Reset";
}

void BoostController::UpdateCloudFlowMultiplier(float deltaTime) {
    const float baseCloudSpeed = cloudPass_ ? (std::max)(cloudPass_->GetCloudBaseFlowSpeed(), 0.0f) : 0.0f;
    const float clampedExtraSpeed = std::clamp(maxCloudBoostExtraSpeed_, 0.0f, 30.0f);
    targetCloudBoostExtraSpeed_ = clampedExtraSpeed * Saturate(currentBoostPower_);
    const float smoothSpeed = (std::max)(cloudBoostMultiplierSmoothSpeed_, 0.0f);
    const float t = smoothSpeed <= 0.0f ? 1.0f : 1.0f - std::exp(-std::clamp(deltaTime, 0.0f, 1.0f / 15.0f) * smoothSpeed);
    currentCloudBoostExtraSpeed_ += (targetCloudBoostExtraSpeed_ - currentCloudBoostExtraSpeed_) * std::clamp(t, 0.0f, 1.0f);
    if (!std::isfinite(currentCloudBoostExtraSpeed_)) {
        currentCloudBoostExtraSpeed_ = 0.0f;
    }
    currentCloudBoostExtraSpeed_ = std::clamp(currentCloudBoostExtraSpeed_, 0.0f, 30.0f);

    targetCloudFlowMultiplier_ = baseCloudSpeed > 0.0001f ? (baseCloudSpeed + targetCloudBoostExtraSpeed_) / baseCloudSpeed : 1.0f;
    currentCloudFlowMultiplier_ = baseCloudSpeed > 0.0001f ? (baseCloudSpeed + currentCloudBoostExtraSpeed_) / baseCloudSpeed : 1.0f;
}

void BoostController::UpdateBoostPostEffect() {
    if (!postEffectController_) {
        return;
    }
    float centerX = 0.5f;
    float centerY = 0.5f;
    const bool centerValid = ProjectPlayerToScreen(centerX, centerY);
    lastBoostEffectCenterX_ = centerX;
    lastBoostEffectCenterY_ = centerY;
    lastBoostEffectCenterValid_ = centerValid;
    postEffectController_->SetBoostPostEffectTarget(currentBoostPower_, centerX, centerY, centerValid);
}

bool BoostController::ProjectPlayerToScreen(float& outX, float& outY) const {
    outX = 0.5f;
    outY = 0.5f;
    if (!player_ || !camera_) {
        return false;
    }
    const Vector3& p = player_->GetWorldPosition();
    const Matrix4x4& vp = camera_->GetViewProjectionMatrix();
    const float clipX = p.x * vp.m[0][0] + p.y * vp.m[1][0] + p.z * vp.m[2][0] + vp.m[3][0];
    const float clipY = p.x * vp.m[0][1] + p.y * vp.m[1][1] + p.z * vp.m[2][1] + vp.m[3][1];
    const float clipW = p.x * vp.m[0][3] + p.y * vp.m[1][3] + p.z * vp.m[2][3] + vp.m[3][3];
    if (clipW <= 0.0001f || !std::isfinite(clipW)) {
        return false;
    }
    const float ndcX = clipX / clipW;
    const float ndcY = clipY / clipW;
    if (!std::isfinite(ndcX) || !std::isfinite(ndcY)) {
        return false;
    }
    outX = std::clamp(ndcX * 0.5f + 0.5f, 0.0f, 1.0f);
    outY = std::clamp(-ndcY * 0.5f + 0.5f, 0.0f, 1.0f);
    return ndcX >= -1.25f && ndcX <= 1.25f && ndcY >= -1.25f && ndcY <= 1.25f;
}

void BoostController::PushMultiplierToCloud() {
    if (cloudPass_) {
        cloudPass_->SetExternalFlowMultiplier(1.0f);
        cloudPass_->SetExternalBoostExtraFlowSpeed(currentCloudBoostExtraSpeed_);
    }
}

unsigned char BoostController::ResolveInputKey() const {
    switch (boostInputKey_) {
    case InputKey::RightShift:
        return DIK_RSHIFT;
    case InputKey::B:
        return DIK_B;
    case InputKey::LeftShift:
    default:
        return DIK_LSHIFT;
    }
}

const char* BoostController::GetStateName() const {
    switch (state_) {
    case State::Boosting:
        return "Boosting";
    case State::Cooldown:
        return "Cooldown";
    case State::Idle:
    default:
        return "Idle";
    }
}

const char* BoostController::GetInputKeyName() const {
    switch (boostInputKey_) {
    case InputKey::RightShift:
        return "Right Shift";
    case InputKey::B:
        return "B";
    case InputKey::LeftShift:
    default:
        return "Left Shift";
    }
}

float BoostController::SmoothStep(float value) {
    const float t = Saturate(value);
    return t * t * (3.0f - 2.0f * t);
}

float BoostController::Saturate(float value) {
    if (!std::isfinite(value)) {
        return 0.0f;
    }
    return std::clamp(value, 0.0f, 1.0f);
}

