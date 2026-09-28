#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
#ifdef USE_IMGUI
    constexpr std::size_t kMaximumDisplayedEvents = 24;

    const char* BoolLabel(bool value) {
        return value ? "はい" : "いいえ";
    }

    void DrawText(const std::string& value) {
        ImGui::TextUnformatted(value.c_str());
    }

    std::string FormatFloat(float value) {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(3) << value;
        return stream.str();
    }

    const char* GetRoleLabel(KrakenColliderPreviewRole role) {
        switch (role) {
        case KrakenColliderPreviewRole::Attack:
            return "攻撃";
        case KrakenColliderPreviewRole::Damage:
            return "胴体";
        case KrakenColliderPreviewRole::WeakPoint:
            return "弱点";
        default:
            return "不明";
        }
    }

    const char* GetTargetLabel(KrakenTentacleCollisionTargetKind target) {
        switch (target) {
        case KrakenTentacleCollisionTargetKind::Player:
            return "プレイヤー";
        case KrakenTentacleCollisionTargetKind::PlayerBullet:
            return "プレイヤー弾";
        default:
            return "不明";
        }
    }

    const char* GetTransitionLabel(
        KrakenTentacleCollisionTransition transition) {
        switch (transition) {
        case KrakenTentacleCollisionTransition::Enter:
            return "開始";
        case KrakenTentacleCollisionTransition::Stay:
            return "継続";
        case KrakenTentacleCollisionTransition::Exit:
            return "終了";
        default:
            return "不明";
        }
    }

    const char* GetProjectileTypeLabel(
        KrakenTentacleCollisionProjectileType type) {
        switch (type) {
        case KrakenTentacleCollisionProjectileType::NormalShot:
            return "通常弾";
        case KrakenTentacleCollisionProjectileType::LockedWingShot:
            return "ロックドウィング弾";
        case KrakenTentacleCollisionProjectileType::None:
        default:
            return "該当なし";
        }
    }

    std::uint32_t GetStableColliderId(
        const KrakenTentacleCollisionPairKey& key) {
        return key.chainIndex * 5 + key.colliderIndex + 1;
    }

    void ClearRoleCollisionHistory(
        KrakenTentacleCollisionRoleDiagnostics& diagnostics) {
        diagnostics.totalIntersectionCount = 0;
        diagnostics.cumulativeIntersectionFrameCount = 0;
        diagnostics.totalEnterCount = 0;
        diagnostics.totalStayCount = 0;
        diagnostics.totalExitCount = 0;
        diagnostics.lastTargetRuntimeId = 0;
        diagnostics.lastIntersectionFrameIndex = 0;
        diagnostics.lastColliderId = 0;
        diagnostics.lastChainIndex = 0;
        diagnostics.lastIntersectionRuntimeTime = 0.0f;
        diagnostics.lastPenetrationDepth = 0.0f;
        diagnostics.lastProjectileType =
            KrakenTentacleCollisionProjectileType::None;
        diagnostics.hasLastIntersection = false;
    }

    void DrawRoleDiagnostics(
        const char* title,
        const char* targetIdLabel,
        const KrakenTentacleCollisionRoleDiagnostics& diagnostics) {
        if (!ImGui::TreeNode(title)) {
            return;
        }
        DrawText(
            std::string("現在 / 今フレーム交差数: ") +
            std::to_string(diagnostics.currentIntersectionCount) + " / " +
            std::to_string(diagnostics.frameIntersectionCount));
        DrawText(
            std::string("今フレーム 開始 / 継続 / 終了: ") +
            std::to_string(diagnostics.frameEnterCount) + " / " +
            std::to_string(diagnostics.frameStayCount) + " / " +
            std::to_string(diagnostics.frameExitCount));
        DrawText(
            std::string("累積 開始 / 継続 / 終了: ") +
            std::to_string(diagnostics.totalEnterCount) + " / " +
            std::to_string(diagnostics.totalStayCount) + " / " +
            std::to_string(diagnostics.totalExitCount));
        DrawText(
            std::string("累積交差数 / 交差フレーム数: ") +
            std::to_string(diagnostics.totalIntersectionCount) + " / " +
            std::to_string(diagnostics.cumulativeIntersectionFrameCount));
        if (diagnostics.hasLastIntersection) {
            DrawText(
                std::string("最後のコライダー / チェーン: ") +
                std::to_string(diagnostics.lastColliderId) + " / " +
                std::to_string(diagnostics.lastChainIndex));
            DrawText(
                std::string(targetIdLabel) + ": " +
                std::to_string(diagnostics.lastTargetRuntimeId));
            DrawText(
                std::string("最後の交差フレーム / 稼働時刻: ") +
                std::to_string(diagnostics.lastIntersectionFrameIndex) +
                " / " +
                FormatFloat(diagnostics.lastIntersectionRuntimeTime) +
                " 秒");
            DrawText(
                std::string("めり込み深度: ") +
                FormatFloat(diagnostics.lastPenetrationDepth));
            if (diagnostics.lastProjectileType !=
                KrakenTentacleCollisionProjectileType::None) {
                DrawText(
                    std::string("弾種: ") +
                    GetProjectileTypeLabel(
                        diagnostics.lastProjectileType));
            }
        } else {
            ImGui::TextDisabled("交差履歴はありません。");
        }
        ImGui::TreePop();
    }
