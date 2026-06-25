#pragma once

#include "Engine/math/Matrix4x4.h"

#include <cstdint>
#include <d3d12.h>
#include <vector>
#include <wrl.h>

class Camera;
class DirectXCommon;

class PlayerJetExhaustBeamRenderer {
public:
    struct Vertex {
        Vector3 position;
        Vector2 uv;
    };

    bool Initialize(DirectXCommon* dxCommon);
    void Draw(const std::vector<Vertex>& vertices, const Camera* camera, float brightness, float flickerStrength, float time, uint32_t mode);

private:
    struct Constants {
        Matrix4x4 viewProjection;
        Vector4 params;
    };

    bool CreateRootSignature();
    bool CreatePipelineState();
    bool EnsureVertexCapacity(size_t vertexCount);

    DirectXCommon* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantResource_;
    Vertex* vertexData_ = nullptr;
    Constants* constantData_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    size_t vertexCapacity_ = 0;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
