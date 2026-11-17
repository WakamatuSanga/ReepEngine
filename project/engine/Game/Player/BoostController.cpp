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

void BoostController::BeginFrame(float unscaledDeltaTime, bool ordinaryGameplay, float realDeltaTime) {
    const float realDt = (std::max)(0.0f, std::isfinite(realDeltaTime) && realDeltaTime >= 0.0f
        ? realDeltaTime : unscaledDeltaTime);
    lastInputTriggered_ = lastCanBoost_ = false;
    boostFrameFraction_ = evadeSweepFraction_ = movementBlend_ = 0.0f;
    if (!ordinaryGameplay || !enableBoost_) { Cancel(); return; }
    if (state_ == State::Cooldown) {
        boostCooldownTimer_ = (std::max)(0.0f, boostCooldownTimer_ - realDt);
        if (boostCooldownTimer_ == 0) state_ = State::Idle;
    }
    Input* input = MyGame::GetInstance()->GetInput();
    if (gameViewInputActive_ && input) {
        lastInputTriggered_ = input->TriggerKey(ResolveInputKey());
        lastCanBoost_ = state_ == State::Idle && boostCooldownTimer_ <= 0;
        if (lastInputTriggered_ && lastCanBoost_) StartBoost(false);
    }
    if (!IsBoosting()) return;

    const auto& tuning = FlightTuning::Get();
    const float remaining = boostDuration_ - boostTimer_;
    const bool finishes = realDt >= remaining - 0.000001f;
    const float step = (std::min)(realDt, remaining);
    const float previous = SmoothStep(boostTimer_ / boostDuration_);
    boostTimer_ = finishes ? boostDuration_ : boostTimer_ + step;
    const float u = boostTimer_ / boostDuration_;
    boostFrameFraction_ = realDt > 0.0f ? step / realDt : 1.0f;
    movementBlend_ = finishes ? 1.0f
        : Saturate((SmoothStep(u) - previous) / (std::max)(0.000001f, 1.0f - previous));
    steeringBlend_ = Saturate(step / .055f) * (1.0f - SmoothStep((u - .55f) / .30f));
    if (finishes) {
        boostCooldownTimer_ = (std::max)(0.0f, boostCooldown_ - (realDt - step));
        state_ = boostCooldownTimer_ > 0.0f ? State::Cooldown : State::Idle;
        ClearActiveEffects();
        return;
    }

    constexpr float pi = 3.14159265358979323846f;
    const float sine = std::sin(pi * u);
    currentBoostPower_ = sine * sine;
    currentTimeScale_ = tuning.boostTimeScale;
    if (player_) {
        const float depth = player_->GetGameplayReferenceDistance() * tuning.depthShrink * currentBoostPower_;
        const float wave = std::sin(2.0f * pi * u);
        player_->SetBoostTrajectory(currentBoostPower_,
            { -startSide_ * tuning.orbitExtra, -tuning.orbitVertical * wave }, depth,
            { .08f * wave * currentBoostPower_, -startSide_ * .20f * currentBoostPower_, 0 },
            startSide_ * 2.0f * pi * SmoothStep(u));
    }
    currentCloudBoostExtraSpeed_ = std::clamp(maxCloudBoostExtraSpeed_, 0.0f, 30.0f) * currentBoostPower_;
    targetCloudBoostExtraSpeed_ = currentCloudBoostExtraSpeed_;
    const float baseCloudSpeed = cloudPass_ ? cloudPass_->GetCloudBaseFlowSpeed() : 0.0f;
    currentCloudFlowMultiplier_ = baseCloudSpeed > .0001f
        ? 1.0f + currentCloudBoostExtraSpeed_ / baseCloudSpeed : 1.0f;
    PushMultiplierToCloud();
}

float BoostController::ResolveGameplayDeltaTime(float deltaTime, float combatScale) {
    const float slowed = deltaTime * boostFrameFraction_ * (std::min)(combatScale, FlightTuning::Get().boostTimeScale);
    const float total = slowed + GetNormalMovementDeltaTime(deltaTime, combatScale);
    evadeSweepFraction_ = total > 0.0f ? slowed / total : boostFrameFraction_;
    return total;
}

float BoostController::GetNormalMovementDeltaTime(float deltaTime, float combatScale) const {
    return deltaTime * (1.0f - boostFrameFraction_) * combatScale;
}

