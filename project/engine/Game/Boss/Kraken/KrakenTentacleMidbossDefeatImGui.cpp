#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"

#include <algorithm>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
#ifdef USE_IMGUI
const char* BoolLabel(bool value) {
    return value ? "はい" : "いいえ";
}

const char* StateLabel(KrakenTentacleMidbossState state) {
    switch (state) {
    case KrakenTentacleMidbossState::Hidden:
        return "非表示";
    case KrakenTentacleMidbossState::Idle:
        return "待機";
    case KrakenTentacleMidbossState::Windup:
        return "振りかぶり";
    case KrakenTentacleMidbossState::WindupHold:
        return "振りかぶり保持";
    case KrakenTentacleMidbossState::Slam:
        return "叩きつけ";
    case KrakenTentacleMidbossState::ImpactHold:
        return "衝撃保持";
    case KrakenTentacleMidbossState::Recovery:
        return "復帰";
    case KrakenTentacleMidbossState::Defeated:
        return "撃破直後停止";
    case KrakenTentacleMidbossState::Retreating:
        return "下方退避中";
    case KrakenTentacleMidbossState::RetreatCompleted:
        return "撃破退避完了";
    default:
        return "不明";
    }
}

void DrawPosition(const char* label, const Vector3& value) {
    ImGui::Text(
        "%s: %.2f, %.2f, %.2f",
        label, value.x, value.y, value.z);
}
#endif
}

