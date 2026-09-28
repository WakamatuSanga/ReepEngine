#include "PlayerLockOnTargetProvider.h"

#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossController.h"
#include "Engine/Game/Enemy/EnemyManager.h"
#include "Engine/Game/KrakenTentacleWeakPointAnchorSnapshot.h"
#include "Engine/Game/Player/PlayerBulletManager.h"
#include "Engine/Game/Targeting/AimCorridorTargetingController.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
bool IsFinite(const Vector3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

bool IsCombatState(KrakenTentacleMidbossState state) {
    switch (state) {
    case KrakenTentacleMidbossState::Idle:
    case KrakenTentacleMidbossState::Windup:
    case KrakenTentacleMidbossState::WindupHold:
    case KrakenTentacleMidbossState::Slam:
    case KrakenTentacleMidbossState::ImpactHold:
    case KrakenTentacleMidbossState::Recovery:
        return true;
    case KrakenTentacleMidbossState::Hidden:
    case KrakenTentacleMidbossState::Defeated:
    case KrakenTentacleMidbossState::Retreating:
    case KrakenTentacleMidbossState::RetreatCompleted:
        return false;
    }
    return false;
}

const char* StateText(KrakenTentacleMidbossState state) {
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

const char* BoolText(bool value) {
    return value ? "はい" : "いいえ";
}
} // namespace

PlayerLockOnTargetProvider::~PlayerLockOnTargetProvider() = default;

void PlayerLockOnTargetProvider::Initialize(
    const EnemyManager* enemyManager,
    const KrakenTentacleMidbossController* krakenRuntime) {
    Finalize();
    enemyManager_ = enemyManager;
    krakenRuntime_ = krakenRuntime;
    normalEnemyViews_.reserve(32);
    debugTargets_.reserve(40);
    krakenTargetIds_.reserve(8);
    initialized_ = true;
    SyncKrakenTargetIds();
    ResetDiagnostics();
}

void PlayerLockOnTargetProvider::Reset() {
    normalEnemyViews_.clear();
    debugTargets_.clear();
    ResetDiagnostics();
    SyncKrakenTargetIds();
}

void PlayerLockOnTargetProvider::Finalize() {
    enemyManager_ = nullptr;
    krakenRuntime_ = nullptr;
    normalEnemyViews_.clear();
    debugTargets_.clear();
    krakenTargetIds_.clear();
    cachedKrakenChainCount_ = 0;
    initialized_ = false;
    ResetDiagnostics();
}

void PlayerLockOnTargetProvider::ResetDiagnostics() const {
    candidateCollectionCount_ = 0;
    idQueryCount_ = 0;
    idQuerySuccessCount_ = 0;
    idQueryFailureCount_ = 0;
    normalEnemyResolveCount_ = 0;
    krakenResolveCount_ = 0;
    invalidCandidateCount_ = 0;
    duplicateIdCount_ = 0;
    duplicateIdRejectionCount_ = 0;
    nonFinitePositionCount_ = 0;
    emptyIdCount_ = 0;
    invalidKindCount_ = 0;
    targetLostCount_ = 0;
    krakenDefeatTargetLostCount_ = 0;
    targetSwitchCount_ = 0;
    lastCandidateCount_ = 0;
    lastNormalEnemyCandidateCount_ = 0;
    lastKrakenCandidateCount_ = 0;
    lastInvalidCandidateCount_ = 0;
    lastNormalSourceCount_ = 0;
    lastNormalIdMatchCount_ = 0;
    lastNormalOrderDifferenceCount_ = 0;
    lastNormalPositionMaximumDifference_ = 0.0f;
}

void PlayerLockOnTargetProvider::SyncKrakenTargetIds() const {
    const std::size_t chainCount =
        initialized_ && krakenRuntime_ && krakenRuntime_->IsInitialized()
        ? krakenRuntime_->GetDetectedChainCount()
        : 0;
    if (chainCount == cachedKrakenChainCount_ &&
        krakenTargetIds_.size() == chainCount) {
        return;
    }

    krakenTargetIds_.clear();
    krakenTargetIds_.reserve(chainCount);
    for (std::size_t index = 0; index < chainCount; ++index) {
        KrakenTargetId target{};
        target.id = "KrakenTentacleWeakPoint_" + std::to_string(index);
        target.chainIndex = static_cast<std::uint32_t>(index);
        krakenTargetIds_.push_back(std::move(target));
    }
    cachedKrakenChainCount_ = chainCount;
}

bool PlayerLockOnTargetProvider::BuildKrakenSnapshot(
    const KrakenTargetId& target,
    PlayerLockOnTargetSnapshot& outSnapshot) const {
    outSnapshot = {};
    outSnapshot.id = target.id;
    outSnapshot.kind = PlayerLockOnTargetKind::KrakenWeakPoint;
    outSnapshot.subTargetIndex = target.chainIndex;
    if (!krakenRuntime_ || !krakenRuntime_->IsInitialized()) {
        return false;
    }

    const float currentHp = krakenRuntime_->GetCurrentHp();
    const bool defeatActive = krakenRuntime_->IsDefeatPending() ||
        krakenRuntime_->IsDefeatStarted() ||
        krakenRuntime_->IsDefeatCompleted();
    outSnapshot.alive = std::isfinite(currentHp) && currentHp > 0.0f &&
        !defeatActive;

    KrakenTentacleWeakPointAnchorSnapshot anchor{};
    const bool anchorFound =
        krakenRuntime_->TryGetWeakPointLockOnAnchorSnapshot(
            target.chainIndex, anchor);
    if (anchorFound) {
        outSnapshot.sourceColliderId = anchor.colliderId;
        outSnapshot.worldPosition = anchor.worldCenter;
        outSnapshot.worldRadius = anchor.worldRadius;
    }
    const bool shapeValid = anchorFound && anchor.valid &&
        IsFinite(anchor.worldCenter) && std::isfinite(anchor.worldRadius) &&
        anchor.worldRadius > 0.0f;
    const bool runtimeTargetable = outSnapshot.alive && anchor.enabled &&
        krakenRuntime_->IsVisible() &&
        IsCombatState(krakenRuntime_->GetRuntimeState());
    outSnapshot.valid = !outSnapshot.id.empty() && shapeValid &&
        runtimeTargetable;
    outSnapshot.targetable = outSnapshot.valid;
    return true;
}

bool PlayerLockOnTargetProvider::ContainsId(
    const std::vector<PlayerLockOnTargetSnapshot>& targets,
    std::string_view id) const {
    return std::any_of(
        targets.begin(), targets.end(),
        [id](const PlayerLockOnTargetSnapshot& target) {
            return target.id == id;
        });
}

void PlayerLockOnTargetProvider::CollectTargetableTargets(
    std::vector<PlayerLockOnTargetSnapshot>& outTargets) const {
    outTargets.clear();
    ++candidateCollectionCount_;
    lastCandidateCount_ = 0;
    lastNormalEnemyCandidateCount_ = 0;
    lastKrakenCandidateCount_ = 0;
    lastInvalidCandidateCount_ = 0;
    lastNormalSourceCount_ = 0;
    lastNormalIdMatchCount_ = 0;
    lastNormalOrderDifferenceCount_ = 0;
    lastNormalPositionMaximumDifference_ = 0.0f;
    if (!initialized_) {
        return;
    }

    normalEnemyViews_.clear();
    if (enemyManager_) {
        enemyManager_->CollectTargetableEnemies(normalEnemyViews_);
    }
    SyncKrakenTargetIds();
    lastNormalSourceCount_ = normalEnemyViews_.size();
    const std::size_t neededCapacity =
        normalEnemyViews_.size() + krakenTargetIds_.size();
    if (outTargets.capacity() < neededCapacity) {
        outTargets.reserve(neededCapacity);
    }

    for (std::size_t index = 0; index < normalEnemyViews_.size(); ++index) {
        const EnemyTargetView& source = normalEnemyViews_[index];
        PlayerLockOnTargetSnapshot target{};
        target.id = source.runtimeId;
        target.worldPosition = source.worldPosition;
        target.worldRadius = source.worldRadius;
        target.kind = PlayerLockOnTargetKind::NormalEnemy;
        target.alive = true;
        target.targetable = true;
        target.valid = !target.id.empty() && IsFinite(target.worldPosition);
        if (target.id.empty()) {
            ++emptyIdCount_;
        }
        if (!IsFinite(target.worldPosition)) {
            ++nonFinitePositionCount_;
        }
        if (!target.valid) {
            ++invalidCandidateCount_;
            ++lastInvalidCandidateCount_;
            continue;
        }
        if (ContainsId(outTargets, target.id)) {
            ++duplicateIdCount_;
            ++duplicateIdRejectionCount_;
            ++lastInvalidCandidateCount_;
            continue;
        }
        if (target.id == source.runtimeId) {
            ++lastNormalIdMatchCount_;
        }
        if (outTargets.size() != index) {
            ++lastNormalOrderDifferenceCount_;
        }
        const float dx = target.worldPosition.x - source.worldPosition.x;
        const float dy = target.worldPosition.y - source.worldPosition.y;
        const float dz = target.worldPosition.z - source.worldPosition.z;
        const float positionDifference = std::sqrt(
            dx * dx + dy * dy + dz * dz);
        lastNormalPositionMaximumDifference_ = (std::max)(
            lastNormalPositionMaximumDifference_, positionDifference);
        outTargets.push_back(std::move(target));
        ++lastNormalEnemyCandidateCount_;
    }

    for (const KrakenTargetId& source : krakenTargetIds_) {
        PlayerLockOnTargetSnapshot target{};
        BuildKrakenSnapshot(source, target);
        if (!target.valid || !target.alive || !target.targetable) {
            ++invalidCandidateCount_;
            ++lastInvalidCandidateCount_;
            if (!IsFinite(target.worldPosition)) {
                ++nonFinitePositionCount_;
            }
            continue;
        }
        if (ContainsId(outTargets, target.id)) {
            ++duplicateIdCount_;
            ++duplicateIdRejectionCount_;
            ++lastInvalidCandidateCount_;
            continue;
        }
        // Only selection needs the body geometry. ID queries for missiles keep the tip anchor.
        if (!krakenRuntime_->TryGetChainLockOnGeometrySnapshot(
                source.chainIndex, target.krakenGeometry)) {
            ++lastInvalidCandidateCount_;
            continue;
        }
        outTargets.push_back(std::move(target));
        ++lastKrakenCandidateCount_;
    }
    lastCandidateCount_ = outTargets.size();
}

bool PlayerLockOnTargetProvider::TryGetTargetSnapshot(
    std::string_view targetId,
    PlayerLockOnTargetSnapshot& outSnapshot) const {
    outSnapshot = {};
    ++idQueryCount_;
    if (!initialized_ || targetId.empty()) {
        if (targetId.empty()) {
            ++emptyIdCount_;
        }
        ++idQueryFailureCount_;
        return false;
    }

    SyncKrakenTargetIds();
    const auto krakenIt = std::find_if(
        krakenTargetIds_.begin(), krakenTargetIds_.end(),
        [targetId](const KrakenTargetId& target) {
            return target.id == targetId;
        });

    EnemyTargetSnapshot enemySnapshot{};
    const bool normalFound = enemyManager_ &&
        enemyManager_->TryGetEnemyTargetSnapshot(targetId, enemySnapshot);
    const EnemyTargetView* normalTargetView = nullptr;
    if (normalFound) {
        normalEnemyViews_.clear();
        enemyManager_->CollectTargetableEnemies(normalEnemyViews_);
        std::size_t matchingNormalIdCount = 0;
        for (const EnemyTargetView& target : normalEnemyViews_) {
            if (target.runtimeId == targetId) {
                ++matchingNormalIdCount;
                normalTargetView = &target;
            }
        }
        if (matchingNormalIdCount > 1) {
            ++duplicateIdCount_;
            ++duplicateIdRejectionCount_;
            ++idQueryFailureCount_;
            return false;
        }
    }
    if (normalFound && krakenIt != krakenTargetIds_.end()) {
        ++duplicateIdCount_;
        ++duplicateIdRejectionCount_;
        ++idQueryFailureCount_;
        return false;
    }

    if (krakenIt != krakenTargetIds_.end()) {
        BuildKrakenSnapshot(*krakenIt, outSnapshot);
        ++idQuerySuccessCount_;
        ++krakenResolveCount_;
        if (!outSnapshot.valid || !outSnapshot.alive ||
            !outSnapshot.targetable) {
            ++targetLostCount_;
            if (!outSnapshot.alive) {
                ++krakenDefeatTargetLostCount_;
            }
        }
        return true;
    }

    if (!normalFound) {
        ++idQueryFailureCount_;
        return false;
    }

    outSnapshot.id = std::string(targetId);
    outSnapshot.worldPosition = enemySnapshot.worldPosition;
    outSnapshot.kind = PlayerLockOnTargetKind::NormalEnemy;
    outSnapshot.alive = enemySnapshot.alive;
    if (normalTargetView) {
        outSnapshot.worldPosition = normalTargetView->worldPosition;
        outSnapshot.worldRadius = normalTargetView->worldRadius;
        outSnapshot.targetable = true;
    }
    outSnapshot.valid = enemySnapshot.valid && !outSnapshot.id.empty() &&
        IsFinite(outSnapshot.worldPosition);
    ++idQuerySuccessCount_;
    ++normalEnemyResolveCount_;
    if (!outSnapshot.valid || !outSnapshot.alive ||
        !outSnapshot.targetable) {
        ++targetLostCount_;
    }
    return true;
}

void PlayerLockOnTargetProvider::DrawImGui(
    const AimCorridorTargetingController* targetingController,
    const PlayerBulletManager* playerBulletManager) {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(480.0f, 620.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(
            "ロックオン対象プロバイダー###PlayerLockOnTargetProvider")) {
        ImGui::End();
        return;
    }
    ImGui::SeparatorText("接続状態");
    ImGui::Text("プロバイダー初期化: %s", BoolText(initialized_));
    ImGui::Text("通常敵管理接続: %s", BoolText(enemyManager_ != nullptr));
    ImGui::Text("クラーケン実行接続: %s", BoolText(krakenRuntime_ != nullptr));

    if (targetingController) {
        targetingController->DrawKrakenNaturalLockDiagnosticsImGui();
    }
    if (playerBulletManager) {
        playerBulletManager->DrawKrakenNaturalLockHomingDiagnosticsImGui();
    }

    ImGui::SeparatorText("候補診断");
    ImGui::Text("全候補数: %zu", lastCandidateCount_);
    ImGui::Text("通常敵候補数: %zu", lastNormalEnemyCandidateCount_);
    ImGui::Text("クラーケン候補数: %zu", lastKrakenCandidateCount_);
    ImGui::Text("無効候補数: %zu", lastInvalidCandidateCount_);
    ImGui::Text("識別子重複数: %zu", duplicateIdCount_);
    ImGui::Text("非有限位置数: %zu", nonFinitePositionCount_);
    ImGui::Text("空識別子数: %zu", emptyIdCount_);

    if (ImGui::CollapsingHeader(
            "通常敵互換比較##NormalEnemyCompatibility",
            ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("変換前候補数: %zu", lastNormalSourceCount_);
        ImGui::Text("変換後候補数: %zu", lastNormalEnemyCandidateCount_);
        ImGui::Text("識別子一致数: %zu", lastNormalIdMatchCount_);
        ImGui::Text(
            "位置最大差: %.6f", lastNormalPositionMaximumDifference_);
        ImGui::Text("候補順の差: %zu", lastNormalOrderDifferenceCount_);
    }

    if (ImGui::CollapsingHeader(
            "クラーケン候補##KrakenCandidates",
            ImGuiTreeNodeFlags_DefaultOpen)) {
        SyncKrakenTargetIds();
        const bool hasKraken = krakenRuntime_ != nullptr;
        ImGui::Text(
            "クラーケン表示中: %s",
            BoolText(hasKraken && krakenRuntime_->IsVisible()));
        ImGui::Text(
            "現在体力: %.1f",
            hasKraken ? krakenRuntime_->GetCurrentHp() : 0.0f);
        ImGui::Text(
            "実行状態: %s",
            hasKraken ? StateText(krakenRuntime_->GetRuntimeState()) : "未接続");
        ImGui::Text("検出チェーン数: %zu", cachedKrakenChainCount_);
        ImGui::Text("有効な弱点数: %zu", lastKrakenCandidateCount_);
        for (const KrakenTargetId& target : krakenTargetIds_) {
            PlayerLockOnTargetSnapshot snapshot{};
            BuildKrakenSnapshot(target, snapshot);
            const std::string label = "チェーン " +
                std::to_string(target.chainIndex) + "##KrakenTarget" +
                std::to_string(target.chainIndex);
            if (!ImGui::TreeNode(label.c_str())) {
                continue;
            }
            ImGui::Text("対象識別子: %s", snapshot.id.c_str());
            ImGui::Text(
                "弱点中心: %.3f, %.3f, %.3f",
                snapshot.worldPosition.x,
                snapshot.worldPosition.y,
                snapshot.worldPosition.z);
            ImGui::Text("弱点半径: %.3f", snapshot.worldRadius);
            ImGui::Text("生存中: %s", BoolText(snapshot.alive));
            ImGui::Text("候補有効: %s", BoolText(snapshot.targetable));
            const char* reason = snapshot.targetable ? "なし" :
                (!snapshot.alive ? "撃破または体力0" :
                (!krakenRuntime_->IsVisible() ? "モデル非表示" :
                (!IsCombatState(krakenRuntime_->GetRuntimeState())
                    ? "戦闘対象外状態" :
                (!snapshot.valid ? "弱点形状が無効" : "弱点が無効"))));
            ImGui::Text("無効理由: %s", reason);
            ImGui::TreePop();
        }
    }

    if (ImGui::CollapsingHeader("統計##Statistics")) {
        ImGui::Text("候補収集回数: %zu", candidateCollectionCount_);
        ImGui::Text("識別子検索回数: %zu", idQueryCount_);
        ImGui::Text("検索成功数: %zu", idQuerySuccessCount_);
        ImGui::Text("検索失敗数: %zu", idQueryFailureCount_);
        ImGui::Text("通常敵解決数: %zu", normalEnemyResolveCount_);
        ImGui::Text("クラーケン解決数: %zu", krakenResolveCount_);
        ImGui::Text("対象種別不正数: %zu", invalidKindCount_);
        ImGui::Text("重複識別子拒否数: %zu", duplicateIdRejectionCount_);
        ImGui::Text("対象喪失数: %zu", targetLostCount_);
        ImGui::Text("撃破による対象喪失数: %zu", krakenDefeatTargetLostCount_);
        ImGui::Text("別対象への乗り換え数: %zu（常に0）", targetSwitchCount_);
    }

    if (ImGui::Button("診断をリセット##ResetProviderDiagnostics")) {
        ResetDiagnostics();
    }
    ImGui::SameLine();
    if (ImGui::Button("候補を再取得##RefreshProviderCandidates")) {
        CollectTargetableTargets(debugTargets_);
    }
    ImGui::End();
#endif
}
