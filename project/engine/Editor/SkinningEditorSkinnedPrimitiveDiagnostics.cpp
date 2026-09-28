#include "SkinningEditor.h"

#include "KrakenPreviewAssetMode.h"
#include "SkinningEditorKrakenMotionPreview.h"
#include "SkinningEditorSkinnedPrimitiveDiagnostics.h"
#include "Engine/Animation/Skeleton.h"
#include "Engine/Graphics/Model/GltfSkinnedModel.h"
#include <memory>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
#ifdef USE_IMGUI
    const char* AssetModeLabel(KrakenPreviewAssetMode assetMode) {
        return assetMode == KrakenPreviewAssetMode::OriginalMatrix
            ? "元アセット（matrix正式対応）"
            : "互換Preview（TRS比較）";
    }

    const char* YesNo(bool value) {
        return value ? "はい" : "いいえ";
    }

    const char* SupportedLabel(bool supported) {
        return supported ? "対応済み" : "未対応";
    }

    const char* PrimitiveModeLabel(std::uint32_t mode) {
        return mode == 4u ? "TRIANGLES" : "未対応Mode";
    }

    const char* IndexComponentTypeLabel(std::uint32_t componentType) {
        switch (componentType) {
        case 5121u:
            return "UNSIGNED_BYTE";
        case 5123u:
            return "UNSIGNED_SHORT";
        case 5125u:
            return "UNSIGNED_INT";
        default:
            return "未対応形式";
        }
    }

    void DrawTooltip(const char* text) {
        if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            return;
        }
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(420.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    void DrawPrimitiveEntry(
        const GltfSkinnedPrimitiveDiagnosticEntry& primitive) {
        ImGui::PushID(static_cast<int>(primitive.sourcePrimitiveIndex));
        const bool isOpen = ImGui::TreeNodeEx(
            "PrimitiveDiagnosticEntry",
            ImGuiTreeNodeFlags_DefaultOpen,
            "Primitive %u",
            primitive.sourcePrimitiveIndex);
        if (isOpen) {
            ImGui::Text("先頭Index: %u", primitive.firstIndex);
            ImGui::Text("Index数: %u", primitive.indexCount);
            ImGui::Text("三角形数: %u", primitive.indexCount / 3u);
            ImGui::Text("最小Index: %u", primitive.minimumIndex);
            ImGui::Text("最大Index: %u", primitive.maximumIndex);
            ImGui::Text("元Material Index: %d", primitive.materialIndex);
            ImGui::Text(
                "Primitiveモード: %u（%s）",
                primitive.mode,
                PrimitiveModeLabel(primitive.mode));
            ImGui::Text(
                "Index成分型: %u（%s）",
                primitive.indexComponentType,
                IndexComponentTypeLabel(primitive.indexComponentType));
            ImGui::Text("Index Accessor: %d", primitive.indicesAccessor);
            ImGui::Text("POSITION Accessor: %d", primitive.accessors.position);
            ImGui::Text("NORMAL Accessor: %d", primitive.accessors.normal);
            ImGui::Text("TEXCOORD_0 Accessor: %d", primitive.accessors.texcoord0);
            ImGui::Text("JOINTS_0 Accessor: %d", primitive.accessors.joints0);
            ImGui::Text("WEIGHTS_0 Accessor: %d", primitive.accessors.weights0);
            ImGui::Text("TANGENT Accessor: %d", primitive.accessors.tangent);
            ImGui::Text("COLOR_0 Accessor: %d", primitive.accessors.color0);
            ImGui::Text("TEXCOORD_1 Accessor: %d", primitive.accessors.texcoord1);
            ImGui::Text("JOINTS_1 Accessor: %d", primitive.accessors.joints1);
            ImGui::Text("WEIGHTS_1 Accessor: %d", primitive.accessors.weights1);
            ImGui::Text("有効: %s", YesNo(primitive.valid));
            if (!primitive.errorMessage.empty()) {
                ImGui::TextWrapped(
                    "Primitiveエラー: %s",
                    primitive.errorMessage.c_str());
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
#endif
}

void SkinningEditor::SetKrakenSkinnedPrimitiveLoadResult(
    const GltfSkinnedPrimitiveDiagnostics& diagnostics,
    const GltfSkinnedModel* activeModel) {
    if (!skinnedPrimitiveDiagnosticsState_) {
        skinnedPrimitiveDiagnosticsState_ =
            std::make_unique<SkinningEditorSkinnedPrimitiveDiagnosticsState>();
    }

    SkinningEditorSkinnedPrimitiveDiagnosticsState& state =
        *skinnedPrimitiveDiagnosticsState_;
    state.diagnostics = diagnostics;
    state.activeModel = activeModel;
    state.hasDiagnostics = true;
}

void SkinningEditor::DrawSkinnedPrimitiveDiagnosticsImGui() {
#ifdef USE_IMGUI
    if (!skinnedPrimitiveDiagnosticsState_) {
        skinnedPrimitiveDiagnosticsState_ =
            std::make_unique<SkinningEditorSkinnedPrimitiveDiagnosticsState>();
    }
    SkinningEditorSkinnedPrimitiveDiagnosticsState& state =
        *skinnedPrimitiveDiagnosticsState_;

    ImGui::SeparatorText(
        "Skinned MultiPrimitive診断##SkinnedMultiPrimitiveDiagnostics");
    ImGui::Text(
        "使用中Previewアセット: %s",
        AssetModeLabel(GetKrakenPreviewAssetMode()));

    if (ImGui::Button(
        "元アセットを再読み込み##ReloadOriginalForPrimitiveDiagnostics")) {
        RequestKrakenPreviewAssetLoad(
            KrakenPreviewAssetMode::OriginalMatrix);
    }
    ImGui::SameLine();
    if (ImGui::Button(
        "互換Previewを再読み込み##ReloadCompatibleForPrimitiveDiagnostics")) {
        RequestKrakenPreviewAssetLoad(
            KrakenPreviewAssetMode::CompatibleTrs);
    }

    if (ImGui::Button(
        "Primitive診断を再実行##RerunSkinnedPrimitiveDiagnostics")) {
        if (state.activeModel) {
            state.diagnostics = state.activeModel->GetPrimitiveDiagnostics();
            state.hasDiagnostics = true;
        } else {
            RequestKrakenPreviewAssetLoad(GetKrakenPreviewAssetMode());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(
        "Bind Poseへ戻す##ReturnPrimitivePreviewBindPose") &&
        krakenMotionPreview_) {
        krakenMotionPreview_->ReturnToBindPoseFromEditor();
    }
    ImGui::SameLine();
    if (ImGui::Button(
        "診断をリセット##ResetSkinnedPrimitiveDiagnostics")) {
        state.diagnostics = {};
        state.hasDiagnostics = false;
    }

    ImGui::TextWrapped(
        "1 Mesh内の全Primitiveは、共有した頂点・Skin Influence・Bone Palette・Skinning結果で描画します。");
    ImGui::TextDisabled(
        "各プリミティブへ元アセットのマテリアルを割り当てます。複数マテリアル対応済み、複数メッシュは未対応です。");

    if (!state.hasDiagnostics) {
        ImGui::TextDisabled("Primitive診断は未実行です。");
        return;
    }

    if (state.activeModel) {
        state.diagnostics = state.activeModel->GetPrimitiveDiagnostics();
    }
    const GltfSkinnedPrimitiveDiagnostics& diagnostics = state.diagnostics;
    const std::size_t paletteCount = state.activeModel
        ? state.activeModel->GetPaletteCount()
        : 0u;
    const std::size_t jointCount = targetSkeleton_
        ? targetSkeleton_->joints.size()
        : 0u;
    const bool computeResourcesReady =
        state.activeModel && state.activeModel->HasComputeSkinningResources();
    const char* activeVertexBuffer = !state.activeModel
        ? "取得不可"
        : (state.activeModel->IsUsingComputeOutputVertices()
            ? "Computeスキニング出力"
            : "CPUスキニング更新結果");

    ImGui::Text(
        "Primitive読込結果: %s",
        diagnostics.loadSucceeded ? "成功" : "失敗");
    if (!diagnostics.sourcePath.empty()) {
        ImGui::TextWrapped("読込パス: %s", diagnostics.sourcePath.c_str());
    }
    ImGui::Text("元Mesh数: %zu", diagnostics.sourceMeshCount);
    ImGui::Text("使用Mesh番号: %zu", diagnostics.sourceMeshIndex);
    ImGui::Text(
        "使用Mesh名: %s",
        diagnostics.meshName.empty() ? "取得不可" : diagnostics.meshName.c_str());
    ImGui::Text("元Primitive数: %zu", diagnostics.sourcePrimitiveCount);
    ImGui::Text("読込Primitive数: %zu", diagnostics.validPrimitiveCount);
    ImGui::Text("無効Primitive数: %zu", diagnostics.invalidPrimitiveCount);
    ImGui::Text("頂点数: %zu", diagnostics.vertexCount);
    ImGui::Text("総Index数: %zu", diagnostics.totalIndexCount);
    ImGui::Text("三角形数: %zu", diagnostics.triangleCount);
    ImGui::Text("Primitive描画範囲数: %zu", diagnostics.rangeCount);
    ImGui::Text("描画呼出し数: %zu", diagnostics.drawCallCount);
    ImGui::Text("共有頂点Stream: %s", YesNo(diagnostics.sharedVertexStream));
    DrawTooltip(
        "すべてのPrimitiveが同じPOSITION、NORMAL、TEXCOORD、JOINTS、WEIGHTSを参照している状態です。");
    ImGui::Text(
        "必須属性一致: %s",
        YesNo(diagnostics.requiredAttributeAccessorsMatch));
    ImGui::Text(
        "任意属性一致: %s",
        YesNo(diagnostics.optionalAttributeAccessorsMatch));
    ImGui::Text(
        "CPUスキニング更新数: %zu",
        diagnostics.cpuSkinningUpdateCount);
    ImGui::Text(
        "スキニングDispatch数: %zu",
        diagnostics.computeDispatchCount);
    ImGui::Text(
        "Dispatchグループ数: %u",
        state.activeModel
            ? state.activeModel->GetDispatchThreadGroupCount()
            : 0u);
    ImGui::Text("Palette数: %zu", paletteCount);
    ImGui::Text("Joint数: %zu", jointCount);
    ImGui::Text(
        "Computeリソース: %s",
        computeResourcesReady ? "準備完了" : "未準備");
    ImGui::Text("現在の描画頂点Buffer: %s", activeVertexBuffer);
    ImGui::Text(
        "元アセットのマテリアル割当: %s",
        YesNo(!diagnostics.usesCommonPreviewMaterial));
    if (diagnostics.usesCommonPreviewMaterial) {
        ImGui::Text(
            "共通プレビューマテリアルの元番号: %d",
            diagnostics.commonPreviewMaterialIndex);
    }
    DrawTooltip(
        "各プリミティブが保持する元マテリアル番号を使い、対応するマテリアルを描画時に選択します。");
    ImGui::Text(
        "複数プリミティブ対応: %s",
        SupportedLabel(diagnostics.multiPrimitiveSupported));
    ImGui::Text(
        "複数マテリアル対応: %s",
        SupportedLabel(diagnostics.multiMaterialSupported));
    ImGui::Text(
        "複数メッシュ対応: %s",
        SupportedLabel(diagnostics.multiMeshSupported));

    if (!diagnostics.errorMessage.empty()) {
        ImGui::TextWrapped(
            "Primitive読込エラー: %s",
            diagnostics.errorMessage.c_str());
    }

    ImGui::SeparatorText(
        "Primitive一覧##SkinnedPrimitiveDiagnosticEntries");
    if (diagnostics.primitives.empty()) {
        ImGui::TextDisabled("Primitive情報を取得できませんでした。");
        return;
    }
    for (const GltfSkinnedPrimitiveDiagnosticEntry& primitive :
        diagnostics.primitives) {
        DrawPrimitiveEntry(primitive);
    }
#endif
}
