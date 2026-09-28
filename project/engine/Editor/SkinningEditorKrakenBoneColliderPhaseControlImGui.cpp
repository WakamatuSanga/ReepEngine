#include "SkinningEditorKrakenMotionPreview.h"

#include "SkinningEditorKrakenAttackMotion.h"
#include "SkinningEditorKrakenBoneColliderPhaseControl.h"
#include "SkinningEditorKrakenBoneColliderPreviewCollection.h"

#include <algorithm>
#include <cmath>

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

    template <typename Collider>
    void DrawSelectedColliderState(
        const Collider& collider,
        const char* shapeLabel,
        std::size_t localIndex,
        const KrakenColliderPhaseMotionSnapshot& snapshot,
        const KrakenBoneColliderPhaseDiagnostics& diagnostics,
        float activeStartRatio) {
        ImGui::Text(
            "コライダー識別子: チェーン %u / %s %zu",
            collider.chainIndex,
            shapeLabel,
            localIndex);
        ImGui::Text(
            "役割: %s",
            GetKrakenColliderPreviewRoleJapaneseLabel(collider.role));
        ImGui::Text("チェーン番号: %u", collider.chainIndex);
        ImGui::Text("形状: %s", shapeLabel);
        ImGui::Text("表示中: %s", YesNo(collider.previewVisible));
        ImGui::Text("コライダー有効設定: %s", YesNo(collider.enabled));
        ImGui::Text("コライダー正常: %s", YesNo(collider.valid));
        ImGui::Text("フェーズ有効予定: %s", YesNo(collider.phaseActive));
        DrawTooltip(
            "将来ゲームプレイでコライダーを有効にする予定の状態です。\n"
            "今回、当たり判定管理には登録していません。");
        ImGui::Text(
            "ゲームプレイ登録: %s",
            YesNo(collider.gameplayRegistered));
        DrawTooltip(
            "実際のゲーム当たり判定へ登録済みかを示します。\n"
            "今回の工程では常に「いいえ」です。");
        ImGui::Text(
            "フェーズ判定理由: %s",
            GetKrakenColliderPhaseReasonJapaneseLabel(
                collider.phaseReason));
        ImGui::Text(
            "現在フェーズ: %s",
            GetKrakenTentacleAttackPhaseJapaneseLabel(snapshot.phase));
        ImGui::Text(
            "振り下ろし進行率: %.3f",
            diagnostics.slamProgress);
        ImGui::Text(
            "有効開始進行率: %.3f",
            activeStartRatio);
    }

    void DrawCounter(
        const char* label,
        std::uint64_t value) {
        ImGui::Text(
            "%s: %llu",
            label,
            static_cast<unsigned long long>(value));
    }
#endif
}

