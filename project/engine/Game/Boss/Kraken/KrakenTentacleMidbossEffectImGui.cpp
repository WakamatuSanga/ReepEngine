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

const char* PositionSourceLabel(
    KrakenTentacleEffectPositionSource source) {
    switch (source) {
    case KrakenTentacleEffectPositionSource::CollisionClosestPoint:
        return "交差判定の最近接点";
    case KrakenTentacleEffectPositionSource::ColliderWorldCenter:
        return "採用コライダーのワールド中心";
    case KrakenTentacleEffectPositionSource::ProjectileWorldCenter:
        return "投射物スナップショットのワールド中心";
    case KrakenTentacleEffectPositionSource::SkinnedBoundsWorldCenter:
        return "現在スキニング境界のワールド中心";
    case KrakenTentacleEffectPositionSource::TentacleTipAverage:
        return "4本の触手先端平均";
    case KrakenTentacleEffectPositionSource::SkeletonRootWorldPosition:
        return "スケルトンルートのワールド位置";
    case KrakenTentacleEffectPositionSource::MidbossWorldPosition:
        return "中ボスのワールド位置";
    case KrakenTentacleEffectPositionSource::None:
    default:
        return "なし";
    }
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

const char* StateLabel(std::uint8_t stateValue) {
    switch (static_cast<KrakenTentacleMidbossState>(stateValue)) {
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

void DrawPosition(const char* label, const Vector3& value) {
    ImGui::Text(
        "%s: %.2f, %.2f, %.2f",
        label, value.x, value.y, value.z);
}
#endif
}

void KrakenTentacleMidbossController::Impl::DrawEffectImGui() {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader(
            "命中・撃破エフェクト##HitAndDefeatEffects",
            ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    KrakenTentacleEffectSettings& settings = effectController.GetSettings();
    const KrakenTentacleEffectDiagnostics& effectDiagnostics =
        effectController.GetDiagnostics();
    ImGui::SeparatorText("設定");
    ImGui::Checkbox(
        "命中エフェクトを有効化##EnableHitEffect",
        &settings.hitEffectEnabled);
    ImGui::Checkbox(
        "弱点命中エフェクトを有効化##EnableWeakHitEffect",
        &settings.weakPointHitEffectEnabled);
    ImGui::Checkbox(
        "撃破エフェクトを有効化##EnableDefeatEffect",
        &settings.defeatEffectEnabled);
    ImGui::DragFloat(
        "本体命中倍率##BodyHitScale",
        &settings.bodyHitScale, 0.01f, 0.25f, 4.0f, "%.2f");
    ImGui::DragFloat(
        "弱点命中倍率##WeakPointHitScale",
        &settings.weakPointHitScale, 0.01f, 0.25f, 4.0f, "%.2f");
    ImGui::DragFloat(
        "撃破エフェクト倍率##DefeatEffectScale",
        &settings.defeatEffectScale, 0.01f, 0.25f, 4.0f, "%.2f");
    settings.bodyHitScale = std::clamp(settings.bodyHitScale, 0.25f, 4.0f);
    settings.weakPointHitScale = std::clamp(
        settings.weakPointHitScale, 0.25f, 4.0f);
    settings.defeatEffectScale = std::clamp(
        settings.defeatEffectScale, 0.25f, 4.0f);
    ImGui::Checkbox(
        "既存の衝撃歪みを使用##UseImpactDistortion",
        &settings.useImpactDistortion);
    ImGui::Text("音未実装: はい");
    ImGui::Text("新規シェーダー未使用: はい");
    ImGui::Text("新規テクスチャ未使用: はい");

    ImGui::SeparatorText("直近の命中エフェクト");
    const KrakenTentacleLastHitEffectDiagnostics& lastHit =
        effectDiagnostics.lastHit;
    if (!lastHit.valid) {
        ImGui::TextDisabled("命中エフェクト履歴はありません。");
    } else {
        ImGui::Text(
            "投射物実行時ID: %llu",
            static_cast<unsigned long long>(
                lastHit.event.projectileRuntimeId));
        ImGui::Text(
            "投射物種別: %s",
            ProjectileTypeLabel(lastHit.event.projectileType));
        ImGui::Text("命中役割: %s", HitRoleLabel(lastHit.event.role));
        DrawPosition("エフェクト位置", lastHit.worldPosition);
        ImGui::Text(
            "位置取得元: %s",
            PositionSourceLabel(lastHit.positionSource));
        ImGui::Text("実行時倍率: %.2f", lastHit.runtimeScale);
        ImGui::Text(
            "生成成功: %s", BoolLabel(lastHit.spawnSucceeded));
        ImGui::Text("生成失敗: %s", BoolLabel(lastHit.spawnFailed));
        ImGui::Text(
            "イベントフレーム: %llu",
            static_cast<unsigned long long>(lastHit.event.frameNumber));
        ImGui::Text(
            "コライダーID: %llu",
            static_cast<unsigned long long>(
                lastHit.event.krakenColliderId));
        ImGui::Text("チェーン番号: %u", lastHit.event.chainIndex);
    }

    ImGui::SeparatorText("撃破エフェクト");
    const KrakenTentacleLastDefeatEffectDiagnostics& lastDefeat =
        effectDiagnostics.lastDefeat;
    ImGui::Text(
        "撃破連番: %llu",
        static_cast<unsigned long long>(lastDefeat.defeatSequenceId));
    ImGui::Text(
        "撃破エフェクト生成済み: %s",
        BoolLabel(lastDefeat.spawned));
    if (lastDefeat.valid) {
        DrawPosition("エフェクト位置", lastDefeat.worldPosition);
        ImGui::Text(
            "位置取得元: %s",
            PositionSourceLabel(lastDefeat.positionSource));
        ImGui::Text("実行時倍率: %.2f", lastDefeat.runtimeScale);
        ImGui::Text(
            "生成要求成功: %s", BoolLabel(lastDefeat.spawnSucceeded));
        ImGui::Text(
            "生成要求失敗: %s", BoolLabel(lastDefeat.spawnFailed));
        ImGui::Text(
            "生成時状態: %s", StateLabel(lastDefeat.stateAtSpawn));
    }
    ImGui::Text("退避中に追従: いいえ");
    ImGui::Text("ウェーブ通知: 0");

    ImGui::SeparatorText("操作");
    if (ImGui::Button("本体命中エフェクトをテスト##TestBodyHitEffect")) {
        pendingCommand =
            KrakenTentacleMidbossPendingCommand::TestBodyHitEffect;
    }
    if (ImGui::Button("弱点命中エフェクトをテスト##TestWeakHitEffect")) {
        pendingCommand =
            KrakenTentacleMidbossPendingCommand::TestWeakPointHitEffect;
    }
    if (ImGui::Button("撃破エフェクトをテスト##TestDefeatEffect")) {
        pendingCommand =
            KrakenTentacleMidbossPendingCommand::TestDefeatEffect;
    }

    ImGui::SeparatorText("統計");
    ImGui::Text(
        "本体命中生成数: %llu",
        static_cast<unsigned long long>(
            effectDiagnostics.bodyHitSpawnCount));
    ImGui::Text(
        "弱点命中生成数: %llu",
        static_cast<unsigned long long>(
            effectDiagnostics.weakPointHitSpawnCount));
    ImGui::Text(
        "撃破エフェクト生成数: %llu",
        static_cast<unsigned long long>(
            effectDiagnostics.defeatSpawnCount));
    ImGui::Text(
        "重複命中抑制数: %llu",
        static_cast<unsigned long long>(
            effectDiagnostics.duplicateHitSuppressionCount));
    ImGui::Text(
        "重複撃破抑制数: %llu",
        static_cast<unsigned long long>(
            effectDiagnostics.duplicateDefeatSuppressionCount));
    ImGui::Text(
        "非有限位置拒否数: %llu",
        static_cast<unsigned long long>(
            effectDiagnostics.nonFinitePositionRejectionCount));
    ImGui::Text(
        "位置代替使用数: %llu",
        static_cast<unsigned long long>(
            effectDiagnostics.positionFallbackCount));
    ImGui::Text(
        "エフェクト管理未接続数: %llu",
        static_cast<unsigned long long>(
            effectDiagnostics.effectManagerMissingCount));
    ImGui::Text(
        "クラーケン側で検出したエフェクト枠不足数: %llu",
        static_cast<unsigned long long>(
            effectDiagnostics.effectPoolShortageCount));
    ImGui::TextWrapped(
        "共有撃破エフェクトの枠不足は、共有管理側の既存診断でも確認します。");
    ImGui::Text(
        "エフェクト生成失敗数: %llu",
        static_cast<unsigned long long>(
            effectDiagnostics.spawnFailureCount));
    ImGui::Text(
        "ダメージなし生成抑制数: %llu",
        static_cast<unsigned long long>(
            effectDiagnostics.damageWithoutEffectSuppressionCount));
    ImGui::Text(
        "HP 0後命中生成抑制数: %llu",
        static_cast<unsigned long long>(
            effectDiagnostics.hpZeroHitEffectSuppressionCount));
    ImGui::TextWrapped(
        "最後のエラー: %s",
        effectDiagnostics.lastError.empty()
            ? "なし" : effectDiagnostics.lastError.c_str());
    ImGui::TextWrapped(
        "最後の警告: %s",
        effectDiagnostics.lastWarning.empty()
            ? "なし" : effectDiagnostics.lastWarning.c_str());
#endif
}
