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
    // 診断状態
    DrawText(
        std::string("診断参照の接続: ") +
        BoolLabel(diagnostics.collisionQueryContextConnected));
    DrawText(
        std::string("プレイヤー形状取得数: ") +
        std::to_string(diagnostics.playerCollisionSnapshotCount) +
        "（有効: " +
        BoolLabel(diagnostics.playerCollisionSnapshotValid) + "）");
    DrawText(
        std::string("プレイヤー弾形状取得数: ") +
        std::to_string(diagnostics.playerBulletCollisionSnapshotCount));
    DrawText(
        std::string("登録要求 / 成功 / 失敗: ") +
        std::to_string(diagnostics.collisionRegistrationRequestedCount) +
        " / " + std::to_string(diagnostics.gameplayRegisteredCount) +
        " / " +
        std::to_string(diagnostics.collisionRegistrationFailureCount));
    DrawText(
        std::string("登録内訳 攻撃 / 胴体 / 弱点: ") +
        std::to_string(diagnostics.registeredAttackColliderCount) +
        " / " +
        std::to_string(diagnostics.registeredDamageColliderCount) +
        " / " + std::to_string(diagnostics.registeredWeakPointCount));
    std::uint64_t latestRegisteredFrame = 0;
    for (const auto& collider : capsuleSnapshots) {
        latestRegisteredFrame = (std::max)(
            latestRegisteredFrame, collider.lastRegisteredFrame);
    }
    for (const auto& collider : tipSnapshots) {
        latestRegisteredFrame = (std::max)(
            latestRegisteredFrame, collider.lastRegisteredFrame);
    }
    DrawText(
        std::string("登録世代 / 最終登録フレーム: ") +
        std::to_string(collisionRegistrationGeneration) + " / " +
        std::to_string(latestRegisteredFrame));
    if (diagnostics.collisionRegistrationFailureCount > 0 &&
        ImGui::TreeNode("登録失敗詳細##CollisionRegistrationFailures")) {
        for (const auto& collider : capsuleSnapshots) {
            if (collider.registrationFailed) {
                DrawText(
                    std::string("コライダー") +
                    std::to_string(collider.colliderId) + " / " +
                    GetRoleLabel(collider.role));
            }
        }
        for (const auto& collider : tipSnapshots) {
            if (collider.registrationFailed) {
                DrawText(
                    std::string("コライダー") +
                    std::to_string(collider.colliderId) + " / 弱点");
            }
        }
        ImGui::TreePop();
    }
    DrawText(
        std::string("今フレームの判定数 / 無効数: ") +
        std::to_string(diagnostics.collisionQueryTestCount) + " / " +
        std::to_string(diagnostics.invalidCollisionQueryCount));
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
