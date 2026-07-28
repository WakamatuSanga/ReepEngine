#include "LockedWingMissileExhaustController.h"

#include "Engine/Graphics/Particle/GpuParticleSystem.h"

#include <algorithm>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
const char* ToJapaneseSlotState(
    LockedWingMissileExhaustController::SlotState state) {
    switch (state) {
    case LockedWingMissileExhaustController::SlotState::Emitting:
        return "Emission中";
    case LockedWingMissileExhaustController::SlotState::Draining:
        return "自然消滅待ち";
    case LockedWingMissileExhaustController::SlotState::Free:
    default:
        return "空き";
    }
}

const char* ToJapaneseBool(bool value) {
    return value ? "はい" : "いいえ";
}
}

void LockedWingMissileExhaustController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SeparatorText("ミサイル噴射炎");
    ImGui::TextWrapped("専用JSON: %s", presetPath_.c_str());
    ImGui::TextWrapped("読込状態: %s", loadStatus_.c_str());
    ImGui::Text("GPU Particle初期化済み: %s", ToJapaneseBool(initialized_));

    ImGui::Text("JSONの基本Scale: %.3f（Player比）", jsonBaseScale_);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Missile Exhaust JSONに保存されたParticle Scaleを、\n"
            "Player Jet ExhaustのCore開始Scaleと比較した値です。");
    }
    ImGui::DragFloat(
        "実行時Scale倍率##MissileExhaustRuntimeScale",
        &runtimeScaleMultiplier_,
        0.01f,
        0.10f,
        3.0f,
        "%.2f");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "JSONの基本Scaleへ追加で掛ける倍率です。\n"
            "初期値は1.0で、JSON側の0.55を二重適用しません。");
    }
    ImGui::Text(
        "最終適用Scale: %.3f",
        jsonBaseScale_ * runtimeScaleMultiplier_);
    ImGui::DragFloat(
        "Exhaust後端オフセット##MissileExhaustRearOffset",
        &exhaustRearOffset_,
        0.01f,
        0.0f,
        2.0f,
        "%.2f");
    ImGui::SliderInt(
        "最大Emitter数##MissileExhaustMaxEmitters",
        &maxEmitterCount_,
        1,
        static_cast<int>(kHardSlotCapacity));
    ImGui::Text("1ミサイルあたりのEmitter数: %u", emittersPerSlot_);
    ImGui::Text("Particle最大Lifetime: %.3f 秒", maxParticleLifetime_);

    uint32_t freeCount = 0;
    uint32_t emittingCount = 0;
    uint32_t drainingCount = 0;
    const int allocationLimit =
        std::clamp(maxEmitterCount_, 1, static_cast<int>(kHardSlotCapacity));
    for (uint32_t index = 0; index < kHardSlotCapacity; ++index) {
        const Slot& slot = slots_[index];
        switch (slot.state) {
        case SlotState::Emitting:
            ++emittingCount;
            break;
        case SlotState::Draining:
            ++drainingCount;
            break;
        case SlotState::Free:
        default:
            if (static_cast<int>(index) < allocationLimit) {
                ++freeCount;
            }
            break;
        }
    }

    uint32_t lastSlotIndex = 0;
    const bool lastHandleValid = DecodeHandle(lastHandle_, lastSlotIndex);
    ImGui::SeparatorText("Emitter Slot状態");
    ImGui::Text("Emitter Handle有効: %s", ToJapaneseBool(lastHandleValid));
    ImGui::Text(
        "最後のHandle: %llu",
        static_cast<unsigned long long>(lastHandle_));
    ImGui::Text("最後のSlot状態: %s", ToJapaneseSlotState(lastSlotState_));
    ImGui::Text("Emission中: %u", emittingCount);
    ImGui::Text("自然消滅待ち: %u", drainingCount);
    ImGui::Text("現在上限内の空き: %u", freeCount);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "自然消滅待ちでは新しいParticleを生成せず、\n"
            "既に発生したParticleがLifetimeで消えるのを待ちます。");
    }
    ImGui::Text(
        "Exhaust位置: %.3f, %.3f, %.3f",
        lastExhaustPosition_.x,
        lastExhaustPosition_.y,
        lastExhaustPosition_.z);
    ImGui::Text(
        "Exhaust方向: %.3f, %.3f, %.3f",
        lastExhaustDirection_.x,
        lastExhaustDirection_.y,
        lastExhaustDirection_.z);
    ImGui::Text(
        "GPU上の推定Particle数: %u",
        particleSystem_ ? particleSystem_->GetActiveCountEstimate() : 0u);

    ImGui::SeparatorText("Emitter統計");
    ImGui::Text("Emitter取得成功数: %llu",
        static_cast<unsigned long long>(emitterAcquireSuccessCount_));
    ImGui::Text("Emitter不足数: %llu",
        static_cast<unsigned long long>(emitterShortageCount_));
    ImGui::Text("Emitter停止数: %llu",
        static_cast<unsigned long long>(emitterStopCount_));
    ImGui::Text("自然消滅待ち開始数: %llu",
        static_cast<unsigned long long>(drainingStartCount_));
    ImGui::Text("Slot解放数: %llu",
        static_cast<unsigned long long>(slotReleaseCount_));
    ImGui::Text("二重点火防止数: %llu",
        static_cast<unsigned long long>(doubleStartPreventionCount_));
    ImGui::Text("二重解放防止数: %llu",
        static_cast<unsigned long long>(doubleReleasePreventionCount_));
    ImGui::Text("不正Transform停止数: %llu",
        static_cast<unsigned long long>(invalidTransformStopCount_));

    if (ImGui::Button(
            "Missile Exhaust JSONを再読込##ReloadMissileExhaustJson")) {
        Reset(true);
        initialized_ = LoadPreset();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "専用JSONを再読込し、Emitter Poolを即時Clearします。");
    }
    if (ImGui::Button(
            "Emitter Poolを即時リセット##ResetMissileExhaustPool")) {
        Reset(true);
        loadStatus_ = "Emitter Poolを即時リセットしました";
    }
#endif
}