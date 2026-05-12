#pragma once
#include "AnimationClip.h"
#include "IScene.h"
#include "Model.h"
#include "Camera.h"
#include "CloudVolume.h"
#include "Object3d.h"
#include "Skybox.h"
#include "Sprite.h"
#include "SkinningEditor.h"
#include "Skeleton.h"
#include "VolumetricCloudPass.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

class GltfSkinnedModel;

class GameScene : public IScene {
public:
    GameScene();
    ~GameScene() override;
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<CloudVolume> cloudVolume_;
    std::unique_ptr<Skybox> skybox_;
    std::unique_ptr<Object3d> object3d_;       // フェンス用
    std::unique_ptr<Object3d> object3dSphere_; // 球用
    std::unique_ptr<Object3d> ringEffectPlane_;
    std::unique_ptr<Object3d> ringEffect_;
    std::unique_ptr<Object3d> effectCylinder_;
    std::unique_ptr<Object3d> ringEffectComparePlaneBillboard_;
    std::unique_ptr<Object3d> ringEffectCompareBillboard_;
    std::unique_ptr<Object3d> ringEffectComparePlaneWorld_;
    std::unique_ptr<Object3d> ringEffectCompareWorld_;
    std::unique_ptr<SkinningEditor> skinningEditor_;
    std::unique_ptr<Skeleton> previewSkeleton_;
    std::unique_ptr<Skeleton> previewSkeletonSecondary_;
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
    Model* ringEffectModel_ = nullptr;
    Model* ringEffectPlaneModel_ = nullptr;
    Model* effectCylinderModel_ = nullptr;

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
    bool isEffectCylinderVisible_ = true;
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
    bool isRingEffectVisible_ = true;
    bool isRingEffectPlaneVisible_ = true;
    bool isRingEffectCompareVisible_ = true;
    bool isRingBillboardEnabled_ = true;
    Vector3 ringEffectTranslate_ = { 0.0f, 1.2f, 3.0f };
    Vector3 ringEffectRotate_ = { 0.0f, 0.0f, 0.0f };
    Vector3 ringEffectScale_ = { 1.6f, 1.6f, 1.0f };
    float ringEffectPlaneScale_ = 1.35f;
    float ringEffectCompareSpacing_ = 4.2f;
    std::array<float, 4> ringEffectPlaneColor_ = { 0.45f, 0.65f, 1.0f, 0.55f };
    std::array<float, 4> ringEffectColor_ = { 1.0f, 1.0f, 1.0f, 0.9f };
    bool isRingAppearancePreviewEnabled_ = false;
    int currentRingUVDirection_ = 0;
    std::array<float, 4> ringInnerColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    std::array<float, 4> ringOuterColor_ = { 1.0f, 0.6f, 0.2f, 1.0f };
    float ringStartAlpha_ = 1.0f;
    float ringEndAlpha_ = 1.0f;
    float ringStartFadeRange_ = 0.15f;
    float ringEndFadeRange_ = 0.15f;
    float ringShapeStartAngle_ = 0.0f;
    float ringShapeEndAngle_ = 360.0f;
    float ringShapeStartRadius_ = 1.0f;
    float ringShapeEndRadius_ = 1.0f;
    bool isRingEffectModelDirty_ = false;
    bool isRingAnimationEnabled_ = false;
    bool isRingUVScrollEnabled_ = false;
    float ringAnimationTime_ = 0.0f;
    AnimationClip animatedCubeClip_{};
    float animatedCubeAnimationTime_ = 0.0f;
    bool hasAnimatedCubeAnimation_ = false;
    float ringUVScrollSpeedX_ = 0.25f;
    float ringUVScrollSpeedY_ = 0.0f;
    bool isRingAlphaAnimationEnabled_ = false;
    float ringAlphaAnimationSpeed_ = 1.5f;
    float ringAlphaAnimationMin_ = 0.15f;
    float ringAlphaAnimationMax_ = 1.0f;
    bool isRingRadiusAnimationEnabled_ = false;
    float ringRadiusAnimationSpeed_ = 1.25f;
    float ringRadiusAnimationStartMin_ = 0.8f;
    float ringRadiusAnimationStartMax_ = 1.2f;
    float ringRadiusAnimationEndMin_ = 0.8f;
    float ringRadiusAnimationEndMax_ = 1.2f;
    bool isRingAngleAnimationEnabled_ = false;
    float ringAngleAnimationSpeed_ = 1.0f;
    float ringAngleAnimationSpan_ = 180.0f;
    float ringAngleAnimationBase_ = 0.0f;

    std::array<float, 4> effectCylinderColor_ = { 0.45f, 0.65f, 1.0f, 0.8f };
    bool isEffectCylinderUVScrollEnabled_ = true;
    float effectCylinderUVScrollSpeedX_ = 0.5f;
    float effectCylinderUVScrollSpeedY_ = 0.0f;
    float effectCylinderTime_ = 0.0f;

    float layoutStartX_ = -1.4f;
    float layoutStartY_ = -0.8f;
    float layoutStartZ_ = 0.0f;
    float layoutStepX_ = 0.22f;
    float layoutStepY_ = 0.11f;
    float layoutStepZ_ = 0.05f;
    VolumetricCloudPass::ProjectedBounds cloudProjectedBounds_{};

#ifdef _DEBUG
    const char* blendModeNames_[6] = { "Normal", "Add", "Subtract", "Multiply", "Screen", "None" };
#endif
};
