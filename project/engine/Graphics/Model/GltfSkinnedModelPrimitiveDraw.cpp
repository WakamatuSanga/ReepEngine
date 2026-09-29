#include "Model.h"
#include "Engine/Core/FrameTimer.h"

void Model::DrawIndexRanges(
    ID3D12GraphicsCommandList* commandList) const {
    if (!commandList) {
        return;
    }

    if (modelData_.indexDrawRanges.empty()) {
        FrameTimer::GetInstance().CountBulletDraw();
        commandList->DrawIndexedInstanced(
            static_cast<UINT>(modelData_.indices.size()),
            1,
            0,
            0,
            0);
        return;
    }

    for (const IndexDrawRange& range : modelData_.indexDrawRanges) {
        FrameTimer::GetInstance().CountBulletDraw();
        commandList->DrawIndexedInstanced(
            range.indexCount,
            1,
            range.firstIndex,
            range.baseVertexLocation,
            0);
    }
}
