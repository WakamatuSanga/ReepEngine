#include "SrvManager.h"
#include <cassert>

std::unique_ptr<SrvManager> SrvManager::instance_ = nullptr;
const uint32_t SrvManager::kMaxSrvCount = 512;

SrvManager* SrvManager::GetInstance() {
    if (!instance_) {
        instance_.reset(new SrvManager());
    }
    return instance_.get();
}

void SrvManager::Initialize(DirectXCommon* dxCommon) {
    assert(dxCommon);
    dxCommon_ = dxCommon;
    useIndex_ = 1;
}

uint32_t SrvManager::Allocate() {
    assert(useIndex_ < kMaxSrvCount);
    uint32_t index = useIndex_;
    useIndex_++;
    return index;
}

bool SrvManager::CanAllocate() const {
    return useIndex_ < kMaxSrvCount;
}

void SrvManager::CreateCBV(uint32_t cbvIndex, ID3D12Resource* pResource, UINT sizeInBytes) {
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
    cbvDesc.BufferLocation = pResource->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = sizeInBytes;

    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = dxCommon_->GetSRVCPUDescriptorHandle(cbvIndex);
    dxCommon_->GetDevice()->CreateConstantBufferView(&cbvDesc, handleCPU);
}

void SrvManager::CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = numElements;
    srvDesc.Buffer.StructureByteStride = structureByteStride;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = dxCommon_->GetSRVCPUDescriptorHandle(srvIndex);
    dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, handleCPU);
}

void SrvManager::CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = mipLevels;

    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = dxCommon_->GetSRVCPUDescriptorHandle(srvIndex);
    dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, handleCPU);
}

void SrvManager::CreateSRVforTextureCube(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MipLevels = mipLevels;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = dxCommon_->GetSRVCPUDescriptorHandle(srvIndex);
    dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, handleCPU);
}

void SrvManager::CreateUAVforStructuredBuffer(uint32_t uavIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride) {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = numElements;
    uavDesc.Buffer.StructureByteStride = structureByteStride;
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = dxCommon_->GetSRVCPUDescriptorHandle(uavIndex);
    dxCommon_->GetDevice()->CreateUnorderedAccessView(pResource, nullptr, &uavDesc, handleCPU);
}

void SrvManager::PreDraw() {
    ID3D12DescriptorHeap* descriptorHeaps[] = { dxCommon_->GetSrvDescriptorHeap() };
    dxCommon_->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);
}

D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index) {
    return dxCommon_->GetSRVCPUDescriptorHandle(index);
}

D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index) {
    return dxCommon_->GetSRVGPUDescriptorHandle(index);
}
