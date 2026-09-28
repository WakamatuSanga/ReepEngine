#include "KrakenTentacleWaveEncounterController.h"

#include "KrakenTentacleMidbossController.h"
#include "KrakenTentacleWaveEncounterConfig.h"
#include "Engine/Game/Camera/CameraShakeController.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void KrakenTentacleWaveEncounterController::DrawEntranceSettingsImGui() {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("登場演出##EntranceSettings")) {
        return;
    }
    ImGui::TextWrapped("変更は次の登場時に反映されます。再確認はRestartしてください。");
    const auto edit = [](const char* label, float& value, float speed, float maximum) {
        const float previous = value;
        if (ImGui::DragFloat(label, &value, speed, 0.0f, maximum, "%.3f",
                ImGuiSliderFlags_AlwaysClamp) &&
            (!std::isfinite(value) || value < 0.0f || value > maximum)) {
            value = previous;
        }
    };
    ImGui::SeparatorText("シェイク");
    edit("揺れる時間（秒）", entranceSettings_.shakeDuration, 0.01f, 10.0f);
    edit("揺れの強さ（ワールド単位）", entranceSettings_.shakeAmplitude, 0.005f, 5.0f);
    ImGui::SeparatorText("カメラ");
    edit("カメラ移行時間（秒）", entranceSettings_.cameraBlendDuration, 0.01f, 10.0f);
    ImGui::SeparatorText("せり上がり");
    edit("せり上がり時間（秒）", entranceSettings_.riseDuration, 0.01f, 10.0f);
    edit("せり上がり量（ワールド単位）", entranceSettings_.riseDistance, 0.1f, 100.0f);
    ImGui::TextWrapped("時間が0の段階は待ち時間なしで進みます。登場順序は変わりません。");
    if (ImGui::Button("登場演出を初期値に戻す")) {
        entranceSettings_ = {};
    }
#endif
}

void KrakenTentacleWaveEncounterController::ClearEntrance() {
    entranceShake_->Reset(camera_);
    entrancePhase_ = EntrancePhase::None;
    entranceElapsed_ = 0.0f;
}

void KrakenTentacleWaveEncounterController::UpdateEntrance(float gameplayDeltaTime) {
    switch (entrancePhase_) {
    case EntrancePhase::Shake:
        entranceShake_->UpdateAndApply(gameplayDeltaTime, camera_, true);
        if (!entranceShake_->IsPlaying()) {
            // Remove the shake before capturing the composition's base offset.
            entranceShake_->Reset(camera_);
            BeginFovOverride();
            if (!fovOverrideRequested_) {
                EnterError("登場用のカメラ構図補間を開始できませんでした。");
                return;
            }
            entrancePhase_ = EntrancePhase::CameraPullback;
        }
        return;
    case EntrancePhase::CameraPullback:
        if (!fovOverrideRequested_ || !fovOverrideActive_) {
            EnterError("登場用のカメラ構図補間が解除されました。");
            return;
        }
        // Wait for the existing SmoothStep blend to actually reach its target.
        if (fovBlendActive_ || fovBlendProgress_ < 1.0f) {
            return;
        }
        if (!kraken_->BeginEntranceForWaveEncounter(
                activeEntranceSettings_.riseDistance)) {
            ++spawnFailureCount_;
            EnterError("触手の出現演出を開始できませんでした。");
            return;
        }
        ++bossShowCount_;
        entranceElapsed_ = 0.0f;
        entrancePhase_ = EntrancePhase::Tentacles;
        return;
    case EntrancePhase::Tentacles: {
        entranceElapsed_ += std::clamp(gameplayDeltaTime, 0.0f, 0.1f);
        const float progress = activeEntranceSettings_.riseDuration > 0.0f
            ? std::clamp(entranceElapsed_ / activeEntranceSettings_.riseDuration, 0.0f, 1.0f)
            : 1.0f;
        if (!kraken_->UpdateEntranceForWaveEncounter(progress)) {
            ++spawnFailureCount_;
            EnterError("触手の出現演出を継続できませんでした。");
            return;
        }
        if (progress >= 1.0f) {
            BeginCombat();
        }
        return;
    }
    case EntrancePhase::None:
        EnterError("登場演出の段階が無効です。");
        return;
    }
}

bool KrakenTentacleWaveEncounterController::BeginCombat() {
    kraken_->SetAttackDamageEnabled(true);
    kraken_->SetProjectileDamageEnabled(true);
    if (!kraken_->IsAttackDamageEnabled() ||
        !kraken_->IsProjectileDamageEnabled()) {
        ++damageSetupFailureCount_;
        EnterError("中ボスDamageを有効化できませんでした。");
        return false;
    }
    ++damageEnableCount_;
    entrancePhase_ = EntrancePhase::None;
    attackTimer_ = 0.0f;
    currentAttackDelay_ = firstAttackDelay_;
    firstAttackPending_ = true;
    schedulerEnabled_ = true;
    state_ = KrakenTentacleWaveEncounterState::Active;
    return true;
}
