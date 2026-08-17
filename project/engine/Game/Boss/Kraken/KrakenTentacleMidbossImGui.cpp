#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"

#include "Engine/Animation/Skeleton.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
#ifdef USE_IMGUI
    const char* GetStateLabel(KrakenTentacleMidbossState state) {
        switch (state) {
        case KrakenTentacleMidbossState::Hidden:
            return "非表示";
        case KrakenTentacleMidbossState::Idle:
            return "待機";
        case KrakenTentacleMidbossState::Windup:
            return "振りかぶり";
        case KrakenTentacleMidbossState::WindupHold:
            return "振りかぶり保持";
        case KrakenTentacleMidbossState::Slam:
            return "叩きつけ";
        case KrakenTentacleMidbossState::ImpactHold:
            return "衝撃保持";
        case KrakenTentacleMidbossState::Recovery:
            return "復帰";
        default:
            return "不明";
        }
    }

    const char* GetPhaseLabel(KrakenTentacleMidbossState state) {
        switch (state) {
        case KrakenTentacleMidbossState::Windup:
            return "振りかぶり";
        case KrakenTentacleMidbossState::WindupHold:
            return "振りかぶり保持";
        case KrakenTentacleMidbossState::Slam:
            return "叩きつけ";
        case KrakenTentacleMidbossState::ImpactHold:
            return "衝撃保持";
        case KrakenTentacleMidbossState::Recovery:
            return "復帰";
        case KrakenTentacleMidbossState::Hidden:
        case KrakenTentacleMidbossState::Idle:
        default:
            return "なし";
        }
    }

    const char* GetPhaseReasonLabel(KrakenColliderPhaseReason reason) {
        switch (reason) {
        case KrakenColliderPhaseReason::DamageAlwaysActive:
            return "ダメージ判定は常時有効";
        case KrakenColliderPhaseReason::WeakPointAlwaysActive:
            return "弱点判定は常時有効";
        case KrakenColliderPhaseReason::AttackSlamLateActive:
            return "叩きつけ後半で攻撃判定有効";
        case KrakenColliderPhaseReason::AttackImpactHoldActive:
            return "衝撃保持中で攻撃判定有効";
        case KrakenColliderPhaseReason::ColliderDisabled:
            return "コライダー無効";
        case KrakenColliderPhaseReason::ColliderInvalid:
            return "コライダー値不正";
        case KrakenColliderPhaseReason::MotionStateInvalid:
            return "モーション状態不正";
        case KrakenColliderPhaseReason::PreviewDisconnected:
            return "Runtime非表示";
        case KrakenColliderPhaseReason::SafetyRecovery:
            return "安全停止中";
        case KrakenColliderPhaseReason::NotAttackMotionMode:
            return "攻撃モーション外";
        case KrakenColliderPhaseReason::AttackChainOutOfRange:
            return "攻撃チェーン範囲外";
        case KrakenColliderPhaseReason::DifferentAttackChain:
            return "攻撃対象外チェーン";
        case KrakenColliderPhaseReason::WindupInactive:
            return "振りかぶり中は無効";
        case KrakenColliderPhaseReason::WindupHoldInactive:
            return "振りかぶり保持中は無効";
        case KrakenColliderPhaseReason::SlamBeforeThreshold:
            return "叩きつけ有効開始前";
        case KrakenColliderPhaseReason::InvalidSlamDuration:
            return "叩きつけ時間不正";
        case KrakenColliderPhaseReason::ImpactHoldDisabled:
            return "衝撃保持判定無効";
        case KrakenColliderPhaseReason::LoopWaitInactive:
            return "ループ待機中";
        case KrakenColliderPhaseReason::RecoveryInactive:
            return "復帰中は無効";
        case KrakenColliderPhaseReason::CompletedInactive:
            return "攻撃完了後は無効";
        case KrakenColliderPhaseReason::UnknownPhase:
            return "不明なフェーズ";
        case KrakenColliderPhaseReason::UnknownRole:
        default:
            return "不明な役割";
        }
    }

    const char* BoolLabel(bool value) {
        return value ? "はい" : "いいえ";
    }

    void DrawVector3Text(const char* label, const Vector3& value) {
        ImGui::Text(
            "%s: (%.3f, %.3f, %.3f)",
            label, value.x, value.y, value.z);
    }

    void DrawBounds(
        const char* label,
        const KrakenTentacleMidbossBoundsSnapshot& bounds) {
        ImGui::Text("%s: %s", label, bounds.valid ? "有効" : "無効");
        if (bounds.valid) {
            ImGui::Indent();
            DrawVector3Text("最小", bounds.minimum);
            DrawVector3Text("最大", bounds.maximum);
            DrawVector3Text("大きさ", bounds.size);
            DrawVector3Text("中心", bounds.center);
            ImGui::Unindent();
        }
    }
