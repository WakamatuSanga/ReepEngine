#pragma once
#include "Engine/Animation/AnimationClip.h"
#include "Engine/Scene/IScene.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Cloud/CloudVolume.h"
#include "Engine/Graphics/Cloud/VolumetricCloudPass.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

class GltfSkinnedModel;
class BlenderLiveSync;
class DirectXCommon;
class EditorCameraController;
class GpuParticleSystem;
class LevelSceneRuntime;
class Model;
class Object3d;
class Player;
class PlayerRailController;
class PlayerEventTriggerBridge;
class PrimitiveEffectSystem;
class RailShooterEventActionBridge;
class RailShooterCameraRig;
class SkinningEditor;
class Skybox;
class Sprite;
struct Skeleton;

class GameScene : public IScene {
public:
    GameScene();
    ~GameScene() override;
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
#ifdef _DEBUG
    void DrawGameViewImGui(DirectXCommon* dxCommon);
#endif

    std::unique_ptr<Camera> camera_;
    std::unique_ptr<CloudVolume> cloudVolume_;
    std::unique_ptr<Skybox> skybox_;
    std::unique_ptr<Object3d> object3d_;       // フェンス用
    std::unique_ptr<Object3d> object3dSphere_; // 球用
    std::unique_ptr<PrimitiveEffectSystem> primitiveEffectSystem_;
    std::unique_ptr<GpuParticleSystem> gpuParticleSystem_;
    std::unique_ptr<SkinningEditor> skinningEditor_;
    std::unique_ptr<EditorCameraController> editorCameraController_;
    std::unique_ptr<LevelSceneRuntime> levelSceneRuntime_;
    std::unique_ptr<BlenderLiveSync> blenderLiveSync_;
    std::unique_ptr<Player> player_;
    std::unique_ptr<PlayerRailController> playerRailController_;
    std::unique_ptr<PlayerEventTriggerBridge> playerEventTriggerBridge_;
    std::unique_ptr<RailShooterCameraRig> railShooterCameraRig_;
    std::unique_ptr<RailShooterEventActionBridge> railShooterEventActionBridge_;
    std::unique_ptr<Skeleton> previewSkeleton_;
    std::unique_ptr<Skeleton> previewSkeletonSecondary_;
    std::unique_ptr<Skeleton> simpleSkinSkeleton_;
    std::unique_ptr<GltfSkinnedModel> simpleSkinSkinnedModel_;
    std::unique_ptr<Object3d> simpleSkinSkinnedObject_;
    std::unique_ptr<Skeleton> walkSkeleton_;
    std::unique_ptr<GltfSkinnedModel> walkSkinnedModel_;
    std::unique_ptr<Object3d> walkSkinnedObject_;
    std::unique_ptr<Skeleton> sneakWalkSkeleton_;
    std::unique_ptr<GltfSkinnedModel> sneakWalkSkinnedModel_;
    std::unique_ptr<Object3d> sneakWalkSkinnedObject_;
    std::unique_ptr<GltfSkinnedModel> animatedCubeModel_;
    std::unique_ptr<Object3d> animatedCubeObject_;
    std::unique_ptr<Sprite> debugSprite_;
    std::vector<std::unique_ptr<Object3d>> primitivePreviewObjects_;

    Model* modelFence_ = nullptr;
    Model* modelSphere_ = nullptr;
    uint32_t texIndexUvChecker_ = 0;
    uint32_t texIndexFence_ = 0;
    uint32_t texIndexMonsterBall_ = 0;
    uint32_t skyboxTextureIndex_ = 0;

    int currentModelTexture_ = 1;
    int currentParticleTexture_ = 0;
    int currentDissolveNoiseTexture_ = 0;
    int currentObjectDissolveMaskTexture_ = 0;
    int currentBlendMode_ = 0;
    int targetObjectIndex_ = 1; // 0=Fence, 1=Sphere

    bool isSkyboxVisible_ = true;
    bool isFenceVisible_ = true;
    bool isSphereVisible_ = true;
    bool isAnimatedCubeVisible_ = true;
    bool isSkinnedModelVisible_ = true;
    bool isParticleVisible_ = true;
    bool isVolumetricCloudVisible_ = true;
    bool isDebugSpriteVisible_ = true;
    bool isSkyboxFollowCamera_ = true;
    std::string skyboxTexturePath_ = "resources/skybox/skybox.dds";
    Vector3 skyboxScale_ = { 100.0f, 100.0f, 100.0f };
    Vector3 skyboxTranslate_ = { 0.0f, 0.0f, 0.0f };
    std::string particleTexturePath_ = "resources/obj/axis/uvChecker.png";
    std::string objectDissolveMaskTexturePath_ = "resources/postEffect/noise0.png";
    bool isSphereEnvironmentMapEnabled_ = true;
    float sphereEnvironmentMapIntensity_ = 1.0f;
    bool isObjectDissolveEnabled_ = false;
    float objectDissolveThreshold_ = 0.0f;
    float objectDissolveEdgeWidth_ = 0.05f;
    float objectDissolveEdgeGlowStrength_ = 0.5f;
    float objectDissolveEdgeNoiseStrength_ = 0.25f;
    std::array<float, 4> objectDissolveEdgeColor_ = { 1.0f, 0.5f, 0.1f, 1.0f };
    bool isObjectRandomEnabled_ = false;
    bool isObjectRandomPreview_ = true;
    float objectRandomIntensity_ = 1.0f;
    float objectRandomTime_ = 0.0f;
    bool isPrimitivePreviewVisible_ = true;
    AnimationClip animatedCubeClip_{};
    float animatedCubeAnimationTime_ = 0.0f;
    bool hasAnimatedCubeAnimation_ = false;

    float layoutStartX_ = -1.4f;
    float layoutStartY_ = -0.8f;
    float layoutStartZ_ = 0.0f;
    float layoutStepX_ = 0.22f;
    float layoutStepY_ = 0.11f;
    float layoutStepZ_ = 0.05f;
    VolumetricCloudPass::ProjectedBounds cloudProjectedBounds_{};

#ifdef _DEBUG
    const char* blendModeNames_[6] = { "Normal", "Add", "Subtract", "Multiply", "Screen", "None" };
    std::array<float, 2> gameViewTopLeft_ = { 0.0f, 0.0f };
    std::array<float, 2> gameViewSize_ = { 0.0f, 0.0f };
    std::array<float, 2> gameViewMouseLocal_ = { 0.0f, 0.0f };
    bool isGameViewHovered_ = false;
    bool isGameViewFocused_ = false;
#endif
};
