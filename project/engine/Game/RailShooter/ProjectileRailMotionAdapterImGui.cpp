#include "Engine/Core/DebugUiWorkspace.h"
#include "ProjectileRailMotionAdapter.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"

namespace {
void DrawVector3(const char* label, const Vector3& value) {
    ImGui::Text("%s: (%.3f, %.3f, %.3f)", label, value.x, value.y, value.z);
}

const char* ToJapaneseState(bool value) {
    return value ? "有効" : "無効";
}

}
#endif

void ProjectileRailMotionAdapter::DrawImGui(
    size_t playerActiveCount,
    size_t enemyActiveCount) {
#ifdef USE_IMGUI
    if (!DebugUiWorkspace::BeginPanel(
            "レール弾道整合性デバッグ###ProjectileRailMotionDebug")) {
        DebugUiWorkspace::EndPanel();
        return;
    }

    const auto drawKindDiagnostics = [](
        const char* heading,
        const KindDiagnostics& diagnostics,
        size_t activeCount,
        const char* muzzleLabel,
        const char* targetLabel) {
        ImGui::SeparatorText(heading);
        ImGui::Text("有効弾数: %llu", static_cast<unsigned long long>(activeCount));
        DrawVector3(muzzleLabel, diagnostics.lastMuzzleWorldPosition);
        DrawVector3(targetLabel, diagnostics.lastTargetWorldPosition);
        DrawVector3("最後の発射方向", diagnostics.lastShotDirection);
        DrawVector3("最後のワールド速度", diagnostics.lastWorldVelocity);
        ImGui::Text(
            "発射数: %llu",
            static_cast<unsigned long long>(diagnostics.shotCount));
        ImGui::Text("最後の消滅理由: %s", diagnostics.lastDespawnReason.c_str());
    };

    ImGui::SeparatorText("共通状態");
    ImGui::Text("Rail Frame有効: %s", ToJapaneseState(currentFrame_.valid));
    ImGui::Text("Rail走行中: %s", ToJapaneseState(currentFrame_.running));
    ImGui::Text(
        "Rail実装: %s",
        currentFrame_.runtimeV2Active ? "Runtime V2" : "Legacy");
    ImGui::Text(
        "Rail Revision: %llu",
        static_cast<unsigned long long>(currentFrame_.revision));
    ImGui::Text(
        "Rail継続性Revision: %llu",
        static_cast<unsigned long long>(currentFrame_.continuityRevision));
    ImGui::Text("Rail Index: %d", currentFrame_.railIndex);
    ImGui::Text("Rail距離: %.3f", currentFrame_.railDistance);
    DrawVector3("前フレームRail位置", previousFrame_.position);
    DrawVector3("現在Rail位置", currentFrame_.position);
    ImGui::Text("Frame移動量: %.3f", frameTranslationDistance_);
    ImGui::Text("Frame回転量: %.3f度", frameRotationDegrees_);
    ImGui::Text("Frame再同期回数: %llu", static_cast<unsigned long long>(frameResyncCount_));
    ImGui::TextUnformatted("ロックオン弾の発進演出中のみレールの動きを反映します。");

    drawKindDiagnostics(
        "Player弾",
        playerDiagnostics_,
        playerActiveCount,
        "最後のMuzzle World位置",
        "最後のTarget World位置");
    drawKindDiagnostics(
        "Enemy弾",
        enemyDiagnostics_,
        enemyActiveCount,
        "最後のEnemy Muzzle位置",
        "最後のPlayer Target位置");

    if (ImGui::Button("Frame診断をリセット##ResetFrameDiagnostics")) {
        ResetDiagnostics();
    }

    DebugUiWorkspace::EndPanel();
#else
    (void)playerActiveCount;
    (void)enemyActiveCount;
#endif
}
