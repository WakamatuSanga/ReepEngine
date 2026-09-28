#include "GltfSkinnedModel.h"

#include "GltfSkinnedModelMaterialData.h"
#include "Model.h"

GltfSkinnedMaterialDiagnostics
GltfSkinnedModel::GetMaterialDiagnostics() const {
    if (!materialState_) {
        return {};
    }

    GltfSkinnedMaterialDiagnostics diagnostics =
        materialState_->diagnostics;
    if (model_ && diagnostics.loadSucceeded &&
        model_->HasLastIndexDrawMaterialBindingResult()) {
        diagnostics.materialBindingCount =
            model_->GetLastMaterialBindingCount();
        diagnostics.drawCallCount =
            diagnostics.materialBindingCount;
        diagnostics.baseColorTextureBindingCount =
            diagnostics.materialBindingCount;
        diagnostics.bindingFailureCount =
            model_->GetLastMaterialBindingFailureCount();
        for (std::size_t bindingIndex = 0;
            bindingIndex < diagnostics.bindings.size();
            ++bindingIndex) {
            GltfSkinnedMaterialBindingDiagnostic& binding =
                diagnostics.bindings[bindingIndex];
            binding.bindingSucceeded =
                model_->WasLastIndexDrawMaterialBindingSuccessful(
                    bindingIndex);
            binding.errorMessage = binding.bindingSucceeded
                ? std::string{}
                : "描画時のMaterial Bindingに失敗しました。";
        }
    }
    return diagnostics;
}