Vector2 BoostController::MovePlayer(Vector2 position, Vector2 input, Vector2 limits) {
    const auto& tuning = FlightTuning::Get();
    const bool hasInput = input.x != 0.0f || input.y != 0.0f;
    const Vector2 direction = hasInput ? input : Vector2{-startSide_, 0.0f};
    if (!steeringInitialized_) {
        steeringX_ = direction.x;
        steeringY_ = direction.y;
        steeringInitialized_ = true;
    } else if (hasInput) {
        // Finish along the chosen line so a last-frame reversal cannot move the landing point abruptly.
        steeringX_ = std::lerp(steeringX_, direction.x, steeringBlend_);
        steeringY_ = std::lerp(steeringY_, direction.y, steeringBlend_);
    }
    const Vector2 landing{
        std::clamp(startX_ + steeringX_ * (.40f + tuning.orbitExtra), -1.0f, 1.0f) * limits.x,
        std::clamp(startY_ + steeringY_ * (.30f + tuning.orbitVertical), -1.0f, 1.0f) * limits.y
    };
    return {std::lerp(position.x, landing.x, movementBlend_), std::lerp(position.y, landing.y, movementBlend_)};
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
        ImGui::Text("状態: %s / 実時間経過 %.2f / 共通時間倍率 %.3f", GetStateName(), boostTimer_, currentTimeScale_);
        ImGui::Text("開始側 %.0f / 開始横位置 %.2f / 強度 %.2f", startSide_, startX_, currentBoostPower_);
        ImGui::Text("雲の基準 %.1f / 追加 %.1f / 実流速 %.1f", cloudPass_ ? cloudPass_->GetCloudBaseFlowSpeed() : 0,
            currentCloudBoostExtraSpeed_, cloudPass_ ? cloudPass_->GetCurrentCloudFlowSpeed() : 0);
        DrawEvadeSettingsImGui();
        ImGui::SliderFloat("雲Boost追加速度", &maxCloudBoostExtraSpeed_, 0, 30);
        ImGui::Text("軌道・時間倍率の設定は飛行調整へ。");
        if (ImGui::Button("テスト発動")) StartBoost(true);
        ImGui::SameLine(); if (ImGui::Button("中断")) Cancel();
    }
    DebugUiWorkspace::EndPanel();
#endif
}

void BoostController::DrawEvadeSettingsImGui() {
#ifdef USE_IMGUI
    ImGui::Text("回避キー: %s / %s / 待ち %.2f 実秒", GetInputKeyName(),
        IsEvadeActive() ? "回避中" : IsCoolingDown() ? "回復待ち" : "READY", GetCooldownRemaining());
    ImGui::SliderFloat("再発動待ち(実秒)", &boostCooldown_, 0, 5);
    ImGui::TextWrapped("初期案は終了から3実秒。ワールドのスローでは遅くなりません。");
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
    startY_ = player_ ? std::clamp(player_->GetLocalOffsetY() / (std::max)(player_->GetMoveLimitY(), .001f), -1.0f, 1.0f) : 0;
    steeringInitialized_ = false;
    startSide_ = startX_ < 0 ? -1.0f : 1.0f;
    state_ = State::Boosting;
    boostTimer_ = 0.0f;
    boostCooldownTimer_ = 0.0f;
    currentBoostPower_ = 0.0f;
    ++boostCount_;
    inputBlockedReason_ = "Boost started";
}

void BoostController::ResetBoost() {
    state_ = State::Idle;
    boostCooldownTimer_ = 0.0f;
    boostFrameFraction_ = evadeSweepFraction_ = movementBlend_ = 0.0f;
    ClearActiveEffects();
    inputBlockedReason_ = "Reset";
}

void BoostController::ClearActiveEffects() {
    currentTimeScale_ = 1.0f;
    boostTimer_ = 0.0f;
    currentBoostPower_ = 0.0f;
    currentCloudFlowMultiplier_ = 1.0f;
    currentCloudBoostExtraSpeed_ = 0.0f;
    targetCloudBoostExtraSpeed_ = 0.0f;
    if (player_) player_->SetBoostTrajectory(0, {}, 0, {});
    if (postEffectController_) postEffectController_->ClearBoostEffects();
    PushMultiplierToCloud();
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

