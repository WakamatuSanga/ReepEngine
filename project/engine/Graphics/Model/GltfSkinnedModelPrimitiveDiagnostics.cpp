#include "GltfSkinnedModel.h"

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
}

bool GltfSkinnedModel::FailPrimitiveLoad(
    const std::string& errorMessage) {
    std::unique_ptr<GltfSkinnedPrimitiveState> failedState =
        std::move(primitiveState_);
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
    return false;
}
