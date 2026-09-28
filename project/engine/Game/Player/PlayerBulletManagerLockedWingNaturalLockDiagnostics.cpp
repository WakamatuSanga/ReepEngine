#include "PlayerBulletManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"

namespace {
const char* BoolText(bool value) {
    return value ? "はい" : "いいえ";
}

bool IsKrakenTargetId(const std::string& targetId) {
    constexpr char kPrefix[] = "KrakenTentacleWeakPoint_";
    return targetId.compare(0, sizeof(kPrefix) - 1, kPrefix) == 0;
}
} // namespace
#endif

void PlayerBulletManager::
DrawKrakenNaturalLockHomingDiagnosticsImGui() const {
#ifdef USE_IMGUI
    ImGui::SeparatorText("クラーケン自然ロック追尾診断");
    const PlayerBulletInstance* selected = nullptr;
    for (const PlayerBulletInstance& instance : bullets_) {
        if (instance.projectileType != PlayerProjectileType::LockedWingShot ||
            !instance.lockedWingLaunch ||
            !IsKrakenTargetId(instance.lockedWingLaunch->lockedTargetId)) {
            continue;
        }
        selected = &instance;
        break;
    }

    const LockedWingLaunchState* state =
        selected ? selected->lockedWingLaunch.get() : nullptr;
    ImGui::Text("クラーケン対象ミサイル: %s", BoolText(state != nullptr));
    ImGui::Text("ミサイル実行識別子: %llu",
        static_cast<unsigned long long>(selected ? selected->runtimeId : 0));
    ImGui::TextWrapped("ミサイル対象識別子: %s",
        state ? state->lockedTargetId.c_str() : "なし");
    ImGui::Text("プロバイダー検索成功: %s",
        BoolText(state && state->targetQuerySucceeded));
    ImGui::Text("対象種別: %s",
        state ? "クラーケン触手の弱点" : "なし");
    if (state) {
        ImGui::Text("対象位置: %.3f, %.3f, %.3f",
            state->targetWorldPosition.x,
            state->targetWorldPosition.y,
            state->targetWorldPosition.z);
        ImGui::Text("対象スナップショット有効: %s",
            BoolText(
                state->targetQuerySucceeded && state->targetAlive &&
                !state->targetLost));
        ImGui::Text("追尾有効: %s", BoolText(state->homingEnabled));
        ImGui::Text("対象までの距離: %.3f", state->targetDistance);
        ImGui::Text("対象喪失: %s", BoolText(state->targetLost));
    } else {
        ImGui::Text("対象位置: 未取得");
        ImGui::Text("対象スナップショット有効: いいえ");
        ImGui::Text("追尾有効: いいえ");
        ImGui::Text("対象までの距離: 未取得");
        ImGui::Text("対象喪失: いいえ");
    }
#endif
}
