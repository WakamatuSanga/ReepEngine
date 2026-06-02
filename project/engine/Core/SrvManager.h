#pragma once
#include "DirectXCommon.h"
#include <memory>

class SrvManager {
    friend struct std::default_delete<SrvManager>;

public:
    static SrvManager* GetInstance();

    void Initialize(DirectXCommon* dxCommon);
    uint32_t Allocate();
    bool CanAllocate() const;
    void CreateCBV(uint32_t cbvIndex, ID3D12Resource* pResource, UINT sizeInBytes);
    void CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);
    void CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels);
    void CreateSRVforTextureCube(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels);
    void CreateUAVforStructuredBuffer(uint32_t uavIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);
    void PreDraw();

    ID3D12DescriptorHeap* GetSrvDescriptorHeap() const { return dxCommon_->GetSrvDescriptorHeap(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

private:
    SrvManager() = default;
    ~SrvManager() = default;
    SrvManager(const SrvManager&) = delete;
    SrvManager& operator=(const SrvManager&) = delete;

    static std::unique_ptr<SrvManager> instance_;
    static const uint32_t kMaxSrvCount;

    DirectXCommon* dxCommon_ = nullptr;
    uint32_t useIndex_ = 0;
};
