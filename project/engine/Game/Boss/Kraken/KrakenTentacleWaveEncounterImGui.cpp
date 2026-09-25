#include "KrakenTentacleWaveEncounterController.h"

#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossController.h"
#include "Engine/Game/Boss/Kraken/KrakenTentacleWaveEncounterConfig.h"
#include "Engine/Game/Camera/RailShooterCameraRig.h"
#include "Engine/Game/RailShooter/EnemyWaveManager.h"
#include "Engine/Graphics/Camera/Camera.h"

#include <numbers>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
const char* BoolText(bool value) {
    return value ? "はい" : "いいえ";
}

const char* EncounterStateText(KrakenTentacleWaveEncounterState state) {
    switch (state) {
    case KrakenTentacleWaveEncounterState::WaitingForWave4:
        return "ウェーブ4待機";
    case KrakenTentacleWaveEncounterState::Starting:
        return "開始処理中";
    case KrakenTentacleWaveEncounterState::Active:
        return "交戦中";
    case KrakenTentacleWaveEncounterState::Defeating:
        return "撃破演出中";
    case KrakenTentacleWaveEncounterState::CompletionPublished:
        return "完了通知済み";
    case KrakenTentacleWaveEncounterState::WaitingForWave5:
        return "ウェーブ5待機";
    case KrakenTentacleWaveEncounterState::Completed:
        return "完了";
    case KrakenTentacleWaveEncounterState::Error:
        return "エラー";
    }
    return "不明";
}

const char* KrakenStateText(KrakenTentacleMidbossState state) {
    switch (state) {
    case KrakenTentacleMidbossState::Hidden: return "非表示";
    case KrakenTentacleMidbossState::Idle: return "待機";
    case KrakenTentacleMidbossState::Windup: return "振りかぶり";
    case KrakenTentacleMidbossState::WindupHold: return "振りかぶり保持";
    case KrakenTentacleMidbossState::Slam: return "振り下ろし";
    case KrakenTentacleMidbossState::ImpactHold: return "衝撃保持";
    case KrakenTentacleMidbossState::Recovery: return "復帰中";
    case KrakenTentacleMidbossState::Defeated: return "撃破済み";
    case KrakenTentacleMidbossState::Retreating: return "退場中";
    case KrakenTentacleMidbossState::RetreatCompleted: return "退場完了";
    }
    return "不明";
}
}

