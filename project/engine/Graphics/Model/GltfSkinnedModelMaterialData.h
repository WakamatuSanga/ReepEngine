#pragma once

#include "Matrix4x4.h"

#include <cstddef>
#include <cstdint>
#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl.h>

enum class GltfSkinnedAlphaMode : std::uint8_t {
    Opaque,
    Mask,
    Blend,
};

struct GltfSkinnedMaterialData {
    int sourceMaterialIndex = -1;
    std::string name;
    Vector4 baseColorFactor{ 1.0f, 1.0f, 1.0f, 1.0f };
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    Vector3 emissiveFactor{ 0.0f, 0.0f, 0.0f };
    int baseColorTextureIndex = -1;
    int baseColorImageIndex = -1;
    std::string baseColorTextureUri;
    std::string resolvedTexturePath;
    std::uint32_t textureHandle = 0;
    GltfSkinnedAlphaMode alphaMode = GltfSkinnedAlphaMode::Opaque;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
    bool textureResolved = false;
    bool textureHandleValid = false;
    bool usingFallbackTexture = false;
    bool usingWhiteFallbackTexture = false;
    bool usingUvCheckerFallbackTexture = false;
    bool isDefaultMaterial = false;
    std::size_t materialConstantBufferSlot = 0;
    std::uint64_t materialConstantBufferGpuAddress = 0;
    bool valid = false;
};

struct GltfSkinnedMaterialBindingDiagnostic {
    std::size_t drawCallIndex = 0;
    std::uint32_t sourcePrimitiveIndex = 0;
    int sourceMaterialIndex = -1;
    std::string materialName;
    std::string resolvedTexturePath;
    std::uint32_t textureHandle = 0;
    std::size_t materialConstantBufferSlot = 0;
    std::uint64_t materialConstantBufferGpuAddress = 0;
    bool usingFallbackTexture = false;
    bool bindingSucceeded = false;
    std::string errorMessage;
};

struct GltfSkinnedMaterialDiagnostics {
    std::string sourcePath;
    std::size_t sourceMaterialCount = 0;
    std::size_t loadedMaterialCount = 0;
    std::size_t validMaterialCount = 0;
    std::size_t invalidMaterialCount = 0;
    std::size_t sourceTextureCount = 0;
    std::size_t uniqueTextureResourceCount = 0;
    std::size_t fallbackTextureCount = 0;
    std::size_t materialConstantBufferCount = 0;
    std::size_t primitiveCount = 0;
    std::size_t drawCallCount = 0;
    std::size_t materialBindingCount = 0;
    std::size_t baseColorTextureBindingCount = 0;
    std::size_t bindingFailureCount = 0;
    bool hasShaderUnusedMaterialItems = false;
    bool multiPrimitiveSupported = false;
    bool multiMaterialSupported = false;
    bool multiMeshSupported = false;
    bool loadSucceeded = false;
    std::string errorMessage;
    std::vector<GltfSkinnedMaterialData> materials;
    std::vector<GltfSkinnedMaterialBindingDiagnostic> bindings;
};

struct GltfSkinnedMaterialRuntime {
    GltfSkinnedMaterialData data{};
    Microsoft::WRL::ComPtr<ID3D12Resource> ownedConstantBuffer;
    void* mappedMaterial = nullptr;
};

struct GltfSkinnedMaterialState {
    std::vector<GltfSkinnedMaterialRuntime> materials;
    int defaultMaterialRuntimeIndex = -1;
    GltfSkinnedMaterialDiagnostics diagnostics{};
};
