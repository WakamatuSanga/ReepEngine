#pragma once
#include "Engine/Graphics/Effect/PrimitiveEffectTypes.h"
#include <cstdint>
#include <memory>

class Camera;
class Model;
class Object3d;
class Object3dCommon;

class RingEffect {
public:
    RingEffect();
    ~RingEffect();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Play();
    void Stop();
    void Update(float deltaTime, const Camera* camera);
    void Draw();
    void DrawImGui();

    void SetVisible(bool isVisible) { isVisible_ = isVisible; }
    bool IsVisible() const { return isVisible_; }

private:
    void RebuildModelIfNeeded();
    void ApplyMaterial(float alpha);
    float GetInnerRadius() const;

    Object3d* object3d_ = nullptr;
    std::unique_ptr<Object3d> ownedObject_;
    Model* ringModel_ = nullptr;
    RingEffectSettings settings_{};
    uint32_t textureIndex_ = 0;
    bool isUsingDefaultTexture_ = true;
    bool isVisible_ = true;
    bool isPlaying_ = true;
    bool isModelDirty_ = true;
    float elapsedTime_ = 0.0f;
};
