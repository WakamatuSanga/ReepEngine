#pragma once
#include <memory>

class Camera;
class CloudVolume;
class ModelCommon;
class Object3dCommon;
struct Transform;

// Display-only Kraken pose. The title's existing cloud pass covers its roots.
class TitleForegroundPresentation {
public:
    TitleForegroundPresentation();
    ~TitleForegroundPresentation();
    bool Initialize(ModelCommon* modelCommon, Object3dCommon* objects, Camera* camera, const CloudVolume* background);
    void BeginDeparture();
    void Update(float deltaTime, const Transform& aircraft);
    void DrawTentacles();
    void DrawClouds();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
