#include "GltfSkinnedModelPrimitiveLoader.h"

#include <limits>
#include <string>
#include <utility>

namespace {
    constexpr std::uint32_t kTrianglePrimitiveMode = 4;
    constexpr std::uint32_t kUnsignedByteComponent = 5121;
    constexpr std::uint32_t kUnsignedShortComponent = 5123;
    constexpr std::uint32_t kUnsignedIntComponent = 5125;

    bool IsSupportedIndexComponent(std::uint32_t componentType) {
        return componentType == kUnsignedByteComponent ||
            componentType == kUnsignedShortComponent ||
            componentType == kUnsignedIntComponent;
    }

    bool IsRequiredAccessorSetValid(
        const GltfSkinnedPrimitiveAccessors& accessors,
        std::string& outAttributeName) {
        if (accessors.position < 0) {
            outAttributeName = "POSITION";
            return false;
        }
        if (accessors.normal < 0) {
            outAttributeName = "NORMAL";
            return false;
        }
        if (accessors.texcoord0 < 0) {
            outAttributeName = "TEXCOORD_0";
            return false;
        }
        if (accessors.joints0 < 0) {
            outAttributeName = "JOINTS_0";
            return false;
        }
        if (accessors.weights0 < 0) {
            outAttributeName = "WEIGHTS_0";
            return false;
        }
        return true;
    }

    bool IsOptionalAccessorSetValid(
        const GltfSkinnedPrimitiveAccessors& accessors,
        std::string& outAttributeName) {
        if (accessors.tangent < -1) {
            outAttributeName = "TANGENT";
            return false;
        }
        if (accessors.color0 < -1) {
            outAttributeName = "COLOR_0";
            return false;
        }
        if (accessors.texcoord1 < -1) {
            outAttributeName = "TEXCOORD_1";
            return false;
        }
        if (accessors.joints1 < -1) {
            outAttributeName = "JOINTS_1";
            return false;
        }
        if (accessors.weights1 < -1) {
            outAttributeName = "WEIGHTS_1";
            return false;
        }
        return true;
    }

    bool RequiredAccessorsMatch(
        const GltfSkinnedPrimitiveAccessors& expected,
        const GltfSkinnedPrimitiveAccessors& actual,
        std::string& outAttributeName) {
        if (actual.position != expected.position) {
            outAttributeName = "POSITION";
            return false;
        }
        if (actual.normal != expected.normal) {
            outAttributeName = "NORMAL";
            return false;
        }
        if (actual.texcoord0 != expected.texcoord0) {
            outAttributeName = "TEXCOORD_0";
            return false;
        }
        if (actual.joints0 != expected.joints0) {
            outAttributeName = "JOINTS_0";
            return false;
        }
        if (actual.weights0 != expected.weights0) {
            outAttributeName = "WEIGHTS_0";
            return false;
        }
        return true;
    }

    bool OptionalAccessorsMatch(
        const GltfSkinnedPrimitiveAccessors& expected,
        const GltfSkinnedPrimitiveAccessors& actual,
        std::string& outAttributeName) {
        if (actual.tangent != expected.tangent) {
            outAttributeName = "TANGENT";
            return false;
        }
        if (actual.color0 != expected.color0) {
            outAttributeName = "COLOR_0";
            return false;
        }
        if (actual.texcoord1 != expected.texcoord1) {
            outAttributeName = "TEXCOORD_1";
            return false;
        }
        if (actual.joints1 != expected.joints1) {
            outAttributeName = "JOINTS_1";
            return false;
        }
        if (actual.weights1 != expected.weights1) {
            outAttributeName = "WEIGHTS_1";
            return false;
        }
        return true;
    }

    GltfSkinnedPrimitiveDiagnosticEntry MakeDiagnosticEntry(
        const GltfSkinnedPrimitiveSource& source) {
        GltfSkinnedPrimitiveDiagnosticEntry entry{};
        entry.sourcePrimitiveIndex = source.sourcePrimitiveIndex;
        entry.accessors = source.accessors;
        entry.indicesAccessor = source.indicesAccessor;
        entry.materialIndex = source.materialIndex;
        entry.mode = source.mode;
        entry.indexComponentType = source.indexComponentType;
        return entry;
    }

