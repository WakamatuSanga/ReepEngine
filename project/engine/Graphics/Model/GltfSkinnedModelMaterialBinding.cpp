#include "GltfSkinnedModelMaterialLoader.h"

#include "Engine/Core/DirectXCommon.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "ModelCommon.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {
    constexpr std::size_t AlignConstantBufferSize(
        std::size_t sizeInBytes) {
        return (sizeInBytes + 0xffu) & ~std::size_t{ 0xffu };
    }

    void WriteMaterial(
        const GltfSkinnedMaterialData& source,
        Model::Material& destination) {
        destination.color = source.baseColorFactor;
        destination.enableLighting = 1;
        destination.padding[0] = 0.0f;
        destination.padding[1] = 0.0f;
        destination.padding[2] = 0.0f;
        destination.uvTransform = MatrixMath::MakeIdentity4x4();
        destination.alphaReference =
            source.alphaMode == GltfSkinnedAlphaMode::Mask
            ? source.alphaCutoff
            : -1.0f;

        // Keep the existing skinned-preview branch for visual compatibility.
        destination.usePBR = 0;
        destination.metallicFactor = source.metallicFactor;
        destination.roughnessFactor = source.roughnessFactor;
        destination.normalScale = 1.0f;
        destination.hasNormalMap = 0;
        destination.hasMetallicRoughnessMap = 0;
        destination.hasSpecularF0Map = 0;
    }

    bool FailBinding(
        GltfSkinnedMaterialState& state,
        Model& model,
        const std::string& errorMessage) {
        model.ClearIndexDrawMaterialBindings();
        for (GltfSkinnedMaterialRuntime& material : state.materials) {
            material.ownedConstantBuffer.Reset();
            material.mappedMaterial = nullptr;
            material.data.materialConstantBufferGpuAddress = 0;
            material.data.valid = false;
        }
        GltfSkinnedMaterialDiagnostics& diagnostics = state.diagnostics;
        diagnostics.materialConstantBufferCount = 0;
        diagnostics.materialBindingCount = 0;
        diagnostics.baseColorTextureBindingCount = 0;
        diagnostics.drawCallCount = 0;
        diagnostics.uniqueTextureResourceCount = 0;
        diagnostics.bindings.clear();
        ++diagnostics.bindingFailureCount;
        diagnostics.loadSucceeded = false;
        diagnostics.errorMessage = errorMessage;
        for (GltfSkinnedMaterialData& material : diagnostics.materials) {
            material.materialConstantBufferGpuAddress = 0;
            material.textureHandle = 0;
            material.textureHandleValid = false;
            material.valid = false;
        }
        state.materials.clear();
        return false;
    }

    int ResolveRuntimeMaterialIndex(
        int sourceMaterialIndex,
        const GltfSkinnedMaterialState& state) {
        if (sourceMaterialIndex == -1) {
            return state.defaultMaterialRuntimeIndex;
        }
        if (sourceMaterialIndex < 0 ||
            static_cast<std::size_t>(sourceMaterialIndex) >=
                state.diagnostics.sourceMaterialCount) {
            return -1;
        }
        return sourceMaterialIndex;
    }
}

