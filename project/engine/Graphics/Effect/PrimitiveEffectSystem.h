#pragma once
#include <memory>

class Camera;
class Object3dCommon;
class RotatingPlaneHitEffect;
class RingEffect;
class CylinderEffect;

class PrimitiveEffectSystem {
public:
    PrimitiveEffectSystem();
    ~PrimitiveEffectSystem();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Update(float deltaTime);
    void Draw();
    void DrawImGui();
    void DrawVisibilityImGui();

    void SetVisible(bool isVisible);
    bool IsVisible() const { return isVisible_; }

private:
    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    std::unique_ptr<RotatingPlaneHitEffect> rotatingPlaneHitEffect_;
    std::unique_ptr<RingEffect> ringEffect_;
    std::unique_ptr<CylinderEffect> cylinderEffect_;
    bool isVisible_ = true;
};
