#pragma once
#include "Engine/Graphics/Effect/IPrimitiveEffect.h"
#include "Engine/Graphics/Effect/PrimitiveEffectTypes.h"
#include <cstdint>
#include <memory>
#include <vector>

class Model;
class Object3d;
class Object3dCommon;

class RotatingPlaneHitEffect : public IPrimitiveEffect {
public:
    RotatingPlaneHitEffect();
    ~RotatingPlaneHitEffect() override;

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Play();
    void PlayAt(const Vector3& position);
    void Stop();
    void Update(float deltaTime, const Camera* camera) override;
    void Draw() override;
    void DrawImGui();

    void SetVisible(bool isVisible) { isVisible_ = isVisible; }
    bool IsVisible() const { return isVisible_; }

private:
    void ApplyMaterial(float alpha);
    int GetActivePlaneCount() const;

    Model* planeModel_ = nullptr;
    std::vector<std::unique_ptr<Object3d>> planes_;
    RotatingPlaneHitEffectSettings settings_{};
    uint32_t textureIndex_ = 0;
    bool isUsingDefaultTexture_ = true;
    bool isVisible_ = true;
    bool isPlaying_ = true;
    float elapsedTime_ = 0.0f;
};