    std::string PrimitivePrefix(const GltfSkinnedPrimitiveSource& source) {
        return "Primitive[" + std::to_string(source.sourcePrimitiveIndex) + "]: ";
    }

    bool FailBuild(
        GltfSkinnedPrimitiveState& state,
        const std::string& message,
        std::size_t diagnosticIndex = std::numeric_limits<std::size_t>::max()) {
        state.combinedIndices.clear();
        state.ranges.clear();
        state.diagnostics.totalIndexCount = 0;
        state.diagnostics.triangleCount = 0;
        state.diagnostics.rangeCount = 0;
        state.diagnostics.drawCallCount = 0;
        state.diagnostics.computeDispatchCount = 0;
        state.diagnostics.cpuSkinningUpdateCount = 0;
        state.diagnostics.usesCommonPreviewMaterial = false;
        state.diagnostics.multiPrimitiveSupported = false;
        state.diagnostics.errorMessage = message;
        state.diagnostics.loadSucceeded = false;
        state.diagnostics.invalidPrimitiveCount =
            state.diagnostics.sourcePrimitiveCount -
            state.diagnostics.validPrimitiveCount;

        if (diagnosticIndex < state.diagnostics.primitives.size()) {
            state.diagnostics.primitives[diagnosticIndex].errorMessage = message;
        }
        return false;
    }

    bool IsMaterialIndexValid(int materialIndex, std::size_t materialCount) {
        return materialIndex == -1 ||
            (materialIndex >= 0 &&
                static_cast<std::size_t>(materialIndex) < materialCount);
    }
}
bool ValidateGltfAccessorByteRange(
    std::size_t binarySize,
    std::size_t bufferViewByteOffset,
    std::size_t bufferViewByteLength,
    std::size_t accessorByteOffset,
    std::size_t accessorCount,
    std::size_t byteStride,
    std::size_t elementByteSize) {
    if (accessorCount == 0 ||
        elementByteSize == 0 ||
        byteStride < elementByteSize) {
        return false;
    }
    if (bufferViewByteOffset > binarySize ||
        bufferViewByteLength > binarySize - bufferViewByteOffset ||
        accessorByteOffset > bufferViewByteLength) {
        return false;
    }

    const std::size_t availableByteLength =
        bufferViewByteLength - accessorByteOffset;
    if (elementByteSize > availableByteLength) {
        return false;
    }

    return accessorCount - 1 <=
        (availableByteLength - elementByteSize) / byteStride;
}


