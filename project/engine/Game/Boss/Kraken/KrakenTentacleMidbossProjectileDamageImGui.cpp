#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"

#include <algorithm>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
#ifdef USE_IMGUI
const char* BoolLabel(bool value) {
    return value ? "はい" : "いいえ";
}

const char* ProjectileTypeLabel(KrakenProjectileSnapshotType type) {
    switch (type) {
    case KrakenProjectileSnapshotType::NormalShot:
        return "通常弾";
    case KrakenProjectileSnapshotType::LockedWingShot:
        return "ロック翼弾";
    case KrakenProjectileSnapshotType::Unknown:
    default:
        return "不明";
    }
}

const char* HitRoleLabel(KrakenProjectileHitRole role) {
    return role == KrakenProjectileHitRole::WeakPoint ? "弱点" : "本体";
}

void DrawHelp(const char* text) {
    ImGui::TextDisabled("（説明）");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", text);
    }
}
#endif
}

void KrakenTentacleMidbossController::Impl::
DrawProjectileDamageImGui() {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader(
            "中ボスHP・プレイヤー弾ダメージ##ProjectileDamage",
            ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::SeparatorText("体力");
    bool damageEnabled = projectileDamageEnabled;
    if (ImGui::Checkbox(
            "プレイヤー弾ダメージを有効化##EnableProjectileDamage",
            &damageEnabled)) {
        projectileDamageEnabled = damageEnabled &&
            health.IsValid() && !health.IsDefeatPending() &&
            !IsDefeatState();
        if (damageEnabled && !projectileDamageEnabled) {
            projectileDamageDiagnostics.lastWarning =
                "撃破待ちまたはHP不正のため有効化できません。";
        }
    }
    float maxHp = health.GetMaxHp();
    if (ImGui::DragFloat(
            "最大HP##MaxHp", &maxHp, 1.0f, 1.0f, 9999.0f, "%.1f")) {
        if (!health.SetMaxHp(maxHp, false)) {
            ++projectileDamageDiagnostics.nonFiniteHpCount;
        }
    }
    ImGui::Text("現在HP: %.1f", health.GetCurrentHp());
    ImGui::Text("HP割合: %.1f%%", health.GetHpRatio() * 100.0f);
    float multiplier = health.GetWeakPointMultiplier();
    if (ImGui::DragFloat(
            "弱点倍率##WeakPointMultiplier",
            &multiplier, 0.05f, 1.0f, 10.0f, "%.2f")) {
        if (!health.SetWeakPointMultiplier(multiplier)) {
            ++projectileDamageDiagnostics.nonFiniteMultiplierCount;
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "触手先端の弱点へ命中した時に、\n"
            "プレイヤー弾のダメージへ掛ける倍率です。");
    }
    ImGui::Text("撃破待ち: %s", BoolLabel(health.IsDefeatPending()));
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "HPが0になると現在姿勢を固定し、\n"
            "下方へ退避して撃破完了を保持します。");
    }
    ImGui::Text("撃破演出実装済み: はい");
    ImGui::Text("ウェーブ未接続: はい");
    ImGui::Text("HP初期値の根拠: 既存基準なしの確認用暫定値");
    ImGui::Text("暫定HP: %.1f", KrakenTentacleMidbossHealth::kProvisionalMaxHp);

    ImGui::SeparatorText("投射物スナップショット");
    ImGui::Text(
        "有効なプレイヤー弾数: %zu",
        diagnostics.playerBulletCollisionSnapshotCount);
    ImGui::Text(
        "有効スナップショット数: %zu",
        diagnostics.playerBulletSnapshotValidCount);
    ImGui::Text("通常弾数: %zu", diagnostics.normalShotSnapshotCount);
    ImGui::Text(
        "ロック翼弾数: %zu", diagnostics.lockedWingShotSnapshotCount);
    ImGui::Text(
        "安定ID重複数: %zu", diagnostics.stableRuntimeIdDuplicateCount);
    ImGui::Text(
        "ダメージ値不正数: %zu",
        diagnostics.invalidBulletDamageSnapshotCount);

    const KrakenProjectileAggregationDiagnostics& aggregation =
        projectileDamageDiagnostics.frameAggregation;
    ImGui::SeparatorText("進入イベント");
    ImGui::Text("今フレーム本体進入数: %zu", aggregation.bodyEnterCount);
    ImGui::Text(
        "今フレーム弱点進入数: %zu", aggregation.weakPointEnterCount);
    ImGui::Text("投射物単位集約数: %zu", aggregation.aggregatedCount);
    ImGui::Text(
        "本体・弱点同時交差数: %zu",
        aggregation.bodyAndWeakPointCount);
    ImGui::Text(
        "弱点優先数: %zu", aggregation.weakPointPriorityCount);
    ImGui::Text("本体抑制数: %zu", aggregation.bodySuppressionCount);
    ImGui::Text(
        "重複本体抑制数: %zu",
        aggregation.duplicateBodySuppressionCount);
    ImGui::Text(
        "重複弱点抑制数: %zu",
        aggregation.duplicateWeakPointSuppressionCount);
    ImGui::Text(
        "古いフレーム拒否数: %zu",
        aggregation.oldFrameRejectionCount);

    ImGui::SeparatorText("直近命中");
    const KrakenProjectileLastHitDiagnostics& lastHit =
        projectileDamageDiagnostics.lastHit;
    if (!lastHit.valid) {
        ImGui::TextDisabled("命中履歴はありません。");
    } else {
        ImGui::Text(
            "投射物実行時ID: %llu",
            static_cast<unsigned long long>(
                lastHit.event.projectileRuntimeId));
        ImGui::Text(
            "投射物種別: %s",
            ProjectileTypeLabel(lastHit.event.projectileType));
        ImGui::Text("命中役割: %s", HitRoleLabel(lastHit.event.role));
        ImGui::Text("チェーン番号: %u", lastHit.event.chainIndex);
        ImGui::Text(
            "コライダーID: %llu",
            static_cast<unsigned long long>(lastHit.event.krakenColliderId));
        ImGui::Text("基礎ダメージ: %.2f", lastHit.event.projectileDamage);
        ImGui::Text("弱点倍率: %.2f", lastHit.weakPointMultiplier);
        ImGui::Text("最終ダメージ: %.2f", lastHit.finalDamage);
        ImGui::Text("適用前HP: %.2f", lastHit.hpBefore);
        ImGui::Text("適用後HP: %.2f", lastHit.hpAfter);
        ImGui::Text(
            "最近接点: %.2f, %.2f, %.2f",
            lastHit.event.closestPoint.x,
            lastHit.event.closestPoint.y,
            lastHit.event.closestPoint.z);
        ImGui::Text("侵入深度: %.3f", lastHit.event.penetrationDepth);
        ImGui::Text(
            "イベントフレーム: %llu",
            static_cast<unsigned long long>(lastHit.event.frameNumber));
        ImGui::Text(
            "弾停止成功: %s", BoolLabel(lastHit.bulletKillSucceeded));
        ImGui::Text(
            "弾停止失敗: %s", BoolLabel(lastHit.bulletKillFailed));
    }

    const KrakenProjectileDamageDiagnostics& damageDiagnostics =
        projectileDamageDiagnostics;
    ImGui::SeparatorText("統計");
    ImGui::Text(
        "本体ダメージ適用回数: %llu",
        static_cast<unsigned long long>(
            damageDiagnostics.bodyDamageAppliedCount));
    ImGui::Text(
        "弱点ダメージ適用回数: %llu",
        static_cast<unsigned long long>(
            damageDiagnostics.weakPointDamageAppliedCount));
    ImGui::Text("累計ダメージ: %.2f", damageDiagnostics.totalDamage);
    ImGui::Text("投射物消費数: %zu", consumedProjectileIds.size());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "同じプレイヤー弾が複数のコライダーへ触れても、\n"
            "中ボスへダメージを与えるのは1回だけです。");
    }
    ImGui::Text(
        "弾停止成功数: %llu",
        static_cast<unsigned long long>(
            damageDiagnostics.bulletKillSuccessCount));
    ImGui::Text(
        "弾停止失敗数: %llu",
        static_cast<unsigned long long>(
            damageDiagnostics.bulletKillFailureCount));
    ImGui::Text(
        "消費済み投射物抑制数: %llu",
        static_cast<unsigned long long>(
            damageDiagnostics.consumedProjectileSuppressionCount));
    ImGui::Text(
        "滞在ダメージ抑制数: %llu",
        static_cast<unsigned long long>(
            damageDiagnostics.stayDamageSuppressionCount));
    ImGui::Text(
        "離脱ダメージ抑制数: %llu",
        static_cast<unsigned long long>(
            damageDiagnostics.exitDamageSuppressionCount));
    ImGui::Text(
        "撃破待ち拒否数: %llu",
        static_cast<unsigned long long>(
            damageDiagnostics.defeatPendingRejectionCount));
    ImGui::Text(
        "ダメージ無効拒否数: %llu",
        static_cast<unsigned long long>(
            damageDiagnostics.projectileDamageDisabledRejectionCount));
    ImGui::Text(
        "HP0到達回数: %llu",
        static_cast<unsigned long long>(
            damageDiagnostics.hpZeroReachedCount));
    ImGui::Text(
        "撃破待ち開始回数: %llu",
        static_cast<unsigned long long>(
            damageDiagnostics.defeatPendingStartCount));
    ImGui::Text(
        "小型敵一撃仕様使用回数: %llu",
        static_cast<unsigned long long>(
            damageDiagnostics.smallEnemyInstantKillUseCount));

    ImGui::SeparatorText("安全診断");
    ImGui::Text("非有限HP数: %llu",
        static_cast<unsigned long long>(damageDiagnostics.nonFiniteHpCount));
    ImGui::Text("非有限ダメージ数: %llu",
        static_cast<unsigned long long>(damageDiagnostics.nonFiniteDamageCount));
    ImGui::Text("非有限倍率数: %llu",
        static_cast<unsigned long long>(damageDiagnostics.nonFiniteMultiplierCount));
    ImGui::Text("実行時ID 0数: %llu",
        static_cast<unsigned long long>(damageDiagnostics.runtimeIdZeroCount));
    ImGui::Text("ID重複数: %llu",
        static_cast<unsigned long long>(damageDiagnostics.duplicateRuntimeIdCount));
    ImGui::Text("弾管理未接続数: %llu",
        static_cast<unsigned long long>(damageDiagnostics.playerBulletManagerMissingCount));
    ImGui::Text("イベント役割不正数: %llu",
        static_cast<unsigned long long>(damageDiagnostics.invalidRoleCount));
    ImGui::Text("停止対象未発見数: %llu",
        static_cast<unsigned long long>(damageDiagnostics.killTargetNotFoundCount));
    ImGui::Text("同一投射物二重ダメージ数: %llu",
        static_cast<unsigned long long>(damageDiagnostics.sameProjectileDoubleDamageCount));
    ImGui::TextWrapped(
        "最後のエラー: %s",
        damageDiagnostics.lastError.empty()
            ? "なし" : damageDiagnostics.lastError.c_str());
    ImGui::TextWrapped(
        "最後の警告: %s",
        damageDiagnostics.lastWarning.empty()
            ? "なし" : damageDiagnostics.lastWarning.c_str());

    ImGui::SeparatorText("操作");
    if (ImGui::Button("ダメージを有効化##EnableDamageAction")) {
        projectileDamageEnabled = health.IsValid() &&
            !health.IsDefeatPending() && !IsDefeatState();
    }
    ImGui::SameLine();
    if (ImGui::Button("ダメージを無効化##DisableDamageAction")) {
        projectileDamageEnabled = false;
    }
    if (ImGui::Button("HPを全回復##HealFull")) {
        HealProjectileDamageHealth();
    }
    ImGui::SameLine();
    if (ImGui::Button("HPを最大値へリセット##ResetHpToMaximum")) {
        HealProjectileDamageHealth();
    }
    if (ImGui::Button("最大HPを現在HPへ反映##ApplyMaximumHp")) {
        HealProjectileDamageHealth();
    }
    if (ImGui::Button("弱点倍率を推奨値へ戻す##ResetWeakMultiplier")) {
        health.SetWeakPointMultiplier(
            KrakenTentacleMidbossHealth::kRecommendedWeakPointMultiplier);
    }
    if (ImGui::Button("ダメージ診断をリセット##ResetDamageDiagnostics")) {
        projectileDamageDiagnostics = {};
        aggregatedProjectileEventsThisFrame.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("投射物命中履歴を消去##ClearProjectileHistory")) {
        consumedProjectileIds.clear();
        projectileDamageDiagnostics.lastHit = {};
    }
    if (ImGui::Button("撃破待ちを解除して全回復##RecoverFromDefeat")) {
        pendingCommand = KrakenTentacleMidbossPendingCommand::RecoverDefeat;
    }
    if (ImGui::Button("標準設定へ戻す##ResetProjectileDamageDefaults")) {
        if (health.IsDefeatPending() || IsDefeatState()) {
            pendingCommand = KrakenTentacleMidbossPendingCommand::RecoverDefeat;
        } else {
            ResetProjectileDamageState(true);
        }
    }
    DrawHelp(
        "中ボスへダメージを与えたプレイヤー弾だけを、\n"
        "既存の弾消滅経路で停止します。");
#endif
}
