#pragma once

class DirectXCommon;
class GameScene;
class VolumetricCloudPass;

struct GameSceneDebugGuiContext {
    GameScene* scene = nullptr;
};

class GameSceneDebugGui {
public:
    GameSceneDebugGui() = default;
    ~GameSceneDebugGui() = default;

    void Initialize(const GameSceneDebugGuiContext& context);
    void Initialize(GameScene* scene);
    void DrawImGui(DirectXCommon* dxCommon, bool showDebugUi, VolumetricCloudPass* volumetricCloudPass);

private:
    void ClearGameViewDebugState();
    void DrawGameViewImGui(DirectXCommon* dxCommon);
    void DrawManagerDebugWindows(DirectXCommon* dxCommon, VolumetricCloudPass* volumetricCloudPass);
    void RefreshSkinningPreviewAfterEditorInput(bool refreshObjectTransform);
    enum class SceneTool { Sprite, Scene, Camera, Cloud, Post, Particles, Object, Visibility, Skinning };
    void DrawSceneToolContents(DirectXCommon* dxCommon, VolumetricCloudPass* volumetricCloudPass, SceneTool tool);

    GameScene* scene_ = nullptr;
};
