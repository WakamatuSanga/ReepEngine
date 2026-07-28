#include "RailShooterCameraRig.h"

#include "Engine/Game/RailShooter/RailPathRuntimeV2.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
#ifdef USE_IMGUI
void HelpMarker(const char* text) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void DrawVector(const char* label, const Vector3& value) {
    ImGui::Text("%s: %.4f, %.4f, %.4f", label, value.x, value.y, value.z);
}
#endif
}

void RailShooterCameraRig::DrawRuntimeV2TrialImGui() {
#ifdef USE_IMGUI
    ImGui::SeparatorText("Runtime V2試験接続");
    ImGui::Checkbox("Runtime V2姿勢を使用##RuntimeV2Pose", &enableRuntimeV2Trial_);
    ImGui::Text("現在の姿勢取得元: %s", activePoseSource_.c_str());
    ImGui::TextWrapped("Runtime V2構築結果: %s", runtimeV2BuildResult_.c_str());
    ImGui::Text("Runtime V2有効: %s", runtimeV2Trial_ && runtimeV2Trial_->IsValid() ? "はい" : "いいえ");
    ImGui::Text("Runtime V2全長: %.3f", runtimeV2Trial_ && runtimeV2Trial_->IsValid()
        ? runtimeV2Trial_->GetTotalLength() : 0.0f);

    ImGui::SeparatorText("姿勢診断");
    DrawVector("現在のRail位置", railCameraPose_.position);
    DrawVector("Look Ahead位置", railCameraPose_.aheadPosition);
    DrawVector("Rail前方向", railCameraPose_.forward);
    DrawVector("Camera前方向", visualCameraForward_);
    DrawVector("Player表示前方向", playerDisplayForward_);
    DrawVector("Camera Up", currentUp_);
    DrawVector("Camera Look Target", cameraLookTarget_);
    ImGui::Text("Rail前方向とCamera前方向の内積: %.5f", railCameraForwardDot_);
    ImGui::Text("Rail前方向とPlayer前方向の内積: %.5f", railPlayerForwardDot_);
    ImGui::Text("Camera Pitch: %.3f度", cameraPitchDegrees_);
    ImGui::Text("Player Pitch: %.3f度", playerPitchDegrees_);
    ImGui::DragFloat("Look Ahead距離##RuntimeV2LookAhead", &runtimeV2LookAheadDistance_, 0.1f, 0.1f, 100.0f, "%.2f");
    HelpMarker("現在位置より先のRail位置を参照し、Cameraの進行方向を求めるための距離です。");
    ImGui::DragFloat("Camera Look距離##RuntimeV2CameraLook", &cameraLookDistance_, 0.1f, 1.0f, 1000.0f, "%.2f");
    HelpMarker("Camera位置からRail前方向へLook Targetを作るための距離です。Camera高さOffsetはPitchへ混ざりません。");
    ImGui::Text("姿勢計算が有効: %s", runtimeV2PoseValid_ ? "はい" : "いいえ");
    ImGui::TextWrapped("ForwardのFallback理由: %s", forwardFallbackReason_.c_str());
    ImGui::Text("Runtime V2 Poseの適用回数: %llu", static_cast<unsigned long long>(runtimeV2PoseApplyCount_));
    ImGui::Text("Legacy Poseの適用回数: %llu", static_cast<unsigned long long>(legacyPoseApplyCount_));
    ImGui::Text("Initial CameraがRail姿勢を上書きした回数: %llu",
        static_cast<unsigned long long>(initialCameraRailPoseOverwriteCount_));
    ImGui::Text("GameMode切り替え時の姿勢同期回数: %llu",
        static_cast<unsigned long long>(gameModePoseSyncCount_));

    if (ImGui::Button("正面姿勢を再同期##ResyncPose")) ForceResyncRuntimeV2Pose();
    ImGui::SameLine();
    if (ImGui::Button("Legacy Forwardを強制使用##ForceLegacyForward")) {
        forceLegacyForward_ = true;
        forceRuntimeV2Forward_ = false;
        ForceResyncRuntimeV2Pose();
    }
    if (ImGui::Button("Runtime V2 Forwardを強制使用##ForceRuntimeV2Forward")) {
        forceRuntimeV2Forward_ = true;
        forceLegacyForward_ = false;
        ForceResyncRuntimeV2Pose();
    }
    ImGui::SameLine();
    if (ImGui::Button("姿勢診断をリセット##ResetPoseDiagnostics")) {
        runtimeV2PoseApplyCount_ = 0;
        legacyPoseApplyCount_ = 0;
        gameModePoseSyncCount_ = 0;
        initialCameraRailPoseOverwriteCount_ = 0;
        railCameraForwardDot_ = 1.0f;
        railPlayerForwardDot_ = 1.0f;
        cameraPitchDegrees_ = 0.0f;
        playerPitchDegrees_ = 0.0f;
        forwardFallbackReason_ = "未評価";
    }

    ImGui::SeparatorText("ブースト時のレール速度");
    ImGui::Text("Boost状態: %s", boostStateActive_ ? "有効" : "無効");
    ImGui::Text("BoostController有効: %s", boostControllerActive_ ? "はい" : "いいえ");
    ImGui::Text("通常Rail Speed: %.3f", railSpeed_);
    ImGui::Text("既存Speed Scale: %.3f", existingRailSpeedScale_);
    ImGui::DragFloat("Boost時Rail倍率##BoostRailMultiplier", &boostRailSpeedMultiplier_, 0.01f, 1.0f, 1.3f, "%.2f");
    HelpMarker("Boost中のRail Speedへ1回だけ掛ける倍率です。Cloud Flowより控えめな値を推奨します。");
    ImGui::Text("現在Rail倍率: %.4f", currentRailSpeedMultiplier_);
    ImGui::Text("目標Rail倍率: %.4f", targetRailSpeedMultiplier_);
    ImGui::Text("最終Rail Speed: %.3f", effectiveRailSpeed_);
    ImGui::DragFloat("Boost加速時間##BoostRailAcceleration", &boostRailAccelerationTime_, 0.01f, 0.01f, 0.5f, "%.2f秒");
    ImGui::DragFloat("通常速度への復帰時間##BoostRailReturn", &boostRailReturnTime_, 0.01f, 0.05f, 1.5f, "%.2f秒");
    ImGui::Text("現在Rail Distance: %.3f", railDistance_);
    ImGui::Text("1フレームのRail進行量: %.6f", lastRailAdvance_);
    ImGui::Text("Boost倍率の適用回数: %llu", static_cast<unsigned long long>(boostMultiplierApplyCount_));
    ImGui::Text("二重適用を検出: %s", boostDoubleApplicationDetected_ ? "はい" : "いいえ");
    ImGui::Text("敵側Boost接近速度補正: %s", enemyBoostCompensationExists_ ? "有効" : "無効");
    ImGui::TextWrapped("Boost中も敵自身のApproach Speedは変えず、Camera Rail Speedだけを控えめに増加させます。");

    if (ImGui::Button("Boost Rail Speedを強制##ForceBoostRail")) {
        forceBoostRailSpeed_ = true;
        forceBoostRailSpeedOff_ = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Boost Rail Speedを解除##ReleaseBoostRail")) {
        forceBoostRailSpeed_ = false;
        forceBoostRailSpeedOff_ = true;
    }
    if (ImGui::Button("Rail Speed設定をリセット##ResetRailSpeed")) {
        boostRailSpeedMultiplier_ = 1.15f;
        boostRailAccelerationTime_ = 0.10f;
        boostRailReturnTime_ = 0.45f;
        boostMultiplierApplyCount_ = 0;
        ResetBoostRailSpeedState();
    }
    ImGui::SameLine();
    if (ImGui::Button("強制設定をすべて解除##ClearRuntimeV2Forces")) {
        ClearRuntimeV2ForceTests();
        ResetBoostRailSpeedState();
        ForceResyncRuntimeV2Pose();
    }
#endif
}
