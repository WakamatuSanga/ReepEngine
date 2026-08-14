#include "SkinningEditorKrakenMotionPreview.h"

#include "SkinningEditorKrakenAttackMotion.h"
#include "SkinningEditorKrakenBoneColliderPhaseControl.h"
#include "SkinningEditorKrakenBoneColliderPreviewCollection.h"
#include "Engine/Animation/Skeleton.h"

#include <algorithm>
#include <cmath>
#include <string>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
#ifdef USE_IMGUI
    const char* YesNo(bool value) {
        return value ? "はい" : "いいえ";
    }

    void DrawTooltip(const char* text) {
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", text);
        }
    }

    const char* FindJointName(
        const Skeleton& skeleton,
        int jointIndex) {
        return jointIndex >= 0 &&
            static_cast<std::size_t>(jointIndex) < skeleton.joints.size()
            ? skeleton.joints[static_cast<std::size_t>(jointIndex)].name.c_str()
            : "なし";
    }

    bool DrawRoleCombo(
        const char* label,
        KrakenColliderPreviewRole& role) {
        constexpr KrakenColliderPreviewRole kRoles[] = {
            KrakenColliderPreviewRole::Attack,
            KrakenColliderPreviewRole::Damage,
            KrakenColliderPreviewRole::WeakPoint,
        };
        bool changed = false;
        if (ImGui::BeginCombo(
                label,
                GetKrakenColliderPreviewRoleJapaneseLabel(role))) {
            for (KrakenColliderPreviewRole candidate : kRoles) {
                const bool selected = candidate == role;
                if (ImGui::Selectable(
                        GetKrakenColliderPreviewRoleJapaneseLabel(candidate),
                        selected)) {
                    role = candidate;
                    changed = true;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    bool DrawJointCombo(
        const char* label,
        const Skeleton& skeleton,
        const std::vector<int>& chainJoints,
        int& jointIndex) {
        bool changed = false;
        if (ImGui::BeginCombo(
                label,
                FindJointName(skeleton, jointIndex))) {
            for (int candidate : chainJoints) {
                if (candidate < 0 ||
                    static_cast<std::size_t>(candidate) >=
                        skeleton.joints.size()) {
                    continue;
                }
                const bool selected = candidate == jointIndex;
                if (ImGui::Selectable(
                        skeleton.joints[static_cast<std::size_t>(
                            candidate)].name.c_str(),
                        selected)) {
                    jointIndex = candidate;
                    changed = true;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    void DrawVector3(const char* label, const Vector3& value) {
        ImGui::Text(
            "%s: %.3f, %.3f, %.3f",
            label,
            value.x,
            value.y,
            value.z);
    }
#endif
}

void SkinningEditorKrakenMotionPreview::DrawBoneColliderPreviewImGui() {
#ifdef USE_IMGUI
    if (!skeleton_ || !boneColliderPreview_) {
        return;
    }

    SkinningEditorKrakenBoneColliderPreviewCollection& preview =
        *boneColliderPreview_;
    KrakenBoneColliderPreviewSettings& settings = preview.GetSettings();
    settings.depthTest = false;
    ImGui::SeparatorText(
        "クラーケン触手コライダープレビュー##KrakenBoneColliderPreview");
    ImGui::Checkbox(
        "コライダープレビューを表示##ShowColliderPreview",
        &settings.showPreview);
    ImGui::SameLine();
    ImGui::Checkbox(
        "カプセルを表示##ShowColliderCapsules",
        &settings.showCapsules);
    ImGui::SameLine();
    ImGui::Checkbox(
        "先端スフィアを表示##ShowColliderTipSphere",
        &settings.showTipSphere);
    ImGui::Checkbox(
        "選択中のコライダーだけ表示##ShowOnlySelectedCollider",
        &settings.showOnlySelected);
    ImGui::SameLine();
    ImGui::Checkbox(
        "全チェーンを表示##ShowAllColliderChains",
        &settings.showAllChains);
    ImGui::Checkbox(
        "攻撃レイヤー##ShowAttackColliderLayer",
        &settings.showAttackColliders);
    ImGui::SameLine();
    ImGui::Checkbox(
        "ダメージレイヤー##ShowDamageColliderLayer",
        &settings.showDamageColliders);
    ImGui::SameLine();
    ImGui::Checkbox(
        "弱点レイヤー##ShowWeakPointColliderLayer",
        &settings.showWeakPointColliders);
    DrawTooltip(
        "表示レイヤーだけを切り替えます。フェーズ有効予定の判定は継続します。");
    ImGui::BeginDisabled();
    ImGui::Checkbox(
        "深度テスト（2D表示では未対応）##ColliderDepthTest",
        &settings.depthTest);
    ImGui::EndDisabled();
    DrawTooltip(
        "既存のボーン表示と同じ2Dオーバーレイを使用するため、深度テストは行いません。");
    ImGui::DragFloat(
        "全体半径倍率##ColliderGlobalRadiusScale",
        &settings.globalRadiusScale,
        0.01f,
        0.05f,
        5.0f,
        "%.2f");
    settings.globalRadiusScale = std::clamp(
        std::isfinite(settings.globalRadiusScale)
            ? settings.globalRadiusScale
            : 1.0f,
        0.05f,
        5.0f);
    DrawTooltip(
        "ローカル半径へプレビュー行列の最大軸スケールと、この倍率を1回だけ掛けます。");

    const KrakenBoneColliderPreviewDiagnostics& diagnostics =
        preview.GetDiagnostics();
    ImGui::Text("検出チェーン数: %u", diagnostics.chainCount);
    ImGui::Text("選択中チェーン: %u", diagnostics.selectedChainIndex);
    ImGui::Text("選択チェーンのボーン数: %u", diagnostics.chainBoneCount);
    ImGui::Text("カプセル数: %u", diagnostics.capsuleCount);
    ImGui::Text("先端スフィア数: %u", diagnostics.tipSphereCount);
    ImGui::Text("有効コライダー数: %u", diagnostics.enabledColliderCount);
    ImGui::Text("無効コライダー数: %u", diagnostics.disabledColliderCount);
    ImGui::Text(
        "現在ポーズへ追従: %s",
        YesNo(diagnostics.currentPoseFollowing));
    DrawTooltip(
        "手動、アイドル、攻撃で変形した現在のボーン位置から毎回更新します。");
    ImGui::Text("ゲームプレイ未接続: はい");
    DrawTooltip(
        "現在はエディタ上の表示だけです。プレイヤーや弾との当たり判定には登録していません。");
    RefreshBoneColliderPhaseControl();
    DrawBoneColliderPhaseControlImGui();
    if (ImGui::CollapsingHeader(
            "全チェーンコライダー状態一覧##AllChainColliderStateList",
            ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto& allChainPreviews = preview.GetChainPreviews();
        for (std::size_t chainIndex = 0;
            chainIndex < allChainPreviews.size();
            ++chainIndex) {
            const auto& chainPreview = allChainPreviews[chainIndex];
            if (!chainPreview) {
                ImGui::Text("チェーン %zu: プレビュー未生成", chainIndex);
                continue;
            }
            ImGui::PushID(static_cast<int>(chainIndex));
            std::vector<KrakenBoneColliderPreview>& allCapsules =
                chainPreview->GetCapsules();
            for (std::size_t localIndex = 0;
                localIndex < allCapsules.size();
                ++localIndex) {
                ImGui::PushID(static_cast<int>(localIndex));
                KrakenBoneColliderPreview& collider =
                    allCapsules[localIndex];
                const std::string label =
                    "チェーン " + std::to_string(chainIndex) +
                    " / カプセル " + std::to_string(localIndex);
                if (ImGui::Selectable(
                        label.c_str(),
                        preview.IsSelected(
                            chainIndex, localIndex, false))) {
                    preview.Select(chainIndex, localIndex, false);
                }
                ImGui::Text(
                    "役割: %s | 有効設定: %s | 正常: %s",
                    GetKrakenColliderPreviewRoleJapaneseLabel(
                        collider.role),
                    YesNo(collider.enabled),
                    YesNo(collider.valid));
                ImGui::Text(
                    "表示中: %s | フェーズ有効予定: %s | ゲームプレイ登録: %s",
                    YesNo(collider.previewVisible),
                    YesNo(collider.phaseActive),
                    YesNo(collider.gameplayRegistered));
                ImGui::TextWrapped(
                    "判定理由: %s",
                    GetKrakenColliderPhaseReasonJapaneseLabel(
                        collider.phaseReason));
                ImGui::PopID();
            }
            KrakenTipSphereColliderPreview& tipSphere =
                chainPreview->GetTipSphere();
            ImGui::PushID("TipSphere");
            const std::string tipLabel =
                "チェーン " + std::to_string(chainIndex) +
                " / 先端スフィア";
            if (ImGui::Selectable(
                    tipLabel.c_str(),
                    preview.IsSelected(chainIndex, 0, true))) {
                preview.Select(chainIndex, 0, true);
            }
            ImGui::Text(
                "役割: %s | 有効設定: %s | 正常: %s | 表示中: %s",
                GetKrakenColliderPreviewRoleJapaneseLabel(tipSphere.role),
                YesNo(tipSphere.enabled),
                YesNo(tipSphere.valid),
                YesNo(tipSphere.previewVisible));
            ImGui::Text(
                "フェーズ有効予定: %s | ゲームプレイ登録: %s",
                YesNo(tipSphere.phaseActive),
                YesNo(tipSphere.gameplayRegistered));
            ImGui::TextWrapped(
                "判定理由: %s",
                GetKrakenColliderPhaseReasonJapaneseLabel(
                    tipSphere.phaseReason));
            ImGui::PopID();
            ImGui::PopID();
        }
    }

    if (ImGui::Button(
            "コライダープレビューを再構築##RebuildColliderPreview")) {
        RefreshBoneColliderPreview(true);
    }
    ImGui::SameLine();
    if (ImGui::Button(
            "全チェーンを自動配置##AutoPlaceColliders")) {
        RefreshBoneColliderPreview(true);
    }
    if (ImGui::Button(
            "推奨半径へ戻す##ResetColliderRadii")) {
        preview.ResetRecommendedRadii();
    }
    ImGui::SameLine();
    if (ImGui::Button(
            "全コライダーを有効化##EnableAllColliders")) {
        preview.SetAllEnabled(true);
    }
    ImGui::SameLine();
    if (ImGui::Button(
            "全コライダーを無効化##DisableAllColliders")) {
        preview.SetAllEnabled(false);
    }
    if (ImGui::Button(
            "コライダー診断を再実行##RunColliderDiagnostics")) {
        preview.RunDiagnostics(*skeleton_);
    }
    ImGui::SameLine();
    if (ImGui::Button(
            "バインドポーズへ戻す##ColliderReturnBind")) {
        ReturnToBindPose(true);
    }
    if (ImGui::Button(
            "選択コライダーを先頭へ##SelectFirstCollider")) {
        preview.SelectFirstForDisplayChain();
    }
    ImGui::SameLine();
    if (ImGui::Button(
            "選択コライダーを次へ##SelectNextCollider")) {
        preview.SelectNext();
    }
    ImGui::SameLine();
    if (ImGui::Button(
            "診断をリセット##ResetColliderDiagnostics")) {
        preview.ResetDiagnostics();
    }

    const std::size_t editableChainIndex =
        preview.GetDisplayChainIndex();
    const auto& chainPreviews = preview.GetChainPreviews();
    if (editableChainIndex >= chainPreviews.size() ||
        !chainPreviews[editableChainIndex]) {
        ImGui::TextDisabled("編集できる触手コライダーがありません。");
        return;
    }
    SkinningEditorKrakenBoneColliderPreview& editablePreview =
        *chainPreviews[editableChainIndex];


    ImGui::SeparatorText(
        "表示対象チェーンの詳細編集##ColliderCapsuleList");
    std::vector<KrakenBoneColliderPreview>& capsules =
        editablePreview.GetCapsules();
    const std::vector<int>& chainJoints =
        editablePreview.GetChainJoints();
    for (std::size_t index = 0; index < capsules.size(); ++index) {
        ImGui::PushID(static_cast<int>(index));
        KrakenBoneColliderPreview& collider = capsules[index];
        const bool selected =
            preview.IsSelected(editableChainIndex, index, false);
        const std::string selectionLabel =
            "チェーン " + std::to_string(editableChainIndex) +
            " / カプセル " + std::to_string(index) +
            "##SelectCapsule";
        if (ImGui::Selectable(selectionLabel.c_str(), selected)) {
            preview.Select(editableChainIndex, index, false);
        }
        ImGui::Text("コライダー番号: %u", collider.colliderIndex);
        DrawRoleCombo("役割##CapsuleRole", collider.role);
        int startJointIndex = collider.startJointIndex;
        int endJointIndex = collider.endJointIndex;
        bool pairChanged = DrawJointCombo(
            "開始ボーン##CapsuleStartBone",
            *skeleton_,
            chainJoints,
            startJointIndex);
        pairChanged |= DrawJointCombo(
            "終了ボーン##CapsuleEndBone",
            *skeleton_,
            chainJoints,
            endJointIndex);
        if (pairChanged) {
            editablePreview.SetCapsuleJointPair(
                index,
                startJointIndex,
                endJointIndex);
        }
        ImGui::Text("開始ジョイント番号: %d", collider.startJointIndex);
        ImGui::Text("終了ジョイント番号: %d", collider.endJointIndex);
        ImGui::DragFloat(
            "ローカル半径##CapsuleLocalRadius",
            &collider.localRadius,
            0.01f,
            0.01f,
            2.0f,
            "%.3f");
        collider.localRadius = std::clamp(
            std::isfinite(collider.localRadius)
                ? collider.localRadius
                : 0.25f,
            0.01f,
            2.0f);
        ImGui::Text("ワールド半径: %.3f", collider.worldRadius);
        ImGui::Text("ワールド長: %.3f", collider.worldLength);
        DrawVector3("ワールド中心", collider.worldCenter);
        DrawVector3("ワールド方向", collider.worldDirection);
        ImGui::Checkbox("有効##CapsuleEnabled", &collider.enabled);
        ImGui::Text("診断有効: %s", YesNo(collider.valid));
        ImGui::Text("プレビュー表示中: %s", YesNo(collider.previewVisible));
        ImGui::Text("フェーズ有効予定: %s", YesNo(collider.phaseActive));
        ImGui::Text("ゲームプレイ登録: %s", YesNo(collider.gameplayRegistered));
        ImGui::TextWrapped(
            "フェーズ判定理由: %s",
            GetKrakenColliderPhaseReasonJapaneseLabel(
                collider.phaseReason));

        ImGui::Separator();
        ImGui::PopID();
    }

    ImGui::SeparatorText(
        "先端スフィア##ColliderTipSphere");
    KrakenTipSphereColliderPreview& tipSphere =
        editablePreview.GetTipSphere();
    const bool tipSelected =
        preview.IsSelected(editableChainIndex, 0, true);
    if (ImGui::Selectable(
            "先端スフィアを選択##SelectTipSphere",
            tipSelected)) {
        preview.Select(editableChainIndex, 0, true);
    }
    DrawRoleCombo("役割##TipSphereRole", tipSphere.role);
    ImGui::Text(
        "先端ボーン: %s",
        FindJointName(*skeleton_, tipSphere.tipJointIndex));
    ImGui::Text("先端ジョイント番号: %d", tipSphere.tipJointIndex);
    ImGui::DragFloat(
        "ローカル半径##TipSphereLocalRadius",
        &tipSphere.localRadius,
        0.01f,
        0.01f,
        2.0f,
        "%.3f");
    tipSphere.localRadius = std::clamp(
        std::isfinite(tipSphere.localRadius)
            ? tipSphere.localRadius
            : 0.30f,
        0.01f,
        2.0f);
    ImGui::Text("ワールド半径: %.3f", tipSphere.worldRadius);
    DrawVector3("ワールド位置", tipSphere.worldPosition);
    ImGui::Text(
        "バインド位置からの移動距離: %.3f",
        tipSphere.distanceFromBind);
    ImGui::Checkbox("有効##TipSphereEnabled", &tipSphere.enabled);
    ImGui::Text("診断有効: %s", YesNo(tipSphere.valid));
    ImGui::Text("プレビュー表示中: %s", YesNo(tipSphere.previewVisible));
    ImGui::Text("フェーズ有効予定: %s", YesNo(tipSphere.phaseActive));
    ImGui::Text("ゲームプレイ登録: %s", YesNo(tipSphere.gameplayRegistered));
    ImGui::TextWrapped(
        "フェーズ判定理由: %s",
        GetKrakenColliderPhaseReasonJapaneseLabel(
            tipSphere.phaseReason));


    ImGui::SeparatorText(
        "コライダー診断##ColliderDiagnostics");
    ImGui::Text(
        "無効ジョイント番号数: %u",
        diagnostics.invalidJointIndexCount);
    ImGui::Text(
        "ゼロ長カプセル数: %u",
        diagnostics.zeroLengthCapsuleCount);
    ImGui::Text(
        "非有限位置数: %u",
        diagnostics.nonFinitePositionCount);
    ImGui::Text(
        "非有限半径数: %u",
        diagnostics.nonFiniteRadiusCount);
    ImGui::Text(
        "0以下半径数: %u",
        diagnostics.nonPositiveRadiusCount);
    ImGui::Text(
        "現在ポーズ更新回数: %llu",
        static_cast<unsigned long long>(
            diagnostics.currentPoseUpdateCount));
    ImGui::Text(
        "デバッグ描画呼出回数: %llu",
        static_cast<unsigned long long>(diagnostics.debugDrawCount));
    ImGui::Text(
        "前回描画形状数: %u",
        diagnostics.lastDebugDrawShapeCount);
    ImGui::Text(
        "先端スフィア更新回数: %llu",
        static_cast<unsigned long long>(
            diagnostics.tipSphereUpdateCount));
    if (!diagnostics.lastError.empty()) {
        ImGui::TextColored(
            ImVec4(1.0f, 0.35f, 0.25f, 1.0f),
            "最後のエラー: %s",
            diagnostics.lastError.c_str());
    } else {
        ImGui::Text("最後のエラー: なし");
    }
    if (!diagnostics.lastWarning.empty()) {
        ImGui::TextColored(
            ImVec4(1.0f, 0.75f, 0.25f, 1.0f),
            "最後の警告: %s",
            diagnostics.lastWarning.c_str());
    } else {
        ImGui::Text("最後の警告: なし");
    }
#endif
}
