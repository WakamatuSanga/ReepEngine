#pragma once
#include "SkyboxCommon.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Matrix4x4.h"
#include <wrl.h>
#include <d3d12.h>
#include <string>

class Skybox {
public:
    struct VertexData {
        Vector4 position;
    };

    struct TransformationMatrix {
        Matrix4x4 WVP;
    };

public:
    void Initialize(SkyboxCommon* skyboxCommon);
    void Update();
    void Draw();

    void SetCamera(Camera* camera) { camera_ = camera; }
    void SetTexture(const std::string& filePath);
    void SetTextureIndex(uint32_t textureIndex) { textureIndex_ = textureIndex; }
    void SetScale(const Vector3& scale) { transform_.scale = scale; }
    void SetTranslate(const Vector3& translate) { transform_.translate = translate; }

private:
    void CreateVertexResource();
    void CreateTransformationMatrixResource();

private:
    SkyboxCommon* skyboxCommon_ = nullptr;
    Camera* camera_ = nullptr;

    Transform transform_{ {100.0f, 100.0f, 100.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
    uint32_t textureIndex_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    VertexData* vertexData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
    TransformationMatrix* transformationMatrixData_ = nullptr;
};
