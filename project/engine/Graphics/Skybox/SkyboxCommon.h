#pragma once
#include "Engine/Core/DirectXCommon.h"
#include <wrl.h>

class SkyboxCommon {
public:
    void Initialize(DirectXCommon* dxCommon);
    void CommonDrawSetting();

    DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

private:
    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
