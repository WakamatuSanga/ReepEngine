#include "AimCorridorTargetingController.h"

#include "AimCorridorTargetMarkerRenderer.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"

namespace {
    const char* GetLockStateName(AimCorridorTargetingController::AimLockState state) {
        switch (state) {
        case AimCorridorTargetingController::AimLockState::Candidate:
            return "候補";
        case AimCorridorTargetingController::AimLockState::Acquiring:
            return "取得中";
        case AimCorridorTargetingController::AimLockState::Locked:
            return "ロック完了";
        case AimCorridorTargetingController::AimLockState::None:
        default:
            return "対象なし";
        }
    }
}
#endif

void AimCorridorTargetingController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(470.0f, 720.0f), ImGuiCond_FirstUseEver);
    const bool panelVisible = ImGui::Begin(
        "エイムコリドー照準・ロックデバッグ###AimCorridorTargetingDebug");
    if (!panelVisible) {
        ImGui::End();
        return;
    }

    if (ImGui::CollapsingHeader("基本設定##TargetingBasics", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("照準・ロック機能を有効化##TargetingEnabled", &enabled_);
        ImGui::Text("ゲームモード有効: %s", gameModeActive_ ? "はい" : "いいえ");
        ImGui::Text("カメラ有効: %s", camera_ ? "はい" : "いいえ");
        ImGui::Text("敵管理有効: %s", enemyManager_ ? "はい" : "いいえ");
        ImGui::Text("メイン照準矩形有効: %s", visibleRect_.valid ? "はい" : "いいえ");
        ImGui::DragInt("最大候補数##MaximumCandidateCount", &maximumCandidateCount_, 1.0f, 1, 32);
    }

    if (ImGui::CollapsingHeader("捕捉範囲##CaptureRange", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("補助捕捉倍率##SoftAssistScale", &softAssistScale_, 0.01f, 1.0f, 2.5f);
        ImGui::Text("表示枠の中心UV: (%.4f, %.4f)", visibleRect_.center.x, visibleRect_.center.y);
        ImGui::Text("表示枠の半サイズUV: (%.4f, %.4f)", visibleRect_.halfSize.x, visibleRect_.halfSize.y);
        ImGui::Text("補助捕捉範囲の半サイズUV: (%.4f, %.4f)", softRect_.halfSize.x, softRect_.halfSize.y);
        ImGui::Checkbox("敵の投影範囲を考慮##ConsiderEnemyBounds", &considerEnemyBounds_);
        ImGui::DragFloat("既定ワールド半径##FallbackWorldRadius", &fallbackWorldRadius_, 0.05f, 0.01f, 100.0f);
        ImGui::DragFloat("最小画面半径##MinimumScreenRadius", &minimumScreenRadius_, 0.001f, 0.001f, 0.1f, "%.3f");
        ImGui::DragFloat("最小対象深度##MinimumTargetDepth", &minimumTargetDepth_, 0.1f, 0.01f, 1000.0f);
        ImGui::DragFloat("最大対象深度##MaximumTargetDepth", &maximumTargetDepth_, 1.0f, 1.0f, 5000.0f);
    }

    if (ImGui::CollapsingHeader("候補選択##CandidateSelection", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("候補数: %d", candidateCount_);
        ImGui::DragFloat("対象保持時間##TargetHoldTime", &targetHoldTime_, 0.01f, 0.0f, 2.0f, "%.2f 秒");
        ImGui::DragFloat("対象切替余裕値##TargetSwitchMargin", &targetSwitchMargin_, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("深度スコア重み##DepthScoreWeight", &depthScoreWeight_, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("表示枠内ボーナス##VisibleRectBonus", &visibleRectBonus_, 0.01f, 0.0f, 1.0f);
        ImGui::Text("現在候補ID: %s", candidateTargetId_.empty() ? "なし" : candidateTargetId_.c_str());
        ImGui::Text("現在候補スコア: %.4f", currentCandidateScore_);
        ImGui::Text("最良候補ID: %s", bestCandidateId_.empty() ? "なし" : bestCandidateId_.c_str());
        ImGui::Text("最良候補スコア: %.4f", bestCandidateScore_);
        if (showDecisionReason_) {
            ImGui::TextWrapped("最後に切り替えた理由: %s", lastSwitchReason_.c_str());
        }
    }

    if (ImGui::CollapsingHeader("ロック取得##LockAcquisition", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("現在のロック状態: %s", GetLockStateName(lockState_));
        ImGui::DragFloat("ロック取得時間##LockAcquireTime", &lockAcquireTime_, 0.01f, 0.2f, 3.0f, "%.2f 秒");
        ImGui::Text("ロック経過時間: %.3f 秒", lockElapsed_);
        ImGui::ProgressBar(lockProgress_, ImVec2(-1.0f, 0.0f), "ロック進行率");
        ImGui::DragFloat("ロック解除猶予時間##LockBreakGraceTime", &lockBreakGraceTime_, 0.01f, 0.0f, 2.0f, "%.2f 秒");
        ImGui::Text("現在の解除猶予タイマー: %.3f 秒", breakGraceElapsed_);
        ImGui::Text("ロック完了回数: %u", lockCompletedCount_);
        ImGui::Text("ロック解除回数: %u", lockBreakCount_);
    }

    if (ImGui::CollapsingHeader("現在の対象##CurrentTarget", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("対象ID: %s", currentTargetValid_ ? currentTarget_.runtimeId.c_str() : "なし");
        ImGui::Text("対象名: %s", currentTargetValid_ ? currentTarget_.runtimeId.c_str() : "なし");
        ImGui::Text("対象種別: %s", currentTargetValid_ ? currentTarget_.enemyType.c_str() : "なし");
        ImGui::Text("対象ワールド位置: (%.3f, %.3f, %.3f)",
            currentTarget_.worldPosition.x, currentTarget_.worldPosition.y, currentTarget_.worldPosition.z);
        ImGui::Text("対象画面UV: (%.4f, %.4f)", currentTarget_.screenUv.x, currentTarget_.screenUv.y);
        ImGui::Text("クリップ W: %.4f", currentTarget_.clipW);
        ImGui::Text("対象カメラ深度: %.4f", currentTarget_.cameraDepth);
        ImGui::Text("表示枠内: %s", currentTarget_.overlapsVisibleRect ? "はい" : "いいえ");
        ImGui::Text("補助捕捉範囲内: %s", currentTarget_.overlapsSoftRect ? "はい" : "いいえ");
        ImGui::Text("投影有効: %s", currentTarget_.projectionValid ? "はい" : "いいえ");
        ImGui::Text("対象有効: %s", currentTargetValid_ ? "はい" : "いいえ");
    }

    if (ImGui::CollapsingHeader("マーカー表示##MarkerDisplay", ImGuiTreeNodeFlags_DefaultOpen)
        && markerRenderer_) {
        markerRenderer_->DrawImGuiSection();
    }

    if (ImGui::CollapsingHeader("画面投影##ProjectionDebug")) {
        ImGui::Checkbox("候補の投影範囲を表示##ShowCandidateBounds", &showCandidateBounds_);
        ImGui::Checkbox("表示枠矩形を表示##ShowVisibleRect", &showVisibleRect_);
        ImGui::Checkbox("補助捕捉矩形を表示##ShowSoftRect", &showSoftRect_);
        ImGui::Checkbox("対象中心を表示##ShowTargetCenter", &showTargetCenter_);
        ImGui::Checkbox("対象半径を表示##ShowTargetRadius", &showTargetRadius_);
        ImGui::Checkbox("判定理由を表示##ShowDecisionReason", &showDecisionReason_);
    }

    if (ImGui::CollapsingHeader("テスト操作##TestControls")) {
        if (ImGui::Button("強制候補状態##ForceCandidate")) {
            debugForcedState_ = static_cast<int>(AimLockState::Candidate);
        }
        ImGui::SameLine();
        if (ImGui::Button("強制取得中状態##ForceAcquiring")) {
            debugForcedState_ = static_cast<int>(AimLockState::Acquiring);
        }
        if (ImGui::Button("強制ロック完了状態##ForceLocked")) {
            debugForcedState_ = static_cast<int>(AimLockState::Locked);
        }
        ImGui::SameLine();
        if (ImGui::Button("強制ロック解除##ForceUnlock")) {
            debugForcedState_ = static_cast<int>(AimLockState::None);
            ClearTarget(true, "テスト操作で強制解除");
        }
        if (ImGui::Button("ロック進行を0へ戻す##ResetLockProgress")) {
            lockElapsed_ = 0.0f;
            lockProgress_ = 0.0f;
            if (HasCandidate()) {
                lockState_ = AimLockState::Candidate;
            }
        }
        if (ImGui::Button("推奨捕捉設定を適用##ApplyRecommendedCapture")) {
            softAssistScale_ = 1.60f;
            targetHoldTime_ = 0.20f;
            targetSwitchMargin_ = 0.15f;
            depthScoreWeight_ = 0.12f;
            visibleRectBonus_ = 0.15f;
            lockAcquireTime_ = 0.85f;
            lockBreakGraceTime_ = 0.25f;
        }
        ImGui::SameLine();
        if (ImGui::Button("照準・ロック状態をリセット##ResetTargetingState")) {
            Reset();
        }
    }
    ImGui::End();

    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const auto toScreen = [displaySize](const Vector2& uv) {
        return ImVec2(uv.x * displaySize.x, uv.y * displaySize.y);
    };
    if (drawList && visibleRect_.valid && showVisibleRect_) {
        drawList->AddRect(toScreen(visibleRect_.minimum), toScreen(visibleRect_.maximum), IM_COL32(215, 255, 60, 220));
    }
    if (drawList && softRect_.valid && showSoftRect_) {
        drawList->AddRect(toScreen(softRect_.minimum), toScreen(softRect_.maximum), IM_COL32(255, 216, 74, 150));
    }
    if (drawList && currentTargetValid_ && showCandidateBounds_) {
        drawList->AddRect(toScreen(currentTarget_.boundsMinimum), toScreen(currentTarget_.boundsMaximum), IM_COL32(255, 154, 50, 220));
    }
    if (drawList && currentTargetValid_ && showTargetCenter_) {
        drawList->AddCircleFilled(toScreen(currentTarget_.screenUv), 4.0f, IM_COL32(255, 74, 42, 230));
    }
    if (drawList && currentTargetValid_ && showTargetRadius_) {
        const float radius = currentTarget_.screenRadius.x * displaySize.x;
        drawList->AddCircle(toScreen(currentTarget_.screenUv), radius, IM_COL32(255, 154, 50, 190));
    }
#endif
}
