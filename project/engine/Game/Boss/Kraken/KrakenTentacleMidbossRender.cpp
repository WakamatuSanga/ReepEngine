#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossControllerInternal.h"

#include "Engine/Core/DirectXCommon.h"
#include "Engine/Graphics/Model/GltfSkinnedModel.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"

void KrakenTentacleMidbossController::Impl::UpdateObjectTransform() {
    worldMatrix = MatrixMath::MakeAffine(
        worldScale, worldRotation, worldPosition);
    if (!object) {
        return;
    }
    object->SetScale(worldScale);
    object->SetRotate(worldRotation);
    object->SetTranslate(worldPosition);
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

    model->DispatchComputeSkinning(directXCommon->GetCommandList());
    diagnostics.computeDispatchCount = 1;
    object3dCommon->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    object->Draw();
    RefreshDrawDiagnostics();
}
