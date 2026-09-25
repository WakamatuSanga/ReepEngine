#include "AimCorridorTargetingController.h"

#include "Engine/Game/Targeting/PlayerLockOnTargetProvider.h"

#include <algorithm>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"

namespace {
const char* BoolText(bool value) {
    return value ? "はい" : "いいえ";
}

const char* LockStateText(
    AimCorridorTargetingController::AimLockState state) {
    switch (state) {
    case AimCorridorTargetingController::AimLockState::Candidate:
        return "候補";
    case AimCorridorTargetingController::AimLockState::Acquiring:
        return "取得中";
    case AimCorridorTargetingController::AimLockState::Locked:
        return "ロック完了";
    case AimCorridorTargetingController::AimLockState::None:
    default:
        return "対象なし";
    }
}
} // namespace
#endif

void AimCorridorTargetingController::
DrawKrakenNaturalLockDiagnosticsImGui() const {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader(
            "クラーケン自然ロック診断##KrakenNaturalLockDiagnostics",
            ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    const std::size_t sourceCount = targetProvider_
        ? targetProvider_->GetKrakenSourceTargetCount() : 0;
    const std::size_t providerCount = targetProvider_
        ? targetProvider_->GetLastKrakenCandidateCount() : 0;
    const std::size_t normalCount = targetProvider_
        ? targetProvider_->GetLastNormalEnemyCandidateCount() : 0;
    ImGui::SeparatorText("経路集計");
    ImGui::Text("生成元候補数: %zu", sourceCount);
    ImGui::Text("プロバイダー追加数: %zu", providerCount);
    ImGui::Text("照準受信候補数: %zu", targetSnapshots_.size());
    ImGui::Text("照準受信クラーケン数: %zu",
        krakenNaturalLockDiagnostics_.size());
    ImGui::Text("照準受信通常敵数: %zu", normalCount);
    ImGui::Text("候補状態数: %d", candidateCount_);
    ImGui::Text("識別子重複拒否数: %zu",
        targetProvider_ ? targetProvider_->GetDuplicateIdCount() : 0);
    ImGui::Text("非有限位置拒否数: %zu",
        targetProvider_ ? targetProvider_->GetNonFinitePositionCount() : 0);

    ImGui::SeparatorText("4弱点要約");
    ImGui::Text("補助捕捉範囲画面UV X: %.4f ～ %.4f",
        softRect_.center.x - softRect_.halfSize.x,
        softRect_.center.x + softRect_.halfSize.x);
    ImGui::Text("補助捕捉範囲画面UV Y: %.4f ～ %.4f",
        softRect_.center.y - softRect_.halfSize.y,
        softRect_.center.y + softRect_.halfSize.y);
    if (!krakenNaturalLockDiagnostics_.empty()) {
        Vector2 minimum = krakenNaturalLockDiagnostics_.front().screenUv;
        Vector2 maximum = minimum;
        for (const KrakenNaturalLockTargetDiagnostic& diagnostic :
             krakenNaturalLockDiagnostics_) {
            minimum.x = (std::min)(minimum.x, diagnostic.screenUv.x);
            minimum.y = (std::min)(minimum.y, diagnostic.screenUv.y);
            maximum.x = (std::max)(maximum.x, diagnostic.screenUv.x);
            maximum.y = (std::max)(maximum.y, diagnostic.screenUv.y);
        }
        ImGui::Text("クラーケン画面境界UV: (%.4f, %.4f) ～ (%.4f, %.4f)",
            minimum.x, minimum.y, maximum.x, maximum.y);
    }
    for (const KrakenNaturalLockTargetDiagnostic& diagnostic :
         krakenNaturalLockDiagnostics_) {
        ImGui::Text(
            "チェーン %u: ワールド %.3f, %.3f, %.3f / 半径 %.3f",
            diagnostic.chainIndex,
            diagnostic.worldPosition.x,
            diagnostic.worldPosition.y,
            diagnostic.worldPosition.z,
            diagnostic.worldRadius);
        ImGui::Text(
            "  画面UV %.4f, %.4f / 距離 %.4f / 許可 %.4f",
            diagnostic.screenUv.x,
            diagnostic.screenUv.y,
            diagnostic.screenDistance,
            diagnostic.lockAllowedDistance);
        ImGui::Text(
            "  前方:%s 画面内:%s 捕捉範囲内:%s 評価値 %.4f / %s",
            BoolText(diagnostic.cameraFront),
            BoolText(diagnostic.viewportInside),
            BoolText(diagnostic.corridorInside),
            diagnostic.candidateScore,
            diagnostic.rejectionReason.c_str());
    }

    for (const KrakenNaturalLockTargetDiagnostic& diagnostic :
         krakenNaturalLockDiagnostics_) {
        const std::string label = "チェーン " +
            std::to_string(diagnostic.chainIndex) +
            "##KrakenNaturalLockTarget" +
            std::to_string(diagnostic.chainIndex);
        if (!ImGui::TreeNode(label.c_str())) {
            continue;
        }

        ImGui::SeparatorText("生成元／プロバイダー");
        ImGui::Text("対象識別子: %s", diagnostic.targetId.c_str());
        ImGui::Text("チェーン番号: %u", diagnostic.chainIndex);
        ImGui::Text("弱点コリダー識別子: %llu",
            static_cast<unsigned long long>(diagnostic.sourceColliderId));
        ImGui::Text("生成元から受信: %s",
            BoolText(diagnostic.sourceReceived));
        ImGui::Text("プロバイダーへ追加: %s",
            BoolText(diagnostic.providerAdded));
        ImGui::Text("最終候補番号: %zu",
            diagnostic.providerCandidateIndex);
        ImGui::Text("ワールド位置: %.3f, %.3f, %.3f",
            diagnostic.worldPosition.x,
            diagnostic.worldPosition.y,
            diagnostic.worldPosition.z);
        ImGui::Text("ワールド半径: %.3f", diagnostic.worldRadius);
        ImGui::Text("有効: %s", BoolText(diagnostic.valid));
        ImGui::Text("生存中: %s", BoolText(diagnostic.alive));
        ImGui::Text("選択可能: %s", BoolText(diagnostic.targetable));
        ImGui::Text("生成元無効理由: %s",
            diagnostic.valid ? "なし" : "プロバイダー受信前または無効");

        ImGui::SeparatorText("カメラ前方・投影");
        ImGui::Text("カメラ位置: %.3f, %.3f, %.3f",
            diagnostic.cameraPosition.x,
            diagnostic.cameraPosition.y,
            diagnostic.cameraPosition.z);
        ImGui::Text("カメラ前方: %.3f, %.3f, %.3f",
            diagnostic.cameraForward.x,
            diagnostic.cameraForward.y,
            diagnostic.cameraForward.z);
        ImGui::Text("カメラ空間位置: %.3f, %.3f, %.3f",
            diagnostic.cameraSpacePosition.x,
            diagnostic.cameraSpacePosition.y,
            diagnostic.cameraSpacePosition.z);
        ImGui::Text("ビュー空間Z: %.3f", diagnostic.viewDepth);
        ImGui::Text("クリップ位置: %.3f, %.3f, %.3f, %.3f",
            diagnostic.clipPosition.x,
            diagnostic.clipPosition.y,
            diagnostic.clipPosition.z,
            diagnostic.clipW);
        ImGui::Text("正規化デバイス座標: %.4f, %.4f",
            diagnostic.ndcPosition.x, diagnostic.ndcPosition.y);
        ImGui::Text("画面UV: %.4f, %.4f",
            diagnostic.screenUv.x, diagnostic.screenUv.y);
        ImGui::Text("画面半径UV: %.4f, %.4f",
            diagnostic.screenRadius.x, diagnostic.screenRadius.y);
        ImGui::Text("カメラ前方: %s",
            BoolText(diagnostic.cameraFront));
        ImGui::Text("ビューポート内: %s",
            BoolText(diagnostic.viewportInside));
        ImGui::Text("投影成功: %s",
            BoolText(diagnostic.projectionValid));
        ImGui::Text("投影失敗理由: %s",
            diagnostic.projectionFailureReason.c_str());

        ImGui::SeparatorText("ロック範囲");
        ImGui::Text("補助捕捉範囲中心UV: %.4f, %.4f",
            softRect_.center.x, softRect_.center.y);
        ImGui::Text("対象画面距離: %.4f",
            diagnostic.screenDistance);
        ImGui::Text("ロック許可対角距離: %.4f",
            diagnostic.lockAllowedDistance);
        ImGui::Text("補助捕捉範囲内: %s",
            BoolText(diagnostic.corridorInside));
        ImGui::Text("候補評価値: %.4f",
            diagnostic.candidateScore);
        ImGui::Text("候補採用: %s",
            BoolText(diagnostic.candidateSelected));
        ImGui::Text("拒否理由: %s",
            diagnostic.rejectionReason.c_str());
        ImGui::TreePop();
    }

    ImGui::SeparatorText("状態遷移");
    ImGui::Text("現在状態: %s", LockStateText(lockState_));
    ImGui::Text("候補対象識別子: %s",
        candidateTargetId_.empty() ? "なし" : candidateTargetId_.c_str());
    ImGui::Text("取得中対象識別子: %s",
        lockState_ == AimLockState::Acquiring
            ? candidateTargetId_.c_str() : "なし");
    ImGui::Text("ロック済み対象識別子: %s",
        lockedTargetId_.empty() ? "なし" : lockedTargetId_.c_str());
    ImGui::Text("取得経過時間: %.3f 秒", lockElapsed_);
    ImGui::Text("取得必要時間: %.3f 秒", lockAcquireTime_);
    ImGui::Text("取得リセット回数: %u", krakenAcquireResetCount_);
    ImGui::Text("候補切替回数: %u", krakenCandidateSwitchCount_);
    ImGui::Text("ロック直後解除回数: %u", krakenImmediateUnlockCount_);
    ImGui::TextWrapped("最後のロック解除理由: %s",
        lastKrakenUnlockReason_.c_str());
    ImGui::Text("ロック済み再検証: プロバイダー収集後の同一識別子");
#endif
}
