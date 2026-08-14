#include "SkinningEditorKrakenMotionPreview.h"

#include "SkinningEditorKrakenAttackMotion.h"
#include "Engine/Animation/Skeleton.h"
#include "Engine/Graphics/Model/GltfSkinnedModel.h"
#include "Engine/Graphics/Model/GltfSkinnedModelPrimitiveData.h"

#include <algorithm>
#include <cstdio>
#include <string>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
#ifdef USE_IMGUI
    const char* YesNo(bool value) {
        return value ? "はい" : "いいえ";
    }

    const char* MotionModeLabel(
        SkinningEditorKrakenMotionPreview::Mode mode) {
        switch (mode) {
        case SkinningEditorKrakenMotionPreview::Mode::Manual:
            return "手動ポーズ";
        case SkinningEditorKrakenMotionPreview::Mode::IdleSway:
            return "アイドルスウェイ";
        case SkinningEditorKrakenMotionPreview::Mode::AttackSlamPreview:
            return "触手攻撃";
        default:
            return "不明";
        }
    }

    void DrawTooltip(const char* text) {
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", text);
        }
    }

    bool DrawAxisCombo(
        const char* label,
        KrakenTentacleAttackLocalAxis& axis) {
        const char* axisLabels[] = { "X", "Y", "Z" };
        int currentAxis = static_cast<int>(axis);
        if (!ImGui::Combo(label, &currentAxis, axisLabels, 3)) {
            return false;
        }
        currentAxis = std::clamp(currentAxis, 0, 2);
        axis = static_cast<KrakenTentacleAttackLocalAxis>(currentAxis);
        return true;
    }

    bool DrawSignCombo(const char* label, float& sign) {
        const char* signLabels[] = { "+1", "-1" };
        int currentSign = sign < 0.0f ? 1 : 0;
        if (!ImGui::Combo(label, &currentSign, signLabels, 2)) {
            return false;
        }
        sign = currentSign == 1 ? -1.0f : 1.0f;
        return true;
    }
#endif
}

