// SpriteCommon.h
#pragma once
#include <wrl.h>
#include <d3d12.h>
#include "Engine/Core/DirectXCommon.h"

class SpriteCommon {
public:
    void Initialize(DirectXCommon* dxCommon);
    DirectXCommon* GetDxCommon() const { return dxCommon_; }

    void CommonDrawSetting();

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

private:
    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
