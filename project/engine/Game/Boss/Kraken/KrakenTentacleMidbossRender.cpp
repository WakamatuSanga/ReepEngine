#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"

#include "Engine/Core/DirectXCommon.h"
#include "Engine/Graphics/Model/GltfSkinnedModel.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"

#include <algorithm>
#include <cmath>

namespace {
    float WindupWarningStrength(KrakenTentacleMidbossState state, float elapsed, float duration) {
        if (!std::isfinite(elapsed) || !std::isfinite(duration) || duration <= 0.0f) {
            return 0.0f;
        }
        const float progress = std::clamp(elapsed / duration, 0.0f, 1.0f);
        if (state == KrakenTentacleMidbossState::Windup) {
            // Two smooth rises/falls, with visible red from the first frame.
            return 0.4f + 0.25f * (1.0f - std::cos(12.5663706f * progress));
        }
        if (state == KrakenTentacleMidbossState::WindupHold) {
            // Finish the final fade in the existing hold; reach normal color at Slam.
            return 0.4f * (1.0f - progress * progress * (3.0f - 2.0f * progress));
        }
        return 0.0f;
    }
}

void KrakenTentacleMidbossController::Impl::UpdateObjectTransform() {
    worldMatrix = MatrixMath::MakeAffine(
        worldScale, worldRotation, worldPosition);
    if (!object) {
        return;
    }
    object->SetScale(worldScale);
    object->SetRotate(worldRotation);
    object->SetTranslate({ worldPosition.x,
        worldPosition.y + entranceVisualOffsetY, worldPosition.z });
    object->SetCamera(camera);
    object->Update();
}

void KrakenTentacleMidbossController::Impl::Draw() {
    diagnostics.computeDispatchCount = 0;
    diagnostics.drawCallCount = 0;
    diagnostics.materialBindingCount = 0;
    if (!IsVisible() || !object3dCommon || !object || !model) {
        return;
    }
    DirectXCommon* directXCommon = object3dCommon->GetDxCommon();
    if (!directXCommon || !directXCommon->GetCommandList()) {
        EnterHidden(
            "描画Command Listが無効なため表示を停止しました。",
            true);
        return;
    }

    const float warning = !entranceActive && !defeatStarted &&
        wholeSlamDiagnostics.attackTargetSnapshotValid
        ? WindupWarningStrength(state, stateElapsedTime, GetCurrentStateDuration()) : 0.0f;
    if (warning > 0.0f && selectedAttackChainIndex < chains.size()) {
        model->SetJointWarning(chains[selectedAttackChainIndex].joints, warning);
    } else {
        model->SetJointWarning({}, 0.0f);
    }
    model->DispatchComputeSkinning(directXCommon->GetCommandList());
    diagnostics.computeDispatchCount = 1;
    object3dCommon->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    object->Draw();
    RefreshDrawDiagnostics();
}