bool InitializeGltfSkinnedMaterialBindings(
    ModelCommon* modelCommon,
    Model& model,
    const std::vector<SkinnedPrimitiveRange>& primitiveRanges,
    GltfSkinnedMaterialState& state) {
    if (!modelCommon || !modelCommon->GetDxCommon()) {
        return FailBinding(
            state,
            model,
            "Material CB生成に必要な描画環境がありません。");
    }
    if (state.materials.empty()) {
        return FailBinding(
            state,
            model,
            "読込済みMaterialがありません。");
    }

    for (std::size_t materialIndex = 0;
        materialIndex < state.materials.size();
        ++materialIndex) {
        GltfSkinnedMaterialRuntime& runtime =
            state.materials[materialIndex];
        Model::Material* mappedMaterial = nullptr;

        if (materialIndex == 0) {
            mappedMaterial = model.GetMaterialData();
            runtime.data.materialConstantBufferGpuAddress =
                model.GetMaterialBufferAddress();
        } else {
            runtime.ownedConstantBuffer =
                modelCommon->GetDxCommon()->CreateBufferResource(
                    AlignConstantBufferSize(sizeof(Model::Material)));
            if (!runtime.ownedConstantBuffer) {
                return FailBinding(
                    state,
                    model,
                    "Material CBの生成に失敗しました。");
            }
            const HRESULT mapResult =
                runtime.ownedConstantBuffer->Map(
                    0,
                    nullptr,
                    reinterpret_cast<void**>(&mappedMaterial));
            if (FAILED(mapResult) || !mappedMaterial) {
                return FailBinding(
                    state,
                    model,
                    "Material CBのMapに失敗しました。");
            }
            runtime.data.materialConstantBufferGpuAddress =
                runtime.ownedConstantBuffer->GetGPUVirtualAddress();
        }

        if (!mappedMaterial ||
            runtime.data.materialConstantBufferGpuAddress == 0) {
            return FailBinding(
                state,
                model,
                "Material CBのGPU Addressを取得できませんでした。");
        }

        runtime.mappedMaterial = mappedMaterial;
        runtime.data.materialConstantBufferSlot = materialIndex;
        WriteMaterial(runtime.data, *mappedMaterial);
        state.diagnostics.materials[materialIndex] = runtime.data;
    }

    std::vector<Model::IndexDrawMaterialBinding> drawBindings;
    drawBindings.reserve(primitiveRanges.size());
    state.diagnostics.bindings.clear();
    state.diagnostics.bindings.reserve(primitiveRanges.size());

    for (std::size_t rangeIndex = 0;
        rangeIndex < primitiveRanges.size();
        ++rangeIndex) {
        const SkinnedPrimitiveRange& range = primitiveRanges[rangeIndex];
        const int runtimeIndex =
            ResolveRuntimeMaterialIndex(range.materialIndex, state);
        if (runtimeIndex < 0 ||
            static_cast<std::size_t>(runtimeIndex) >=
                state.materials.size()) {
            return FailBinding(
                state,
                model,
                "PrimitiveのMaterial Indexが範囲外です。");
        }

        GltfSkinnedMaterialRuntime& runtime =
            state.materials[static_cast<std::size_t>(runtimeIndex)];
        if (!runtime.data.valid ||
            !runtime.data.textureHandleValid ||
            !runtime.mappedMaterial ||
            runtime.data.materialConstantBufferGpuAddress == 0) {
            return FailBinding(
                state,
                model,
                "Primitiveへ割り当てるMaterialが無効です。");
        }

        Model::IndexDrawMaterialBinding binding{};
        binding.materialBufferAddress =
            runtime.data.materialConstantBufferGpuAddress;
        binding.mappedMaterial =
            static_cast<Model::Material*>(runtime.mappedMaterial);
        binding.baseColorTextureIndex = runtime.data.textureHandle;
        binding.normalTextureIndex = runtime.data.textureHandle;
        binding.metallicRoughnessTextureIndex = runtime.data.textureHandle;
        binding.specularF0TextureIndex = runtime.data.textureHandle;
        binding.usePBR = false;
        drawBindings.push_back(binding);

        GltfSkinnedMaterialBindingDiagnostic diagnostic{};
        diagnostic.drawCallIndex = rangeIndex;
        diagnostic.sourcePrimitiveIndex = range.sourcePrimitiveIndex;
        diagnostic.sourceMaterialIndex = range.materialIndex;
        diagnostic.materialName = runtime.data.name;
        diagnostic.resolvedTexturePath =
            runtime.data.resolvedTexturePath;
        diagnostic.textureHandle = runtime.data.textureHandle;
        diagnostic.materialConstantBufferSlot =
            runtime.data.materialConstantBufferSlot;
        diagnostic.materialConstantBufferGpuAddress =
            runtime.data.materialConstantBufferGpuAddress;
        diagnostic.usingFallbackTexture =
            runtime.data.usingFallbackTexture;
        diagnostic.bindingSucceeded = false;
        diagnostic.errorMessage = "描画待ちです。";
        state.diagnostics.bindings.push_back(std::move(diagnostic));
    }

    model.SetIndexDrawMaterialBindings(std::move(drawBindings));
    GltfSkinnedMaterialDiagnostics& diagnostics = state.diagnostics;
    diagnostics.materialConstantBufferCount = state.materials.size();
    diagnostics.primitiveCount = primitiveRanges.size();
    diagnostics.drawCallCount = primitiveRanges.size();
    diagnostics.materialBindingCount = primitiveRanges.size();
    diagnostics.baseColorTextureBindingCount = primitiveRanges.size();
    diagnostics.bindingFailureCount = 0;
    diagnostics.multiPrimitiveSupported = true;
    diagnostics.multiMaterialSupported = true;
    diagnostics.multiMeshSupported = false;
    diagnostics.loadSucceeded = true;
    diagnostics.errorMessage.clear();
    return true;
}

