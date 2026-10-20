#pragma once
#include <dxgiformat.h>
#include <memory>
#include <wrl.h>

class DirectXCommon;
struct ID3D12Device;
struct ID3D12RootSignature;
struct ID3D12PipelineState;
struct IDxcBlob;

// One immutable cloud pipeline bundle per device. Only weak entries are cached.
// The complete recipe (entry/targets/build flags/root layout/format/blend/depth)
// is fixed here; these are not interchangeable with any other engine PSOs.
class VolumetricCloudPipelineResources {
public:
    static constexpr DXGI_FORMAT kColorFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    static std::shared_ptr<VolumetricCloudPipelineResources> Acquire(DirectXCommon& dxCommon);
    ~VolumetricCloudPipelineResources();
    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
    ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }
    ID3D12RootSignature* GetCompositeRootSignature() const { return compositeRootSignature_.Get(); }
    ID3D12PipelineState* GetCompositePipelineState() const { return compositePipelineState_.Get(); }
private:
    explicit VolumetricCloudPipelineResources(DirectXCommon& dxCommon);
    void CreateRootSignature(DirectXCommon& dxCommon);
    void CreatePipelineState(DirectXCommon& dxCommon, IDxcBlob* vertexShaderBlob, IDxcBlob* pixelShaderBlob);
    void CreateCompositeRootSignature(DirectXCommon& dxCommon);
    void CreateCompositePipelineState(DirectXCommon& dxCommon, IDxcBlob* vs, IDxcBlob* ps);
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> compositeRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> compositePipelineState_;
};
