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
    void DrawManagerDebugWindows();
    void RefreshSkinningPreviewAfterEditorInput(bool refreshObjectTransform);
    void DrawSceneToolWindows(DirectXCommon* dxCommon, VolumetricCloudPass* volumetricCloudPass);

    GameScene* scene_ = nullptr;
};