#endif
}

void KrakenTentacleMidbossController::Impl::DrawCollisionQueryImGui() {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader(
            "非破壊衝突照会診断##CollisionQueryDiagnostics",
            ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    bool enabled = collisionQueryEnabled;
    if (ImGui::Checkbox(
            "交差診断を有効化##EnableCollisionQuery", &enabled)) {
        collisionQueryEnabled = enabled;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "読み取り専用です。ダメージ、弾消滅、演出は発生しません。");
    }
    ImGui::SameLine();
    if (ImGui::Button("履歴を消去##ClearCollisionQueryHistory")) {
        diagnostics.collisionQueryFrameCount = 0;
        diagnostics.totalCollisionQueryTestCount = 0;
        diagnostics.totalCollisionEnterCount = 0;
        diagnostics.totalCollisionStayCount = 0;
        diagnostics.totalCollisionExitCount = 0;
        diagnostics.totalBodyAndWeakPointSameBulletCount = 0;
        diagnostics.duplicateCollisionQueryCount = 0;
        ClearRoleCollisionHistory(diagnostics.attackPlayerCollision);
        ClearRoleCollisionHistory(diagnostics.damageBulletCollision);
        ClearRoleCollisionHistory(diagnostics.weakPointBulletCollision);
    }
    ImGui::SeparatorText("プレイヤー形状");
    DrawText(std::string("有効: ") +
        BoolLabel(diagnostics.playerCollisionSnapshotValid));
    DrawText(std::string("生存: ") + BoolLabel(diagnostics.playerAlive));
    DrawText(std::string("衝突有効: ") +
        BoolLabel(diagnostics.playerCollisionEnabled));
    ImGui::Text("中心: %.3f, %.3f, %.3f",
        diagnostics.playerCollisionCenter.x,
        diagnostics.playerCollisionCenter.y,
        diagnostics.playerCollisionCenter.z);
    ImGui::Text("半径: %.3f", diagnostics.playerCollisionRadius);
    ImGui::TextUnformatted("形状: 球");

    ImGui::SeparatorText("プレイヤー弾形状");
    DrawText(std::string("稼働数 / 有効数 / 無効数: ") +
        std::to_string(diagnostics.playerBulletCollisionSnapshotCount) +
        " / " +
        std::to_string(diagnostics.playerBulletSnapshotValidCount) +
        " / " +
        std::to_string(diagnostics.playerBulletSnapshotInvalidCount));
    DrawText(std::string("通常弾 / ロックドウィング弾: ") +
        std::to_string(diagnostics.normalShotSnapshotCount) + " / " +
        std::to_string(diagnostics.lockedWingShotSnapshotCount));
    DrawText(std::string("識別子重複数: ") +
        std::to_string(diagnostics.stableRuntimeIdDuplicateCount));
    DrawText(std::string("最後の実行時識別子: ") +
        std::to_string(diagnostics.lastPlayerBulletRuntimeId));
    DrawText(std::string("最大同時形状数: ") +
        std::to_string(
            diagnostics.maximumConcurrentBulletSnapshotCount));
    DrawText(std::string("収集前後の状態一致: ") +
        BoolLabel(diagnostics.bulletSnapshotUnchanged) +
        "（不一致回数: " +
        std::to_string(diagnostics.bulletSnapshotMutationCount) + "）");

    ImGui::SeparatorText("交差診断");
    DrawText(std::string("診断参照の接続: ") +
        BoolLabel(diagnostics.collisionQueryContextConnected));
    DrawText(std::string("対象コライダー数: ") +
        std::to_string(diagnostics.collisionQueryTargetCount));
    DrawText(std::string("対象内訳 攻撃 / 胴体 / 弱点: ") +
        std::to_string(diagnostics.queryTargetAttackColliderCount) +
        " / " +
        std::to_string(diagnostics.queryTargetDamageColliderCount) +
        " / " +
        std::to_string(diagnostics.queryTargetWeakPointCount));
    DrawText(std::string("候補ペア数 / 実行判定数 / 無効数: ") +
        std::to_string(diagnostics.collisionQueryCandidatePairCount) +
        " / " + std::to_string(diagnostics.collisionQueryTestCount) +
        " / " + std::to_string(diagnostics.invalidCollisionQueryCount));
    DrawText(
        std::string("現在交差 攻撃→プレイヤー / 胴体→弾 / 弱点→弾: ") +
        std::to_string(diagnostics.currentAttackPlayerPairCount) +
        " / " +
        std::to_string(diagnostics.currentDamageBulletPairCount) +
        " / " +
        std::to_string(diagnostics.currentWeakPointBulletPairCount));
    DrawText(
        std::string("現在交差ペア総数: ") +
        std::to_string(diagnostics.currentCollisionPairCount));
    DrawText(
        std::string("今フレーム 開始 / 継続 / 終了: ") +
        std::to_string(diagnostics.collisionEnterCount) + " / " +
        std::to_string(diagnostics.collisionStayCount) + " / " +
        std::to_string(diagnostics.collisionExitCount));
    DrawText(
        std::string("累積 開始 / 継続 / 終了: ") +
        std::to_string(diagnostics.totalCollisionEnterCount) + " / " +
        std::to_string(diagnostics.totalCollisionStayCount) + " / " +
        std::to_string(diagnostics.totalCollisionExitCount));
    DrawText(
        std::string("同一弾の胴体・弱点同時交差 今回 / 累積: ") +
        std::to_string(diagnostics.bodyAndWeakPointSameBulletCount) +
        " / " +
        std::to_string(
            diagnostics.totalBodyAndWeakPointSameBulletCount));
    DrawText(
        std::string("診断更新フレーム数 / 二重呼出し検出: ") +
        std::to_string(diagnostics.collisionQueryFrameCount) + " / " +
        std::to_string(diagnostics.duplicateCollisionQueryCount));

    ImGui::SeparatorText("副作用監視");
    DrawText(std::string("プレイヤー耐久値変更回数: ") +
        std::to_string(diagnostics.playerHpChangeRequestCount));
    DrawText(std::string("プレイヤー無敵開始回数: ") +
        std::to_string(diagnostics.playerInvincibilityRequestCount));
    DrawText(std::string("弾消滅要求回数: ") +
        std::to_string(diagnostics.bulletKillRequestCount));
    DrawText(std::string("弾の寿命変更回数: ") +
        std::to_string(diagnostics.bulletLifetimeChangeRequestCount));
    DrawText(std::string("中ボス耐久値変更回数: ") +
        std::to_string(diagnostics.midbossHpChangeRequestCount));
    DrawText(std::string("ゲームプレイ登録: ") +
        BoolLabel(diagnostics.gameplayRegistrationObserved));

    ImGui::SeparatorText("安全性");
    DrawText(std::string("実行時識別子重複数: ") +
        std::to_string(diagnostics.stableRuntimeIdDuplicateCount));
    DrawText(std::string("同一ペア二重追加数: ") +
        std::to_string(diagnostics.duplicateCollisionPairCount));
    DrawText(std::string("古いペア残存数: ") +
        std::to_string(diagnostics.staleCollisionPairCount));
    DrawText(std::string("非有限コライダー数 / 非有限球数: ") +
        std::to_string(diagnostics.nonFiniteColliderCount) + " / " +
        std::to_string(diagnostics.nonFiniteSphereCount));
    DrawText(std::string("ゼロ長カプセル数: ") +
        std::to_string(diagnostics.zeroLengthColliderCount));
    DrawText(std::string("不正プレイヤー形状数 / 不正弾形状数: ") +
        std::to_string(diagnostics.invalidPlayerSnapshotCount) + " / " +
        std::to_string(diagnostics.invalidBulletSnapshotCount));
    if (!lastCollisionQueryWarning.empty()) {
        DrawText(
            std::string("診断警告: ") + lastCollisionQueryWarning);
    }
    DrawRoleDiagnostics(
        "攻撃→プレイヤー詳細##AttackPlayerCollisionDetails",
        "最後のプレイヤー識別子",
        diagnostics.attackPlayerCollision);
    DrawRoleDiagnostics(
        "胴体→プレイヤー弾詳細##DamageBulletCollisionDetails",
        "最後のプレイヤー弾識別子",
        diagnostics.damageBulletCollision);
    DrawRoleDiagnostics(
        "弱点→プレイヤー弾詳細##WeakPointBulletCollisionDetails",
        "最後のプレイヤー弾識別子",
        diagnostics.weakPointBulletCollision);
    ImGui::Separator();
    ImGui::TextUnformatted("今フレームの交差遷移:");
    const std::size_t displayCount = (std::min)(
        collisionFrameEvents.size(), kMaximumDisplayedEvents);
    if (displayCount == 0) {
        ImGui::TextDisabled("交差遷移はありません。");
    }
    for (std::size_t index = 0; index < displayCount; ++index) {
        const KrakenTentacleCollisionEventSnapshot& event =
            collisionFrameEvents[index];
        const std::string eventText =
            std::string("・") + GetTransitionLabel(event.transition) +
            ": " + GetRoleLabel(event.pair.key.role) + "→" +
            GetTargetLabel(event.pair.key.targetKind) +
            " / コライダー" +
            std::to_string(GetStableColliderId(event.pair.key)) +
            " / チェーン" + std::to_string(event.pair.key.chainIndex);
        DrawText(
            eventText + " / 対象識別子 " +
            std::to_string(event.pair.key.targetRuntimeId) +
            " / 距離 " + FormatFloat(event.pair.centerDistance) +
            " / 半径合計 " + FormatFloat(event.pair.radiusSum));
    }
    if (collisionFrameEvents.size() > displayCount) {
        DrawText(
            std::string("ほか ") +
            std::to_string(collisionFrameEvents.size() - displayCount) +
            " 件は表示を省略しました。");
    }
#endif
}
