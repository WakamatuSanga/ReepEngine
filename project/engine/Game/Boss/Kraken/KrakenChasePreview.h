#pragma once

#include <memory>

class Camera;
class ModelCommon;
class Object3dCommon;
class RailShooterCameraRig;
struct Vector3;

class KrakenChasePreview {
public:
    KrakenChasePreview();
    ~KrakenChasePreview();

    bool Start(ModelCommon& modelCommon, Object3dCommon& objectCommon,
        Camera& camera, const RailShooterCameraRig& rail, const Vector3& playerPosition);
    void Update(float deltaTime);
    void Draw() const;
    void DrawImGui() const;
    bool IsActive() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
