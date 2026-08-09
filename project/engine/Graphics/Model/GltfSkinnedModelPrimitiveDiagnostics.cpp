#include "GltfSkinnedModel.h"

#include "GltfSkinnedModelMaterialData.h"
#include "GltfSkinnedModelPrimitiveData.h"
#include "Model.h"

#include <memory>
#include <utility>

const GltfSkinnedPrimitiveDiagnostics&
GltfSkinnedModel::GetPrimitiveDiagnostics() const {
    static const GltfSkinnedPrimitiveDiagnostics emptyDiagnostics{};
    return primitiveState_
        ? primitiveState_->diagnostics
        : emptyDiagnostics;
}

void GltfSkinnedModel::ResetLoadedState() {
    model_.reset();
    skeleton_ = nullptr;
    sourceVertices_.clear();
    inverseBindMatrices_.clear();
    jointPalette_.clear();
    sourceBounds_ = {};
    skinnedBounds_ = {};
    textureDebugInfo_ = {};

    skinningComputeRootSignature_.Reset();
    skinningComputePipelineState_.Reset();
    skinningInformationResource_.Reset();
    skinningInformationData_ = nullptr;
    skinningInformationCBVIndex_ = 0;
    skinningInformationCBVHandleCPU_ = {};
    skinningInformationCBVHandleGPU_ = {};

    inputVerticesResource_.Reset();
    inputVerticesSRVIndex_ = 0;
    inputVerticesSRVHandleCPU_ = {};
    inputVerticesSRVHandleGPU_ = {};
    vertexInfluenceResource_.Reset();
    vertexInfluenceSRVIndex_ = 0;
    vertexInfluenceSRVHandleCPU_ = {};
    vertexInfluenceSRVHandleGPU_ = {};
    matrixPaletteResource_.Reset();
    matrixPaletteData_ = nullptr;
    matrixPaletteSRVIndex_ = 0;
    matrixPaletteSRVHandleCPU_ = {};
    matrixPaletteSRVHandleGPU_ = {};
    outputVerticesResource_.Reset();
    outputVerticesUAVIndex_ = 0;
    outputVerticesUAVHandleCPU_ = {};
    outputVerticesUAVHandleGPU_ = {};
    outputVerticesVertexBufferView_ = {};
    outputVerticesState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    useComputeOutputVertices_ = false;

    primitiveState_ = std::make_unique<GltfSkinnedPrimitiveState>();
    materialState_ = std::make_unique<GltfSkinnedMaterialState>();
}

bool GltfSkinnedModel::FailPrimitiveLoad(
    const std::string& errorMessage) {
    std::unique_ptr<GltfSkinnedPrimitiveState> failedState =
        std::move(primitiveState_);
    std::unique_ptr<GltfSkinnedMaterialState> failedMaterialState =
        std::move(materialState_);
    ResetLoadedState();
    if (failedState) {
        primitiveState_ = std::move(failedState);
    }

    GltfSkinnedPrimitiveState& state = *primitiveState_;
    state.combinedIndices.clear();
    state.ranges.clear();
    GltfSkinnedPrimitiveDiagnostics& diagnostics = state.diagnostics;
    diagnostics.validPrimitiveCount = 0;
    diagnostics.invalidPrimitiveCount = diagnostics.sourcePrimitiveCount;
    diagnostics.totalIndexCount = 0;
    diagnostics.triangleCount = 0;
    diagnostics.rangeCount = 0;
    diagnostics.drawCallCount = 0;
    diagnostics.computeDispatchCount = 0;
    diagnostics.cpuSkinningUpdateCount = 0;
    diagnostics.usesCommonPreviewMaterial = false;
    diagnostics.loadSucceeded = false;
    diagnostics.errorMessage = errorMessage;
    for (GltfSkinnedPrimitiveDiagnosticEntry& primitive :
        diagnostics.primitives) {
        primitive.valid = false;
    }

    if (failedMaterialState) {
        materialState_->diagnostics =
            std::move(failedMaterialState->diagnostics);
    }
    GltfSkinnedMaterialDiagnostics& materialDiagnostics =
        materialState_->diagnostics;
    materialState_->materials.clear();
    materialState_->defaultMaterialRuntimeIndex = -1;
    materialDiagnostics.validMaterialCount = 0;
    materialDiagnostics.invalidMaterialCount =
        materialDiagnostics.loadedMaterialCount > 0
        ? materialDiagnostics.loadedMaterialCount
        : materialDiagnostics.sourceMaterialCount;
    materialDiagnostics.materialConstantBufferCount = 0;
    materialDiagnostics.drawCallCount = 0;
    materialDiagnostics.materialBindingCount = 0;
    materialDiagnostics.baseColorTextureBindingCount = 0;
    materialDiagnostics.bindings.clear();
    materialDiagnostics.loadSucceeded = false;
    if (materialDiagnostics.errorMessage.empty()) {
        materialDiagnostics.errorMessage = errorMessage;
    }
    for (GltfSkinnedMaterialData& material :
        materialDiagnostics.materials) {
        material.materialConstantBufferGpuAddress = 0;
        material.textureHandle = 0;
        material.textureHandleValid = false;
        material.valid = false;
    }
    return false;
}