void SkinningEditorKrakenMotionPreview::DrawAttackMotionImGui() {
#ifdef USE_IMGUI
    if (!attackMotion_ || !skeleton_) {
        return;
    }

    SkinningEditorKrakenAttackMotion& attack = *attackMotion_;
    const std::size_t chainCount = chains_.size();
    const std::size_t selectedChainIndex =
        attack.GetSelectedChainIndex();
    const bool hasSelectedChain =
        selectedChainIndex < chainCount &&
        !chains_[selectedChainIndex].joints.empty();
    const Chain* selectedChain = hasSelectedChain
        ? &chains_[selectedChainIndex]
        : nullptr;
    const KrakenTentacleAttackPreviewSettings currentSettings =
        attack.GetSettings();
    const std::size_t selectedChainBoneCount = selectedChain
        ? selectedChain->joints.size()
        : 0;
    const std::size_t targetBoneCount = selectedChainBoneCount -
        (std::min)(selectedChainBoneCount,
            static_cast<std::size_t>(currentSettings.fixedLeadingBoneCount));

    const Joint* chainRoot = nullptr;
    const Joint* chainTip = nullptr;
    if (selectedChain) {
        const int rootIndex = selectedChain->joints.front();
        const int tipIndex = selectedChain->joints.back();
        if (rootIndex >= 0 &&
            rootIndex < static_cast<int>(skeleton_->joints.size())) {
            chainRoot = &skeleton_->joints[
                static_cast<std::size_t>(rootIndex)];
        }
        if (tipIndex >= 0 &&
            tipIndex < static_cast<int>(skeleton_->joints.size())) {
            chainTip = &skeleton_->joints[
                static_cast<std::size_t>(tipIndex)];
        }
    }
    const char* previewAssetPath = "取得できません";
    if (model_) {
        const GltfSkinnedPrimitiveDiagnostics& primitiveDiagnostics =
            model_->GetPrimitiveDiagnostics();
        if (!primitiveDiagnostics.sourcePath.empty()) {
            previewAssetPath = primitiveDiagnostics.sourcePath.c_str();
        }
    }

    ImGui::SeparatorText(
        "触手攻撃Motion Preview##KrakenTentacleAttackMotionPreview");
    ImGui::Text(
        "プレビューアセット: %s", previewAssetPath);
    ImGui::Text("動作モード: %s", MotionModeLabel(mode_));
    ImGui::Text("攻撃再生中: %s", YesNo(attack.IsPlaying()));
    ImGui::Text("一時停止中: %s", YesNo(attack.IsPaused()));
    ImGui::Text("ループ: %s", YesNo(attack.IsLoopEnabled()));
    ImGui::Text("検出チェーン数: %d", static_cast<int>(chainCount));
    ImGui::Text(
        "現在フェーズ: %s",
        GetKrakenTentacleAttackPhaseJapaneseLabel(attack.GetPhase()));
    ImGui::Text("フェーズ経過時間: %.3f 秒", attack.GetPhaseElapsedTime());
    ImGui::Text("全体経過時間: %.3f 秒", attack.GetElapsedTime());
    ImGui::Text("動作全長: %.3f 秒", attack.GetMotionDuration());
    if (attack.IsWaitingForLoop()) {
        ImGui::Text(
            "ループ待機経過時間: %.3f 秒",
            attack.GetLoopWaitElapsedTime());
    }

    ImGui::SeparatorText("攻撃対象チェーン##AttackChainSelection");
    ImGui::BeginDisabled(attack.IsPlaying() || chainCount == 0);
    const std::string selectedChainLabel = hasSelectedChain
        ? "Chain " + std::to_string(selectedChainIndex)
        : "選択なし";
    if (ImGui::BeginCombo(
        "選択チェーン##AttackSelectedChain",
        selectedChainLabel.c_str())) {
        for (std::size_t chainIndex = 0;
            chainIndex < chainCount;
            ++chainIndex) {
            const std::string label =
                "Chain " + std::to_string(chainIndex) +
                "（" + std::to_string(chains_[chainIndex].joints.size()) +
                "ボーン）";
            const bool isSelected = chainIndex == selectedChainIndex;
            if (ImGui::Selectable(label.c_str(), isSelected)) {
                attack.SelectChain(chainIndex, chainCount);
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    if (attack.IsPlaying()) {
        ImGui::TextDisabled("再生中は対象チェーンを変更できません。");
    }
    ImGui::Text(
        "選択チェーン番号: %s",
        hasSelectedChain ? std::to_string(selectedChainIndex).c_str() : "なし");
    ImGui::Text(
        "チェーン根元ボーン: %s",
        chainRoot ? chainRoot->name.c_str() : "なし");
    ImGui::Text(
        "チェーン先端ボーン: %s",
        chainTip ? chainTip->name.c_str() : "なし");
    ImGui::Text(
        "チェーンのボーン数: %d",
        selectedChain ? static_cast<int>(selectedChain->joints.size()) : 0);
    ImGui::Text(
        "対象ボーン数: %d",
        static_cast<int>(targetBoneCount));

    const KrakenTentacleAttackPoseTotals poseTotals =
        attack.EvaluatePoseTotals();
    ImGui::Text("現在の主軸合計角度: %.2f 度", poseTotals.primaryDegrees);
    ImGui::Text("現在の副軸合計角度: %.2f 度", poseTotals.secondaryDegrees);

    ImGui::SeparatorText("再生操作##AttackPlaybackControls");
    ImGui::BeginDisabled(!hierarchyValid_ || chainCount == 0);
    if (ImGui::Button("1回再生##AttackPlayOnce")) {
        if (attack.PlayOnce(chainCount)) {
            mode_ = Mode::AttackSlamPreview;
            isPaused_ = true;
            motionTime_ = 0.0f;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("ループ再生##AttackPlayLoop")) {
        if (attack.PlayLoop(chainCount)) {
            mode_ = Mode::AttackSlamPreview;
            isPaused_ = true;
            motionTime_ = 0.0f;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!attack.IsPlaying() || attack.IsPaused());
    if (ImGui::Button("一時停止##AttackPause")) {
        attack.Pause();
    }
    ImGui::EndDisabled();

    ImGui::BeginDisabled(!attack.IsPlaying() || !attack.IsPaused());
    if (ImGui::Button("再開##AttackResume")) {
        attack.Resume();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("停止##AttackStop")) {
        attack.Stop();
        mode_ = Mode::AttackSlamPreview;
        isPaused_ = true;
        motionTime_ = 0.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("最初から再生##AttackRestart")) {
        if (attack.Restart(chainCount)) {
            mode_ = Mode::AttackSlamPreview;
            isPaused_ = true;
            motionTime_ = 0.0f;
        }
    }

    bool loopEnabled = attack.IsLoopEnabled();
    if (ImGui::Checkbox("ループ##AttackLoopEnabled", &loopEnabled)) {
        attack.SetLoopEnabled(loopEnabled);
    }
    ImGui::EndDisabled();

    if (ImGui::Button("バインドポーズへ戻す##AttackReturnBind")) {
        attack.Stop();
        attack.ClearLastError();
        std::fill(
            manualRotationDegrees_.begin(),
            manualRotationDegrees_.end(),
            Vector3{});
        mode_ = Mode::Manual;
        isPaused_ = true;
        motionTime_ = 0.0f;
        runtimeError_.clear();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!hierarchyValid_);
    if (ImGui::Button("アイドルスウェイへ戻す##AttackReturnIdle")) {
        attack.Stop();
        mode_ = Mode::IdleSway;
        isPaused_ = false;
        motionTime_ = 0.0f;
    }
    ImGui::EndDisabled();

    ImGui::SeparatorText("フェーズ移動##AttackPhaseSeek");
    ImGui::BeginDisabled(!hierarchyValid_ || chainCount == 0);
    const auto jumpToPhase = [&](KrakenTentacleAttackPreviewPhase phase) {
        if (attack.JumpToPhase(phase, chainCount)) {
            mode_ = Mode::AttackSlamPreview;
            isPaused_ = true;
            motionTime_ = 0.0f;
        }
    };
    if (ImGui::Button("振りかぶり開始##SeekAttackWindup")) {
        jumpToPhase(KrakenTentacleAttackPreviewPhase::Windup);
    }
    ImGui::SameLine();
    if (ImGui::Button("振りかぶり停止##SeekAttackWindupHold")) {
        jumpToPhase(KrakenTentacleAttackPreviewPhase::WindupHold);
    }
    ImGui::SameLine();
    if (ImGui::Button("振り下ろし開始##SeekAttackSlam")) {
        jumpToPhase(KrakenTentacleAttackPreviewPhase::Slam);
    }
    if (ImGui::Button("打撃位置停止##SeekAttackImpactHold")) {
        jumpToPhase(KrakenTentacleAttackPreviewPhase::ImpactHold);
    }
    ImGui::SameLine();
    if (ImGui::Button("復帰開始##SeekAttackRecovery")) {
        jumpToPhase(KrakenTentacleAttackPreviewPhase::Recovery);
    }
    ImGui::EndDisabled();

    ImGui::SeparatorText("フェーズ時間##AttackPhaseDurations");
    KrakenTentacleAttackPreviewSettings settings = currentSettings;
    bool settingsChanged = false;
    settingsChanged |= ImGui::DragFloat(
        "振りかぶり時間##AttackWindupDuration",
        &settings.windupDuration, 0.01f, 0.10f, 2.00f, "%.2f 秒");
    settingsChanged |= ImGui::DragFloat(
        "振りかぶり停止時間##AttackWindupHoldDuration",
        &settings.windupHoldDuration, 0.01f, 0.00f, 1.00f, "%.2f 秒");
    settingsChanged |= ImGui::DragFloat(
        "振り下ろし時間##AttackSlamDuration",
        &settings.slamDuration, 0.01f, 0.05f, 1.00f, "%.2f 秒");
    DrawTooltip(
        "振りかぶり姿勢から打撃姿勢へ触手を振り下ろす時間です。");
    settingsChanged |= ImGui::DragFloat(
        "打撃位置停止時間##AttackImpactHoldDuration",
        &settings.impactHoldDuration, 0.01f, 0.00f, 1.00f, "%.2f 秒");
    DrawTooltip(
        "選択中の攻撃コライダーが有効予定になる停止フェーズです。ゲームプレイには未登録です。");
    settingsChanged |= ImGui::DragFloat(
        "復帰時間##AttackRecoveryDuration",
        &settings.recoveryDuration, 0.01f, 0.10f, 2.00f, "%.2f 秒");
    settingsChanged |= ImGui::DragFloat(
        "ループ待機時間##AttackLoopInterval",
        &settings.loopInterval, 0.01f, 0.00f, 2.00f, "%.2f 秒");

    ImGui::SeparatorText("攻撃ポーズ設定##AttackPoseSettings");
    settingsChanged |= ImGui::DragFloat(
        "振りかぶり主軸合計角度##AttackWindupPrimary",
        &settings.windupPrimaryTotalDegrees,
        0.5f, -120.0f, 120.0f, "%.1f 度");
    settingsChanged |= ImGui::DragFloat(
        "振りかぶり副軸合計角度##AttackWindupSecondary",
        &settings.windupSecondaryTotalDegrees,
        0.5f, -60.0f, 60.0f, "%.1f 度");
    settingsChanged |= ImGui::DragFloat(
        "振り下ろし主軸合計角度##AttackSlamPrimary",
        &settings.slamPrimaryTotalDegrees,
        0.5f, -120.0f, 120.0f, "%.1f 度");
    settingsChanged |= ImGui::DragFloat(
        "振り下ろし副軸合計角度##AttackSlamSecondary",
        &settings.slamSecondaryTotalDegrees,
        0.5f, -60.0f, 60.0f, "%.1f 度");
    settingsChanged |= ImGui::DragFloat(
        "先端配分バイアス（Tip Bias）##AttackTipBias",
        &settings.tipBias, 0.05f, 0.10f, 8.00f, "%.2f");
    DrawTooltip(
        "チェーン内で先端側のボーンへ、どの程度大きく回転を配分するか調整します。");

    const int maximumFixedBones = selectedChain
        ? (std::max)(
            static_cast<int>(selectedChain->joints.size()) - 1,
            0)
        : 0;
    int fixedLeadingBoneCount = std::clamp(
        static_cast<int>(settings.fixedLeadingBoneCount),
        0,
        maximumFixedBones);
    if (ImGui::DragInt(
        "固定する先頭ボーン数##AttackFixedLeadingBones",
        &fixedLeadingBoneCount,
        1.0f,
        0,
        maximumFixedBones)) {
        settings.fixedLeadingBoneCount = static_cast<std::uint32_t>(
            std::clamp(fixedLeadingBoneCount, 0, maximumFixedBones));
        settingsChanged = true;
    }
    settingsChanged |= DrawAxisCombo(
        "主回転軸##AttackPrimaryAxis",
        settings.primaryAxis);
    settingsChanged |= DrawSignCombo(
        "主回転符号##AttackPrimarySign",
        settings.primarySign);
    settingsChanged |= DrawAxisCombo(
        "副回転軸##AttackSecondaryAxis",
        settings.secondaryAxis);
    settingsChanged |= DrawSignCombo(
        "副回転符号##AttackSecondarySign",
        settings.secondarySign);

    if (settingsChanged) {
        attack.SetSettings(settings);
    }
    if (ImGui::Button("推奨設定へ戻す##ResetAttackRecommendedSettings")) {
        attack.ResetRecommendedSettings();
    }

    ImGui::SeparatorText("先端位置診断##AttackTipDiagnostics");
    ImGui::Text(
        "先端ボーン名: %s",
        chainTip ? chainTip->name.c_str() : "なし");
    ImGui::Text("先端ジョイント番号: %d", attackTipDiagnostics_.jointIndex);
    if (attackTipDiagnostics_.valid) {
        const Vector3 worldDifference{
            attackTipDiagnostics_.worldPosition.x -
                attackTipDiagnostics_.bindWorldPosition.x,
            attackTipDiagnostics_.worldPosition.y -
                attackTipDiagnostics_.bindWorldPosition.y,
            attackTipDiagnostics_.worldPosition.z -
                attackTipDiagnostics_.bindWorldPosition.z,
        };
        ImGui::Text(
            "先端スケルトン座標: %.3f, %.3f, %.3f",
            attackTipDiagnostics_.skeletonPosition.x,
            attackTipDiagnostics_.skeletonPosition.y,
            attackTipDiagnostics_.skeletonPosition.z);
        ImGui::Text(
            "先端ワールド座標: %.3f, %.3f, %.3f",
            attackTipDiagnostics_.worldPosition.x,
            attackTipDiagnostics_.worldPosition.y,
            attackTipDiagnostics_.worldPosition.z);
        ImGui::Text(
            "バインドポーズ先端との差: %.3f, %.3f, %.3f",
            worldDifference.x,
            worldDifference.y,
            worldDifference.z);
        ImGui::Text(
            "先端移動距離: %.3f",
            attackTipDiagnostics_.distanceFromBind);
    } else {
        ImGui::TextDisabled("先端位置を取得できません。");
    }

    ImGui::Text(
        "打撃位置停止フェーズ中: %s",
        YesNo(
            attack.GetPhase() ==
            KrakenTentacleAttackPreviewPhase::ImpactHold));
    ImGui::Text("打撃位置停止の開始回数: %u", attack.GetImpactStartCount());
    ImGui::Text("打撃位置停止の開始時刻: %.3f 秒", attack.GetLastImpactStartTime());

    ImGui::SeparatorText("スキニング安全診断##AttackSkinningDiagnostics");
    ImGui::Text("スキニング更新成功: %s", YesNo(diagnostics_.skinningUpdateSucceeded));
    ImGui::Text("ボーン表示同期: %s", YesNo(diagnostics_.boneOverlaySynchronized));
    ImGui::Text("パレット数: %u", diagnostics_.paletteMatrixCount);
    ImGui::Text("変化パレット数: %u", diagnostics_.changedPaletteMatrixCount);
    ImGui::Text("単位パレット数: %u", diagnostics_.identityPaletteMatrixCount);
    ImGui::Text("非有限パレット数: %u", diagnostics_.nonFinitePaletteMatrixCount);
    ImGui::Text("ウェイトなし頂点数: %u", diagnostics_.verticesWithoutWeights);
    ImGui::Text("無効ジョイント参照数: %u", diagnostics_.invalidJointInfluenceCount);
    ImGui::Text("非有限スキニング頂点数: %u",
        diagnostics_.nonFiniteSkinnedVertexCount);
    ImGui::Text("異常な境界: %s", YesNo(diagnostics_.abnormalBoundsDetected));
    ImGui::Text("安全復帰回数: %u", diagnostics_.safetyRecoveryCount);
    if (diagnostics_.skinnedBounds.isValid) {
        ImGui::Text(
            "スキニング後の境界最小: %.3f, %.3f, %.3f",
            diagnostics_.skinnedBounds.min.x,
            diagnostics_.skinnedBounds.min.y,
            diagnostics_.skinnedBounds.min.z);
        ImGui::Text(
            "スキニング後の境界最大: %.3f, %.3f, %.3f",
            diagnostics_.skinnedBounds.max.x,
            diagnostics_.skinnedBounds.max.y,
            diagnostics_.skinnedBounds.max.z);
        ImGui::Text(
            "スキニング後の境界サイズ: %.3f, %.3f, %.3f",
            diagnostics_.skinnedBounds.size.x,
            diagnostics_.skinnedBounds.size.y,
            diagnostics_.skinnedBounds.size.z);
    }

    const std::string& attackError = attack.GetLastError();
    const std::string poseError =
        attackPoseResult_ ? attackPoseResult_->errorMessage : std::string{};
    if (!runtimeError_.empty()) {
        ImGui::TextColored(
            ImVec4(1.0f, 0.40f, 0.25f, 1.0f),
            "最後のエラー: %s",
            runtimeError_.c_str());
    } else if (!attackError.empty()) {
        ImGui::TextColored(
            ImVec4(1.0f, 0.40f, 0.25f, 1.0f),
            "最後のエラー: %s",
            attackError.c_str());
    } else if (!poseError.empty()) {
        ImGui::TextColored(
            ImVec4(1.0f, 0.40f, 0.25f, 1.0f),
            "最後のエラー: %s",
            poseError.c_str());
    } else {
        ImGui::Text("最後のエラー: なし");
    }
#endif
}
