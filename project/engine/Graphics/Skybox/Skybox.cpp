#include "Skybox.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include <cassert>

using namespace MatrixMath;

void Skybox::Initialize(SkyboxCommon* skyboxCommon) {
    assert(skyboxCommon);
    skyboxCommon_ = skyboxCommon;

    CreateVertexResource();
    CreateTransformationMatrixResource();
}

void Skybox::SetTexture(const std::string& filePath) {
    TextureManager* texManager = TextureManager::GetInstance();
    texManager->LoadTexture(filePath);
    textureIndex_ = texManager->GetTextureIndexByFilePath(filePath);

    assert(texManager->GetMetaData(textureIndex_).IsCubemap());
}

void Skybox::Update() {
    if (camera_) {
        Matrix4x4 worldMatrix = MakeAffine(transform_.scale, transform_.rotate, transform_.translate);
        transformationMatrixData_->WVP = Multipty(worldMatrix, camera_->GetViewProjectionMatrix());
    } else {
        transformationMatrixData_->WVP = MakeIdentity4x4();
    }
}

void Skybox::Draw() {
    assert(skyboxCommon_);

    ID3D12GraphicsCommandList* commandList = skyboxCommon_->GetDxCommon()->GetCommandList();
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->SetGraphicsRootConstantBufferView(0, transformationMatrixResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(1, TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_));
    commandList->DrawInstanced(36, 1, 0, 0);
}

void Skybox::CreateVertexResource() {
    vertexResource_ = skyboxCommon_->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * 36);
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * 36;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

    const float s = 1.0f;
    const VertexData vertices[] = {
        {{-s,  s,  s, 1.0f}}, {{ s,  s,  s, 1.0f}}, {{-s, -s,  s, 1.0f}},
        {{-s, -s,  s, 1.0f}}, {{ s,  s,  s, 1.0f}}, {{ s, -s,  s, 1.0f}},

        {{ s,  s, -s, 1.0f}}, {{-s,  s, -s, 1.0f}}, {{ s, -s, -s, 1.0f}},
        {{ s, -s, -s, 1.0f}}, {{-s,  s, -s, 1.0f}}, {{-s, -s, -s, 1.0f}},

        {{-s,  s, -s, 1.0f}}, {{-s,  s,  s, 1.0f}}, {{-s, -s, -s, 1.0f}},
        {{-s, -s, -s, 1.0f}}, {{-s,  s,  s, 1.0f}}, {{-s, -s,  s, 1.0f}},

        {{ s,  s,  s, 1.0f}}, {{ s,  s, -s, 1.0f}}, {{ s, -s,  s, 1.0f}},
        {{ s, -s,  s, 1.0f}}, {{ s,  s, -s, 1.0f}}, {{ s, -s, -s, 1.0f}},

        {{-s,  s, -s, 1.0f}}, {{ s,  s, -s, 1.0f}}, {{-s,  s,  s, 1.0f}},
        {{-s,  s,  s, 1.0f}}, {{ s,  s, -s, 1.0f}}, {{ s,  s,  s, 1.0f}},

        {{-s, -s,  s, 1.0f}}, {{ s, -s,  s, 1.0f}}, {{-s, -s, -s, 1.0f}},
        {{-s, -s, -s, 1.0f}}, {{ s, -s,  s, 1.0f}}, {{ s, -s, -s, 1.0f}},
    };

    for (size_t i = 0; i < _countof(vertices); ++i) {
        vertexData_[i] = vertices[i];
    }
}

void Skybox::CreateTransformationMatrixResource() {
    transformationMatrixResource_ = skyboxCommon_->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));
    transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));
    transformationMatrixData_->WVP = MakeIdentity4x4();
}
