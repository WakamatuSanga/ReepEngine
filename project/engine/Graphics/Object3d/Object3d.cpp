#include "Object3d.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include <cassert>

using namespace MatrixMath;

void Object3d::Initialize(Object3dCommon* object3dCommon) {
    assert(object3dCommon);
    object3dCommon_ = object3dCommon;

    transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

    CreateTransformationMatrixResource();
    CreateDirectionalLightResource();
    CreateEnvironmentMapResource();
    CreateDissolveResource();
    CreateRandomNoiseResource();
    CreateRingAppearanceResource();
}

void Object3d::Update() {
    Matrix4x4 worldMatrix = MakeAffine(transform_.scale, transform_.rotate, transform_.translate);

    // 法線変換用の逆転置行列
    Matrix4x4 worldInverse = Inverse(worldMatrix);
    Matrix4x4 worldInverseTranspose = Transpoce(worldInverse);

    if (camera_) {
        const Matrix4x4& viewProjection = camera_->GetViewProjectionMatrix();
        transformationMatrixData_->WVP = Multipty(worldMatrix, viewProjection);

        if (directionalLightData_) {
            directionalLightData_->cameraPosition = camera_->GetTranslate();
        }
    } else {
        transformationMatrixData_->WVP = MakeIdentity4x4();
    }

    transformationMatrixData_->World = worldMatrix;
    transformationMatrixData_->WorldInverseTranspose = worldInverseTranspose;
}

void Object3d::Draw() {
    ID3D12GraphicsCommandList* commandList = object3dCommon_->GetDxCommon()->GetCommandList();

    commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(4, environmentMapResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(6, dissolveResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(8, randomNoiseResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(9, ringAppearanceResource_->GetGPUVirtualAddress());

    if (environmentTextureIndex_ != static_cast<uint32_t>(-1)) {
        commandList->SetGraphicsRootDescriptorTable(5, TextureManager::GetInstance()->GetSrvHandleGPU(environmentTextureIndex_));
    }
    if (dissolveMaskTextureIndex_ != static_cast<uint32_t>(-1)) {
        commandList->SetGraphicsRootDescriptorTable(7, TextureManager::GetInstance()->GetSrvHandleGPU(dissolveMaskTextureIndex_));
    }

    if (model_) {
        model_->Draw();
    }
}

void Object3d::CreateTransformationMatrixResource() {
    transformationMatrixResource_ = object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));
    transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));
    transformationMatrixData_->WVP = MakeIdentity4x4();
    transformationMatrixData_->World = MakeIdentity4x4();
    transformationMatrixData_->WorldInverseTranspose = MakeIdentity4x4();
}

void Object3d::CreateDirectionalLightResource() {
    directionalLightResource_ = object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(DirectionalLight));
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));
    directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    directionalLightData_->direction = { 0.0f, -1.0f, 0.0f };
    directionalLightData_->intensity = 1.0f;
    directionalLightData_->cameraPosition = { 0.0f, 0.0f, 0.0f };
    directionalLightData_->shininess = 50.0f;
}

void Object3d::CreateEnvironmentMapResource() {
    environmentMapResource_ = object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(EnvironmentMapData));
    environmentMapResource_->Map(0, nullptr, reinterpret_cast<void**>(&environmentMapData_));
    environmentMapData_->enableEnvironmentMap = 0;
    environmentMapData_->intensity = 1.0f;
    environmentMapData_->padding[0] = 0.0f;
    environmentMapData_->padding[1] = 0.0f;
}

void Object3d::SetDissolveMaskTexture(const std::string& path)
{
    TextureManager::GetInstance()->LoadTexture(path);
    dissolveMaskTextureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(path);
}

void Object3d::CreateDissolveResource()
{
    dissolveResource_ = object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(DissolveData));
    dissolveResource_->Map(0, nullptr, reinterpret_cast<void**>(&dissolveData_));
    dissolveData_->enableDissolve = 0;
    dissolveData_->threshold = 0.0f;
    dissolveData_->edgeWidth = 0.05f;
    dissolveData_->edgeGlowStrength = 0.5f;
    dissolveData_->edgeNoiseStrength = 0.25f;
    dissolveData_->padding[0] = 0.0f;
    dissolveData_->padding[1] = 0.0f;
    dissolveData_->padding[2] = 0.0f;
    dissolveData_->edgeColor = { 1.0f, 0.5f, 0.1f, 1.0f };
    SetDissolveMaskTexture("resources/postEffect/noise0.png");
}

void Object3d::CreateRandomNoiseResource()
{
    randomNoiseResource_ = object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(RandomNoiseData));
    randomNoiseResource_->Map(0, nullptr, reinterpret_cast<void**>(&randomNoiseData_));
    randomNoiseData_->enableRandom = 0;
    randomNoiseData_->previewRandom = 0;
    randomNoiseData_->intensity = 1.0f;
    randomNoiseData_->time = 0.0f;
}

void Object3d::CreateRingAppearanceResource()
{
    ringAppearanceResource_ = object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(RingAppearanceData));
    ringAppearanceResource_->Map(0, nullptr, reinterpret_cast<void**>(&ringAppearanceData_));
    ringAppearanceData_->enableRingAppearance = 0;
    ringAppearanceData_->uvDirection = 0;
    ringAppearanceData_->innerRadiusRatio = 0.45f;
    ringAppearanceData_->startAlpha = 1.0f;
    ringAppearanceData_->endAlpha = 1.0f;
    ringAppearanceData_->startFadeRange = 0.15f;
    ringAppearanceData_->endFadeRange = 0.15f;
    ringAppearanceData_->padding = 0.0f;
    ringAppearanceData_->innerColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    ringAppearanceData_->outerColor = { 1.0f, 0.6f, 0.2f, 1.0f };
}
