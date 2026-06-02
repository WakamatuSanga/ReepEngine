#pragma once
#include "Object3dCommon.h"
#include "Engine/Graphics/Model/Model.h"
#include "Matrix4x4.h"
#include "Engine/Graphics/Camera/Camera.h"
#include <wrl.h>
#include <d3d12.h>
#include <cstdint>
#include <string>

class Object3d {
    struct EnvironmentMapData {
        int32_t enableEnvironmentMap;
        float intensity;
        float padding[2];
    };

    struct DissolveData {
        int32_t enableDissolve;
        float threshold;
        float edgeWidth;
        float edgeGlowStrength;
        float edgeNoiseStrength;
        float padding[3];
        Vector4 edgeColor;
    };

    struct RandomNoiseData {
        int32_t enableRandom;
        int32_t previewRandom;
        float intensity;
        float time;
    };

    struct RingAppearanceData {
        int32_t enableRingAppearance;
        int32_t uvDirection;
        float innerRadiusRatio;
        float startAlpha;
        float endAlpha;
        float startFadeRange;
        float endFadeRange;
        float padding;
        Vector4 innerColor;
        Vector4 outerColor;
    };

public:
    struct TransformationMatrix {
        Matrix4x4 WVP;
        Matrix4x4 World;
        Matrix4x4 WorldInverseTranspose; // 非均一スケール対応
    };

    struct DirectionalLight {
        Vector4 color;
        Vector3 direction;
        float intensity;
        Vector3 cameraPosition; // 鏡面反射用カメラ座標
        float shininess;        // 光沢度
    };

public:
    void Initialize(Object3dCommon* object3dCommon);
    void Update();
    void Draw();

    void SetModel(Model* model) { model_ = model; }
    void SetCamera(Camera* camera) { camera_ = camera; }
    void SetEnvironmentTextureIndex(uint32_t textureIndex) { environmentTextureIndex_ = textureIndex; }
    void SetEnvironmentMapEnabled(bool isEnabled) { environmentMapData_->enableEnvironmentMap = isEnabled ? 1 : 0; }
    void SetEnvironmentMapIntensity(float intensity) { environmentMapData_->intensity = intensity; }
    void SetDissolveEnabled(bool isEnabled) { dissolveData_->enableDissolve = isEnabled ? 1 : 0; }
    void SetDissolveThreshold(float threshold) { dissolveData_->threshold = threshold; }
    void SetDissolveEdgeWidth(float edgeWidth) { dissolveData_->edgeWidth = edgeWidth; }
    void SetDissolveEdgeGlowStrength(float edgeGlowStrength) { dissolveData_->edgeGlowStrength = edgeGlowStrength; }
    void SetDissolveEdgeNoiseStrength(float edgeNoiseStrength) { dissolveData_->edgeNoiseStrength = edgeNoiseStrength; }
    void SetDissolveEdgeColor(const Vector4& edgeColor) { dissolveData_->edgeColor = edgeColor; }
    void SetDissolveMaskTextureIndex(uint32_t textureIndex) { dissolveMaskTextureIndex_ = textureIndex; }
    void SetDissolveMaskTexture(const std::string& path);
    void SetRandomEnabled(bool isEnabled) { randomNoiseData_->enableRandom = isEnabled ? 1 : 0; }
    void SetRandomPreview(bool isPreview) { randomNoiseData_->previewRandom = isPreview ? 1 : 0; }
    void SetRandomIntensity(float intensity) { randomNoiseData_->intensity = intensity; }
    void SetRandomTime(float time) { randomNoiseData_->time = time; }
    void SetRingAppearanceEnabled(bool isEnabled) { ringAppearanceData_->enableRingAppearance = isEnabled ? 1 : 0; }
    void SetRingUVDirection(int32_t uvDirection) { ringAppearanceData_->uvDirection = uvDirection; }
    void SetRingInnerRadiusRatio(float innerRadiusRatio) { ringAppearanceData_->innerRadiusRatio = innerRadiusRatio; }
    void SetRingStartAlpha(float startAlpha) { ringAppearanceData_->startAlpha = startAlpha; }
    void SetRingEndAlpha(float endAlpha) { ringAppearanceData_->endAlpha = endAlpha; }
    void SetRingStartFadeRange(float startFadeRange) { ringAppearanceData_->startFadeRange = startFadeRange; }
    void SetRingEndFadeRange(float endFadeRange) { ringAppearanceData_->endFadeRange = endFadeRange; }
    void SetRingInnerColor(const Vector4& innerColor) { ringAppearanceData_->innerColor = innerColor; }
    void SetRingOuterColor(const Vector4& outerColor) { ringAppearanceData_->outerColor = outerColor; }

    void SetScale(const Vector3& scale) { transform_.scale = scale; }
    void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
    void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
    Transform& GetTransform() { return transform_; }

    DirectionalLight* GetDirectionalLightData() { return directionalLightData_; }

private:
    void CreateTransformationMatrixResource();
    void CreateDirectionalLightResource();
    void CreateEnvironmentMapResource();
    void CreateDissolveResource();
    void CreateRandomNoiseResource();
    void CreateRingAppearanceResource();

private:
    Object3dCommon* object3dCommon_ = nullptr;
    Model* model_ = nullptr;
    Camera* camera_ = nullptr;

    Transform transform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
    TransformationMatrix* transformationMatrixData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
    DirectionalLight* directionalLightData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> environmentMapResource_;
    EnvironmentMapData* environmentMapData_ = nullptr;
    uint32_t environmentTextureIndex_ = static_cast<uint32_t>(-1);

    Microsoft::WRL::ComPtr<ID3D12Resource> dissolveResource_;
    DissolveData* dissolveData_ = nullptr;
    uint32_t dissolveMaskTextureIndex_ = static_cast<uint32_t>(-1);

    Microsoft::WRL::ComPtr<ID3D12Resource> randomNoiseResource_;
    RandomNoiseData* randomNoiseData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> ringAppearanceResource_;
    RingAppearanceData* ringAppearanceData_ = nullptr;
};
