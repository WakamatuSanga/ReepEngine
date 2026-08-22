#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>

namespace {
#ifdef USE_IMGUI
    constexpr int kStandardEnemyBulletDamage = 1;

    const char* BoolLabel(bool value) {
        return value ? "はい" : "いいえ";
    }

    const char* StateLabel(KrakenTentacleMidbossState state) {
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
        case KrakenTentacleMidbossState::Defeated:
            return "撃破直後停止";
        case KrakenTentacleMidbossState::Retreating:
            return "下方退避中";
        case KrakenTentacleMidbossState::RetreatCompleted:
            return "撃破退避完了";
        default:
            return "不明";
        }
    }

    const char* DamageResultLabel(KrakenAttackDamageResult result) {
        switch (result) {
        case KrakenAttackDamageResult::None:
            return "未試行";
        case KrakenAttackDamageResult::Disabled:
            return "攻撃ダメージ無効";
        case KrakenAttackDamageResult::Applied:
            return "ダメージ適用成功";
        case KrakenAttackDamageResult::BlockedByDamageInvincibility:
            return "既存ダメージ無敵により拒否";
        case KrakenAttackDamageResult::BlockedByBarrelRoll:
            return "バレルロール無敵により拒否";
        case KrakenAttackDamageResult::BlockedByDeath:
            return "死亡中のため拒否";
        case KrakenAttackDamageResult::ContextUnavailable:
            return "プレイヤー参照未接続";
        case KrakenAttackDamageResult::DamageApiRejected:
            return "既存ダメージ処理が拒否";
        case KrakenAttackDamageResult::SequenceUnavailable:
            return "攻撃連番が無効";
        case KrakenAttackDamageResult::HitAlreadyConsumed:
            return "同じ攻撃で試行済み";
        case KrakenAttackDamageResult::OldFrame:
            return "古いフレームを拒否";
        case KrakenAttackDamageResult::ChainMismatch:
            return "攻撃チェーン不一致";
        case KrakenAttackDamageResult::PhaseMismatch:
            return "攻撃フェーズ不一致";
        default:
            return "不明";
        }
    }
#endif
}