void SkinningEditorKrakenMotionPreview::
DrawBoneColliderPhaseControlImGui() {
#ifdef USE_IMGUI
    if (!boneColliderPreview_ || !boneColliderPhaseControl_) {
        return;
    }

    SkinningEditorKrakenBoneColliderPreviewCollection& collection =
        *boneColliderPreview_;
    SkinningEditorKrakenBoneColliderPhaseControl& phaseControl =
        *boneColliderPhaseControl_;
    KrakenBoneColliderPhaseControlSettings phaseSettings =
        phaseControl.GetSettings();
    if (!phaseSettings.impactHoldActive) {
        phaseSettings.impactHoldActive = true;
        phaseControl.SetSettings(phaseSettings);
    }

    const KrakenBoneColliderPhaseDiagnostics& phaseDiagnostics =
        phaseControl.GetDiagnostics();
    const KrakenColliderPhaseMotionSnapshot& snapshot =
        phaseDiagnostics.snapshot;

    ImGui::SeparatorText(
        "コライダーフェーズ有効状態##KrakenColliderPhaseControl");

    ImGui::SeparatorText(
        "現在の動作状態##ColliderPhaseMotionState");
    ImGui::Text(
        "動作モード: %s",
        GetKrakenColliderPhaseMotionModeJapaneseLabel(
            snapshot.motionMode));
    ImGui::Text(
        "攻撃モーション有効: %s",
        YesNo(snapshot.motionMode ==
            KrakenColliderPhaseMotionMode::AttackSlamPreview));
    ImGui::Text("攻撃再生中: %s", YesNo(snapshot.playing));
    ImGui::Text("一時停止中: %s", YesNo(snapshot.paused));
    ImGui::Text("ループ: %s", YesNo(snapshot.loop));
    ImGui::Text("ループ待機中: %s", YesNo(snapshot.waitingForLoop));
    ImGui::Text(
        "選択中の攻撃チェーン: %zu",
        snapshot.selectedChainIndex);
    ImGui::Text(
        "現在フェーズ: %s",
        GetKrakenTentacleAttackPhaseJapaneseLabel(snapshot.phase));
    ImGui::Text(
        "フェーズ経過時間: %.3f 秒",
        snapshot.phaseElapsedTime);
    ImGui::Text(
        "フェーズ時間: %.3f 秒",
        snapshot.phaseDuration);
    ImGui::Text(
        "フェーズ進行率: %.3f",
        snapshot.phaseNormalizedTime);
    ImGui::Text("プレビュー接続: %s", YesNo(snapshot.connected));
    ImGui::Text("動作状態有効: %s", YesNo(snapshot.valid));
    ImGui::Text(
        "安全復帰中: %s",
        YesNo(snapshot.safetyRecovery));

    ImGui::SeparatorText(
        "攻撃コライダー設定##ColliderPhaseSettings");
    bool settingsChanged = ImGui::SliderFloat(
        "振り下ろし有効開始進行率##AttackActiveStartRatio",
        &phaseSettings.attackActiveStartRatio,
        0.0f,
        1.0f,
        "%.3f",
        ImGuiSliderFlags_AlwaysClamp);
    DrawTooltip(
        "振り下ろしフェーズのどの位置から、攻撃コライダーを\n"
        "有効予定にするかを指定します。0.65の場合、\n"
        "振り下ろしの65％以降で有効になります。");
    phaseSettings.attackActiveStartRatio = std::clamp(
        std::isfinite(phaseSettings.attackActiveStartRatio)
            ? phaseSettings.attackActiveStartRatio
            : 0.65f,
        0.0f,
        1.0f);
    bool impactHoldActive = true;
    ImGui::BeginDisabled();
    ImGui::Checkbox(
        "打撃位置停止を有効時間へ含める##ImpactHoldActive",
        &impactHoldActive);
    ImGui::EndDisabled();
    DrawTooltip(
        "打撃位置停止中は、選択中の攻撃チェーンだけを有効予定にします。");
    phaseSettings.impactHoldActive = true;
    if (settingsChanged) {
        phaseControl.SetSettings(phaseSettings);
        RefreshBoneColliderPhaseControl();
    }

    ImGui::SeparatorText(
        "コライダー状態##ColliderPhaseCounts");
    ImGui::Text(
        "全コライダー数: %u",
        phaseDiagnostics.currentColliderCount);
    ImGui::Text(
        "有効予定コライダー数: %u",
        phaseDiagnostics.currentActiveCount);
    ImGui::Text(
        "無効予定コライダー数: %u",
        phaseDiagnostics.currentInactiveCount);
    ImGui::Text(
        "攻撃コライダー総数: %u",
        phaseDiagnostics.currentAttackColliderCount);
    ImGui::Text(
        "攻撃有効予定数: %u",
        phaseDiagnostics.currentAttackActiveCount);
    ImGui::Text(
        "攻撃無効予定数: %u",
        phaseDiagnostics.currentAttackInactiveCount);
    ImGui::Text(
        "ダメージ有効予定数: %u / %u",
        phaseDiagnostics.currentDamageActiveCount,
        phaseDiagnostics.currentDamageColliderCount);
    ImGui::Text(
        "弱点有効予定数: %u / %u",
        phaseDiagnostics.currentWeakPointActiveCount,
        phaseDiagnostics.currentWeakPointColliderCount);
    ImGui::Text(
        "表示コライダー数: %u",
        phaseDiagnostics.currentPreviewVisibleCount);
    ImGui::Text(
        "ゲームプレイ登録数: %u",
        phaseDiagnostics.currentGameplayRegisteredCount);
    ImGui::Text("ゲームプレイ未接続: はい");

    ImGui::SeparatorText(
        "選択コライダー##SelectedColliderPhaseState");
    if (const KrakenBoneColliderPreview* selectedCapsule =
            collection.GetSelectedCapsule()) {
        DrawSelectedColliderState(
            *selectedCapsule,
            "カプセル",
            collection.GetSelectedLocalIndex(),
            snapshot,
            phaseDiagnostics,
            phaseSettings.attackActiveStartRatio);
    } else if (const KrakenTipSphereColliderPreview* selectedTip =
            collection.GetSelectedTipSphere()) {
        DrawSelectedColliderState(
            *selectedTip,
            "先端スフィア",
            collection.GetSelectedLocalIndex(),
            snapshot,
            phaseDiagnostics,
            phaseSettings.attackActiveStartRatio);
    } else {
        ImGui::TextDisabled("選択中のコライダーはありません。");
    }

    ImGui::SeparatorText(
        "遷移診断##ColliderPhaseTransitionDiagnostics");
    DrawCounter(
        "フェーズ評価パス回数",
        phaseDiagnostics.evaluationPassCount);
    DrawCounter(
        "コライダーフェーズ評価回数",
        phaseDiagnostics.phaseEvaluationCount);
    DrawCounter(
        "攻撃コライダー評価回数",
        phaseDiagnostics.attackEvaluationCount);
    DrawCounter(
        "ダメージコライダー評価回数",
        phaseDiagnostics.damageEvaluationCount);
    DrawCounter(
        "弱点コライダー評価回数",
        phaseDiagnostics.weakPointEvaluationCount);
    DrawCounter(
        "無効予定から有効予定への遷移回数",
        phaseDiagnostics.activationTransitionCount);
    DrawCounter(
        "有効予定から無効予定への遷移回数",
        phaseDiagnostics.deactivationTransitionCount);
    ImGui::Text(
        "最後に有効予定になったフェーズ: %s",
        phaseDiagnostics.hasLastActivation
            ? GetKrakenTentacleAttackPhaseJapaneseLabel(
                phaseDiagnostics.lastActivatedPhase)
            : "なし");
    ImGui::Text(
        "最後に無効予定になったフェーズ: %s",
        phaseDiagnostics.hasLastDeactivation
            ? GetKrakenTentacleAttackPhaseJapaneseLabel(
                phaseDiagnostics.lastDeactivatedPhase)
            : "なし");
    if (phaseDiagnostics.hasLastTransition) {
        ImGui::Text(
            "最後の遷移時刻: %.3f 秒",
            phaseDiagnostics.lastTransitionTime);
        ImGui::Text(
            "最後の遷移フレーム: %llu",
            static_cast<unsigned long long>(
                phaseDiagnostics.lastTransitionFrame));
    } else {
        ImGui::Text("最後の遷移時刻: なし");
        ImGui::Text("最後の遷移フレーム: なし");
    }
    DrawCounter(
        "同一フレーム二重遷移数",
        phaseDiagnostics.sameFrameDoubleTransitionCount);
    DrawCounter(
        "不正な動作状態数",
        phaseDiagnostics.invalidMotionSnapshotCount);
    DrawCounter(
        "未接続状態数",
        phaseDiagnostics.disconnectedSnapshotCount);
    DrawCounter(
        "安全復帰状態数",
        phaseDiagnostics.safetyRecoveryCount);
    DrawCounter(
        "範囲外チェーン数",
        phaseDiagnostics.outOfRangeChainCount);
    DrawCounter(
        "不正フェーズ数",
        phaseDiagnostics.invalidPhaseCount);
    DrawCounter(
        "振り下ろし時間不正数",
        phaseDiagnostics.invalidSlamDurationCount);
    DrawCounter(
        "ゲームプレイ登録検出数",
        phaseDiagnostics.gameplayRegistrationDetectionCount);
    ImGui::Text(
        "未生成チェーンプレビュー数: %u",
        phaseDiagnostics.nullChainPreviewCount);
    if (!phaseDiagnostics.lastWarning.empty()) {
        ImGui::TextColored(
            ImVec4(1.0f, 0.72f, 0.25f, 1.0f),
            "最後の警告: %s",
            phaseDiagnostics.lastWarning.c_str());
    } else {
        ImGui::Text("最後の警告: なし");
    }

    ImGui::SeparatorText(
        "フェーズ診断操作##ColliderPhaseOperations");
    if (ImGui::Button(
            "推奨フェーズ設定へ戻す##ResetRecommendedPhaseSettings")) {
        phaseControl.ResetRecommendedSettings();
        RefreshBoneColliderPhaseControl();
    }
    ImGui::SameLine();
    if (ImGui::Button(
            "フェーズ診断をリセット##ResetPhaseDiagnostics")) {
        phaseControl.ResetDiagnostics();
    }

    const bool phaseSeekDisabled =
        !hierarchyValid_ || !attackMotion_ || chains_.empty();
    ImGui::BeginDisabled(phaseSeekDisabled);
    const auto enterAttackMode = [&]() {
        mode_ = Mode::AttackSlamPreview;
        isPaused_ = true;
        motionTime_ = 0.0f;
    };
    const auto seekSlam = [&](float normalizedTime) {
        if (attackMotion_->SeekPhaseNormalizedTime(
                KrakenTentacleAttackPreviewPhase::Slam,
                normalizedTime,
                chains_.size())) {
            enterAttackMode();
            RefreshBoneColliderPhaseControl();
        }
    };
    const float threshold =
        phaseControl.GetSettings().attackActiveStartRatio;
    const float beforeThreshold = (std::max)(
        0.0f,
        threshold - 0.001f);
    if (ImGui::Button(
            "振り下ろし有効直前へ移動##SeekBeforeSlamActivation")) {
        seekSlam(beforeThreshold);
    }
    ImGui::SameLine();
    if (ImGui::Button(
            "振り下ろし有効開始へ移動##SeekSlamActivationBoundary")) {
        seekSlam(threshold);
    }

    const auto jumpToPhase = [&](KrakenTentacleAttackPreviewPhase phase) {
        if (attackMotion_->JumpToPhase(phase, chains_.size())) {
            enterAttackMode();
            RefreshBoneColliderPhaseControl();
        }
    };
    if (ImGui::Button(
            "打撃位置停止へ移動##SeekColliderImpactHold")) {
        jumpToPhase(KrakenTentacleAttackPreviewPhase::ImpactHold);
    }
    ImGui::SameLine();
    if (ImGui::Button(
            "復帰へ移動##SeekColliderRecovery")) {
        jumpToPhase(KrakenTentacleAttackPreviewPhase::Recovery);
    }
    ImGui::EndDisabled();

    if (ImGui::Button(
            "全攻撃コライダーを再評価##ReevaluateAttackColliders")) {
        RefreshBoneColliderPhaseControl();
    }
    ImGui::SameLine();
    if (ImGui::Button(
            "選択チェーンを再同期##ResynchronizeAttackChain")) {
        collection.SetDisplayChainIndex(
            ResolveBoneColliderChainIndex());
        collection.ResynchronizeSelection();
        RefreshBoneColliderPhaseControl();
    }

    if (attackMotion_ && !attackMotion_->GetLastError().empty()) {
        ImGui::TextColored(
            ImVec4(1.0f, 0.35f, 0.25f, 1.0f),
            "フェーズ操作エラー: %s",
            attackMotion_->GetLastError().c_str());
    }
#endif
}
