#include "BlenderRailPipelineVerification.h"

#include "BlenderRailPipelineVerificationRenderer.h"
#include "Engine/Game/RailShooter/RailPathRuntimeV2.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Level/LevelSceneData.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"

#include <cmath>
#include <cstdio>

namespace {
float Distance(const Vector3& a, const Vector3& b) {
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return std::sqrt(x * x + y * y + z * z);
}

void DrawVector(const char* label, const Vector3& value) {
    ImGui::Text("%s: (%.3f, %.3f, %.3f)", label, value.x, value.y, value.z);
}

void DrawState(bool ok, const char* okText, const char* errorText, bool warning = false) {
    const ImVec4 color = ok
        ? ImVec4(0.10f, 0.85f, 0.78f, 1.0f)
        : warning ? ImVec4(1.0f, 0.82f, 0.15f, 1.0f) : ImVec4(1.0f, 0.28f, 0.12f, 1.0f);
    ImGui::TextColored(color, "[%s] %s", ok ? "OK" : warning ? "警告" : "エラー", ok ? okText : errorText);
}

void DrawUnknown(const char* text) {
    ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.55f, 1.0f), "[未確認] %s", text);
}
}
#endif

void BlenderRailPipelineVerification::DrawImGui(
    const LevelSceneData& sceneData,
    const LevelRailRuntime& railRuntime,
    bool axisConversionEnabled) {
#ifdef USE_IMGUI
    if (camera_ && statistics_.convertedPoints.valid) {
        statistics_.cameraToStartDistance = Distance(camera_->GetTranslate(), statistics_.convertedPoints.first);
        statistics_.cameraToCenterDistance = Distance(camera_->GetTranslate(), statistics_.convertedPoints.boundsCenter);
    }

    ImGui::SetNextWindowSize(ImVec2(720.0f, 820.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Blenderレール連携確認###BlenderRailPipelineVerification")) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("レール連携を一括確認##VerifyAll")) {
        if (selectedRailId_.empty()) SelectFirstValidRail(sceneData);
        if (renderer_) {
            auto& config = renderer_->GetConfig();
            config.enabled = true;
            config.showRuntimeLine = true;
            config.showNodes = true;
            config.showDistanceMarkers = true;
            config.showStartEnd = true;
            config.showLegacyLine = false;
            config.showTangents = false;
        }
        BuildPreview(sceneData, railRuntime, axisConversionEnabled, "一括確認");
    }
    ImGui::SameLine();
    if (ImGui::Button("読込情報だけ更新##RefreshReadInfo")) {
        RefreshStageInformation(sceneData, railRuntime, axisConversionEnabled);
        nextAction_ = "必要に応じてRuntime V2を再構築してください。";
    }
    ImGui::SameLine();
    if (ImGui::Button("Runtime V2を再構築##ManualRebuild")) {
        BuildPreview(sceneData, railRuntime, axisConversionEnabled, "手動Rebuild");
    }

    if (ImGui::CollapsingHeader("1. データ取得元##DataSource", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("現在のデータ取得元: %s", DataSourceName(dataSource_));
        ImGui::Text("最後の読込または反映理由: %s", lastApplyReason_.c_str());
        ImGui::Text("最後の読込または反映時刻: %s", lastApplyTime_.c_str());
        ImGui::TextWrapped("Stage JSONの要求パス: %s", requestedJsonPath_.empty() ? "未設定" : requestedJsonPath_.c_str());
        ImGui::TextWrapped("Stage JSONの解決済み絶対パス: %s", resolvedJsonPath_.empty() ? "未確認" : resolvedJsonPath_.c_str());
        ImGui::Text("Stage JSONの存在: %s", jsonExists_ ? "あり" : "なし");
        ImGui::TextWrapped("最後のStage JSON読込結果: %s", lastJsonLoadResult_.empty() ? "未確認" : lastJsonLoadResult_.c_str());
        ImGui::Separator();
        ImGui::Text("UDP Receiverの動作状態: %s", liveSync_.receiverRunning ? "受信中" : "停止中");
        ImGui::Text("UDP受信パケット数: %llu", static_cast<unsigned long long>(liveSync_.receivedPacketCount));
        ImGui::Text("UDP反映パケット数: %llu", static_cast<unsigned long long>(liveSync_.appliedPacketCount));
        ImGui::Text("自動反映: %s", liveSync_.autoApplyEnabled ? "有効" : "無効");
        ImGui::Text("最後のUDP受信時刻: %s", liveSync_.lastReceiveTime.empty() ? "未受信" : liveSync_.lastReceiveTime.c_str());
        ImGui::TextWrapped("最後のUDP反映結果: %s", liveSync_.lastApplyStatus.empty() ? "未確認" : liveSync_.lastApplyStatus.c_str());
        ImGui::TextWrapped("最後のエラー: %s", liveSync_.lastError.empty() ? "なし" : liveSync_.lastError.c_str());
        ImGui::TextWrapped("Stage JSONはBlenderでExportしたファイルを、ゲーム起動時または手動Reload時に読み込みます。");
        ImGui::TextWrapped("UDP Live SyncはBlenderからゲームへ直接送信されたデータです。Stage JSONファイルは更新されません。");
    }

    if (ImGui::CollapsingHeader("2. Stage読込状態##StageStatus", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawState(!sceneData.name.empty() || !sceneData.rails.empty() || !sceneData.objects.empty(),
            "Stageデータを読み込みました", "Stageデータを確認できません");
        ImGui::Text("Stage名: %s", sceneData.name.empty() ? "未設定" : sceneData.name.c_str());
        ImGui::Text("読込Rail数: %zu", railCount_);
        ImGui::Text("有効Rail数: %zu", statistics_.validRailCount);
        ImGui::Text("無効Rail数: %zu", statistics_.invalidRailCount);
        ImGui::Text("座標変換: %s", axisConversionEnabled
            ? "Blender (x,y,z) → Game (x,z,y) をLevelRailRuntime段階で1回適用"
            : "無効");
    }

    if (ImGui::CollapsingHeader("3. Rail読込状態##RailStatus", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("確認対象Rail Index: %s", selectedRailIndex_ == static_cast<size_t>(-1)
            ? "未選択" : std::to_string(selectedRailIndex_).c_str());
        ImGui::Text("確認対象Rail名: %s", selectedRailName_.empty() ? "未選択" : selectedRailName_.c_str());
        ImGui::Text("Rail ID: %s", selectedRailId_.empty() ? "未選択" : selectedRailId_.c_str());
        ImGui::Text("Rail Type: %s", selectedRailType_.empty() ? "未設定" : selectedRailType_.c_str());
        ImGui::Text("Waypoint数: %zu", selectedWaypointCount_);
        ImGui::Text("Sample Count: %zu（JSON WaypointをLegacy評価した点数）", legacySampledPoints_.size());
        ImGui::Text("Loop: %s / Reverse Direction: %s / Visible: %s / Speed: %.3f",
            selectedRailLoop_ ? "有効" : "無効", selectedRailReverse_ ? "有効" : "無効",
            selectedRailVisible_ ? "表示" : "非表示", selectedRailSpeed_);
        ImGui::Text("非有限値の数: %zu", statistics_.nonFinitePointCount);
        ImGui::Text("連続重複点の数: %zu", statistics_.consecutiveDuplicateCount);
        ImGui::Text("全長0候補: %s", statistics_.zeroLengthCandidate ? "該当" : "非該当");
        ImGui::TextWrapped("最後のRail読込エラー: %s", lastRailReadError_.empty() ? "なし" : lastRailReadError_.c_str());
        if (statistics_.loaderPoints.valid && ImGui::TreeNode("Loader読込座標##LoaderPoints")) {
            DrawVector("先頭", statistics_.loaderPoints.first);
            DrawVector("中間", statistics_.loaderPoints.middle);
            DrawVector("最後", statistics_.loaderPoints.last);
            ImGui::TreePop();
        }
        if (statistics_.convertedPoints.valid && ImGui::TreeNode("LevelRailRuntime変換後座標##ConvertedPoints")) {
            DrawVector("先頭", statistics_.convertedPoints.first);
            DrawVector("中間", statistics_.convertedPoints.middle);
            DrawVector("最後", statistics_.convertedPoints.last);
            DrawVector("最大X付近", statistics_.convertedPoints.maximumX);
            DrawVector("最大Y付近", statistics_.convertedPoints.maximumY);
            DrawVector("最大Z付近", statistics_.convertedPoints.maximumZ);
            ImGui::TreePop();
        }
        if (statistics_.runtimePoints.valid && ImGui::TreeNode("Runtime V2 Node座標##RuntimePoints")) {
            DrawVector("先頭", statistics_.runtimePoints.first);
            DrawVector("中間", statistics_.runtimePoints.middle);
            DrawVector("最後", statistics_.runtimePoints.last);
            ImGui::TreePop();
        }
    }

    if (ImGui::CollapsingHeader("4. 確認対象Rail##RailSelection", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* preview = selectedRailName_.empty() ? "未選択" : selectedRailName_.c_str();
        if (ImGui::BeginCombo("Rail一覧##PreviewRailCombo", preview)) {
            for (size_t index = 0; index < sceneData.rails.size(); ++index) {
                const LevelRail& rail = sceneData.rails[index];
                const bool selected = index == selectedRailIndex_;
                const std::string label = (rail.name.empty() ? "名称なし" : rail.name)
                    + "##RailItem" + std::to_string(index);
                if (ImGui::Selectable(label.c_str(), selected)) {
                    selectedRailId_ = rail.railId.empty() ? rail.name : rail.railId;
                    selectedRailIndex_ = index;
                    BuildPreview(sceneData, railRuntime, axisConversionEnabled, "Preview Rail選択変更");
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (ImGui::Button("最初の有効レールを選択##SelectFirstValid")) {
            SelectFirstValidRail(sceneData);
            BuildPreview(sceneData, railRuntime, axisConversionEnabled, "Preview Rail選択変更");
        }
        ImGui::SameLine();
        if (ImGui::Button("前のRail##PreviousRail")) {
            SelectRelativeRail(sceneData, -1);
            BuildPreview(sceneData, railRuntime, axisConversionEnabled, "Preview Rail選択変更");
        }
        ImGui::SameLine();
        if (ImGui::Button("次のRail##NextRail")) {
            SelectRelativeRail(sceneData, 1);
            BuildPreview(sceneData, railRuntime, axisConversionEnabled, "Preview Rail選択変更");
        }
        ImGui::SameLine();
        if (ImGui::Button("選択を解除##ClearRailSelection")) {
            selectedRailId_.clear();
            selectedRailName_.clear();
            selectedRailType_.clear();
            selectedRailIndex_ = static_cast<size_t>(-1);
            Clear();
        }
        ImGui::TextDisabled("この選択は診断Preview専用です。Gameplay Camera、Enemy、EventFlagには反映しません。");
    }

    if (ImGui::CollapsingHeader("5. Runtime V2 Build状態##BuildStatus", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("最後のBuild理由: %s", lastBuildReason_.c_str());
        ImGui::Text("Build回数: %llu / 成功: %llu / 失敗: %llu",
            static_cast<unsigned long long>(buildCount_), static_cast<unsigned long long>(buildSuccessCount_),
            static_cast<unsigned long long>(buildFailureCount_));
        ImGui::Text("最後のBuild時刻: %s", lastBuildTime_.c_str());
        ImGui::Text("使用Rail名: %s / ID: %s", buildRailName_.empty() ? "未構築" : buildRailName_.c_str(),
            buildRailId_.empty() ? "未構築" : buildRailId_.c_str());
        ImGui::Text("Adapter入力Waypoint数: %zu / Reverse適用: %s", adapterInputCount_, selectedRailReverse_ ? "あり" : "なし");
        ImGui::Text("Adapter変換: %s", adapterSucceeded_ ? "成功" : "未成功");
        if (previewRuntime_) {
            ImGui::Text("重複除外数: %zu", previewRuntime_->GetDuplicateNodeSkipCount());
            ImGui::Text("Runtime Node数: %zu / Segment数: %zu", previewRuntime_->GetNodeCount(), previewRuntime_->GetSegmentCount());
            ImGui::Text("Arc Sample数: %zu / Total Length: %.3f", previewRuntime_->GetArcLengthSampleCount(), previewRuntime_->GetTotalLength());
        }
        ImGui::Text("Open / Loop: %s", selectedRailLoop_ ? "Loop指定（Runtime V2診断線は開始・終了を識別）" : "Open");
        ImGui::TextColored(buildSucceeded_ ? ImVec4(0.10f, 0.85f, 0.78f, 1.0f) : ImVec4(1.0f, 0.28f, 0.12f, 1.0f),
            "最後のBuild結果: %s", lastBuildResult_.c_str());
        ImGui::TextWrapped("最後のBuildエラー: %s", lastBuildError_.empty() ? "なし" : lastBuildError_.c_str());
    }

    if (ImGui::CollapsingHeader("6. 3Dデバッグ表示##DebugDraw", ImGuiTreeNodeFlags_DefaultOpen) && renderer_) {
        auto& config = renderer_->GetConfig();
        bool rebuild = false;
        rebuild |= ImGui::Checkbox("3Dデバッグ表示##DebugEnabled", &config.enabled);
        rebuild |= ImGui::Checkbox("Runtime V2線##RuntimeLine", &config.showRuntimeLine);
        ImGui::SameLine();
        rebuild |= ImGui::Checkbox("Node##Nodes", &config.showNodes);
        ImGui::SameLine();
        rebuild |= ImGui::Checkbox("等距離Marker##Markers", &config.showDistanceMarkers);
        rebuild |= ImGui::Checkbox("Start / End##Endpoints", &config.showStartEnd);
        ImGui::SameLine();
        rebuild |= ImGui::Checkbox("Legacy Rail##LegacyLine", &config.showLegacyLine);
        ImGui::SameLine();
        rebuild |= ImGui::Checkbox("Tangent##Tangents", &config.showTangents);
        rebuild |= ImGui::SliderFloat("Marker間隔##MarkerInterval", &config.markerInterval, 1.0f, 1000.0f, "%.1f");
        rebuild |= ImGui::SliderFloat("Tangent間隔##TangentInterval", &config.tangentInterval, 1.0f, 1000.0f, "%.1f");
        if (rebuild && buildSucceeded_) RebuildRenderer();
        if (ImGui::Button("レール可視化を有効化##EnableVisualization")) config.enabled = true;
        ImGui::SameLine();
        if (ImGui::Button("レール可視化を無効化##DisableVisualization")) config.enabled = false;
        const auto& stats = renderer_->GetStats();
        ImGui::Text("Line数: %zu / 上限: %zu / 省略: %zu", stats.lineCount, config.maximumLineCount, stats.skippedLineCount);
        ImGui::Text("Node数: %zu / 上限: %zu / 省略: %zu", stats.nodeCount, config.maximumNodeCount, stats.skippedNodeCount);
        ImGui::Text("Marker数: %zu / 上限: %zu / 省略: %zu", stats.markerCount, config.maximumMarkerCount, stats.skippedMarkerCount);
        ImGui::Text("Tangent数: %zu / 上限: %zu / 省略: %zu", stats.tangentCount, config.maximumTangentCount, stats.skippedTangentCount);
    }

    if (ImGui::CollapsingHeader("Pipeline状態##PipelineState", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawState(!sceneData.name.empty() || !sceneData.rails.empty(), "Stageデータが存在します", "Stageデータがありません");
        DrawState(railCount_ > 0, "Railが1本以上存在します", "Railがありません");
        DrawState(!selectedRailId_.empty(), "確認対象Railが選択されています", "確認対象Railが未選択です");
        DrawState(selectedWaypointCount_ >= 2, "Waypointが2点以上あります", "Waypoint数が不足しています");
        DrawState(statistics_.nonFinitePointCount == 0, "Waypointは有限値です", "非有限値があります");
        DrawState(coordinateConversionApplied_, "LevelRailRuntime段階の座標変換を適用しました", "座標変換が無効です", !coordinateConversionApplied_);
        DrawState(adapterSucceeded_, "Adapter変換に成功しました", "Adapter変換は未成功です");
        DrawState(buildSucceeded_, "Runtime V2 Buildに成功しました", "Runtime V2 Buildは未成功です");
        DrawState(previewRuntime_ && !previewRuntime_->GetArcLengthTable().empty(), "Arc Length Table構築に成功しました", "Arc Length Tableは未構築です");
        DrawState(renderer_ && renderer_->GetConfig().enabled, "3Dデバッグ表示が有効です", "3Dデバッグ表示が無効です", true);
    }

    if (ImGui::CollapsingHeader("7. 次に行う操作##NextAction", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("%s", nextAction_.c_str());
        if (statistics_.convertedPoints.valid) {
            DrawVector("Rail開始位置", statistics_.convertedPoints.first);
            DrawVector("Rail終了位置", statistics_.convertedPoints.last);
            DrawVector("Rail Bounds最小値", statistics_.convertedPoints.boundsMinimum);
            DrawVector("Rail Bounds最大値", statistics_.convertedPoints.boundsMaximum);
            DrawVector("Rail Bounds中心", statistics_.convertedPoints.boundsCenter);
            if (camera_) DrawVector("Camera位置", camera_->GetTranslate());
            ImGui::Text("CameraからRail開始点までの距離: %.3f", statistics_.cameraToStartDistance);
            ImGui::Text("CameraからRail Bounds中心までの距離: %.3f", statistics_.cameraToCenterDistance);
        }
    }

    if (ImGui::CollapsingHeader("8. エラー・警告##ErrorsWarnings", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (railCount_ == 0) ImGui::TextWrapped("現在のStageデータにRailがありません。Blenderでレールを生成し、Live Sync送信またはStage Exportを実行してください。");
        else if (selectedWaypointCount_ < 2) ImGui::TextWrapped("選択RailのWaypoint数が2未満です。Blender側のCurveとSample Countを確認してください。");
        else if (!buildSucceeded_) ImGui::TextWrapped("Railは読み込まれていますが、Runtime V2を構築できません。重複点、非有限値、Rail全長を確認してください。");
        else ImGui::TextWrapped("Runtime V2のBuildには成功しています。線が見えない場合は、3Dデバッグ表示、Camera位置、Rail開始位置を確認してください。");
        ImGui::TextWrapped("UDP Live Syncの内容はStage JSONファイルへ保存されません。現在のデータ取得元を確認してください。");
        if (statistics_.cameraToCenterDistance > 5000.0f) ImGui::TextWrapped("Railが現在のCameraから離れた位置にあります。Debug Cameraを使用するか、Blender側の開始位置を確認してください。");
    }

    if (ImGui::Button("診断をリセット##ResetDiagnostics")) ResetDiagnostics();
    ImGui::End();
#else
    static_cast<void>(sceneData);
    static_cast<void>(axisConversionEnabled);
#endif
}