void KrakenTentacleMidbossController::Impl::DrawAttackDamageImGui() {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader(
            "触手攻撃ダメージ##AttackDamage",
            ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::SeparatorText("設定##AttackDamageSettings");
    bool damageEnabled = attackDamageEnabled;
    if (ImGui::Checkbox(
            "攻撃ダメージを有効化##AttackDamageEnabled",
            &damageEnabled)) {
        attackDamageEnabled = damageEnabled &&
            !health.IsDefeatPending() && !IsDefeatState();
    }
    ImGui::DragInt(
        "ダメージ量##AttackDamageAmount", &attackDamage,
        1.0f, 1, 3);
    attackDamage = std::clamp(attackDamage, 1, 3);
    ImGui::Text("標準敵弾ダメージ: %d", kStandardEnemyBulletDamage);
    ImGui::Text("1攻撃1回: はい（固定）");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "同じ叩きつけ攻撃中は、プレイヤーと何度交差しても"
            "ダメージ試行は1回だけです。");
    }
    ImGui::Text("交差開始時だけダメージ: はい（固定）");
    ImGui::Text("交差継続ダメージ: いいえ（固定）");
    ImGui::Text("デバッグコライダー表示に依存: いいえ");
    ImGui::Text("汎用衝突管理機構を使用: いいえ");

    ImGui::SeparatorText("現在の攻撃##CurrentAttackDamage");
    ImGui::Text(
        "攻撃連番: %llu",
        static_cast<unsigned long long>(currentAttackSequenceId));
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "振りかぶり開始時に更新される、1回の触手攻撃を"
            "識別する番号です。");
    }
    ImGui::Text("選択チェーン: %zu", selectedAttackChainIndex);
    ImGui::Text("現在状態: %s", StateLabel(state));
    ImGui::Text("攻撃フェーズ: %s", StateLabel(state));
    ImGui::Text("叩きつけ進行率: %.3f", GetSlamProgress());
    ImGui::Text(
        "攻撃コライダーのフェーズ有効: %s",
        BoolLabel(attackDamageDiagnostics.attackPhaseActive));
    ImGui::Text(
        "ヒット試行消費済み: %s",
        BoolLabel(hitAttemptConsumedThisAttack));
    ImGui::Text(
        "ダメージ適用済み: %s", BoolLabel(damageAppliedThisAttack));
    ImGui::Text(
        "無敵などで拒否: %s", BoolLabel(damageBlockedThisAttack));
    ImGui::Text(
        "最後の交差開始フレーム: %llu",
        static_cast<unsigned long long>(
            attackDamageDiagnostics.lastEnterFrame));
    ImGui::Text(
        "最後のコライダー番号: %llu",
        static_cast<unsigned long long>(
            attackDamageDiagnostics.lastColliderId));
    ImGui::Text(
        "最後のめり込み深度: %.4f",
        attackDamageDiagnostics.lastPenetrationDepth);

    ImGui::SeparatorText("プレイヤー状態##AttackDamagePlayerState");
    ImGui::Text(
        "プレイヤー接続: %s",
        BoolLabel(attackDamageDiagnostics.playerConnected));
    ImGui::Text(
        "生存中: %s", BoolLabel(attackDamageDiagnostics.playerAlive));
    ImGui::Text(
        "ダメージ受付可能: %s",
        BoolLabel(attackDamageDiagnostics.damageAcceptable));
    ImGui::Text(
        "既存ダメージ無敵中: %s",
        BoolLabel(attackDamageDiagnostics.damageInvincible));
    ImGui::Text(
        "バレルロール無敵中: %s",
        BoolLabel(attackDamageDiagnostics.barrelRollInvincible));
    ImGui::Text("復帰無敵: 該当機能なし");
    ImGui::Text("適用前体力: %d", attackDamageDiagnostics.hpBefore);
    ImGui::Text("適用後体力: %d", attackDamageDiagnostics.hpAfter);
    ImGui::Text(
        "既存ダメージ処理結果: %s",
        DamageResultLabel(attackDamageDiagnostics.lastResult));
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "交差開始時に無敵だった場合はダメージを適用せず、"
            "その攻撃中は再試行しません。");
    }

    ImGui::SeparatorText("統計##AttackDamageStatistics");
    ImGui::Text(
        "攻撃開始回数: %llu",
        static_cast<unsigned long long>(
            attackDamageDiagnostics.attackStartCount));
    ImGui::Text(
        "ダメージ試行回数: %llu",
        static_cast<unsigned long long>(
            attackDamageDiagnostics.damageAttemptCount));
    ImGui::Text(
        "ダメージ適用回数: %llu",
        static_cast<unsigned long long>(
            attackDamageDiagnostics.damageAppliedCount));
    ImGui::Text(
        "無敵拒否回数: %llu",
        static_cast<unsigned long long>(
            attackDamageDiagnostics.invincibilityRejectionCount));
    ImGui::Text(
        "死亡中拒否回数: %llu",
        static_cast<unsigned long long>(
            attackDamageDiagnostics.playerDeathRejectionCount));
    ImGui::Text(
        "参照無効回数: %llu",
        static_cast<unsigned long long>(
            attackDamageDiagnostics.contextUnavailableCount));
    ImGui::Text(
        "同一攻撃ヒット抑制回数: %llu",
        static_cast<unsigned long long>(
            attackDamageDiagnostics.sameAttackHitSuppressionCount));
    ImGui::Text(
        "交差継続抑制回数: %llu",
        static_cast<unsigned long long>(
            attackDamageDiagnostics.stayDamageSuppressionCount));
    ImGui::Text(
        "交差終了抑制回数: %llu",
        static_cast<unsigned long long>(
            attackDamageDiagnostics.exitDamageSuppressionCount));
    ImGui::Text(
        "再交差開始抑制回数: %llu",
        static_cast<unsigned long long>(
            attackDamageDiagnostics.reenterDamageSuppressionCount));
    ImGui::Text(
        "同一フレーム二重処理防止回数: %llu",
        static_cast<unsigned long long>(
            attackDamageDiagnostics.sameFrameSuppressionCount));
    ImGui::Text(
        "古い事象拒否回数: %llu",
        static_cast<unsigned long long>(
            attackDamageDiagnostics.oldEventRejectionCount));
    ImGui::Text(
        "チェーン不一致拒否回数: %llu",
        static_cast<unsigned long long>(
            attackDamageDiagnostics.chainMismatchRejectionCount));
    ImGui::Text(
        "フェーズ不一致拒否回数: %llu",
        static_cast<unsigned long long>(
            attackDamageDiagnostics.phaseMismatchRejectionCount));

    ImGui::SeparatorText("副作用確認##AttackDamageSideEffects");
    ImGui::Text(
        "1攻撃中の最大ダメージ回数: %u（1以下: %s）",
        attackDamageDiagnostics.maximumDamageCountPerAttack,
        BoolLabel(
            attackDamageDiagnostics.maximumDamageCountPerAttack <= 1));
    ImGui::Text("プレイヤー弾消滅要求: 0");
    ImGui::Text("中ボス体力変更: 0");
    ImGui::Text("新規エフェクト発生: 0");
    ImGui::Text(
        "ゲームプレイ登録: %s",
        BoolLabel(diagnostics.gameplayRegistrationObserved));
    ImGui::Text("最後のダメージ元: クラーケン触手");

    ImGui::SeparatorText("操作##AttackDamageOperations");
    if (ImGui::Button("攻撃ダメージを有効化##EnableAttackDamage")) {
        attackDamageEnabled = !health.IsDefeatPending() && !IsDefeatState();
    }
    ImGui::SameLine();
    if (ImGui::Button("攻撃ダメージを無効化##DisableAttackDamage")) {
        attackDamageEnabled = false;
    }
    if (ImGui::Button("ダメージ診断をリセット##ResetDamageDiagnostics")) {
        attackDamageDiagnostics = {};
    }
    if (ImGui::Button("現在攻撃のヒット履歴をリセット##ResetCurrentHit")) {
        hitAttemptConsumedThisAttack = false;
        damageAppliedThisAttack = false;
        damageBlockedThisAttack = false;
        exitObservedAfterHitAttempt = false;
        attackDamageDiagnostics.currentDamageCount = 0;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "デバッグ専用です。実運用では使用しません。"
            "次の交差開始までダメージは発生しません。");
    }
    if (ImGui::Button("次の攻撃でダメージ確認##CheckNextAttack")) {
        pendingCommand = KrakenTentacleMidbossPendingCommand::StartAttack;
    }
    ImGui::SameLine();
    if (ImGui::Button("標準ダメージへ戻す##RestoreStandardDamage")) {
        attackDamage = kStandardEnemyBulletDamage;
    }
#endif
}
