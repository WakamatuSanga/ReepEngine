#include "RailPathRuntimeV2.h"

#include "Engine/Graphics/Camera/Camera.h"

#include <algorithm>
#include <cstdio>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
#ifdef USE_IMGUI
void HelpMarker(const char* text) {
    ImGui::SameLine(); ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip(); ImGui::PushTextWrapPos(ImGui::GetFontSize() * 32.0f);
        ImGui::TextUnformatted(text); ImGui::PopTextWrapPos(); ImGui::EndTooltip();
    }
}

Vector4 TransformPoint(const Vector3& value, const Matrix4x4& matrix) {
    return {
        value.x * matrix.m[0][0] + value.y * matrix.m[1][0] + value.z * matrix.m[2][0] + matrix.m[3][0],
        value.x * matrix.m[0][1] + value.y * matrix.m[1][1] + value.z * matrix.m[2][1] + matrix.m[3][1],
        value.x * matrix.m[0][2] + value.y * matrix.m[1][2] + value.z * matrix.m[2][2] + matrix.m[3][2],
        value.x * matrix.m[0][3] + value.y * matrix.m[1][3] + value.z * matrix.m[2][3] + matrix.m[3][3],
    };
}
#endif
}

void RailPathRuntimeV2::DrawImGui() {
#ifdef USE_IMGUI
    if (!ImGui::Begin("レールパスRuntime V2デバッグ###RailPathRuntimeV2Debug")) { ImGui::End(); return; }
    bool rebuildDebug = false;

    if (ImGui::CollapsingHeader("構築状態", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Runtime V2を有効化###RuntimeEnabled", &runtimeEnabled_);
        HelpMarker("Runtime V2はデバッグプレビュー専用で、通常GameModeのCameraにはまだ接続していません。");
        ImGui::Text("Build成功: %s", valid_ ? "はい" : "いいえ");
        ImGui::Text("構築結果: %s", lastBuildResult_.c_str());
        ImGui::Text("有効: %s", valid_ ? "はい" : "いいえ");
        ImGui::Text("Adapter方式: %s", adapterMode_.c_str());
        ImGui::Text("入力ノード数: %zu / 有効ノード数: %zu", sourceNodes_.size(), nodes_.size());
        ImGui::Text("除外した連続重複: %zu", duplicateNodeSkipCount_);
        ImGui::Text("セグメント数: %zu / Arcサンプル数: %zu / 全長: %.3f", GetSegmentCount(), arcLengthTable_.size(), totalLength_);
        if (!lastError_.empty()) ImGui::TextColored({ 1.0f, 0.35f, 0.25f, 1.0f }, "エラー: %s", lastError_.c_str());
    }

    if (ImGui::CollapsingHeader("Spline設定", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextUnformatted("補間方式: Centripetal Catmull–Rom");
        ImGui::Text("alpha: %.2f", alpha_); HelpMarker("alpha=0.5 の centripetal パラメータ化を固定で使用します。");
        ImGui::Text("オープンレール: %s", openRail_ ? "はい" : "いいえ");
        bool closed = false; ImGui::BeginDisabled(); ImGui::Checkbox("Closed Loop（今回は無効）###ClosedRail", &closed); ImGui::EndDisabled();
        ImGui::DragFloat("連続重複判定距離###DuplicateNodeEpsilon", &duplicateNodeDistanceEpsilon_, 0.0001f, 0.00001f, 0.1f, "%.5f");
        duplicateNodeDistanceEpsilon_ = std::clamp(duplicateNodeDistanceEpsilon_, 0.00001f, 0.1f);
        HelpMarker("隣接ノードがこの距離以下なら、構築時に後側を除外します。");
    }

    if (ImGui::CollapsingHeader("Arc Length設定", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderInt("セグメント当たりサンプル数###SamplesPerSegment", &samplesPerSegment_, 8, 128);
        HelpMarker("増やすと距離近似の精度が上がりますが、再構築時間とメモリ使用量も増えます。");
        if (ImGui::Button("Tableを再構築###RebuildArcLengthTable")) Rebuild();
        HelpMarker("Arc Length Tableは距離からSpline上の位置を一定速度に近く検索するための累積距離表です。");
        ImGui::Text("テーブル要素数: %zu", arcLengthTable_.size());
        ImGui::Text("テーブル有効: %s", arcTableValid_ ? "はい" : "いいえ");
        ImGui::Text("累積距離が単調増加: %s", cumulativeDistanceMonotonic_ ? "はい" : "いいえ");
        if (!arcLengthTable_.empty()) ImGui::Text("開始距離 / 終了距離: %.3f / %.3f", arcLengthTable_.front().cumulativeDistance, arcLengthTable_.back().cumulativeDistance);
    }

    if (ImGui::CollapsingHeader("サンプル確認", ImGuiTreeNodeFlags_DefaultOpen)) {
        const float maxDistance = (std::max)(totalLength_, 0.001f);
        if (ImGui::SliderFloat("距離###PreviewDistance", &previewDistance_, 0.0f, maxDistance, "%.3f")) {
            previewDistance_ = std::clamp(previewDistance_, 0.0f, totalLength_);
            previewNormalizedDistance_ = totalLength_ > 0.0f ? previewDistance_ / totalLength_ : 0.0f;
            previewSample_ = SampleByDistance(previewDistance_);
        }
        if (ImGui::SliderFloat("正規化距離###PreviewNormalizedDistance", &previewNormalizedDistance_, 0.0f, 1.0f, "%.3f")) {
            previewSample_ = SampleByNormalizedDistance(previewNormalizedDistance_); previewDistance_ = previewSample_.distance;
        }
        ImGui::Text("Sample有効: %s / Clamp後の距離: %.3f", previewSample_.valid ? "はい" : "いいえ", previewSample_.distance);
        ImGui::Text("位置: %.3f, %.3f, %.3f", previewSample_.position.x, previewSample_.position.y, previewSample_.position.z);
        ImGui::Text("接線: %.3f, %.3f, %.3f", previewSample_.tangent.x, previewSample_.tangent.y, previewSample_.tangent.z);
        ImGui::Text("セグメント: %u / 区間内 t: %.4f", previewSample_.segmentIndex, previewSample_.segmentT);
    }

    if (ImGui::CollapsingHeader("等距離検証", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("検証間隔###ValidationInterval", &validationInterval_, 0.1f, 0.05f, 20.0f, "%.2f");
        ImGui::Text("検証結果: %s / 区間数: %zu", validationSucceeded_ ? "正常" : "未検証または失敗", validationSampleCount_);
        ImGui::Text("実移動量 最小 / 最大 / 平均: %.4f / %.4f / %.4f", validationMinimumStep_, validationMaximumStep_, validationAverageStep_);
        ImGui::Text("期待距離との差（最大）: %.4f", validationMaximumError_);
    }

    if (ImGui::CollapsingHeader("既存Rail比較")) {
        if (ImGui::SliderInt("比較サンプル数###ComparisonSamples", &comparisonSampleCount_, 8, 256)) RunLegacyComparison();
        ImGui::Text("既存Rail全長: %.3f / 全長差: %.3f", legacyTotalLength_, comparisonLengthDifference_);
        ImGui::Text("開始差 / 終了差: %.4f / %.4f", comparisonStartDifference_, comparisonEndDifference_);
        ImGui::Text("位置差 最大 / 平均: %.4f / %.4f", comparisonMaximumDifference_, comparisonAverageDifference_);
    }

    if (ImGui::CollapsingHeader("3Dデバッグ表示", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Checkbox("3D表示を有効化###DebugDrawEnabled", &debugDrawEnabled_)) rebuildDebug = true;
        if (ImGui::Checkbox("ノード表示###ShowNodes", &showNodes_)) rebuildDebug = true;
        ImGui::Checkbox("ノード番号表示###ShowNodeIndices", &showNodeIndices_);
        if (ImGui::Checkbox("V2ライン表示###ShowRuntimeLine", &showRuntimeLine_)) rebuildDebug = true;
        if (ImGui::Checkbox("既存Railライン表示###ShowLegacyLine", &showLegacyLine_)) rebuildDebug = true;
        if (ImGui::Checkbox("等距離マーカー表示###ShowDistanceMarkers", &showDistanceMarkers_)) rebuildDebug = true;
        ImGui::SameLine(); ImGui::TextColored({ 0.3f, 1.0f, 0.25f, 1.0f }, "一定ワールド距離");
        if (ImGui::Checkbox("接線表示###ShowTangent", &showTangent_)) rebuildDebug = true;
        if (ImGui::DragFloat("マーカー間隔###MarkerInterval", &distanceMarkerInterval_, 0.1f, 0.1f, 50.0f, "%.2f")) rebuildDebug = true;
        if (ImGui::DragFloat("接線長###TangentLength", &tangentLength_, 0.1f, 0.1f, 20.0f, "%.2f")) rebuildDebug = true;
        if (ImGui::DragFloat("線の太さ###LineThickness", &lineThickness_, 0.005f, 0.005f, 0.25f, "%.3f")) rebuildDebug = true;
        if (ImGui::SliderInt("最大Draw数###MaximumDebugDrawCount", &maximumDebugDrawCount_, 32, 2048)) rebuildDebug = true;
        ImGui::Text("今回の描画オブジェクト数: %zu", lastDebugDrawCount_);
    }

    ImGui::SeparatorText("テスト操作");
    if (ImGui::Button("既存Railから再構築###RebuildSelectedRail")) Rebuild();
    ImGui::SameLine();
    if (ImGui::Button("推奨設定を適用###ResetRecommended")) {
        duplicateNodeDistanceEpsilon_ = 0.001f; samplesPerSegment_ = 32; validationInterval_ = 1.0f; distanceMarkerInterval_ = 5.0f; Rebuild();
    }
    if (ImGui::Button("開始位置を確認###PreviewStart")) { previewDistance_ = 0.0f; previewNormalizedDistance_ = 0.0f; previewSample_ = SampleByDistance(0.0f); }
    ImGui::SameLine();
    if (ImGui::Button("中間位置を確認###PreviewMiddle")) { previewDistance_ = totalLength_ * 0.5f; previewNormalizedDistance_ = 0.5f; previewSample_ = SampleByDistance(previewDistance_); }
    ImGui::SameLine();
    if (ImGui::Button("終了位置を確認###PreviewEnd")) { previewDistance_ = totalLength_; previewNormalizedDistance_ = 1.0f; previewSample_ = SampleByDistance(totalLength_); }
    ImGui::SameLine();
    if (ImGui::Button("一定距離検証を実行###RunValidation")) { RunDistanceValidation(); RunLegacyComparison(); }
    if (ImGui::Button("Runtime状態をクリア###ClearBuild")) Clear();
    ImGui::SameLine();
    if (ImGui::Button("Debug表示をリセット###ResetDebugView")) {
        showNodes_ = true; showNodeIndices_ = false; showRuntimeLine_ = true; showLegacyLine_ = false; showDistanceMarkers_ = true; showTangent_ = true; rebuildDebug = true;
    }
    if (rebuildDebug) RebuildDebugObjects();
    DrawNodeIndexOverlay();
    ImGui::End();
#endif
}

void RailPathRuntimeV2::DrawNodeIndexOverlay() const {
#ifdef USE_IMGUI
    if (!debugDrawEnabled_ || !showNodeIndices_ || externalDebugHidden_ || !camera_) return;
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    for (size_t index = 0; index < nodes_.size(); ++index) {
        const Vector4 clip = TransformPoint(nodes_[index].position, camera_->GetViewProjectionMatrix());
        if (clip.w <= 0.0001f) continue;
        const float ndcX = clip.x / clip.w; const float ndcY = clip.y / clip.w; const float ndcZ = clip.z / clip.w;
        if (ndcZ < 0.0f || ndcZ > 1.0f) continue;
        const ImVec2 screen{ (ndcX * 0.5f + 0.5f) * displaySize.x, (-ndcY * 0.5f + 0.5f) * displaySize.y };
        char label[32]{}; std::snprintf(label, sizeof(label), "ノード %zu", index);
        ImGui::GetForegroundDrawList()->AddText(screen, IM_COL32(255, 255, 255, 255), label);
    }
#endif
}