#endif
}

void KrakenTentacleMidbossController::Impl::DrawImGui() {
#ifdef USE_IMGUI
    if (!ImGui::Begin(
            "クラーケン触手中ボスRuntimeデバッグ"
            "###KrakenTentacleMidbossRuntimeDebug")) {
        ImGui::End();
        return;
    }

    if (ImGui::CollapsingHeader(
            "Runtime状態##RuntimeStatus",
            ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("初期化成功: %s", BoolLabel(initialized));
        ImGui::Text("モデル読込成功: %s", BoolLabel(modelLoaded));
        ImGui::Text("スケルトン有効: %s", BoolLabel(skeletonValid));
        ImGui::Text("現在状態: %s", GetStateLabel(state));
        ImGui::Text("表示中: %s", BoolLabel(IsVisible()));
        ImGui::Text("状態経過時間: %.3f 秒", stateElapsedTime);
        ImGui::Text("全体稼働時間: %.3f 秒", totalActiveTime);
        ImGui::Text(
            "スケール済みデルタ時間: %.4f 秒", lastScaledDeltaTime);
        ImGui::Text("安全停止中: %s", BoolLabel(safetyStopped));
        ImGui::TextWrapped(
            "最後のエラー: %s",
            lastError.empty() ? "なし" : lastError.c_str());
        ImGui::TextWrapped(
            "最後の警告: %s",
            lastWarning.empty() ? "なし" : lastWarning.c_str());
    }

    if (ImGui::CollapsingHeader("モデル情報##ModelInformation")) {
        ImGui::TextWrapped(
            "アセットパス: %s",
            resolvedAssetPath.empty()
                ? requestedAssetPath.c_str()
                : resolvedAssetPath.c_str());
        ImGui::Text("メッシュ数: %zu", diagnostics.meshCount);
        ImGui::Text("プリミティブ数: %zu", diagnostics.primitiveCount);
        ImGui::Text("マテリアル数: %zu", diagnostics.materialCount);
        ImGui::Text("頂点数: %zu", diagnostics.vertexCount);
        ImGui::Text("インデックス数: %zu", diagnostics.indexCount);
        ImGui::Text("三角形数: %zu", diagnostics.triangleCount);
        ImGui::Text("ジョイント数: %zu", diagnostics.jointCount);
        ImGui::Text("パレット数: %zu", diagnostics.paletteCount);
        ImGui::Text("ルート名: %s", rootName.c_str());
        ImGui::Text("チェーン数: %zu", chains.size());
        ImGui::Text("描画呼出し数: %zu", diagnostics.drawCallCount);
        ImGui::Text(
            "マテリアル割当数: %zu",
            diagnostics.materialBindingCount);
        ImGui::Text(
            "CPUスキニング更新数: %zu",
            diagnostics.cpuSkinningUpdateCount);
        ImGui::Text(
            "Computeスキニング実行数: %zu",
            diagnostics.computeDispatchCount);
        ImGui::Separator();
        ImGui::Text("プリミティブ0 → KrakenSkin");
        ImGui::Text("プリミティブ1 → KrakenSucker");
        ImGui::Text("プリミティブ2 → KrakenSuckerInner");
    }

    if (ImGui::CollapsingHeader("配置##Placement")) {
        // 編集値は次回のRuntime Updateで描画・Debug Draw・診断へ反映する。
        ImGui::DragFloat3(
            "ワールド位置##WorldPosition",
            &worldPosition.x, 0.1f, -10000.0f, 10000.0f, "%.3f");
        Vector3 rotationDegrees = {
            worldRotation.x * 180.0f / std::numbers::pi_v<float>,
            worldRotation.y * 180.0f / std::numbers::pi_v<float>,
            worldRotation.z * 180.0f / std::numbers::pi_v<float>,
        };
        if (ImGui::DragFloat3(
                "ワールド回転（度）##WorldRotation",
                &rotationDegrees.x, 0.5f, -360.0f, 360.0f, "%.2f")) {
            worldRotation = {
                rotationDegrees.x * std::numbers::pi_v<float> / 180.0f,
                rotationDegrees.y * std::numbers::pi_v<float> / 180.0f,
                rotationDegrees.z * std::numbers::pi_v<float> / 180.0f,
            };
        }
        ImGui::DragFloat3(
            "ワールド拡縮##WorldScale",
            &worldScale.x, 0.01f, 0.01f, 100.0f, "%.3f");
        ImGui::DragFloat(
            "カメラ前方距離##CameraForwardOffset",
            &cameraForwardOffset, 0.1f, -1000.0f, 1000.0f, "%.2f");
        ImGui::DragFloat(
            "カメラ右方向距離##CameraRightOffset",
            &cameraRightOffset, 0.1f, -1000.0f, 1000.0f, "%.2f");
        ImGui::DragFloat(
            "カメラ上方向距離##CameraUpOffset",
            &cameraUpOffset, 0.1f, -1000.0f, 1000.0f, "%.2f");
        if (ImGui::Button(
                "現在カメラ前方へ配置##PlaceInFrontOfCamera")) {
            PlaceInFrontOfCamera();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "現在位置を一度だけ設定します。カメラへ継続追従しません。");
        }
    }

    if (ImGui::CollapsingHeader("モーション##Motion")) {
        ImGui::Checkbox(
            "待機揺れ有効##IdleSwayEnabled", &idleSwayEnabled);
        ImGui::Text("待機時間: %.3f 秒", idleTime);
        const bool attackPlaying = IsAttackState();
        ImGui::BeginDisabled(attackPlaying || chains.empty());
        char chainPreview[32]{};
        std::snprintf(
            chainPreview, sizeof(chainPreview),
            "チェーン %zu", selectedAttackChainIndex);
        if (ImGui::BeginCombo(
                "攻撃チェーン##AttackChain", chainPreview)) {
            for (std::size_t chainIndex = 0;
                chainIndex < chains.size(); ++chainIndex) {
                char option[32]{};
                std::snprintf(
                    option, sizeof(option), "チェーン %zu", chainIndex);
                if (ImGui::Selectable(
                        option, chainIndex == selectedAttackChainIndex)) {
                    selectedAttackChainIndex = chainIndex;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        if (selectedAttackChainIndex < chains.size()) {
            const KrakenTentacleChain& chain =
                chains[selectedAttackChainIndex];
            const int rootIndex = chain.joints.empty()
                ? -1 : chain.joints.front();
            const int tipIndex = chain.joints.empty()
                ? -1 : chain.joints.back();
            const char* chainRootName =
                skeleton && rootIndex >= 0 &&
                    static_cast<std::size_t>(rootIndex) <
                        skeleton->joints.size()
                ? skeleton->joints[static_cast<std::size_t>(rootIndex)].name.c_str()
                : "取得不可";
            const char* chainTipName =
                skeleton && tipIndex >= 0 &&
                    static_cast<std::size_t>(tipIndex) <
                        skeleton->joints.size()
                ? skeleton->joints[static_cast<std::size_t>(tipIndex)].name.c_str()
                : "取得不可";
            ImGui::Text("チェーン根元: %d / %s", rootIndex, chainRootName);
            ImGui::Text("チェーン先端: %d / %s", tipIndex, chainTipName);
            ImGui::Text("ボーン数: %zu", chain.joints.size());
        }
        ImGui::Text("現在状態: %s", GetStateLabel(state));
        ImGui::Text("現在フェーズ: %s", GetPhaseLabel(state));
        ImGui::Text("状態経過時間: %.3f 秒", stateElapsedTime);
        ImGui::Text("状態時間: %.3f 秒", GetCurrentStateDuration());
        ImGui::Text("叩きつけ進行率: %.3f", GetSlamProgress());
        ImGui::Text("攻撃再生中: %s", BoolLabel(attackPlaying));
        bool selectedAttackActive = false;
        for (const KrakenTentacleMidbossCapsuleSnapshot& collider :
            capsuleSnapshots) {
            if (collider.chainIndex == selectedAttackChainIndex &&
                collider.role == KrakenColliderPreviewRole::Attack) {
                selectedAttackActive = collider.phaseActive;
                break;
            }
        }
        ImGui::Text(
            "攻撃コライダー有効予定: %s",
            BoolLabel(selectedAttackActive));
        if (selectedAttackChainIndex < tipSnapshots.size()) {
            DrawVector3Text(
                "先端ワールド位置",
                tipSnapshots[selectedAttackChainIndex].worldPosition);
        }
    }

    if (ImGui::CollapsingHeader(
            "操作##Operations", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("中ボスを表示##ShowMidboss")) {
            pendingCommand = KrakenTentacleMidbossPendingCommand::Show;
        }
        ImGui::SameLine();
        if (ImGui::Button("中ボスを非表示##HideMidboss")) {
            pendingCommand = KrakenTentacleMidbossPendingCommand::Hide;
        }
        if (ImGui::Button("待機へ戻す##ReturnToIdle")) {
            pendingCommand =
                KrakenTentacleMidbossPendingCommand::ReturnToIdle;
        }
        ImGui::SameLine();
        if (ImGui::Button("攻撃を1回再生##PlayAttackOnce")) {
            pendingCommand =
                KrakenTentacleMidbossPendingCommand::StartAttack;
        }
        if (ImGui::Button("攻撃を停止##StopAttack")) {
            pendingCommand =
                KrakenTentacleMidbossPendingCommand::StopAttack;
        }
        ImGui::SameLine();
        if (ImGui::Button("Bind Poseへ戻す##ReturnToBindPose")) {
            pendingCommand =
                KrakenTentacleMidbossPendingCommand::ReturnToBindPose;
        }
        if (ImGui::Button("状態をリセット##ResetState")) {
            pendingCommand =
                KrakenTentacleMidbossPendingCommand::ResetState;
        }
        ImGui::SameLine();
        if (ImGui::Button("Runtime全体をリセット##ResetRuntime")) {
            pendingCommand =
                KrakenTentacleMidbossPendingCommand::ResetRuntime;
        }
    }

    if (ImGui::CollapsingHeader("デバッグ表示##DebugDisplay")) {
        ImGui::Checkbox("ボーンを表示##ShowBones", &showBones);
        ImGui::Checkbox("ジョイントを表示##ShowJoints", &showJoints);
        ImGui::Checkbox("コライダーを表示##ShowColliders", &showColliders);
        ImGui::Checkbox(
            "攻撃コライダーを表示##ShowAttackColliders",
            &showAttackColliders);
        ImGui::Checkbox(
            "ダメージコライダーを表示##ShowDamageColliders",
            &showDamageColliders);
        ImGui::Checkbox(
            "弱点を表示##ShowWeakPoints", &showWeakPoints);
        if (ImGui::RadioButton(
                "選択チェーンだけ表示##SelectedChainOnly",
                showSelectedChainOnly)) {
            showSelectedChainOnly = true;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton(
                "全チェーン表示##AllChains",
                !showSelectedChainOnly)) {
            showSelectedChainOnly = false;
        }
    }

    if (ImGui::CollapsingHeader("コライダー診断##ColliderDiagnostics")) {
        ImGui::Text("コライダー総数: %zu", diagnostics.colliderCount);
        ImGui::Text("攻撃数: %zu", diagnostics.attackColliderCount);
        ImGui::Text("ダメージ数: %zu", diagnostics.damageColliderCount);
        ImGui::Text("弱点数: %zu", diagnostics.weakPointCount);
        ImGui::Text("フェーズ有効数: %zu", diagnostics.phaseActiveCount);
        ImGui::Text(
            "診断照会登録数: %zu",
            diagnostics.gameplayRegisteredCount);
        ImGui::Text(
            "無効ジョイント数: %zu",
            diagnostics.invalidColliderJointCount);
        ImGui::Text(
            "ゼロ長数: %zu", diagnostics.zeroLengthColliderCount);
        ImGui::Text(
            "非有限数: %zu", diagnostics.nonFiniteColliderCount);
        ImGui::Text(
            "最後のフェーズ理由: %s",
            GetPhaseReasonLabel(diagnostics.lastPhaseReason));
    }

    DrawCollisionQueryImGui();

    if (ImGui::CollapsingHeader("安全診断##SafetyDiagnostics")) {
        ImGui::Text(
            "非有限パレット数: %zu",
            diagnostics.nonFinitePaletteCount);
        ImGui::Text(
            "非有限スキニング頂点数: %zu",
            diagnostics.nonFiniteSkinnedVertexCount);
        ImGui::Text(
            "ウェイトなし頂点数: %zu",
            diagnostics.weightlessVertexCount);
        ImGui::Text(
            "無効ジョイント参照数: %zu",
            diagnostics.invalidJointInfluenceCount);
        ImGui::Text(
            "境界異常: %s", BoolLabel(diagnostics.boundsAbnormal));
        ImGui::Text(
            "安全復帰数: %zu", diagnostics.safetyRecoveryCount);
        ImGui::Text(
            "モデル読込失敗数: %zu",
            diagnostics.modelLoadFailureCount);
        ImGui::Text(
            "攻撃開始拒否数: %zu",
            diagnostics.attackStartRejectedCount);
        ImGui::Text(
            "範囲外チェーン数: %zu",
            diagnostics.outOfRangeChainCount);
        DrawBounds("元モデル境界", diagnostics.sourceBounds);
        DrawBounds("スキニング後境界", diagnostics.skinnedBounds);
    }
    ImGui::End();
#endif
}