void KrakenTentacleWaveEncounterController::DrawImGui() {
#ifdef USE_IMGUI
    if (!ImGui::Begin(
            "クラーケン中ボス ウェーブ4連携"
            "###KrakenTentacleWave4Encounter")) {
        ImGui::End();
        return;
    }

    const bool hasManager = waveManager_ != nullptr;
    DrawEntranceSettingsImGui();
    if (kraken_) {
        kraken_->DrawPlacementImGui();
    }
    const bool hasKraken = kraken_ != nullptr;
    const bool hasRail = railRig_ != nullptr;
    const std::string currentWaveId =
        hasManager ? waveManager_->GetCurrentWaveId() : "(未接続)";
    const std::size_t currentIndex = hasManager
        ? waveManager_->GetCurrentWaveIndex()
        : EnemyWaveManager::kInvalidWaveIndex;
    const bool canAttack = state_ == KrakenTentacleWaveEncounterState::Active &&
        schedulerEnabled_ && hasKraken && kraken_->IsVisible() &&
        kraken_->GetCurrentHp() > 0.0f && !kraken_->IsDefeatPending() &&
        !kraken_->IsDefeatStarted() && !kraken_->IsDefeatCompleted() &&
        kraken_->GetRuntimeState() == KrakenTentacleMidbossState::Idle &&
        kraken_->IsAttackDamageEnabled() &&
        kraken_->IsProjectileDamageEnabled() && detectedChainCount_ > 0;

    if (ImGui::CollapsingHeader("ウェーブ情報##WaveInfo", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("現在ウェーブID: %s", currentWaveId.c_str());
        ImGui::Text("ユーザー向けウェーブ番号: %s",
            IsTargetWaveCurrent() ? "4" : (IsNextWaveCurrent() ? "5" : "-") );
        if (currentIndex == EnemyWaveManager::kInvalidWaveIndex) {
            ImGui::Text("内部番号: 無効");
        } else {
            ImGui::Text("内部番号: %zu", currentIndex);
        }
        ImGui::Text("ウェーブ改訂番号: %llu",
            static_cast<unsigned long long>(hasManager ?
                waveManager_->GetCurrentWaveRevision() : 0));
        ImGui::Text("対象ウェーブID: %s",
            KrakenTentacleWaveEncounterConfig::kTargetWaveId.data());
        ImGui::Text("対象ウェーブ改訂番号: %llu",
            static_cast<unsigned long long>(targetWaveRevision_));
        ImGui::Text("ウェーブ4検出: %s", BoolText(IsTargetWaveCurrent()));
        ImGui::Text("ウェーブ5検出: %s", BoolText(IsNextWaveCurrent()));
        ImGui::Text("ウェーブ4開始回数: %llu",
            static_cast<unsigned long long>(wave4StartCount_));
        ImGui::Text("ウェーブ5移行回数: %llu",
            static_cast<unsigned long long>(wave5TransitionCount_));
    }

    if (ImGui::CollapsingHeader("中ボス戦状態##Encounter", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("中ボス戦状態: %s", EncounterStateText(state_));
        ImGui::Text("交戦開始済み: %s", BoolText(encounterStartedForRevision_));
        ImGui::Text("交戦完了: %s", BoolText(state_ == KrakenTentacleWaveEncounterState::Completed));
        ImGui::Text("交戦エラー: %s", BoolText(state_ == KrakenTentacleWaveEncounterState::Error));
        ImGui::Text("中ボス自動制御中: %s", BoolText(IsControllingKraken()));
        ImGui::Text("中ボス実行状態: %s", hasKraken ?
            KrakenStateText(kraken_->GetRuntimeState()) : "未接続");
        ImGui::Text("中ボス表示中: %s", BoolText(hasKraken && kraken_->IsVisible()));
        ImGui::Text("現在体力: %.1f", hasKraken ? kraken_->GetCurrentHp() : 0.0f);
        ImGui::Text("最大体力: %.1f", hasKraken ? kraken_->GetMaxHp() : 0.0f);
        ImGui::Text("撃破開始: %s", BoolText(hasKraken && kraken_->IsDefeatStarted()));
        ImGui::Text("撃破完了: %s", BoolText(hasKraken && kraken_->IsDefeatCompleted()));
        ImGui::Text("撃破シーケンス識別子: %llu",
            static_cast<unsigned long long>(hasKraken ? kraken_->GetDefeatSequenceId() : 0));
        ImGui::Text("攻撃ダメージ: %s", BoolText(hasKraken && kraken_->IsAttackDamageEnabled()));
        ImGui::Text("弾ダメージ: %s", BoolText(hasKraken && kraken_->IsProjectileDamageEnabled()));
    }

    if (ImGui::CollapsingHeader("外部完了条件##Objective")) {
        ImGui::Text("完了条件設定済み: %s", BoolText(hasManager && waveManager_->IsExternalWaveObjectiveConfigured()));
        ImGui::Text("対象ウェーブID: %s", hasManager ?
            waveManager_->GetExternalWaveObjectiveTargetWaveId().c_str() : "未接続");
        ImGui::Text("完了条件達成: %s", BoolText(hasManager && waveManager_->IsExternalWaveObjectiveCompleted()));
        ImGui::Text("ウェーブ完了保留中: %s", BoolText(hasManager && waveManager_->IsCurrentWaveBlockedByExternalObjective()));
        ImGui::Text("自動完了通知済み: %s", BoolText(completionPublished_));
        ImGui::Text("完了通知回数: %llu", static_cast<unsigned long long>(completionPublishCount_));
        ImGui::Text("二重通知抑制数: %llu", static_cast<unsigned long long>(duplicateCompletionSuppressionCount_));
        ImGui::Text("手動完了検出数: %llu", static_cast<unsigned long long>(manualObjectiveCompletionCount_));
    }

    if (ImGui::CollapsingHeader("レール進行停止##RailHold")) {
        ImGui::Text("レール停止有効: %s", BoolText(railHoldEnabled_));
        ImGui::Text("レール速度倍率: %.3f", hasRail ? railRig_->GetExternalEncounterRailSpeedMultiplier() : 0.0f);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("中ボス戦中だけレールの最終速度へ掛ける倍率です。\n0の場合、レール距離だけが停止し、プレイヤー入力や中ボス動作は停止しません。");
        }
        ImGui::Text("基本レール速度: %.3f", hasRail ? railRig_->GetBaseRailSpeed() : 0.0f);
        ImGui::Text("既存速度倍率: %.3f", hasRail ? railRig_->GetExistingRailSpeedScale() : 0.0f);
        ImGui::Text("加速倍率: %.3f", hasRail ? railRig_->GetBoostRailSpeedMultiplier() : 0.0f);
        ImGui::Text("実効レール速度: %.3f", hasRail ? railRig_->GetEffectiveRailSpeed() : 0.0f);
        ImGui::Text("レール距離: %.3f", hasRail ? railRig_->GetRailDistance() : 0.0f);
        ImGui::Text("レール距離増分: %.6f", hasRail ? railRig_->GetLastRailAdvance() : 0.0f);
        ImGui::Text("レール停止成功: %s", BoolText(railStopSucceeded_));
        ImGui::Text("レール復帰成功: %s", BoolText(railResumeSucceeded_));
        ImGui::Text("エラー時復帰成功: %s", BoolText(errorRailResumeSucceeded_));
        ImGui::Text("基本速度変更: なし");
        ImGui::Text("加速設定変更: なし");
    }

    if (ImGui::CollapsingHeader(
            "ウェーブ4画面構図##Wave4CameraFraming",
            ImGuiTreeNodeFlags_DefaultOpen)) {
        constexpr float kRadiansToDegrees =
            180.0f / std::numbers::pi_v<float>;
        const float currentFov = camera_ ? camera_->GetFovY() : 0.0f;
        const Vector3 currentViewOffset = camera_
            ? camera_->GetViewTranslationOffset()
            : Vector3{};
        ImGui::Text("通常画角: %.2f 度",
            baseFovY_ * kRadiansToDegrees);
        ImGui::Text("ウェーブ4画角: %.2f 度",
            wave4FovY_ * kRadiansToDegrees);
        ImGui::Text("現在画角: %.2f 度",
            currentFov * kRadiansToDegrees);
        ImGui::Text("画角追加量: %.2f 度",
            wave4FovAdditionDegrees_);
        ImGui::Text("世界上方向の描画偏移: %.2f",
            wave4VerticalViewOffset_);
        ImGui::Text("カメラ後方の描画偏移: %.2f",
            wave4BackwardViewOffset_);
        ImGui::Text("通常時の描画偏移: (%.2f, %.2f, %.2f)",
            baseViewTranslationOffset_.x,
            baseViewTranslationOffset_.y,
            baseViewTranslationOffset_.z);
        ImGui::Text("ウェーブ4の描画偏移: (%.2f, %.2f, %.2f)",
            wave4ViewTranslationOffset_.x,
            wave4ViewTranslationOffset_.y,
            wave4ViewTranslationOffset_.z);
        ImGui::Text("現在の描画偏移: (%.2f, %.2f, %.2f)",
            currentViewOffset.x, currentViewOffset.y,
            currentViewOffset.z);
        ImGui::Text("物理配置と描画構図の分離: はい");
        ImGui::Text("カメラ回転を維持: %s",
            BoolText(framingCameraRotationUnchanged_));
        ImGui::Text("構図開始時前方との内積: %.6f",
            framingCameraForwardDot_);
        ImGui::Text("画角上書き中: %s",
            BoolText(fovOverrideActive_));
        ImGui::Text("画角遷移中: %s", BoolText(fovBlendActive_));
        ImGui::Text("遷移進行率: %.3f", fovBlendProgress_);
        ImGui::Text("遷移時間: %.2f 秒", fovBlendDuration_);
        ImGui::Text("通常画角復帰確認: %s",
            BoolText(fovRestoreVerified_));
        ImGui::Text("通常描画偏移復帰確認: %s",
            BoolText(viewTranslationRestoreVerified_));
        ImGui::SeparatorText("画面内確認");
        ImGui::Text("見えている弱点数: %zu", visibleWeakPointCount_);
        ImGui::Text("可視触手数（先端／弱点／上側中点）: %zu",
            visibleTentacleCount_);
        ImGui::Text("可視条件を満たす: %s",
            BoolText(visibleTentacleCount_ >= 3));
        for (std::size_t chainIndex = 0;
            chainIndex < visibleTentacleChains_.size(); ++chainIndex) {
            ImGui::Text("チェーン%zu可視: %s", chainIndex,
                BoolText(visibleTentacleChains_[chainIndex]));
        }
        ImGui::Text("画面境界有効: %s",
            BoolText(framingScreenBoundsValid_));
        ImGui::Text("画面境界最小: (%.1f, %.1f)",
            framingScreenMinimum_.x, framingScreenMinimum_.y);
        ImGui::Text("画面境界最大: (%.1f, %.1f)",
            framingScreenMaximum_.x, framingScreenMaximum_.y);
        ImGui::Text("画面高占有率: %.1f%%",
            framingScreenHeightOccupancy_ * 100.0f);
        ImGui::Text("根元切断面が画面下: %s",
            BoolText(framingRootSideHidden_));
        ImGui::Text("近接面警告: %s",
            BoolText(framingNearPlaneWarning_));
    }

    if (ImGui::CollapsingHeader("自動攻撃管理##AttackScheduler")) {
        ImGui::Text("自動攻撃有効: %s", BoolText(schedulerEnabled_));
        ImGui::Text("最初の攻撃待機: %.2f 秒", firstAttackDelay_);
        ImGui::Text("次の攻撃間隔: %.2f 秒", nextAttackInterval_);
        ImGui::Text("再試行待機: %.2f 秒", retryDelay_);
        ImGui::Text("攻撃待機時間: %.3f 秒", attackTimer_);
        ImGui::Text("攻撃可能: %s", BoolText(canAttack));
        ImGui::Text("現在攻撃チェーン: %zu", hasKraken ? kraken_->GetSelectedAttackChain() : 0);
        ImGui::Text("次の攻撃チェーン: %zu", nextAttackChain_);
        ImGui::Text("検出チェーン数: %zu", detectedChainCount_);
        ImGui::Text("攻撃開始回数: %llu", static_cast<unsigned long long>(attackStartCount_));
        ImGui::Text("攻撃開始拒否回数: %llu", static_cast<unsigned long long>(attackRejectedCount_));
        ImGui::Text("撃破後攻撃抑制数: %llu", static_cast<unsigned long long>(defeatAttackSuppressionCount_));
        ImGui::Text("攻撃順: 0 → 1 → 2 → 3 → 0");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("中ボスが待機へ戻った後、指定時間を待って次のチェーンで攻撃します。");
        }
    }

    if (ImGui::CollapsingHeader("配置と完了##PlacementCompletion")) {
        ImGui::Text("前方偏移: %.3f", hasKraken ? kraken_->GetCameraForwardOffset() : 0.0f);
        ImGui::Text("右偏移: %.3f", hasKraken ? kraken_->GetCameraRightOffset() : 0.0f);
        ImGui::Text("上偏移: %.3f", hasKraken ? kraken_->GetCameraUpOffset() : 0.0f);
        ImGui::Text("出現世界座標: (%.2f, %.2f, %.2f)", spawnWorldPosition_.x, spawnWorldPosition_.y, spawnWorldPosition_.z);
        ImGui::Text("出現時カメラ座標: (%.2f, %.2f, %.2f)", spawnCameraPosition_.x, spawnCameraPosition_.y, spawnCameraPosition_.z);
        ImGui::Text("出現時カメラ前方: (%.3f, %.3f, %.3f)", spawnCameraForward_.x, spawnCameraForward_.y, spawnCameraForward_.z);
        ImGui::Text("カメラ継続追従: いいえ");
        ImGui::Separator();
        ImGui::Text("完了通知済み: %s", BoolText(completionPublished_));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("中ボスの撃破落下完了を1回だけ検出し、レール復帰とウェーブ4目標完了を行った状態です。");
        }
        ImGui::Text("通知済み撃破識別子: %llu", static_cast<unsigned long long>(publishedDefeatSequenceId_));
        ImGui::Text("ウェーブ5待機時間: %.3f 秒", waitingForWave5Timer_);
        ImGui::Text("ウェーブ5移行制限時間: %.3f 秒", wave5TransitionTimeout_);
        ImGui::Text("レール復帰済み: %s", BoolText(hasRail && railRig_->GetExternalEncounterRailSpeedMultiplier() == 1.0f));
        ImGui::Text("ダメージ無効済み: %s", BoolText(hasKraken && !kraken_->IsAttackDamageEnabled() && !kraken_->IsProjectileDamageEnabled()));
        ImGui::Text("中ボス非表示済み: %s", BoolText(hasKraken && !kraken_->IsVisible()));
    }

    if (ImGui::CollapsingHeader(
            "ウェーブ4再入場診断##Wave4Reentry",
            ImGuiTreeNodeFlags_DefaultOpen)) {
        const std::uint64_t currentRevision = hasManager
            ? waveManager_->GetCurrentWaveRevision()
            : 0;
        const char* objectiveResyncResult =
            !objectiveIncompleteResyncAttempted_
                ? "未実行"
                : (lastObjectiveIncompleteResyncSucceeded_
                    ? "成功"
                    : "失敗");
        const char* rearmResult = !lastRearmAttempted_
            ? "未実行"
            : (lastRearmSucceeded_ ? "成功" : "失敗");

        ImGui::Text("再入場対応有効: はい");
        ImGui::Text("現在ウェーブID: %s", currentWaveId.c_str());
        ImGui::Text("現在ウェーブ改訂番号: %llu",
            static_cast<unsigned long long>(currentRevision));
        ImGui::Text("処理済みウェーブID: %s",
            handledWaveId_.empty() ? "なし" : handledWaveId_.c_str());
        ImGui::Text("処理済みウェーブ改訂番号: %llu",
            static_cast<unsigned long long>(handledWaveRevision_));
        ImGui::Text("中ボス戦状態: %s", EncounterStateText(state_));
        ImGui::Text("新しい改訂番号のウェーブ4: %s",
            BoolText(IsNewRevisionWave4()));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "同じwave_004でも、ウェーブ改訂番号が前回と異なる新しい開始であることを示します。");
        }
        ImGui::Text("再準備が必要: %s", BoolText(IsRearmRequired()));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "ウェーブ5到達後に、新しい改訂番号のウェーブ4へ戻った時、中ボス戦を再び開始できる状態へ戻します。");
        }
        ImGui::Text("外部完了条件設定済み: %s", BoolText(hasManager &&
            waveManager_->IsExternalWaveObjectiveConfigured()));
        ImGui::Text("外部完了条件達成: %s", BoolText(hasManager &&
            waveManager_->IsExternalWaveObjectiveCompleted()));
        ImGui::Text("外部完了条件の未完了再同期: %s",
            objectiveResyncResult);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "空のウェーブ4が中ボス出現前に完了しないよう、敵ウェーブ管理の更新前に外部完了条件を未完了へ戻します。");
        }
        ImGui::Text("レール倍率: %.3f", hasRail
            ? railRig_->GetExternalEncounterRailSpeedMultiplier()
            : 0.0f);
        ImGui::Text("攻撃ダメージ: %s", BoolText(hasKraken &&
            kraken_->IsAttackDamageEnabled()));
        ImGui::Text("弾ダメージ: %s", BoolText(hasKraken &&
            kraken_->IsProjectileDamageEnabled()));
        ImGui::Text("自動攻撃: %s", BoolText(schedulerEnabled_));
        ImGui::Text("新しい改訂番号の再入場検出数: %llu",
            static_cast<unsigned long long>(
                newRevisionWave4ReentryDetectionCount_));
        ImGui::Text("再準備成功数: %llu",
            static_cast<unsigned long long>(rearmSuccessCount_));
        ImGui::Text("再準備失敗数: %llu",
            static_cast<unsigned long long>(rearmFailureCount_));
        ImGui::Text("同一改訂番号抑制数: %llu",
            static_cast<unsigned long long>(
                sameRevisionReentrySuppressionCount_));
        ImGui::Text("対象外状態拒否数: %llu",
            static_cast<unsigned long long>(
                invalidStateReentryRejectionCount_));
        ImGui::Text("外部完了条件未完了再同期成功数: %llu",
            static_cast<unsigned long long>(
                objectiveIncompleteResyncSuccessCount_));
        ImGui::Text("外部完了条件未完了再同期失敗数: %llu",
            static_cast<unsigned long long>(
                objectiveIncompleteResyncFailureCount_));
        ImGui::Text("再準備後の開始成功数: %llu",
            static_cast<unsigned long long>(rearmStartingSuccessCount_));
        ImGui::Text("再準備後の開始失敗数: %llu",
            static_cast<unsigned long long>(rearmStartingFailureCount_));
        ImGui::Text("中ボス初期化数: %llu",
            static_cast<unsigned long long>(bossResetCount_));
        ImGui::Text("中ボス表示数: %llu",
            static_cast<unsigned long long>(bossShowCount_));
        ImGui::Text("中ボス配置数: %llu",
            static_cast<unsigned long long>(bossPlacementCount_));
        ImGui::Text("ダメージ有効化数: %llu",
            static_cast<unsigned long long>(damageEnableCount_));
        ImGui::Text("レール再同期数: %llu",
            static_cast<unsigned long long>(railRearmResyncCount_));
        ImGui::Text("レール停止開始数: %llu",
            static_cast<unsigned long long>(railHoldStartCount_));
        ImGui::Separator();
        ImGui::Text("最後の旧ウェーブID: %s",
            lastReentryOldWaveId_.c_str());
        ImGui::Text("最後の新ウェーブID: %s",
            lastReentryNewWaveId_.c_str());
        ImGui::Text("最後の旧改訂番号: %llu",
            static_cast<unsigned long long>(lastReentryOldWaveRevision_));
        ImGui::Text("最後の新改訂番号: %llu",
            static_cast<unsigned long long>(lastReentryNewWaveRevision_));
        ImGui::Text("再入場前の中ボス戦状態: %s",
            EncounterStateText(lastReentryStateBefore_));
        ImGui::Text("再入場後の中ボス戦状態: %s",
            EncounterStateText(lastReentryStateAfter_));
        ImGui::Text("最後の再準備結果: %s", rearmResult);
        ImGui::TextWrapped("最後の再準備失敗理由: %s",
            lastRearmFailureReason_.c_str());
    }

    if (ImGui::CollapsingHeader("操作##Controls")) {
        if (ImGui::Button("中ボス戦状態を再取得##Refresh")) {
            pendingDebugCommand_ = PendingDebugCommand::Refresh;
        }
        ImGui::SameLine();
        const bool canForceStart = state_ == KrakenTentacleWaveEncounterState::WaitingForWave4 && IsTargetWaveCurrent();
        ImGui::BeginDisabled(!canForceStart);
        if (ImGui::Button("ウェーブ4交戦を強制開始##ForceStart")) {
            pendingDebugCommand_ = PendingDebugCommand::ForceStart;
        }
        ImGui::EndDisabled();
        if (ImGui::Button("自動攻撃を有効化##SchedulerOn") && state_ == KrakenTentacleWaveEncounterState::Active) {
            pendingDebugCommand_ = PendingDebugCommand::SchedulerOn;
        }
        ImGui::SameLine();
        if (ImGui::Button("自動攻撃を無効化##SchedulerOff")) {
            pendingDebugCommand_ = PendingDebugCommand::SchedulerOff;
        }
        ImGui::BeginDisabled(!canAttack);
        if (ImGui::Button("次の攻撃を即時開始##AttackNow")) {
            pendingDebugCommand_ = PendingDebugCommand::AttackNow;
        }
        ImGui::EndDisabled();
        if (ImGui::Button("レール停止を再同期##RailResync")) {
            pendingDebugCommand_ = PendingDebugCommand::RailResync;
        }
        ImGui::SameLine();
        if (ImGui::Button("外部完了条件を再同期##ObjectiveResync") && hasManager) {
            pendingDebugCommand_ = PendingDebugCommand::ObjectiveResync;
        }
        if (ImGui::Button("中ボス戦状態を初期化##Reset")) {
            pendingDebugCommand_ = PendingDebugCommand::Reset;
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(state_ != KrakenTentacleWaveEncounterState::Error);
        if (ImGui::Button("エラーを解除##ClearError")) {
            pendingDebugCommand_ = PendingDebugCommand::ClearError;
        }
        ImGui::EndDisabled();
        if (ImGui::Button("ウェーブ4完了を模擬##SimulateCompletion")) {
            pendingDebugCommand_ = PendingDebugCommand::SimulateCompletion;
        }
    }

    if (ImGui::CollapsingHeader("安全診断##Safety")) {
        ImGui::Text("ウェーブ管理未接続数: %llu", static_cast<unsigned long long>(managerMissingCount_));
        ImGui::Text("中ボス未接続数: %llu", static_cast<unsigned long long>(krakenMissingCount_));
        ImGui::Text("カメラ未接続数: %llu", static_cast<unsigned long long>(cameraMissingCount_));
        ImGui::Text("レール未接続数: %llu", static_cast<unsigned long long>(railMissingCount_));
        ImGui::Text("外部完了条件不正数: %llu", static_cast<unsigned long long>(invalidObjectiveCount_));
        ImGui::Text("改訂番号不正数: %llu", static_cast<unsigned long long>(invalidRevisionCount_));
        ImGui::Text("チェーン0数: %llu", static_cast<unsigned long long>(zeroChainCount_));
        ImGui::Text("出現失敗数: %llu", static_cast<unsigned long long>(spawnFailureCount_));
        ImGui::Text("レール停止失敗数: %llu", static_cast<unsigned long long>(railHoldFailureCount_));
        ImGui::Text("レール復帰失敗数: %llu", static_cast<unsigned long long>(railResumeFailureCount_));
        ImGui::Text("ダメージ設定失敗数: %llu", static_cast<unsigned long long>(damageSetupFailureCount_));
        ImGui::Text("外部完了条件の完了失敗数: %llu", static_cast<unsigned long long>(objectiveCompletionFailureCount_));
        ImGui::Text("ウェーブ移行超過数: %llu", static_cast<unsigned long long>(waveTransitionTimeoutCount_));
        ImGui::Text("交戦二重開始防止数: %llu", static_cast<unsigned long long>(duplicateStartSuppressionCount_));
        ImGui::Text("中ボス二重出現防止数: %llu", static_cast<unsigned long long>(duplicateSpawnSuppressionCount_));
        ImGui::Text("完了二重通知防止数: %llu", static_cast<unsigned long long>(duplicateCompletionSuppressionCount_));
        ImGui::Text("予期しないウェーブ変更数: %llu", static_cast<unsigned long long>(unexpectedWaveChangeCount_));
        ImGui::TextWrapped("最後のエラー: %s", lastError_.c_str());
        ImGui::TextWrapped("最後の警告: %s", lastWarning_.c_str());
    }

    ImGui::End();
#endif
}