void KrakenTentacleMidbossController::Impl::DrawDefeatImGui() {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader(
            "撃破・落下モーション##DefeatMotion",
            ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::SeparatorText("状態");
    ImGui::Text("HP 0: %s", BoolLabel(health.GetCurrentHp() <= 0.0f));
    ImGui::Text("撃破待ち: %s", BoolLabel(health.IsDefeatPending()));
    ImGui::Text("撃破開始済み: %s", BoolLabel(defeatStarted));
    ImGui::Text("撃破完了: %s", BoolLabel(defeatCompleted));
    ImGui::Text("現在状態: %s", StateLabel(state));
    ImGui::Text("状態経過時間: %.3f 秒", stateElapsedTime);
    DrawPosition("撃破開始ワールド位置", defeatStartWorldPosition);
    DrawPosition("現在ワールド位置", worldPosition);
    ImGui::Text("落下距離: %.2f", defeatSettings.retreatDistance);
    ImGui::Text(
        "落下進行率: %.1f%%",
        defeatDiagnostics.retreatProgress * 100.0f);
    ImGui::Text("固定姿勢有効: %s", BoolLabel(defeatFrozenPoseValid));
    ImGui::Text("モデル表示: %s", BoolLabel(IsVisible()));
    ImGui::Text(
        "ボーン表示: %s",
        BoolLabel(IsVisible() && (showBones || showJoints)));
    ImGui::Text(
        "コライダー表示: %s",
        BoolLabel(IsVisible() && showColliders));
    ImGui::Text(
        "診断対象コライダー数: %zu",
        diagnostics.collisionQueryTargetCount);
    ImGui::Text("攻撃ダメージ: %s", BoolLabel(attackDamageEnabled));
    ImGui::Text(
        "投射物ダメージ: %s", BoolLabel(projectileDamageEnabled));
    ImGui::Text("ウェーブ未接続: はい");
    ImGui::Text("撃破音未実装: はい");

    ImGui::SeparatorText("設定");
    ImGui::DragFloat(
        "撃破直後停止時間##DefeatedHold",
        &defeatSettings.holdTime, 0.01f, 0.0f, 1.0f, "%.2f 秒");
    ImGui::DragFloat(
        "退避時間##RetreatDuration",
        &defeatSettings.retreatDuration, 0.01f, 0.1f, 3.0f, "%.2f 秒");
    ImGui::DragFloat(
        "退避距離##RetreatDistance",
        &defeatSettings.retreatDistance, 0.1f, 1.0f, 100.0f, "%.1f");
    defeatSettings = SanitizeKrakenTentacleDefeatSettings(defeatSettings);
    ImGui::Text("補間: 3次イーズアウト");
    ImGui::Text("ワールド下方向: -Y");
    if (ImGui::Button("推奨値へ戻す##ResetDefeatSettings")) {
        defeatSettings = {};
    }

    ImGui::SeparatorText("操作");
    if (ImGui::Button("撃破落下を強制開始##ForceDefeat")) {
        pendingCommand = KrakenTentacleMidbossPendingCommand::ForceDefeat;
    }
    if (ImGui::Button("撃破状態を解除して全回復##RecoverDefeat")) {
        pendingCommand = KrakenTentacleMidbossPendingCommand::RecoverDefeat;
    }
    if (ImGui::Button("撃破前位置へ戻す##RestoreDefeatPosition")) {
        pendingCommand =
            KrakenTentacleMidbossPendingCommand::RestoreDefeatPosition;
    }
    ImGui::SameLine();
    if (ImGui::Button("実行時全体をリセット##DefeatRuntimeReset")) {
        pendingCommand = KrakenTentacleMidbossPendingCommand::ResetRuntime;
    }
    if (ImGui::Button("カメラ前方へ再配置##DefeatPlaceInFront")) {
        PlaceInFrontOfCamera();
    }

    ImGui::SeparatorText("診断");
    ImGui::Text(
        "撃破開始回数: %llu",
        static_cast<unsigned long long>(defeatDiagnostics.beginCount));
    ImGui::Text(
        "撃破開始重複防止回数: %llu",
        static_cast<unsigned long long>(
            defeatDiagnostics.duplicateBeginSuppressionCount));
    ImGui::Text(
        "撃破直後停止から退避への遷移回数: %llu",
        static_cast<unsigned long long>(
            defeatDiagnostics.retreatBeginCount));
    ImGui::Text(
        "退避完了回数: %llu",
        static_cast<unsigned long long>(
            defeatDiagnostics.retreatCompleteCount));
    ImGui::Text(
        "固定姿勢保存成功数: %llu",
        static_cast<unsigned long long>(
            defeatDiagnostics.frozenPoseCaptureSuccessCount));
    ImGui::Text(
        "固定姿勢保存失敗数: %llu",
        static_cast<unsigned long long>(
            defeatDiagnostics.frozenPoseCaptureFailureCount));
    ImGui::Text(
        "非有限位置数: %llu",
        static_cast<unsigned long long>(
            defeatDiagnostics.nonFinitePositionCount));
    ImGui::Text(
        "非有限時間数: %llu",
        static_cast<unsigned long long>(
            defeatDiagnostics.nonFiniteTimeCount));
    ImGui::Text(
        "落下距離不正数: %llu",
        static_cast<unsigned long long>(
            defeatDiagnostics.invalidDistanceCount));
    const bool queryStopped = !IsDefeatState() ||
        (diagnostics.collisionQueryTargetCount == 0 &&
            diagnostics.currentCollisionPairCount == 0);
    ImGui::Text("判定停止成功: %s", BoolLabel(queryStopped));
    ImGui::Text(
        "攻撃停止成功: %s",
        BoolLabel(!IsDefeatState() || !attackDamageEnabled));
    ImGui::Text(
        "追加ダメージ拒否数: %llu",
        static_cast<unsigned long long>(
            defeatDiagnostics.additionalDamageRejectionCount));
    ImGui::Text(
        "HP 0後の弾停止要求数: %llu",
        static_cast<unsigned long long>(
            defeatDiagnostics.postHpZeroBulletKillRequestCount));
    ImGui::Text(
        "ウェーブ通知回数: %llu",
        static_cast<unsigned long long>(
            defeatDiagnostics.waveNotificationCount));
    ImGui::TextWrapped(
        "最後のエラー: %s",
        defeatDiagnostics.lastError.empty()
            ? "なし" : defeatDiagnostics.lastError.c_str());
    ImGui::TextWrapped(
        "最後の警告: %s",
        defeatDiagnostics.lastWarning.empty()
            ? "なし" : defeatDiagnostics.lastWarning.c_str());
#endif
}
