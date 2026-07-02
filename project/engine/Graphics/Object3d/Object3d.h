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
    bool IsValid() const { return initialized_; }

    void SetModel(Model* model) { model_ = model; }
    void SetCamera(Camera* camera) { camera_ = camera; }
    void SetEnvironmentTextureIndex(uint32_t textureIndex) { environmentTextureIndex_ = textureIndex; }
    void SetEnvironmentMapEnabled(bool isEnabled) { if (environmentMapData_) { environmentMapData_->enableEnvironmentMap = isEnabled ? 1 : 0; } }
    void SetEnvironmentMapIntensity(float intensity) { if (environmentMapData_) { environmentMapData_->intensity = intensity; } }
    void SetDissolveEnabled(bool isEnabled) { if (dissolveData_) { dissolveData_->enableDissolve = isEnabled ? 1 : 0; } }
    void SetDissolveThreshold(float threshold) { if (dissolveData_) { dissolveData_->threshold = threshold; } }
    void SetDissolveEdgeWidth(float edgeWidth) { if (dissolveData_) { dissolveData_->edgeWidth = edgeWidth; } }
    void SetDissolveEdgeGlowStrength(float edgeGlowStrength) { if (dissolveData_) { dissolveData_->edgeGlowStrength = edgeGlowStrength; } }
    void SetDissolveEdgeNoiseStrength(float edgeNoiseStrength) { if (dissolveData_) { dissolveData_->edgeNoiseStrength = edgeNoiseStrength; } }
    void SetDissolveEdgeColor(const Vector4& edgeColor) { if (dissolveData_) { dissolveData_->edgeColor = edgeColor; } }
    void SetDissolveMaskTextureIndex(uint32_t textureIndex) { dissolveMaskTextureIndex_ = textureIndex; }
    void SetDissolveMaskTexture(const std::string& path);
    void SetRandomEnabled(bool isEnabled) { if (randomNoiseData_) { randomNoiseData_->enableRandom = isEnabled ? 1 : 0; } }
    void SetRandomPreview(bool isPreview) { if (randomNoiseData_) { randomNoiseData_->previewRandom = isPreview ? 1 : 0; } }
    void SetRandomIntensity(float intensity) { if (randomNoiseData_) { randomNoiseData_->intensity = intensity; } }
    void SetRandomTime(float time) { if (randomNoiseData_) { randomNoiseData_->time = time; } }
    void SetRingAppearanceEnabled(bool isEnabled) { if (ringAppearanceData_) { ringAppearanceData_->enableRingAppearance = isEnabled ? 1 : 0; } }
    void SetRingUVDirection(int32_t uvDirection) { if (ringAppearanceData_) { ringAppearanceData_->uvDirection = uvDirection; } }
    void SetRingInnerRadiusRatio(float innerRadiusRatio) { if (ringAppearanceData_) { ringAppearanceData_->innerRadiusRatio = innerRadiusRatio; } }
    void SetRingStartAlpha(float startAlpha) { if (ringAppearanceData_) { ringAppearanceData_->startAlpha = startAlpha; } }
    void SetRingEndAlpha(float endAlpha) { if (ringAppearanceData_) { ringAppearanceData_->endAlpha = endAlpha; } }
    void SetRingStartFadeRange(float startFadeRange) { if (ringAppearanceData_) { ringAppearanceData_->startFadeRange = startFadeRange; } }
    void SetRingEndFadeRange(float endFadeRange) { if (ringAppearanceData_) { ringAppearanceData_->endFadeRange = endFadeRange; } }
    void SetRingInnerColor(const Vector4& innerColor) { if (ringAppearanceData_) { ringAppearanceData_->innerColor = innerColor; } }
    void SetRingOuterColor(const Vector4& outerColor) { if (ringAppearanceData_) { ringAppearanceData_->outerColor = outerColor; } }

    void SetScale(const Vector3& scale) { transform_.scale = scale; }
    void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
    void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
    Transform& GetTransform() { return transform_; }

    DirectionalLight* GetDirectionalLightData() { return directionalLightData_; }

private:
    bool CreateTransformationMatrixResource();
    bool CreateDirectionalLightResource();
    bool CreateEnvironmentMapResource();
    bool CreateDissolveResource();
    bool CreateRandomNoiseResource();
    bool CreateRingAppearanceResource();

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
    bool initialized_ = false;
};
