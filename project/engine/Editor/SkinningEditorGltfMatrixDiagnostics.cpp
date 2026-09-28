#include "SkinningEditor.h"

#include "SkinningEditorGltfMatrixDiagnostics.h"
#include "SkinningEditorKrakenMotionPreview.h"
#include <memory>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
    const char* AssetModeLabel(KrakenPreviewAssetMode assetMode) {
        return assetMode == KrakenPreviewAssetMode::OriginalMatrix
            ? "元アセット（matrix正式対応）"
            : "互換Preview（TRS比較）";
    }

#ifdef USE_IMGUI
    void DrawTooltip(const char* text) {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(420.0f);
            ImGui::TextUnformatted(text);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }
#endif
}

void SkinningEditor::SetKrakenGltfPreviewLoadResult(
    KrakenPreviewAssetMode assetMode,
    const GltfNodeMatrixDiagnostics& diagnostics,
    const SkinningEditorSkeletonSnapshot& snapshot) {
    if (!gltfMatrixDiagnosticsState_) {
        gltfMatrixDiagnosticsState_ =
            std::make_unique<SkinningEditorGltfMatrixDiagnosticsState>();
    }

    SkinningEditorGltfMatrixDiagnosticsState& state =
        *gltfMatrixDiagnosticsState_;
    state.assetMode = assetMode;
    state.requestedAssetMode = assetMode;
    state.nodeDiagnostics = diagnostics;
    state.hasNodeDiagnostics = true;
    state.comparison = {};
    if (assetMode == KrakenPreviewAssetMode::OriginalMatrix) {
        state.originalSnapshot = snapshot;
    } else {
        state.compatibleSnapshot = snapshot;
    }
}

bool SkinningEditor::ConsumeKrakenGltfPreviewLoadRequest(
    KrakenPreviewAssetMode& assetMode) {
    if (!gltfMatrixDiagnosticsState_ ||
        !gltfMatrixDiagnosticsState_->hasPendingLoadRequest) {
        return false;
    }

    assetMode = gltfMatrixDiagnosticsState_->requestedAssetMode;
    gltfMatrixDiagnosticsState_->hasPendingLoadRequest = false;
    return true;
}

KrakenPreviewAssetMode SkinningEditor::GetKrakenPreviewAssetMode() const {
    return gltfMatrixDiagnosticsState_
        ? gltfMatrixDiagnosticsState_->assetMode
        : KrakenPreviewAssetMode::OriginalMatrix;
}
void SkinningEditor::RequestKrakenPreviewAssetLoad(
    KrakenPreviewAssetMode assetMode) {
    if (!gltfMatrixDiagnosticsState_) {
        gltfMatrixDiagnosticsState_ =
            std::make_unique<SkinningEditorGltfMatrixDiagnosticsState>();
    }
    gltfMatrixDiagnosticsState_->assetMode = assetMode;
    gltfMatrixDiagnosticsState_->requestedAssetMode = assetMode;
    gltfMatrixDiagnosticsState_->hasPendingLoadRequest = true;
}