bool BuildGltfSkinnedModelPrimitiveState(
    const GltfSkinnedPrimitiveBuildRequest& request,
    const GltfSkinnedPrimitiveIndexDecoder& decodeIndices,
    GltfSkinnedPrimitiveState& outState) {
    GltfSkinnedPrimitiveState state{};
    GltfSkinnedPrimitiveDiagnostics& diagnostics = state.diagnostics;
    diagnostics.sourcePath = request.sourcePath;
    diagnostics.meshName = request.meshName;
    diagnostics.sourceMeshCount = request.sourceMeshCount;
    diagnostics.sourceMeshIndex = request.sourceMeshIndex;
    diagnostics.sourcePrimitiveCount = request.primitives.size();
    diagnostics.vertexCount = request.vertexCount;
    diagnostics.commonPreviewMaterialIndex = request.commonPreviewMaterialIndex;
    diagnostics.multiMaterialSupported = false;
    diagnostics.multiMeshSupported = false;
    diagnostics.primitives.reserve(request.primitives.size());
    for (const GltfSkinnedPrimitiveSource& source : request.primitives) {
        diagnostics.primitives.push_back(MakeDiagnosticEntry(source));
    }

    if (request.sourceMeshCount == 0 ||
        request.sourceMeshIndex >= request.sourceMeshCount) {
        FailBuild(state, "参照先のMesh indexが範囲外です。");
        outState = std::move(state);
        return false;
    }
    if (request.sourceMeshCount != 1) {
        FailBuild(state, "複数Meshのスキニングプレビューには未対応です。");
        outState = std::move(state);
        return false;
    }
    if (request.primitives.empty()) {
        FailBuild(state, "MeshにPrimitiveがありません。");
        outState = std::move(state);
        return false;
    }
    if (request.vertexCount == 0) {
        FailBuild(state, "共有頂点ストリームの頂点数が0です。");
        outState = std::move(state);
        return false;
    }
    if (!decodeIndices) {
        FailBuild(state, "Index Decoderが設定されていません。");
        outState = std::move(state);
        return false;
    }
    if (!IsMaterialIndexValid(
        request.commonPreviewMaterialIndex,
        request.materialCount)) {
        FailBuild(state, "先頭PrimitiveのMaterial Indexが範囲外です。");
        outState = std::move(state);
        return false;
    }

    const GltfSkinnedPrimitiveAccessors& expectedAccessors =
        request.primitives.front().accessors;
    std::string attributeName;
    if (!IsRequiredAccessorSetValid(expectedAccessors, attributeName)) {
        diagnostics.requiredAttributeAccessorsMatch = false;
        FailBuild(
            state,
            PrimitivePrefix(request.primitives.front()) +
                "必須属性 " + attributeName + " のAccessorがありません。",
            0);
        outState = std::move(state);
        return false;
    }
    diagnostics.requiredAttributeAccessorsMatch = true;

    if (!IsOptionalAccessorSetValid(expectedAccessors, attributeName)) {
        diagnostics.optionalAttributeAccessorsMatch = false;
        FailBuild(
            state,
            PrimitivePrefix(request.primitives.front()) +
                "任意属性 " + attributeName + " のAccessor indexが不正です。",
            0);
        outState = std::move(state);
        return false;
    }
    diagnostics.optionalAttributeAccessorsMatch = true;

    state.combinedIndices.reserve(0);
    state.ranges.reserve(request.primitives.size());

    for (std::size_t primitivePosition = 0;
        primitivePosition < request.primitives.size();
        ++primitivePosition) {
        const GltfSkinnedPrimitiveSource& source =
            request.primitives[primitivePosition];
        GltfSkinnedPrimitiveDiagnosticEntry& entry =
            diagnostics.primitives[primitivePosition];
        const std::string prefix = PrimitivePrefix(source);

        if (!IsRequiredAccessorSetValid(source.accessors, attributeName)) {
            diagnostics.requiredAttributeAccessorsMatch = false;
            FailBuild(
                state,
                prefix + "必須属性 " + attributeName +
                    " のAccessorがありません。",
                primitivePosition);
            outState = std::move(state);
            return false;
        }
        if (!RequiredAccessorsMatch(
            expectedAccessors,
            source.accessors,
            attributeName)) {
            diagnostics.requiredAttributeAccessorsMatch = false;
            FailBuild(
                state,
                prefix + "必須属性 " + attributeName +
                    " のAccessorがPrimitive[0]と一致しません。",
                primitivePosition);
            outState = std::move(state);
            return false;
        }
        if (!IsOptionalAccessorSetValid(source.accessors, attributeName)) {
            diagnostics.optionalAttributeAccessorsMatch = false;
            FailBuild(
                state,
                prefix + "任意属性 " + attributeName +
                    " のAccessor indexが不正です。",
                primitivePosition);
            outState = std::move(state);
            return false;
        }
        if (!OptionalAccessorsMatch(
            expectedAccessors,
            source.accessors,
            attributeName)) {
            diagnostics.optionalAttributeAccessorsMatch = false;
            FailBuild(
                state,
                prefix + "任意属性 " + attributeName +
                    " のAccessorがPrimitive[0]と一致しません。",
                primitivePosition);
            outState = std::move(state);
            return false;
        }
        if (source.mode != kTrianglePrimitiveMode) {
            FailBuild(
                state,
                prefix + "modeはTRIANGLES(4)のみ対応しています。",
                primitivePosition);
            outState = std::move(state);
            return false;
        }
        if (source.indicesAccessor < 0) {
            FailBuild(
                state,
                prefix + "indices Accessorがありません。",
                primitivePosition);
            outState = std::move(state);
            return false;
        }
        if (!IsSupportedIndexComponent(source.indexComponentType)) {
            FailBuild(
                state,
                prefix +
                    "Index componentTypeは5121/5123/5125のみ対応しています。",
                primitivePosition);
            outState = std::move(state);
            return false;
        }
        if (!IsMaterialIndexValid(source.materialIndex, request.materialCount)) {
            FailBuild(
                state,
                prefix + "Material indexが範囲外です。",
                primitivePosition);
            outState = std::move(state);
            return false;
        }

        std::vector<std::uint32_t> decodedIndices;
        if (!decodeIndices(source, decodedIndices)) {
            FailBuild(
                state,
                prefix + "Index Accessorのデコードに失敗しました。",
                primitivePosition);
            outState = std::move(state);
            return false;
        }
        if (decodedIndices.empty()) {
            FailBuild(
                state,
                prefix + "Index Accessorが空です。",
                primitivePosition);
            outState = std::move(state);
            return false;
        }
        if ((decodedIndices.size() % 3) != 0) {
            FailBuild(
                state,
                prefix + "Index数が3の倍数ではありません。",
                primitivePosition);
            outState = std::move(state);
            return false;
        }

        constexpr std::size_t kMaximumRangeValue =
            static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max());
        if (decodedIndices.size() > kMaximumRangeValue ||
            state.combinedIndices.size() >
                kMaximumRangeValue - decodedIndices.size()) {
            FailBuild(
                state,
                prefix + "統合Index範囲がuint32_tを超えます。",
                primitivePosition);
            outState = std::move(state);
            return false;
        }

        std::uint32_t minimumIndex =
            std::numeric_limits<std::uint32_t>::max();
        std::uint32_t maximumIndex = 0;
        for (std::uint32_t index : decodedIndices) {
            if (static_cast<std::size_t>(index) >= request.vertexCount) {
                FailBuild(
                    state,
                    prefix + "頂点範囲外のIndex " +
                        std::to_string(index) + " があります。",
                    primitivePosition);
                outState = std::move(state);
                return false;
            }
            if (index < minimumIndex) {
                minimumIndex = index;
            }
            if (index > maximumIndex) {
                maximumIndex = index;
            }
        }

        SkinnedPrimitiveRange range{};
        range.sourcePrimitiveIndex = source.sourcePrimitiveIndex;
        range.firstIndex =
            static_cast<std::uint32_t>(state.combinedIndices.size());
        range.indexCount = static_cast<std::uint32_t>(decodedIndices.size());
        range.materialIndex = source.materialIndex;
        range.mode = source.mode;
        range.indexComponentType = source.indexComponentType;
        range.accessors = source.accessors;
        range.indicesAccessor = source.indicesAccessor;
        range.valid = true;

        entry.firstIndex = range.firstIndex;
        entry.indexCount = range.indexCount;
        entry.minimumIndex = minimumIndex;
        entry.maximumIndex = maximumIndex;
        entry.valid = true;

        state.combinedIndices.insert(
            state.combinedIndices.end(),
            decodedIndices.begin(),
            decodedIndices.end());
        state.ranges.push_back(range);
        ++diagnostics.validPrimitiveCount;
    }

    diagnostics.invalidPrimitiveCount = 0;
    diagnostics.totalIndexCount = state.combinedIndices.size();
    diagnostics.triangleCount = diagnostics.totalIndexCount / 3;
    diagnostics.rangeCount = state.ranges.size();
    diagnostics.drawCallCount = state.ranges.size();
    diagnostics.sharedVertexStream = true;
    diagnostics.requiredAttributeAccessorsMatch = true;
    diagnostics.optionalAttributeAccessorsMatch = true;
    diagnostics.computeDispatchCount = 1;
    diagnostics.cpuSkinningUpdateCount = 1;
    diagnostics.usesCommonPreviewMaterial = true;
    diagnostics.multiPrimitiveSupported = true;
    diagnostics.errorMessage.clear();
    diagnostics.loadSucceeded = true;

    outState = std::move(state);
    return true;
}
