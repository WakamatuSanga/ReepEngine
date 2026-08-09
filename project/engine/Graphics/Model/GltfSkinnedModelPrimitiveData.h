#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct GltfSkinnedPrimitiveAccessors {
    int position = -1;
    int normal = -1;
    int texcoord0 = -1;
    int joints0 = -1;
    int weights0 = -1;

    // Optional attributes also have to reference the same accessor as
    // Primitive 0.  -1 means that the attribute is absent.
    int tangent = -1;
    int color0 = -1;
    int texcoord1 = -1;
    int joints1 = -1;
    int weights1 = -1;
};

struct GltfSkinnedPrimitiveSource {
    std::uint32_t sourcePrimitiveIndex = 0;
    GltfSkinnedPrimitiveAccessors accessors{};
    int indicesAccessor = -1;
    int materialIndex = -1;
    std::uint32_t mode = 4;
    std::uint32_t indexComponentType = 0;
};

struct SkinnedPrimitiveRange {
    std::uint32_t sourcePrimitiveIndex = 0;
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    int materialIndex = -1;
    std::uint32_t mode = 4;
    std::uint32_t indexComponentType = 0;
    GltfSkinnedPrimitiveAccessors accessors{};
    int indicesAccessor = -1;
    bool valid = false;
};

struct GltfSkinnedPrimitiveDiagnosticEntry {
    std::uint32_t sourcePrimitiveIndex = 0;
    GltfSkinnedPrimitiveAccessors accessors{};
    int indicesAccessor = -1;
    int materialIndex = -1;
    std::uint32_t mode = 4;
    std::uint32_t indexComponentType = 0;
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    std::uint32_t minimumIndex = 0;
    std::uint32_t maximumIndex = 0;
    bool valid = false;
    std::string errorMessage;
};

struct GltfSkinnedPrimitiveDiagnostics {
    std::string sourcePath;
    std::string meshName;
    std::size_t sourceMeshCount = 0;
    std::size_t sourceMeshIndex = 0;
    std::size_t sourcePrimitiveCount = 0;
    std::size_t validPrimitiveCount = 0;
    std::size_t invalidPrimitiveCount = 0;
    std::size_t vertexCount = 0;
    std::size_t totalIndexCount = 0;
    std::size_t triangleCount = 0;
    std::size_t rangeCount = 0;
    std::size_t drawCallCount = 0;
    bool sharedVertexStream = false;
    bool requiredAttributeAccessorsMatch = false;
    bool optionalAttributeAccessorsMatch = false;

    // Skinning is shared by all ranges: one dispatch and one CPU update.
    std::size_t computeDispatchCount = 0;
    std::size_t cpuSkinningUpdateCount = 0;
    int commonPreviewMaterialIndex = -1;
    bool usesCommonPreviewMaterial = false;
    bool multiPrimitiveSupported = false;
    bool multiMaterialSupported = false;
    bool multiMeshSupported = false;

    std::string errorMessage;
    std::vector<GltfSkinnedPrimitiveDiagnosticEntry> primitives;
    bool loadSucceeded = false;
};

struct GltfSkinnedPrimitiveState {
    std::vector<std::uint32_t> combinedIndices;
    std::vector<SkinnedPrimitiveRange> ranges;
    GltfSkinnedPrimitiveDiagnostics diagnostics{};
};
