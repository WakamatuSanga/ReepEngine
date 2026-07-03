#include "Object3d.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Utility/Logger.h"
#include <string>

using namespace MatrixMath;

namespace {
    bool CreateAndMapObject3dBuffer(
        Object3dCommon* object3dCommon,
        size_t sizeInBytes,
        Microsoft::WRL::ComPtr<ID3D12Resource>& resource,
        void** mappedData,
        const char* label) {
        if (mappedData) {
            *mappedData = nullptr;
        }
        if (!object3dCommon) {
            Logger::Log(std::string("[Object3d] ") + label + " failed: object3dCommon_ is null");
            return false;
        }

        DirectXCommon* dxCommon = object3dCommon->GetDxCommon();
        if (!dxCommon) {
            Logger::Log(std::string("[Object3d] ") + label + " failed: dxCommon is null");
            return false;
        }

        resource = dxCommon->CreateBufferResource(sizeInBytes);
        if (!resource) {
            Logger::Log(std::string("[Object3d] ") + label + " failed: CreateBufferResource returned null");
            return false;
        }

        HRESULT hr = resource->Map(0, nullptr, mappedData);
        if (FAILED(hr) || !mappedData || !*mappedData) {
            Logger::Log(std::string("[Object3d] ") + label + " failed: Map failed hr=" + std::to_string(static_cast<long>(hr)));
            resource.Reset();
            if (mappedData) {
                *mappedData = nullptr;
            }
            return false;
        }
        return true;
    }
}

void Object3d::Initialize(Object3dCommon* object3dCommon) {
    initialized_ = false;
    object3dCommon_ = object3dCommon;

    if (!object3dCommon_) {
        Logger::Log("[Object3d] Initialize failed: object3dCommon is null");
        return;
    }
    if (!object3dCommon_->GetDxCommon()) {
        Logger::Log("[Object3d] Initialize failed: dxCommon is null");
        return;
    }

    transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

    initialized_ =
        CreateTransformationMatrixResource() &&
        CreateDirectionalLightResource() &&
        CreateEnvironmentMapResource() &&
        CreateDissolveResource() &&
        CreateRandomNoiseResource() &&
        CreateRingAppearanceResource();

    if (!initialized_) {
        Logger::Log("[Object3d] Initialize failed: one or more resources were not created");
    }
}

void Object3d::Update() {
    if (!initialized_ || !transformationMatrixData_) {
        return;
    }

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
    if (!initialized_ || !object3dCommon_ || !object3dCommon_->GetDxCommon()) {
        return;
    }
    if (!transformationMatrixResource_ || !directionalLightResource_ || !environmentMapResource_ ||
        !dissolveResource_ || !randomNoiseResource_ || !ringAppearanceResource_) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = object3dCommon_->GetDxCommon()->GetCommandList();
    if (!commandList) {
        return;
    }

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

bool Object3d::CreateTransformationMatrixResource() {
    if (!CreateAndMapObject3dBuffer(
        object3dCommon_,
        sizeof(TransformationMatrix),
        transformationMatrixResource_,
        reinterpret_cast<void**>(&transformationMatrixData_),
        "transformationMatrixResource_")) {
        return false;
    }
    transformationMatrixData_->WVP = MakeIdentity4x4();
    transformationMatrixData_->World = MakeIdentity4x4();
    transformationMatrixData_->WorldInverseTranspose = MakeIdentity4x4();
    return true;
}

bool Object3d::CreateDirectionalLightResource() {
    if (!CreateAndMapObject3dBuffer(
        object3dCommon_,
        sizeof(DirectionalLight),
        directionalLightResource_,
        reinterpret_cast<void**>(&directionalLightData_),
        "directionalLightResource_")) {
        return false;
    }
    directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    directionalLightData_->direction = { 0.0f, -1.0f, 0.0f };
    directionalLightData_->intensity = 1.0f;
    directionalLightData_->cameraPosition = { 0.0f, 0.0f, 0.0f };
    directionalLightData_->shininess = 50.0f;
    return true;
}

bool Object3d::CreateEnvironmentMapResource() {
    if (!CreateAndMapObject3dBuffer(
        object3dCommon_,
        sizeof(EnvironmentMapData),
        environmentMapResource_,
        reinterpret_cast<void**>(&environmentMapData_),
        "environmentMapResource_")) {
        return false;
    }
    environmentMapData_->enableEnvironmentMap = 0;
    environmentMapData_->intensity = 1.0f;
    environmentMapData_->padding[0] = 0.0f;
    environmentMapData_->padding[1] = 0.0f;
    return true;
}

void Object3d::SetDissolveMaskTexture(const std::string& path)
{
    TextureManager::GetInstance()->LoadTexture(path);
    dissolveMaskTextureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(path);
}

bool Object3d::CreateDissolveResource()
{
    if (!CreateAndMapObject3dBuffer(
        object3dCommon_,
        sizeof(DissolveData),
        dissolveResource_,
        reinterpret_cast<void**>(&dissolveData_),
        "dissolveResource_")) {
        return false;
    }
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
    return true;
}

bool Object3d::CreateRandomNoiseResource()
{
    if (!CreateAndMapObject3dBuffer(
        object3dCommon_,
        sizeof(RandomNoiseData),
        randomNoiseResource_,
        reinterpret_cast<void**>(&randomNoiseData_),
        "randomNoiseResource_")) {
        return false;
    }
    randomNoiseData_->enableRandom = 0;
    randomNoiseData_->previewRandom = 0;
    randomNoiseData_->intensity = 1.0f;
    randomNoiseData_->time = 0.0f;
    return true;
}

bool Object3d::CreateRingAppearanceResource()
{
    if (!CreateAndMapObject3dBuffer(
        object3dCommon_,
        sizeof(RingAppearanceData),
        ringAppearanceResource_,
        reinterpret_cast<void**>(&ringAppearanceData_),
        "ringAppearanceResource_")) {
        return false;
    }
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
    return true;
}