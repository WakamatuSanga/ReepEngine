#include "AimCorridorVisualController.h"

#include "AimCorridorVisualRenderer.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void AimCorridorVisualController::DrawImGui() {
#ifdef USE_IMGUI
    // Aim CorridorのImGuiは、表示文言を日本語、隠しIDを英語で統一する。
    if (!ImGui::Begin("エイムコリドー表示デバッグ###AimCorridorVisualDebug")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("エイムコリドーを有効化##AimCorridorEnable", &enabled_);
    ImGui::Text("ゲームモード有効: %s", gameModeActive_ ? "はい" : "いいえ");
    ImGui::Text("現在表示中: %s", isVisible_ ? "はい" : "いいえ");
    ImGui::Text("現在の透明度: %.3f", currentAlpha_);
    ImGui::Text("ゲームモード中は常時表示: はい");
    ImGui::Text("アクティブカメラポインター有効: %s", camera_ ? "はい" : "いいえ");
    ImGui::Text("プレイヤー位置へ追従: はい");
    ImGui::Text("深度判定を無効化: はい");
    ImGui::Text("描画回数: %u", lastDrawCount_);
    ImGui::Text("手前テクスチャ読込済み: %s", renderer_ && renderer_->IsNearTextureLoaded() ? "はい" : "いいえ");
    ImGui::Text("奥テクスチャ読込済み: %s", renderer_ && renderer_->IsFarTextureLoaded() ? "はい" : "いいえ");
    ImGui::Text("手前テクスチャパス: %s", nearTexturePath_.c_str());
    ImGui::Text("奥テクスチャパス: %s", farTexturePath_.c_str());

    if (ImGui::CollapsingHeader("通常照準表示##MainReticlePresentationSection", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* displayModeItems[] = { "メイン照準のみ", "ワールド回廊デバッグ" };
        int displayModeIndex = static_cast<int>(displayMode_);
        if (ImGui::Combo("表示モード##AimCorridorDisplayMode", &displayModeIndex, displayModeItems, 2)) {
            displayMode_ = static_cast<DisplayMode>(displayModeIndex);
        }
        ImGui::Checkbox("メイン照準を自動サイズ計算##AutoMainReticleSize", &autoMainReticleSize_);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Far最終中心の深度とFOVから、画面上の高さが一定になるWorldサイズを計算します。");
        }
        ImGui::DragFloat("メイン照準の画面高さ比##MainReticleViewportHeightRatio",
            &mainReticleViewportHeightRatio_, 0.001f, 0.05f, 0.16f, "%.3f");
        ImGui::DragFloat("メイン照準の手動ワールド高さ##ManualMainReticleWorldHeight",
            &manualMainReticleWorldHeight_, 0.05f, 0.01f, 100.0f, "%.3f");
        ImGui::Text("メイン照準の実効ワールド高さ: %.4f", effectiveMainReticleWorldHeight_);
        ImGui::Text("メイン照準の実効ワールド幅: %.4f", effectiveMainReticleWorldWidth_);
        ImGui::Text("メイン照準の画面上の推定幅: %.1f px", estimatedMainReticlePixelWidth_);
        ImGui::Text("メイン照準の画面上の推定高さ: %.1f px", estimatedMainReticlePixelHeight_);
        ImGui::Text("メイン照準のカメラ深度: %.4f", mainReticleDepth_);
        ImGui::Text("メイン照準の中心World位置: %.3f, %.3f, %.3f",
            mainReticleCenterWorld_.x, mainReticleCenterWorld_.y, mainReticleCenterWorld_.z);
        if (mainReticleProjectionValid_) {
            ImGui::Text("メイン照準の中心画面UV: %.6f, %.6f",
                mainReticleCenterScreenUv_.x, mainReticleCenterScreenUv_.y);
        } else {
            ImGui::Text("メイン照準の中心画面UV: 無効");
        }
        ImGui::Text("メイン照準サイズが有効: %s", mainReticleSizeValid_ ? "はい" : "いいえ");
        ImGui::DragFloat("メイン照準の透明度##MainReticleAlpha",
            &mainReticleAppearance_.alpha, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("メイン照準の本体輝度##MainReticleCoreIntensity",
            &mainReticleAppearance_.coreIntensity, 0.01f, 0.0f, 2.0f);
        ImGui::DragFloat("メイン照準のGlow強度##MainReticleGlowIntensity",
            &mainReticleAppearance_.glowIntensity, 0.01f, 0.0f, 2.0f);
        ImGui::DragFloat("メイン照準のGlow透明度##MainReticleGlowAlpha",
            &mainReticleAppearance_.glowAlpha, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("メイン照準のGlow範囲##MainReticleGlowRadius",
            &mainReticleAppearance_.glowRadiusTexels, 0.05f, 0.0f, 4.0f);
        ImGui::DragFloat("メイン照準の脈動量##MainReticlePulseAmount",
            &mainReticleAppearance_.pulseAmount, 0.005f, 0.0f, 0.25f);
        ImGui::DragFloat("メイン照準の脈動速度##MainReticlePulseRate",
            &mainReticlePulseRate_, 0.05f, 0.0f, 10.0f);
        ImGui::Checkbox("ひし形プレビューを表示##DebugDiamondPreview", &debugDiamondPreview_);
        ImGui::DragFloat("ひし形プレビューの倍率##DebugDiamondPreviewScale",
            &debugDiamondPreviewScale_, 0.01f, 1.0f, 2.0f, "%.2f");
        const char* visualStateItems[] = { "通常", "候補", "取得中", "ロック完了" };
        int visualStateIndex = static_cast<int>(reticleVisualState_);
        if (ImGui::Combo("表示状態プレビュー##ReticleVisualState",
            &visualStateIndex, visualStateItems, 4)) {
            reticleVisualState_ = static_cast<AimReticleVisualState>(visualStateIndex);
        }
        if (ImGui::Button("推奨メイン照準サイズを適用##ApplyRecommendedMainReticleSize")) {
            autoMainReticleSize_ = true;
            mainReticleViewportHeightRatio_ = 0.095f;
        }
        if (ImGui::Button("メイン照準だけ表示##ShowMainReticleOnly")) {
            displayMode_ = DisplayMode::MainReticleOnly;
        }
        ImGui::SameLine();
        if (ImGui::Button("ワールド回廊を表示##ShowWorldCorridor")) {
            displayMode_ = DisplayMode::WorldCorridorDebug;
        }
        if (ImGui::Button("ひし形プレビューを表示##ShowDiamondPreview")) {
            debugDiamondPreview_ = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("ひし形プレビューを非表示##HideDiamondPreview")) {
            debugDiamondPreview_ = false;
        }
        if (ImGui::Button("通常照準設定をリセット##ResetMainReticlePresentation")) {
            ResetPresentationParameters();
        }
    }

    if (ImGui::CollapsingHeader("配置軸##AxisSection", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* axisModeNames[] = { "カメラ前方向", "カメラから照準原点を通る軸" };
        int axisMode = static_cast<int>(axisMode_);
        if (ImGui::Combo("配置軸モード##AxisMode", &axisMode, axisModeNames, 2)) {
            axisMode_ = static_cast<AxisMode>(axisMode);
        }
        bool useCameraThroughPlayerRay = axisMode_ == AxisMode::CameraThroughAimOrigin;
        if (ImGui::Checkbox("カメラからプレイヤーを通る軸を使用##UseCameraThroughPlayerRay", &useCameraThroughPlayerRay)) {
            axisMode_ = useCameraThroughPlayerRay ? AxisMode::CameraThroughAimOrigin : AxisMode::CameraForward;
        }
        ImGui::Text("カメラ位置: %.3f, %.3f, %.3f", cameraPosition_.x, cameraPosition_.y, cameraPosition_.z);
        ImGui::Text("プレイヤー描画位置: %.3f, %.3f, %.3f", playerRenderPosition_.x, playerRenderPosition_.y, playerRenderPosition_.z);
        ImGui::Text("照準原点: %.3f, %.3f, %.3f", aimOrigin_.x, aimOrigin_.y, aimOrigin_.z);
        ImGui::Text("カメラ前方向: %.4f, %.4f, %.4f", cameraForward_.x, cameraForward_.y, cameraForward_.z);
        ImGui::Text("照準前方向: %.3f, %.3f, %.3f", aimForward_.x, aimForward_.y, aimForward_.z);
        ImGui::Text("照準Rayの長さ: %.4f", aimRayLength_);
        ImGui::Text("照準前方向とカメラ前方向の内積: %.6f", aimForwardDotCameraForward_);
    }

    if (ImGui::CollapsingHeader("位置設定##PositionSection", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("手前枠の中心: %.3f, %.3f, %.3f", nearCenter_.x, nearCenter_.y, nearCenter_.z);
        ImGui::Text("奥枠の中心: %.3f, %.3f, %.3f", farCenter_.x, farCenter_.y, farCenter_.z);
        ImGui::DragFloat("手前枠の距離##NearDistance", &nearDistance_, 0.1f, 0.1f, 1000.0f);
        ImGui::DragFloat("奥枠の距離##FarDistance", &farDistance_, 0.1f, 0.2f, 2000.0f);
        ImGui::Text("奥行き間隔: %.3f", farDistance_ - nearDistance_);
        if (ImGui::Checkbox("新しい奥行き既定値を使用##UseNewDepthDefaults", &useNewDepthDefaults_)) {
            nearDistance_ = useNewDepthDefaults_ ? 28.0f : 18.0f;
            farDistance_ = useNewDepthDefaults_ ? 70.0f : 55.0f;
            ResetLeadState();
        }
        ImGui::DragFloat3("原点オフセット X / Y / Z##OriginOffset", &originOffset_.x, 0.05f);
        ImGui::DragFloat2("手前枠オフセット X / Y##NearOffset", &nearOffset_.x, 0.05f);
        ImGui::DragFloat2("奥枠オフセット X / Y##FarOffset", &farOffset_.x, 0.05f);
    }

    if (ImGui::CollapsingHeader("基準画面オフセット##BaseScreenOffsetSection", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("基準画面オフセットを有効化##EnableBaseScreenOffset", &baseScreenOffsetEnabled_);
        ImGui::DragFloat("手前枠の基準オフセット X##NearBaseScreenOffsetX",
            &nearBaseScreenOffset_.x, 0.005f, -0.15f, 0.15f, "%.3f");
        ImGui::DragFloat("手前枠の基準オフセット Y##NearBaseScreenOffsetY",
            &nearBaseScreenOffset_.y, 0.005f, -0.20f, 0.25f, "%.3f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("正のYで画面上方向へ移動します。");
        }
        ImGui::DragFloat("奥枠の基準オフセット X##FarBaseScreenOffsetX",
            &farBaseScreenOffset_.x, 0.005f, -0.20f, 0.20f, "%.3f");
        ImGui::DragFloat("奥枠の基準オフセット Y##FarBaseScreenOffsetY",
            &farBaseScreenOffset_.y, 0.005f, -0.25f, 0.35f, "%.3f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("正のYで画面上方向へ移動します。");
        }
        ImGui::Text("手前枠の最終画面オフセット: %.4f, %.4f",
            nearFinalScreenOffset_.x, nearFinalScreenOffset_.y);
        ImGui::Text("奥枠の最終画面オフセット: %.4f, %.4f",
            farFinalScreenOffset_.x, farFinalScreenOffset_.y);
        ImGui::Text("手前枠の基準ワールドオフセット: %.3f, %.3f, %.3f",
            nearBaseScreenWorldOffset_.x, nearBaseScreenWorldOffset_.y, nearBaseScreenWorldOffset_.z);
        ImGui::Text("奥枠の基準ワールドオフセット: %.3f, %.3f, %.3f",
            farBaseScreenWorldOffset_.x, farBaseScreenWorldOffset_.y, farBaseScreenWorldOffset_.z);
        if (playerProjectionValid_) {
            ImGui::Text("プレイヤーの画面UV: %.6f, %.6f", playerScreenUv_.x, playerScreenUv_.y);
        } else {
            ImGui::Text("プレイヤーの画面UV: 無効");
        }
        if (aimOriginProjectionValid_) {
            ImGui::Text("照準原点の画面UV: %.6f, %.6f", aimOriginScreenUv_.x, aimOriginScreenUv_.y);
        } else {
            ImGui::Text("照準原点の画面UV: 無効");
        }
        if (nearProjectionValid_) {
            ImGui::Text("手前枠の画面UV: %.6f, %.6f", nearScreenUv_.x, nearScreenUv_.y);
        } else {
            ImGui::Text("手前枠の画面UV: 無効");
        }
        if (farProjectionValid_) {
            ImGui::Text("奥枠の画面UV: %.6f, %.6f", farScreenUv_.x, farScreenUv_.y);
        } else {
            ImGui::Text("奥枠の画面UV: 無効");
        }
        ImGui::Text("プレイヤーから手前枠までの画面距離（UV）: %.6f", playerToNearScreenDistance_);
        ImGui::Text("プレイヤーから奥枠までの画面距離（UV）: %.6f", playerToFarScreenDistance_);
        ImGui::Text("手前枠から奥枠までの画面距離（UV）: %.6f", nearToFarScreenDistance_);
        ImGui::Text("前方配置が有効: %s", forwardPlacementActive_ ? "はい" : "いいえ");
        ImGui::Text("上下方向が正常: %s", verticalDirectionNormal_ ? "はい" : "いいえ");
        if (ImGui::Button("推奨オフセットを適用##ApplyRecommendedBaseOffsets")) {
            ResetBaseScreenOffsetParameters();
        }
        if (ImGui::Button("基準画面オフセットを一時無効化##TemporarilyDisableBaseScreenOffset")) {
            baseScreenOffsetEnabled_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("基準画面オフセットを復元##RestoreBaseScreenOffset")) {
            baseScreenOffsetEnabled_ = true;
        }
        if (ImGui::Button("基準画面オフセットをリセット##ResetBaseScreenOffset")) {
            ResetBaseScreenOffsetParameters();
        }
        if (ImGui::Button("先行・追従を固定して配置確認##FreezeLeadForPlacementTest")) {
            freezeLeadState_ = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Glowを一時無効化して配置確認##DisableGlowForPlacementTest")) {
            disableGlow_ = true;
        }
    }

    if (ImGui::CollapsingHeader("奥行き表現##DepthAppearanceSection", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("画面比率から奥枠サイズを自動計算##AutoFarSizeFromScreenRatio", &autoFarSizeFromScreenRatio_);
        ImGui::DragFloat("奥枠／手前枠の目標画面高さ比##TargetFarToNearScreenHeightRatio",
            &targetFarToNearScreenHeightRatio_, 0.005f, 0.55f, 0.65f, "%.3f");
        ImGui::Text("手前枠の実効ワールド高さ: %.4f", nearWorldHeight_);
        ImGui::DragFloat("奥枠の手動ワールド高さ##FarManualWorldHeight", &farWorldHeight_, 0.05f, 0.01f, 100.0f);
        ImGui::Text("奥枠の実効ワールド高さ: %.4f", effectiveFarWorldHeight_);
        ImGui::Text("手前枠のカメラ深度: %.4f", nearLeadDepth_);
        ImGui::Text("奥枠のカメラ深度: %.4f", farLeadDepth_);
        ImGui::Text("手前枠の投影高さ推定: %.7f", nearProjectedHeightEstimate_);
        ImGui::Text("奥枠の投影高さ推定: %.7f", farProjectedHeightEstimate_);
        ImGui::Text("現在の奥枠／手前枠画面比率: %.5f", currentFarToNearScreenHeightRatio_);
        ImGui::Text("奥枠／手前枠の目標比率: %.5f", targetFarToNearScreenHeightRatio_);
        ImGui::Text("比率誤差: %.6f", depthAppearanceRatioError_);
        ImGui::Text("奥行き計算が有効: %s", depthAppearanceValid_ ? "はい" : "いいえ");
    }

    if (ImGui::CollapsingHeader("先行・追従##LeadLagSection", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Checkbox("先行・追従を有効化##EnableLeadLag", &leadLagEnabled_) && !leadLagEnabled_) {
            ResetLeadState();
        }
        ImGui::Text("プレイヤー移動入力（生値）: %.4f, %.4f", rawPlayerMoveInput_.x, rawPlayerMoveInput_.y);
        ImGui::Text("正規化済み移動入力: %.4f, %.4f", normalizedMoveInput_.x, normalizedMoveInput_.y);
        ImGui::DragFloat("手前枠の先行量 X##NearLeadAmountX", &nearLeadAmountX_, 0.001f, 0.0f, 0.10f, "%.3f");
        ImGui::DragFloat("手前枠の先行量 Y##NearLeadAmountY", &nearLeadAmountY_, 0.001f, 0.0f, 0.08f, "%.3f");
        ImGui::DragFloat("奥枠の先行量 X##FarLeadAmountX", &farLeadAmountX_, 0.001f, 0.0f, 0.18f, "%.3f");
        ImGui::DragFloat("奥枠の先行量 Y##FarLeadAmountY", &farLeadAmountY_, 0.001f, 0.0f, 0.14f, "%.3f");
        ImGui::DragFloat("手前枠の応答時間##NearResponseTime", &nearResponseTime_, 0.001f, 0.001f, 2.0f, "%.3f s");
        ImGui::DragFloat("奥枠の応答時間##FarResponseTime", &farResponseTime_, 0.001f, 0.001f, 2.0f, "%.3f s");
        ImGui::DragFloat("手前枠の復帰時間##NearReturnTime", &nearReturnTime_, 0.001f, 0.001f, 2.0f, "%.3f s");
        ImGui::DragFloat("奥枠の復帰時間##FarReturnTime", &farReturnTime_, 0.001f, 0.001f, 2.0f, "%.3f s");
        ImGui::Separator();
        ImGui::Text("手前枠の目標画面オフセット: %.5f, %.5f", nearLeadTargetScreen_.x, nearLeadTargetScreen_.y);
        ImGui::Text("手前枠の現在画面オフセット: %.5f, %.5f", nearLeadCurrentScreen_.x, nearLeadCurrentScreen_.y);
        ImGui::Text("奥枠の目標画面オフセット: %.5f, %.5f", farLeadTargetScreen_.x, farLeadTargetScreen_.y);
        ImGui::Text("奥枠の現在画面オフセット: %.5f, %.5f", farLeadCurrentScreen_.x, farLeadCurrentScreen_.y);
        ImGui::Text("手前枠のワールド先行オフセット: %.3f, %.3f, %.3f",
            nearLeadWorldOffset_.x, nearLeadWorldOffset_.y, nearLeadWorldOffset_.z);
        ImGui::Text("奥枠のワールド先行オフセット: %.3f, %.3f, %.3f",
            farLeadWorldOffset_.x, farLeadWorldOffset_.y, farLeadWorldOffset_.z);
        ImGui::Text("手前枠の基準中心: %.3f, %.3f, %.3f", nearBaseCenter_.x, nearBaseCenter_.y, nearBaseCenter_.z);
        ImGui::Text("奥枠の基準中心: %.3f, %.3f, %.3f", farBaseCenter_.x, farBaseCenter_.y, farBaseCenter_.z);
        ImGui::Text("手前枠の最終中心: %.3f, %.3f, %.3f", nearCenter_.x, nearCenter_.y, nearCenter_.z);
        ImGui::Text("奥枠の最終中心: %.3f, %.3f, %.3f", farCenter_.x, farCenter_.y, farCenter_.z);
        ImGui::Text("手前／奥のカメラ深度: %.3f / %.3f", nearLeadDepth_, farLeadDepth_);
        ImGui::Text("FOV Y / アスペクト比: %.5f / %.5f", leadFovY_, leadAspectRatio_);
    }

    if (ImGui::CollapsingHeader("画面投影情報##ProjectionSection", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (aimOriginProjectionValid_) {
            ImGui::Text("照準原点の画面UV: %.6f, %.6f", aimOriginScreenUv_.x, aimOriginScreenUv_.y);
        } else {
            ImGui::Text("照準原点の画面UV: 無効");
        }
        if (nearProjectionValid_) {
            ImGui::Text("手前枠の画面UV: %.6f, %.6f", nearScreenUv_.x, nearScreenUv_.y);
        } else {
            ImGui::Text("手前枠の画面UV: 無効");
        }
        if (farProjectionValid_) {
            ImGui::Text("奥枠の画面UV: %.6f, %.6f", farScreenUv_.x, farScreenUv_.y);
        } else {
            ImGui::Text("奥枠の画面UV: 無効");
        }
        ImGui::Text("手前枠の画面差分: %.6f, %.6f", nearScreenDelta_.x, nearScreenDelta_.y);
        ImGui::Text("奥枠の画面差分: %.6f, %.6f", farScreenDelta_.x, farScreenDelta_.y);
    }

    if (ImGui::CollapsingHeader("サイズ設定##SizeSection", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("手前枠のワールド高さ##NearWorldHeight", &nearWorldHeight_, 0.05f, 0.01f, 100.0f);
        ImGui::Text("手前枠のワールド幅: %.3f", nearWorldWidth_);
        ImGui::DragFloat("奥枠の手動ワールド高さ##FarManualHeight", &farWorldHeight_, 0.05f, 0.01f, 100.0f);
        ImGui::Text("奥枠の実効ワールド高さ: %.3f", effectiveFarWorldHeight_);
        ImGui::Text("奥枠の実効ワールド幅: %.3f", farWorldWidth_);
        ImGui::Text("テクスチャの縦横比を維持: はい");
    }

    if (ImGui::CollapsingHeader("手前枠の見た目##NearAppearanceSection")) {
        ImGui::DragFloat("手前枠の透明度##NearAlpha", &nearAppearance_.alpha, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("手前枠の本体輝度##NearCoreIntensity", &nearAppearance_.coreIntensity, 0.01f, 0.0f, 2.0f);
        ImGui::DragFloat("手前枠のGlow強度##NearGlowIntensity", &nearAppearance_.glowIntensity, 0.01f, 0.0f, 2.0f);
        ImGui::DragFloat("手前枠のGlow透明度##NearGlowAlpha", &nearAppearance_.glowAlpha, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("手前枠のGlow範囲##NearGlowRadius", &nearAppearance_.glowRadiusTexels, 0.05f, 0.0f, 4.0f);
        ImGui::DragFloat("手前枠の脈動量##NearPulseAmount", &nearAppearance_.pulseAmount, 0.005f, 0.0f, 0.25f);
    }

    if (ImGui::CollapsingHeader("奥枠の見た目##FarAppearanceSection")) {
        ImGui::DragFloat("奥枠の透明度##FarAlpha", &farAppearance_.alpha, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("奥枠の本体輝度##FarCoreIntensity", &farAppearance_.coreIntensity, 0.01f, 0.0f, 2.0f);
        ImGui::DragFloat("奥枠のGlow強度##FarGlowIntensity", &farAppearance_.glowIntensity, 0.01f, 0.0f, 2.0f);
        ImGui::DragFloat("奥枠のGlow透明度##FarGlowAlpha", &farAppearance_.glowAlpha, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("奥枠のGlow範囲##FarGlowRadius", &farAppearance_.glowRadiusTexels, 0.05f, 0.0f, 4.0f);
        ImGui::DragFloat("奥枠の脈動量##FarPulseAmount", &farAppearance_.pulseAmount, 0.005f, 0.0f, 0.25f);
    }

    if (ImGui::CollapsingHeader("共通の見た目##CommonAppearanceSection")) {
        ImGui::DragFloat("脈動速度##PulseRate", &pulseRate_, 0.05f, 0.0f, 10.0f);
        ImGui::Checkbox("脈動を固定##FreezePulse", &freezePulse_);
        ImGui::Checkbox("Glowを無効化##DisableGlow", &disableGlow_);
        ImGui::Checkbox("本体線だけ表示##ShowCoreOnly", &showCoreOnly_);
        ImGui::Checkbox("透明度を最大に固定##ForceFullAlpha", &forceFullAlpha_);
    }

    if (ImGui::CollapsingHeader("テスト操作##TestSection", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Checkbox("強制表示##ForceShow", &forceShow_) && forceShow_) {
            forceHide_ = false;
        }
        if (ImGui::Checkbox("強制非表示##ForceHide", &forceHide_) && forceHide_) {
            forceShow_ = false;
        }
        if (ImGui::Button("カメラ前方向軸を強制使用##ForceCameraForwardAxis")) {
            axisMode_ = AxisMode::CameraForward;
        }
        if (ImGui::Button("プレイヤー通過軸を強制使用##ForceCameraThroughPlayerAxis")) {
            axisMode_ = AxisMode::CameraThroughAimOrigin;
        }
        ImGui::SameLine();
        if (ImGui::Button("配置軸モードをリセット##ResetAxisMode")) {
            axisMode_ = AxisMode::CameraThroughAimOrigin;
        }
        if (ImGui::Button("左入力を強制##ForceMoveInputLeft")) {
            forceMoveInputForDebug_ = true;
            forcedMoveInputForDebug_ = { -1.0f, 0.0f };
        }
        ImGui::SameLine();
        if (ImGui::Button("右入力を強制##ForceMoveInputRight")) {
            forceMoveInputForDebug_ = true;
            forcedMoveInputForDebug_ = { 1.0f, 0.0f };
        }
        if (ImGui::Button("上入力を強制##ForceMoveInputUp")) {
            forceMoveInputForDebug_ = true;
            forcedMoveInputForDebug_ = { 0.0f, 1.0f };
        }
        ImGui::SameLine();
        if (ImGui::Button("下入力を強制##ForceMoveInputDown")) {
            forceMoveInputForDebug_ = true;
            forcedMoveInputForDebug_ = { 0.0f, -1.0f };
        }
        if (ImGui::Button("強制移動入力を解除##ForceMoveInputOff")) {
            forceMoveInputForDebug_ = true;
            forcedMoveInputForDebug_ = {};
        }
        ImGui::SameLine();
        if (ImGui::Button("プレイヤー移動入力を使用##UsePlayerMoveInput")) {
            forceMoveInputForDebug_ = false;
        }
        ImGui::Checkbox("先行・追従状態を固定##FreezeLeadState", &freezeLeadState_);
        if (ImGui::Checkbox("デバッグ用にプレイヤー位置を固定##FreezePlayerPositionForDebug", &freezePlayerPositionForDebug_)
            && freezePlayerPositionForDebug_) {
            frozenPlayerRenderPosition_ = playerRenderPosition_;
        }
        if (ImGui::Checkbox("デバッグ用にカメラ位置を固定##FreezeCameraForDebug", &freezeCameraForDebug_) && freezeCameraForDebug_) {
            frozenCameraPosition_ = cameraPosition_;
            frozenCameraForward_ = cameraForward_;
            frozenCameraRight_ = cameraRight_;
            frozenCameraUp_ = cameraUp_;
        }
        if (ImGui::Button("推奨比率 0.60 を適用##ApplyRecommendedRatio")) {
            autoFarSizeFromScreenRatio_ = true;
            targetFarToNearScreenHeightRatio_ = 0.60f;
        }
        if (ImGui::Button("比率 0.55 を適用##ApplyRatio055")) {
            autoFarSizeFromScreenRatio_ = true;
            targetFarToNearScreenHeightRatio_ = 0.55f;
        }
        ImGui::SameLine();
        if (ImGui::Button("比率 0.65 を適用##ApplyRatio065")) {
            autoFarSizeFromScreenRatio_ = true;
            targetFarToNearScreenHeightRatio_ = 0.65f;
        }
        if (ImGui::Button("奥枠の手動高さを使用##UseManualFarHeight")) {
            autoFarSizeFromScreenRatio_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("奥行き表現をリセット##ResetDepthAppearance")) {
            ResetDepthAppearanceParameters();
        }
        if (ImGui::Button("Glowを一時的に無効化##TemporarilyDisableGlow")) {
            disableGlow_ = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Glow設定を復元##RestoreGlowSettings")) {
            disableGlow_ = false;
        }
        ImGui::Checkbox("サイズ確認用に先行・追従を固定##FreezeLeadForSizeTest", &freezeLeadState_);
        if (ImGui::Button("先行・追従設定をリセット##ResetLeadParameters")) {
            ResetLeadParameters();
            ResetLeadState();
        }
        ImGui::SameLine();
        if (ImGui::Button("先行・追従状態をリセット##ResetLeadState")) {
            ResetLeadState();
        }
        if (ImGui::Button("見た目設定をリセット##ResetVisualParameters")) {
            ResetVisualParameters();
        }
        ImGui::SameLine();
        if (ImGui::Button("位置設定をリセット##ResetPositionParameters")) {
            ResetPositionParameters();
        }
    }

    ImGui::End();
#endif
}
