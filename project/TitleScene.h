#pragma once
#include "Engine/Scene/IScene.h"
#include "TitleLaunchSequence.h"
#include "Engine/math/Matrix4x4.h"
#include <memory>

class Camera;
class Model;
class Object3d;
class PlayerJetExhaustController;
class TitleSortieEffects;
class TitleLogoController;
class TitleForegroundPresentation;
class Skybox;
class CloudVolume;
class VolumetricCloudPass;

// タイトル画面のシーン
class TitleScene : public IScene {
public:
    TitleScene();
    ~TitleScene() override;
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void DrawOverlay() override;
    void Finalize() override;
    bool UsesFullscreenScenePresentation() const override { return true; }
private:
    void UpdateDisplay(float deltaTime);

    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Object3d> aircraft_;
    std::unique_ptr<PlayerJetExhaustController> exhaust_;
    std::unique_ptr<TitleSortieEffects> sortieEffects_;
    std::unique_ptr<TitleLogoController> logo_;
    std::unique_ptr<TitleForegroundPresentation> foreground_;
    std::unique_ptr<Skybox> skybox_;
    std::unique_ptr<CloudVolume> cloudVolume_;
    std::unique_ptr<VolumetricCloudPass> cloudPass_;
    Model* model_ = nullptr; // Owned by ModelManager.
    float savedModelAlpha_ = 1.0f;
    TitleLaunchSequence sequence_;
    bool transitionRequested_ = false;
    Transform aircraftStart_{};
    float aircraftVisibility_ = 0.0f;
};
