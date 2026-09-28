#include "GltfSkinnedModel.h"
#include "Model.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

void GltfSkinnedModel::SetJointWarning(
    const std::vector<int>& jointIndices, float strength) {
    // CPU, compute shader and graphics input layout share this vertex format.
    static_assert(sizeof(Model::VertexData) == 52);
    static_assert(offsetof(Model::VertexData, jointWarning) == 48);
    if (!inputVerticesResource_) {
        return;
    }
    strength = std::isfinite(strength) ? std::clamp(strength, 0.0f, 1.0f) : 0.0f;
    Model::VertexData* vertices = nullptr;
    const D3D12_RANGE noRead{ 0, 0 };
    if (FAILED(inputVerticesResource_->Map(
        0, &noRead, reinterpret_cast<void**>(&vertices)))) {
        return;
    }
    // Updated before dispatch; the existing end-of-frame fence protects reuse.
    // Only the warning attribute is written: bind positions/normals/UVs stay intact.
    for (size_t vertexIndex = 0; vertexIndex < sourceVertices_.size(); ++vertexIndex) {
        const SourceVertex& source = sourceVertices_[vertexIndex];
        float selectedWeight = 0.0f;
        float totalWeight = 0.0f;
        if (strength > 0.0f) {
            for (size_t i = 0; i < source.weights.size(); ++i) {
                const float weight = source.weights[i];
                if (weight <= 0.000001f || !std::isfinite(weight)) {
                    continue;
                }
                totalWeight += weight;
                if (std::find(jointIndices.begin(), jointIndices.end(),
                    static_cast<int>(source.joints[i])) != jointIndices.end()) {
                    selectedWeight += weight;
                }
            }
        }
        vertices[vertexIndex].jointWarning = totalWeight > 0.000001f
            ? strength * selectedWeight / totalWeight : 0.0f;
    }
    inputVerticesResource_->Unmap(0, nullptr);
}
