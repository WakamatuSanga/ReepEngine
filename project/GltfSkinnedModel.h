#pragma once
#include "Matrix4x4.h"
#include <d3d12.h>
#include <wrl.h>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Model;
class ModelCommon;
class DirectXCommon;
struct Skeleton;

class GltfSkinnedModel {
public:
    struct Bounds {
        bool isValid = false;
        Vector3 min{ 0.0f, 0.0f, 0.0f };
        Vector3 max{ 0.0f, 0.0f, 0.0f };
        Vector3 size{ 0.0f, 0.0f, 0.0f };
        Vector3 center{ 0.0f, 0.0f, 0.0f };
    };

    GltfSkinnedModel() = default;
    ~GltfSkinnedModel();

    GltfSkinnedModel(const GltfSkinnedModel&) = delete;
    GltfSkinnedModel& operator=(const GltfSkinnedModel&) = delete;

    bool Initialize(ModelCommon* modelCommon, Skeleton* skeleton, const std::string& gltfPath);
    bool InitializeStatic(ModelCommon* modelCommon, const std::string& gltfPath);
    void UpdateSkinning();
    void DispatchComputeSkinning(ID3D12GraphicsCommandList* commandList);
    void SetUseComputeOutputVertices(bool enabled);

    Model* GetModel() const { return model_.get(); }
    bool IsValid() const { return model_ != nullptr; }
    bool IsUsingComputeOutputVertices() const { return useComputeOutputVertices_; }
    uint32_t GetVertexCount() const { return static_cast<uint32_t>(sourceVertices_.size()); }
    uint32_t GetInfluenceCount() const { return static_cast<uint32_t>(sourceVertices_.size()); }
    uint32_t GetPaletteCount() const { return static_cast<uint32_t>(jointPalette_.size()); }
    uint32_t GetDispatchThreadGroupCount() const { return (GetVertexCount() + 1023u) / 1024u; }
    bool HasComputeSkinningResources() const {
        return skinningComputeRootSignature_ &&
            skinningComputePipelineState_ &&
            skinningInformationResource_ &&
            inputVerticesResource_ &&
            vertexInfluenceResource_ &&
            matrixPaletteResource_ &&
            outputVerticesResource_;
    }
    const Bounds& GetSourceBounds() const { return sourceBounds_; }
    const Bounds& GetSkinnedBounds() const { return skinnedBounds_; }

private:
    struct SkinningInformation {
        uint32_t numVertices = 0;
        uint32_t padding[3]{};
    };

    struct SourceVertex {
        Vector3 position;
        Vector3 normal;
        Vector2 texcoord;
        std::array<uint32_t, 4> joints;
        std::array<float, 4> weights;
    };

    struct VertexInfluence {
        std::array<float, 4> weights{};
        std::array<uint32_t, 4> indices{};
    };

private:
    bool InitializeComputeSkinningPipeline(ModelCommon* modelCommon);
    bool CreateComputeRootSignature(DirectXCommon* dxCommon);
    bool CreateComputePipelineState(DirectXCommon* dxCommon);
    bool InitializeComputeSkinningResources(ModelCommon* modelCommon);

private:
    Skeleton* skeleton_ = nullptr;
    std::unique_ptr<Model> model_;
    std::vector<SourceVertex> sourceVertices_;
    std::vector<Matrix4x4> inverseBindMatrices_;
    std::vector<Matrix4x4> jointPalette_;
    Bounds sourceBounds_{};
    Bounds skinnedBounds_{};
    Microsoft::WRL::ComPtr<ID3D12RootSignature> skinningComputeRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> skinningComputePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> skinningInformationResource_;
    SkinningInformation* skinningInformationData_ = nullptr;
    uint32_t skinningInformationCBVIndex_ = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE skinningInformationCBVHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE skinningInformationCBVHandleGPU_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> inputVerticesResource_;
    uint32_t inputVerticesSRVIndex_ = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE inputVerticesSRVHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE inputVerticesSRVHandleGPU_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexInfluenceResource_;
    uint32_t vertexInfluenceSRVIndex_ = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE vertexInfluenceSRVHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE vertexInfluenceSRVHandleGPU_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> matrixPaletteResource_;
    Matrix4x4* matrixPaletteData_ = nullptr;
    uint32_t matrixPaletteSRVIndex_ = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE matrixPaletteSRVHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE matrixPaletteSRVHandleGPU_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> outputVerticesResource_;
    uint32_t outputVerticesUAVIndex_ = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE outputVerticesUAVHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE outputVerticesUAVHandleGPU_{};
    D3D12_VERTEX_BUFFER_VIEW outputVerticesVertexBufferView_{};
    D3D12_RESOURCE_STATES outputVerticesState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    bool useComputeOutputVertices_ = false;
};
