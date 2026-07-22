#include "PlayerRailFlightVisualTiltController.h"

#include "Player.h"

#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
#ifdef USE_IMGUI
const char* BoolText(bool value) {
    return value ? "はい" : "いいえ";
}

void DrawVector(const char* label, const Vector3& value) {
    ImGui::Text("%s: %.3f, %.3f, %.3f", label, value.x, value.y, value.z);
}

void DrawTooltip(const char* text) {
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", text);
}
#endif
}

void PlayerRailFlightVisualTiltController::DrawImGui() {
#ifdef USE_IMGUI
    UpdatePlayerDiagnostics();
    ImGui::SetNextWindowSize(ImVec2(430.0f, 690.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(
        "プレイヤー・レール飛行姿勢デバッグ###PlayerRailFlightVisualTiltDebug")) {
        ImGui::End();
        return;
    }

    ImGui::SeparatorText("基本設定");
    ImGui::Checkbox(
        "レールカーブBankを有効化##RailBankEnabled",
        &railBankEnabled_);
    ImGui::Text("Player有効: %s", BoolText(playerEnabled_));
    ImGui::Text("GameMode: %s", BoolText(gameModeActive_));
    ImGui::Text("Player生存中: %s", BoolText(playerAlive_));
    ImGui::Text("Rail走行中: %s", BoolText(railRunning_));
    ImGui::Text("Rail Pose有効: %s", BoolText(railPoseValid_));
    ImGui::Text("Visual Tilt適用中: %s", BoolText(visualTiltApplying_));

    ImGui::SeparatorText("レール姿勢");
    DrawVector("現在Rail Forward", currentRailForward_);
    DrawVector("先読みRail Forward", aheadRailForward_);
    DrawVector("Rail Up", railUp_);
    ImGui::Text("Rail距離: %.3f", railDistance_);
    ImGui::Text("Runtime V2姿勢を使用: %s", BoolText(runtimeV2Active_));
    ImGui::DragFloat(
        "Bank先読み距離##RailBankLookAheadDistance",
        &railBankLookAheadDistance_, 0.1f, 0.1f, 100.0f, "%.2f");
    ImGui::Text("水平Forwardが有効: %s", BoolText(horizontalForwardValid_));
    ImGui::Text("符号付きTurn角: %.3f度", signedTurnDegrees_);
    const char* curveDirectionText = "直進";
    if (curveDirection_ == CurveDirection::Right) curveDirectionText = "右";
    if (curveDirection_ == CurveDirection::Left) curveDirectionText = "左";
    ImGui::Text("カーブ方向: %s", curveDirectionText);

    ImGui::SeparatorText("カーブBank");
    ImGui::DragFloat(
        "最大Rail Bank角##MaxRailBankDegrees",
        &maxRailBankDegrees_, 0.1f, 0.0f, 30.0f, "%.2f度");
    DrawTooltip(
        "レールの左右カーブによって機体を傾ける最大角度です。"
        "Cameraや当たり判定には影響しません。");
    ImGui::DragFloat(
        "Bankゲイン##RailBankGain",
        &railBankGain_, 0.01f, 0.0f, 3.0f, "%.2f");
    ImGui::DragFloat(
        "Bank応答時間##RailBankResponseTime",
        &railBankResponseTime_, 0.01f, 0.01f, 1.0f, "%.2f秒");
    ImGui::DragFloat(
        "Bank復帰時間##RailBankReturnTime",
        &railBankReturnTime_, 0.01f, 0.01f, 1.5f, "%.2f秒");
    ImGui::DragFloat(
        "Bankスナップ閾値##RailBankSnapEpsilon",
        &railBankSnapEpsilonDegrees_, 0.01f, 0.0f, 5.0f, "%.2f度");
    ImGui::Text("目標Bank角: %.3f度", targetRailBankDegrees_);
    ImGui::Text("現在Bank角: %.3f度", currentRailBankDegrees_);
    ImGui::Text(
        "Bank適用回数: %llu",
        static_cast<unsigned long long>(bankApplyCount_));
    ImGui::TextWrapped("Bank無効理由: %s", bankDisabledReason_.c_str());

    ImGui::SeparatorText("現在の状態");
    DrawVector("Player基本Forward", playerBaseForward_);
    DrawVector("Player表示Forward", playerDisplayForward_);
    ImGui::Text("Player基本Pitch: %.3f度", playerBasePitchDegrees_);
    ImGui::Text("Player最終Roll参考値 (Euler Z): %.3f度", playerFinalRollDegrees_);
    ImGui::Text("Visual Rotationが有限: %s", BoolText(visualRotationFinite_));
    ImGui::Text("CameraへBank未適用: はい");
    ImGui::Text("CollisionへBank未適用: はい");
    ImGui::Text("射撃方向へBank未適用: はい");

    ImGui::SeparatorText("Barrel Roll連携");
    ImGui::Text(
        "Barrel Roll中: %s",
        BoolText(player_ && player_->IsBarrelRolling()));
    ImGui::Text("Action Rotation有効: %s", BoolText(actionRotationActive_));
    DrawVector("Action回転", playerActionRotation_);
    ImGui::TextWrapped(
        "表示合成順: 既存Base / Model軸補正 / Input Tilt / Barrel Roll姿勢"
        " → Player基本Forward軸のRail Bank");
    ImGui::Text(
        "Barrel Roll終了後の復帰状態: %s",
        std::fabs(currentRailBankDegrees_ - targetRailBankDegrees_) <=
            railBankSnapEpsilonDegrees_
        ? "現在カーブのBankへ復帰済み"
        : "現在カーブのBankへ補間中");

    ImGui::SeparatorText("テスト操作");
    ImGui::TextWrapped(
        "明示的な強制BankはDebugModeでもPlayerモデルの表示だけへ適用します。");
    if (ImGui::Button("右カーブBankを強制##ForceRightBank")) {
        forcedBankMode_ = ForcedBankMode::Right;
    }
    ImGui::SameLine();
    if (ImGui::Button("左カーブBankを強制##ForceLeftBank")) {
        forcedBankMode_ = ForcedBankMode::Left;
    }
    if (ImGui::Button("Bankを0へ強制##ForceZeroBank")) {
        forcedBankMode_ = ForcedBankMode::Zero;
    }
    ImGui::SameLine();
    if (ImGui::Button("強制状態を解除##ReleaseForcedBank")) {
        forcedBankMode_ = ForcedBankMode::None;
    }
    const char* forcedModeText = "解除";
    if (forcedBankMode_ == ForcedBankMode::Right) forcedModeText = "右";
    if (forcedBankMode_ == ForcedBankMode::Left) forcedModeText = "左";
    if (forcedBankMode_ == ForcedBankMode::Zero) forcedModeText = "0";
    ImGui::Text("強制Bank状態: %s", forcedModeText);

    if (ImGui::Button("推奨Bank設定を適用##ApplyRecommendedBankSettings")) {
        ApplyRecommendedSettings();
    }
    ImGui::SameLine();
    if (ImGui::Button("Bank設定をリセット##ResetBankSettings")) {
        ApplyRecommendedSettings();
        Reset();
    }
    if (ImGui::Button("Bank状態をリセット##ResetBankState")) {
        Reset();
    }

    ClampSettings();
    ImGui::End();
#endif
}
