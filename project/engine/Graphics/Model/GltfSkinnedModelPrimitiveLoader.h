#pragma once

#include "GltfSkinnedModelPrimitiveData.h"

#include <functional>
#include <string>
#include <vector>

struct GltfSkinnedPrimitiveBuildRequest {
    std::string sourcePath;
    std::string meshName;
    std::size_t sourceMeshCount = 0;
    std::size_t sourceMeshIndex = 0;
    std::size_t vertexCount = 0;
    std::size_t materialCount = 0;
    int commonPreviewMaterialIndex = -1;
    std::vector<GltfSkinnedPrimitiveSource> primitives;
};

using GltfSkinnedPrimitiveIndexDecoder = std::function<bool(
    const GltfSkinnedPrimitiveSource& source,
    std::vector<std::uint32_t>& decodedIndices)>;
bool ValidateGltfAccessorByteRange(
    std::size_t binarySize,
    std::size_t bufferViewByteOffset,
    std::size_t bufferViewByteLength,
    std::size_t accessorByteOffset,
    std::size_t accessorCount,
    std::size_t byteStride,
    std::size_t elementByteSize);


bool BuildGltfSkinnedModelPrimitiveState(
    const GltfSkinnedPrimitiveBuildRequest& request,
    const GltfSkinnedPrimitiveIndexDecoder& decodeIndices,
    GltfSkinnedPrimitiveState& outState);
