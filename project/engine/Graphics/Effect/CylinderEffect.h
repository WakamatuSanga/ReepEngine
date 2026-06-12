#pragma once
#include "Engine/Graphics/Effect/PrimitiveEffectTypes.h"
#include <cstdint>
#include <memory>

class Camera;
class Model;
class Object3d;
class Object3dCommon;

class CylinderEffect {
public:
    CylinderEffect();
    ~CylinderEffect();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Play();
    void PlayAt(const Vector3& position);
    void Stop();
    void Update(float deltaTime);
    void Draw();
    void DrawImGui();

    void SetVisible(bool isVisible) { isVisible_ = isVisible; }
    bool IsVisible() const { return isVisible_; }

private:
    void ApplyMaterial(float alpha);

    Model* cylinderModel_ = nullptr;
    std::unique_ptr<Object3d> cylinder_;
    CylinderEffectSettings settings_{};
    uint32_t textureIndex_ = 0;
    bool isUsingDefaultTexture_ = true;
    bool isVisible_ = true;
    bool isPlaying_ = true;
    float elapsedTime_ = 0.0f;
    float uvTime_ = 0.0f;
};
