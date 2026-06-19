#pragma once
#include <memory>
#include <string>
#include <cstddef>

class Camera;
class Object3dCommon;
class RotatingPlaneHitEffect;
class RingEffect;
class CylinderEffect;
struct Vector3;

class PrimitiveEffectSystem {
public:
    PrimitiveEffectSystem();
    ~PrimitiveEffectSystem();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Update(float deltaTime);
    void Draw();
    void DrawImGui();
    void DrawVisibilityImGui();
    size_t GetEffectCount() const;

    void PlayHitEffectAt(const Vector3& position);
    void PlayRingEffectAt(const Vector3& position);
    void PlayRingEffectAt(const Vector3& position, float scaleMultiplier, float alphaMultiplier = 1.0f);
    void PlayCylinderEffectAt(const Vector3& position);
    bool PlayPresetAt(const std::string& effectType, const Vector3& position, std::string& resultMessage);

    void SetVisible(bool isVisible);
    bool IsVisible() const { return isVisible_; }
    void SetDiagnosticSuppressed(bool isSuppressed) { diagnosticSuppressed_ = isSuppressed; }
    bool IsDiagnosticSuppressed() const { return diagnosticSuppressed_; }

private:
    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    std::unique_ptr<RotatingPlaneHitEffect> rotatingPlaneHitEffect_;
    std::unique_ptr<RingEffect> ringEffect_;
    std::unique_ptr<CylinderEffect> cylinderEffect_;
    bool isVisible_ = true;
    bool diagnosticSuppressed_ = false;
};
