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
    if (!ImGui::Begin(
            "レール弾道整合性デバッグ###ProjectileRailMotionDebug")) {
        ImGui::End();
        return;
    }

    const auto drawKindDiagnostics = [](
        const char* heading,
        const KindDiagnostics& diagnostics,
        size_t activeCount,
        bool trackingActive,
        const char* muzzleLabel,
        const char* targetLabel) {
        ImGui::SeparatorText(heading);
        ImGui::Text("有効弾数: %llu", static_cast<unsigned long long>(activeCount));
        ImGui::Text(
            "Rail追従対象数: %llu",
            static_cast<unsigned long long>(trackingActive ? activeCount : 0));
        DrawVector3(muzzleLabel, diagnostics.lastMuzzleWorldPosition);
        DrawVector3(targetLabel, diagnostics.lastTargetWorldPosition);
        DrawVector3("最後の発射方向", diagnostics.lastShotDirection);
        DrawVector3("最後の相対速度", diagnostics.lastRelativeVelocity);
        ImGui::Text(
            "発射数: %llu",
            static_cast<unsigned long long>(diagnostics.shotCount));
        ImGui::Text(
            "Frame差分適用数: %llu",
            static_cast<unsigned long long>(diagnostics.applyCount));
        ImGui::Text(
            "Frame差分スキップ数: %llu",
            static_cast<unsigned long long>(diagnostics.skipCount));
        ImGui::Text(
            "生成Frameスキップ数: %llu",
            static_cast<unsigned long long>(diagnostics.spawnFrameSkipCount));
        ImGui::Text(
            "二重適用検出数: %llu",
            static_cast<unsigned long long>(diagnostics.doubleApplyCount));
        ImGui::Text("最後の消滅理由: %s", diagnostics.lastDespawnReason.c_str());
    };

    ImGui::SeparatorText("共通状態");
    ImGui::Text("初期化: %s", ToJapaneseState(initialized_));
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
    ImGui::Text(
        "Frame差分適用回数: %llu",
        static_cast<unsigned long long>(frameDeltaApplyCount_));
    ImGui::Text(
        "Frame差分スキップ回数: %llu",
        static_cast<unsigned long long>(frameDeltaSkipCount_));
    ImGui::Text(
        "Frame再同期回数: %llu",
        static_cast<unsigned long long>(frameResyncCount_));
    ImGui::Text(
        "登録Projectile数: %llu",
        static_cast<unsigned long long>(registeredProjectileCount_));
    ImGui::Text("最後のFrame状態: %s", lastFrameStatus_.c_str());
    ImGui::Text("最後のスキップ理由: %s", lastSkipReason_.c_str());
    ImGui::Text(
        "二重適用検出: %s",
        doubleApplicationDetected_ ? "検出あり" : "検出なし");
    ImGui::Text(
        "二重適用検出総数: %llu",
        static_cast<unsigned long long>(doubleApplicationCount_));

    drawKindDiagnostics(
        "Player弾",
        playerDiagnostics_,
        playerActiveCount,
        IsTrackingActive(ProjectileKind::Player),
        "最後のMuzzle World位置",
        "最後のTarget World位置");
    drawKindDiagnostics(
        "Enemy弾",
        enemyDiagnostics_,
        enemyActiveCount,
        IsTrackingActive(ProjectileKind::Enemy),
        "最後のEnemy Muzzle位置",
        "最後のPlayer Target位置");

    ImGui::SeparatorText("テスト");
    bool trackingStateChanged = false;
    trackingStateChanged |= ImGui::Checkbox(
        "Rail Frame追従を一時無効化##DisableAllTracking",
        &forceDisableTracking_);
    trackingStateChanged |= ImGui::Checkbox(
        "Player弾だけ追従を無効化##DisablePlayerTracking",
        &forceDisablePlayerTracking_);
    trackingStateChanged |= ImGui::Checkbox(
        "Enemy弾だけ追従を無効化##DisableEnemyTracking",
        &forceDisableEnemyTracking_);
    if (trackingStateChanged) {
        forceResyncRequested_ = true;
    }

    ImGui::DragFloat(
        "最大許容Frame移動量##MaximumFrameTranslation",
        &maxFrameTranslation_,
        0.1f,
        0.1f,
        1000.0f,
        "%.2f");
    ImGui::DragFloat(
        "最大許容Frame回転量##MaximumFrameRotation",
        &maxFrameRotationDegrees_,
        0.1f,
        1.0f,
        180.0f,
        "%.2f度");

    if (ImGui::Button("Frame診断をリセット##ResetFrameDiagnostics")) {
        ResetDiagnostics();
    }
    ImGui::SameLine();
    if (ImGui::Button("次フレームで再同期##ForceFrameResync")) {
        forceResyncRequested_ = true;
    }
    if (ImGui::Button("強制状態をすべて解除##ClearAllForcedStates")) {
        ClearForcedStates();
    }

    ImGui::End();
#else
    (void)playerActiveCount;
    (void)enemyActiveCount;
#endif
}