D3D12_GPU_VIRTUAL_ADDRESS Model::GetMaterialBufferAddress() const {
    return materialResource_
        ? materialResource_->GetGPUVirtualAddress()
        : 0;
}

void Model::SetIndexDrawMaterialBindings(
    std::vector<IndexDrawMaterialBinding> bindings) {
    indexDrawMaterialBindings_ = std::move(bindings);
    lastMaterialBindingSucceeded_.assign(
        indexDrawMaterialBindings_.size(),
        std::uint8_t{ 0 });
    lastMaterialBindingCount_ = 0;
    lastMaterialBindingFailureCount_ = 0;
    hasLastIndexDrawMaterialBindingResult_ = false;
}

void Model::ClearIndexDrawMaterialBindings() {
    indexDrawMaterialBindings_.clear();
    lastMaterialBindingSucceeded_.clear();
    lastMaterialBindingCount_ = 0;
    lastMaterialBindingFailureCount_ = 0;
    hasLastIndexDrawMaterialBindingResult_ = false;
}

bool Model::WasLastIndexDrawMaterialBindingSuccessful(
    size_t bindingIndex) const {
    return bindingIndex < lastMaterialBindingSucceeded_.size() &&
        lastMaterialBindingSucceeded_[bindingIndex] != 0;
}

bool Model::DrawIndexRangesWithMaterials(
    ID3D12GraphicsCommandList* commandList) const {
    if (indexDrawMaterialBindings_.empty()) {
        return false;
    }

    lastMaterialBindingCount_ = 0;
    lastMaterialBindingFailureCount_ = 0;
    hasLastIndexDrawMaterialBindingResult_ = true;
    std::fill(
        lastMaterialBindingSucceeded_.begin(),
        lastMaterialBindingSucceeded_.end(), std::uint8_t{ 0 });
    if (!commandList ||
        indexDrawMaterialBindings_.size() !=
            modelData_.indexDrawRanges.size()) {
        lastMaterialBindingFailureCount_ =
            (std::max)(indexDrawMaterialBindings_.size(),
                modelData_.indexDrawRanges.size());
        return true;
    }

    const IndexDrawMaterialBinding& firstBinding =
        indexDrawMaterialBindings_.front();
    TextureManager* textureManager = TextureManager::GetInstance();
    commandList->SetGraphicsRootDescriptorTable(
        10,
        textureManager->GetSrvHandleGPU(firstBinding.normalTextureIndex));
    commandList->SetGraphicsRootDescriptorTable(
        11,
        textureManager->GetSrvHandleGPU(
            firstBinding.metallicRoughnessTextureIndex));
    commandList->SetGraphicsRootDescriptorTable(
        12,
        textureManager->GetSrvHandleGPU(
            firstBinding.specularF0TextureIndex));

    for (std::size_t rangeIndex = 0;
        rangeIndex < modelData_.indexDrawRanges.size();
        ++rangeIndex) {
        const IndexDrawMaterialBinding& binding =
            indexDrawMaterialBindings_[rangeIndex];
        const IndexDrawRange& range =
            modelData_.indexDrawRanges[rangeIndex];
        if (!binding.mappedMaterial ||
            binding.materialBufferAddress == 0) {
            ++lastMaterialBindingFailureCount_;
            continue;
        }

        binding.mappedMaterial->usePBR =
            (binding.usePBR && !globalPbrLightingDisabled_) ? 1 : 0;
        commandList->SetGraphicsRootConstantBufferView(
            0,
            binding.materialBufferAddress);
        commandList->SetGraphicsRootDescriptorTable(
            2,
            textureManager->GetSrvHandleGPU(
                binding.baseColorTextureIndex));
        commandList->DrawIndexedInstanced(
            range.indexCount,
            1,
            range.firstIndex,
            range.baseVertexLocation,
            0);
        lastMaterialBindingSucceeded_[rangeIndex] = 1;
        ++lastMaterialBindingCount_;
    }
    return true;
}
