#include "SkinningEditor.h"

#include "KrakenPreviewAssetMode.h"
#include "SkinningEditorKrakenMotionPreview.h"
#include "SkinningEditorSkinnedMaterialDiagnostics.h"
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
            : "互換プレビュー（TRS比較）";
    }

    const char* YesNo(bool value) {
        return value ? "はい" : "いいえ";
    }

    const char* SupportedLabel(bool supported) {
        return supported ? "対応済み" : "未対応";
    }

    const char* AlphaModeLabel(GltfSkinnedAlphaMode alphaMode) {
        switch (alphaMode) {
        case GltfSkinnedAlphaMode::Opaque:
            return "OPAQUE";
        case GltfSkinnedAlphaMode::Mask:
            return "MASK";
        case GltfSkinnedAlphaMode::Blend:
            return "BLEND";
        default:
            return "不明";
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

    void DrawMaterialEntry(const GltfSkinnedMaterialData& material) {
        ImGui::PushID(material.sourceMaterialIndex);
        const char* materialName = material.name.empty()
            ? "（名前なし）"
            : material.name.c_str();
        const bool isOpen = ImGui::TreeNodeEx(
            "SkinnedMaterialDiagnosticEntry",
            ImGuiTreeNodeFlags_DefaultOpen,
            "マテリアル %d：%s",
            material.sourceMaterialIndex,
            materialName);
        if (isOpen) {
            ImGui::Text("元マテリアル番号: %d", material.sourceMaterialIndex);
            ImGui::Text("名前: %s", materialName);
            ImGui::Text(
                "ベースカラー係数: %.4f, %.4f, %.4f, %.4f",
                material.baseColorFactor.x,
                material.baseColorFactor.y,
                material.baseColorFactor.z,
                material.baseColorFactor.w);
            ImGui::Text("Metallic係数: %.4f", material.metallicFactor);
            ImGui::Text("Roughness係数: %.4f", material.roughnessFactor);
            ImGui::Text(
                "Emissive係数: %.4f, %.4f, %.4f",
                material.emissiveFactor.x,
                material.emissiveFactor.y,
                material.emissiveFactor.z);
            ImGui::Text(
                "ベースカラーテクスチャ番号: %d",
                material.baseColorTextureIndex);
            ImGui::Text(
                "ベースカラー画像番号: %d",
                material.baseColorImageIndex);
            ImGui::TextWrapped(
                "テクスチャURI: %s",
                material.baseColorTextureUri.empty()
                    ? "（なし）"
                    : material.baseColorTextureUri.c_str());
            ImGui::TextWrapped(
                "解決済みテクスチャパス: %s",
                material.resolvedTexturePath.empty()
                    ? "（なし）"
                    : material.resolvedTexturePath.c_str());
            ImGui::Text(
                "テクスチャ解決: %s",
                material.textureResolved ? "成功" : "失敗");
            ImGui::Text("テクスチャハンドル: %u", material.textureHandle);
            ImGui::Text(
                "テクスチャハンドル有効: %s",
                YesNo(material.textureHandleValid));
            ImGui::Text(
                "代替テクスチャ使用: %s",
                YesNo(material.usingFallbackTexture));
            ImGui::Text(
                "アルファモード: %s",
                AlphaModeLabel(material.alphaMode));
            ImGui::Text("アルファしきい値: %.4f", material.alphaCutoff);
            ImGui::Text(
                "両面描画指定: %s",
                YesNo(material.doubleSided));
            ImGui::Text(
                "既定マテリアル: %s",
                YesNo(material.isDefaultMaterial));
            ImGui::Text(
                "マテリアル定数バッファ番号: %zu",
                material.materialConstantBufferSlot);
            ImGui::Text(
                "マテリアル定数バッファGPUアドレス: 0x%016llX",
                static_cast<unsigned long long>(
                    material.materialConstantBufferGpuAddress));
            ImGui::Text(
                "現在の描画で使用: ベースカラー係数 / ベースカラーテクスチャ / アルファしきい値");
            ImGui::TextDisabled(
                "現在のプレビュー設定で未使用: Metallic / Roughness");
            ImGui::TextDisabled(
                "シェーダー未対応または状態情報のみ: Emissive / アルファモード / 両面描画指定");
            ImGui::Text("有効: %s", YesNo(material.valid));
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    void DrawBindingEntry(
        const GltfSkinnedMaterialBindingDiagnostic& binding) {
        ImGui::PushID(static_cast<int>(binding.drawCallIndex));
        const bool isOpen = ImGui::TreeNodeEx(
            "SkinnedMaterialBindingDiagnosticEntry",
            ImGuiTreeNodeFlags_DefaultOpen,
            "描画 %zu：プリミティブ %u → マテリアル %d",
            binding.drawCallIndex,
            binding.sourcePrimitiveIndex,
            binding.sourceMaterialIndex);
        if (isOpen) {
            ImGui::Text("描画呼出し番号: %zu", binding.drawCallIndex);
            ImGui::Text(
                "元プリミティブ番号: %u",
                binding.sourcePrimitiveIndex);
            ImGui::Text(
                "元マテリアル番号: %d",
                binding.sourceMaterialIndex);
            ImGui::Text(
                "マテリアル名: %s",
                binding.materialName.empty()
                    ? "（名前なし）"
                    : binding.materialName.c_str());
            ImGui::TextWrapped(
                "解決済みテクスチャパス: %s",
                binding.resolvedTexturePath.empty()
                    ? "（なし）"
                    : binding.resolvedTexturePath.c_str());
            ImGui::Text("テクスチャハンドル: %u", binding.textureHandle);
            ImGui::Text(
                "マテリアル定数バッファ番号: %zu",
                binding.materialConstantBufferSlot);
            ImGui::Text(
                "マテリアル定数バッファGPUアドレス: 0x%016llX",
                static_cast<unsigned long long>(
                    binding.materialConstantBufferGpuAddress));
            ImGui::Text(
                "代替テクスチャ使用: %s",
                YesNo(binding.usingFallbackTexture));
            ImGui::Text(
                "バインド結果: %s",
                binding.bindingSucceeded ? "成功" : "失敗");
            if (!binding.errorMessage.empty()) {
                ImGui::TextWrapped(
                    "バインドエラー: %s",
                    binding.errorMessage.c_str());
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
#endif
}

void SkinningEditor::SetKrakenSkinnedMaterialLoadResult(
    const GltfSkinnedMaterialDiagnostics& diagnostics,
    const GltfSkinnedModel* activeModel) {
    if (!skinnedMaterialDiagnosticsState_) {
        skinnedMaterialDiagnosticsState_ =
            std::make_unique<SkinningEditorSkinnedMaterialDiagnosticsState>();
    }

    SkinningEditorSkinnedMaterialDiagnosticsState& state =
        *skinnedMaterialDiagnosticsState_;
    state.diagnostics = diagnostics;
    state.activeModel = activeModel;
    state.hasDiagnostics = true;
}

void SkinningEditor::DrawSkinnedMaterialDiagnosticsImGui() {
#ifdef USE_IMGUI
    if (!skinnedMaterialDiagnosticsState_) {
        skinnedMaterialDiagnosticsState_ =
            std::make_unique<SkinningEditorSkinnedMaterialDiagnosticsState>();
    }
    SkinningEditorSkinnedMaterialDiagnosticsState& state =
        *skinnedMaterialDiagnosticsState_;

    ImGui::SeparatorText(
        "Skinned MultiMaterial診断##SkinnedMultiMaterialDiagnostics");
    ImGui::Text(
        "使用中プレビューアセット: %s",
        AssetModeLabel(GetKrakenPreviewAssetMode()));

    if (ImGui::Button(
        "元アセットを再読み込み##ReloadOriginalForMaterialDiagnostics")) {
        RequestKrakenPreviewAssetLoad(
            KrakenPreviewAssetMode::OriginalMatrix);
    }
    ImGui::SameLine();
    if (ImGui::Button(
        "互換プレビューを再読み込み##ReloadCompatibleForMaterialDiagnostics")) {
        RequestKrakenPreviewAssetLoad(
            KrakenPreviewAssetMode::CompatibleTrs);
    }

    if (ImGui::Button(
        "マテリアル診断を再実行##RerunSkinnedMaterialDiagnostics")) {
        if (state.activeModel) {
            state.diagnostics =
                state.activeModel->GetMaterialDiagnostics();
            state.hasDiagnostics = true;
        } else {
            RequestKrakenPreviewAssetLoad(GetKrakenPreviewAssetMode());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(
        "バインドポーズへ戻す##ReturnMaterialPreviewBindPose") &&
        krakenMotionPreview_) {
        krakenMotionPreview_->ReturnToBindPoseFromEditor();
    }
    ImGui::SameLine();
    if (ImGui::Button(
        "診断をリセット##ResetSkinnedMaterialDiagnostics")) {
        state.diagnostics = {};
        state.hasDiagnostics = false;
    }

    ImGui::TextWrapped(
        "各プリミティブが保持する元マテリアル番号を使い、描画ごとにマテリアルとベースカラーテクスチャを切り替えます。");
    ImGui::TextDisabled(
        "頂点、スキニング結果、ボーンパレット、結合済みインデックスバッファは全プリミティブで共有します。");

    if (!state.hasDiagnostics) {
        ImGui::TextDisabled("マテリアル診断は未実行です。");
        return;
    }

    if (state.activeModel) {
        state.diagnostics =
            state.activeModel->GetMaterialDiagnostics();
    }
    const GltfSkinnedMaterialDiagnostics& diagnostics =
        state.diagnostics;

    ImGui::Text(
        "マテリアル読込結果: %s",
        diagnostics.loadSucceeded ? "成功" : "失敗");
    if (!diagnostics.sourcePath.empty()) {
        ImGui::TextWrapped(
            "読込パス: %s",
            diagnostics.sourcePath.c_str());
    }
    ImGui::Text(
        "元マテリアル数: %zu",
        diagnostics.sourceMaterialCount);
    ImGui::Text(
        "読込マテリアル数: %zu",
        diagnostics.loadedMaterialCount);
    ImGui::Text(
        "有効マテリアル数: %zu",
        diagnostics.validMaterialCount);
    ImGui::Text(
        "無効マテリアル数: %zu",
        diagnostics.invalidMaterialCount);
    ImGui::Text("プリミティブ数: %zu", diagnostics.primitiveCount);
    ImGui::Text("描画呼出し数: %zu", diagnostics.drawCallCount);
    ImGui::Text(
        "マテリアルバインド数: %zu",
        diagnostics.materialBindingCount);
    ImGui::Text(
        "ベースカラーテクスチャバインド数: %zu",
        diagnostics.baseColorTextureBindingCount);
    ImGui::Text(
        "バインド失敗数: %zu",
        diagnostics.bindingFailureCount);
    ImGui::Text(
        "元テクスチャ数: %zu",
        diagnostics.sourceTextureCount);
    ImGui::Text(
        "固有テクスチャリソース数: %zu",
        diagnostics.uniqueTextureResourceCount);
    ImGui::Text(
        "代替テクスチャ数: %zu",
        diagnostics.fallbackTextureCount);
    ImGui::Text(
        "マテリアル定数バッファ数: %zu",
        diagnostics.materialConstantBufferCount);
    ImGui::Text(
        "複数プリミティブ対応: %s",
        SupportedLabel(diagnostics.multiPrimitiveSupported));
    ImGui::Text(
        "複数マテリアル対応: %s",
        SupportedLabel(diagnostics.multiMaterialSupported));
    DrawTooltip(
        "プリミティブが保持する元マテリアル番号を使い、描画ごとにマテリアル定数バッファとテクスチャを切り替えます。");
    ImGui::Text(
        "複数メッシュ対応: %s",
        SupportedLabel(diagnostics.multiMeshSupported));
    ImGui::Text(
        "シェーダーで未使用のマテリアル項目あり: %s",
        YesNo(diagnostics.hasShaderUnusedMaterialItems));
    DrawTooltip(
        "glTFから値を読み込んで保持していますが、現在のスキン付きモデル用シェーダーでは見た目に使用していない項目です。");

    if (!diagnostics.errorMessage.empty()) {
        ImGui::TextWrapped(
            "マテリアル読込エラー: %s",
            diagnostics.errorMessage.c_str());
    }

    ImGui::SeparatorText(
        "マテリアル一覧##SkinnedMaterialDiagnosticEntries");
    if (diagnostics.materials.empty()) {
        ImGui::TextDisabled("マテリアル情報を取得できませんでした。");
    } else {
        for (const GltfSkinnedMaterialData& material :
            diagnostics.materials) {
            DrawMaterialEntry(material);
        }
    }

    ImGui::SeparatorText(
        "プリミティブ割当##SkinnedMaterialBindingDiagnosticEntries");
    if (diagnostics.bindings.empty()) {
        ImGui::TextDisabled(
            "描画ごとのマテリアル割当を取得できませんでした。");
    } else {
        for (const GltfSkinnedMaterialBindingDiagnostic& binding :
            diagnostics.bindings) {
            DrawBindingEntry(binding);
        }
    }
#endif
}