void SkinningEditor::DrawGltfNodeMatrixDiagnosticsImGui() {
#ifdef USE_IMGUI
    if (!gltfMatrixDiagnosticsState_) {
        gltfMatrixDiagnosticsState_ =
            std::make_unique<SkinningEditorGltfMatrixDiagnosticsState>();
    }
    SkinningEditorGltfMatrixDiagnosticsState& state =
        *gltfMatrixDiagnosticsState_;


    ImGui::SeparatorText("glTF Node変換診断##GltfNodeTransformDiagnostics");
    ImGui::Text("使用中Previewアセット: %s", AssetModeLabel(state.assetMode));
    if (ImGui::RadioButton(
        "元アセット##OriginalMatrixAsset",
        state.assetMode == KrakenPreviewAssetMode::OriginalMatrix)) {
        RequestKrakenPreviewAssetLoad(KrakenPreviewAssetMode::OriginalMatrix);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton(
        "互換Preview##CompatibleTrsAsset",
        state.assetMode == KrakenPreviewAssetMode::CompatibleTrs)) {
        RequestKrakenPreviewAssetLoad(KrakenPreviewAssetMode::CompatibleTrs);
    }

    if (ImGui::Button("元アセットを読み込む##LoadOriginalMatrix")) {
        RequestKrakenPreviewAssetLoad(KrakenPreviewAssetMode::OriginalMatrix);
    }
    ImGui::SameLine();
    if (ImGui::Button("互換Previewを読み込む##LoadCompatibleTrs")) {
        RequestKrakenPreviewAssetLoad(KrakenPreviewAssetMode::CompatibleTrs);
    }
    if (ImGui::Button("Node変換診断を再実行##RerunNodeDiagnostics")) {
        RequestKrakenPreviewAssetLoad(state.assetMode);
    }
    ImGui::SameLine();
    if (ImGui::Button("Skeleton比較を実行##CompareSkeletons")) {
        state.comparison = CompareSkinningEditorSkeletons(
            state.originalSnapshot,
            state.compatibleSnapshot);
    }
    ImGui::SameLine();
    if (ImGui::Button("Bind Poseへ戻す##ReturnMatrixPreviewBindPose") &&
        krakenMotionPreview_) {
        krakenMotionPreview_->ReturnToBindPoseFromEditor();
    }
    if (ImGui::Button("診断をリセット##ResetNodeDiagnostics")) {
        state.nodeDiagnostics = {};
        state.originalSnapshot = {};
        state.compatibleSnapshot = {};
        state.comparison = {};
        state.hasNodeDiagnostics = false;
    }

    ImGui::TextWrapped(
        "元アセットの3 Primitiveは、共有Skinning結果を使用してすべて描画します。");
    ImGui::TextDisabled(
        "複数プリミティブ・複数マテリアル対応済み / 元マテリアル使用 / 複数メッシュ未対応");

    if (!state.hasNodeDiagnostics) {
        ImGui::TextDisabled("Node変換診断は未実行です。");
        return;
    }

    const GltfNodeMatrixDiagnostics& diagnostics = state.nodeDiagnostics;
    ImGui::Text("読込結果: %s",
        diagnostics.loadSucceeded ? "成功" : "失敗");
    ImGui::Text("総Node数: %zu", diagnostics.totalNodeCount);
    ImGui::Text("matrix Node数: %zu", diagnostics.matrixNodeCount);
    DrawTooltip(
        "translation / rotation / scaleではなく、glTFのmatrix[16]でLocal Transformを保持しているNode数です。");
    ImGui::Text("TRS Node数: %zu", diagnostics.trsNodeCount);
    ImGui::Text("Identity Node数: %zu", diagnostics.identityNodeCount);
    ImGui::Text("matrix分解成功数: %zu",
        diagnostics.matrixDecompositionSuccessCount);
    ImGui::Text("matrix分解失敗数: %zu",
        diagnostics.matrixDecompositionFailureCount);
    ImGui::Text("matrix / TRS同時存在数: %zu",
        diagnostics.matrixTrsConflictCount);
    ImGui::Text("Shear検出数: %zu", diagnostics.shearDetectedCount);
    ImGui::Text("非有限値数: %zu", diagnostics.nonFiniteMatrixCount);
    ImGui::Text("極小Scale数: %zu", diagnostics.nearZeroScaleCount);
    ImGui::Text("再構築誤差超過数: %zu",
        diagnostics.reconstructionErrorExceededCount);
    ImGui::Text("最大再構築誤差: %.9g",
        diagnostics.maxReconstructionError);
    DrawTooltip(
        "matrixをTRSへ分解し、同じTRSから再構築したMatrixとの差です。");
    ImGui::Text("最大誤差Node: %s",
        diagnostics.maxErrorNodeName.empty()
            ? "なし"
            : diagnostics.maxErrorNodeName.c_str());
    if (!diagnostics.errorMessage.empty()) {
        ImGui::TextWrapped("エラー理由: %s", diagnostics.errorMessage.c_str());
        ImGui::Text("失敗Node: %s",
            diagnostics.failureNodeName.empty()
                ? "取得不可"
                : diagnostics.failureNodeName.c_str());
        ImGui::Text("失敗Node Index: %d", diagnostics.failureNodeIndex);
        ImGui::Text("失敗Joint Index: %d", diagnostics.failureJointIndex);
    }

    const SkinningEditorSkeletonSnapshot& activeSnapshot =
        state.assetMode == KrakenPreviewAssetMode::OriginalMatrix
        ? state.originalSnapshot
        : state.compatibleSnapshot;
    ImGui::Text("Joint数: %zu", activeSnapshot.joints.size());
    ImGui::Text("Root Joint: %s",
        activeSnapshot.rootName.empty()
            ? "取得不可"
            : activeSnapshot.rootName.c_str());

    ImGui::SeparatorText("互換Previewとの比較結果##SkeletonComparison");
    if (!state.comparison.executed) {
        ImGui::TextDisabled(
            "元アセットと互換Previewを読み込んだ後、比較を実行してください。");
        return;
    }

    const SkinningEditorSkeletonComparisonResult& comparison =
        state.comparison;
    ImGui::Text("比較結果: %s", comparison.succeeded ? "一致" : "不一致");
    ImGui::Text("不一致Joint数: %zu", comparison.mismatchJointCount);
    ImGui::Text("Local Matrix最大差: %.9g",
        comparison.maxLocalMatrixDifference);
    ImGui::Text("Global Matrix最大差: %.9g",
        comparison.maxGlobalMatrixDifference);
    ImGui::Text("Joint位置最大差: %.9g",
        comparison.maxPositionDifference);
    ImGui::Text("Palette Matrix最大差: %.9g",
        comparison.maxPaletteMatrixDifference);
    ImGui::Text("最大差Joint: %s",
        comparison.maxErrorJointName.empty()
            ? "なし"
            : comparison.maxErrorJointName.c_str());
    if (!comparison.errorMessage.empty()) {
        ImGui::TextWrapped("比較エラー: %s", comparison.errorMessage.c_str());
    }
#endif
}
