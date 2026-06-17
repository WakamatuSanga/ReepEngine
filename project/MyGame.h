#pragma once

#include "Engine/Core/WinApp.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Core/SrvManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Graphics/Sprite/SpriteCommon.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include "Engine/Graphics/Skybox/SkyboxCommon.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Particle/ParticleManager.h"
#include "Engine/Graphics/Cloud/VolumetricCloudPass.h"
#include "Engine/Input/Input.h"
#include "Engine/Core/ImGuiManager.h"
#include "Engine/Audio/Audio.h"
#include <memory>

// ゲームエンジン全体を管理するクラス（シングルトン化）
class MyGame {
    friend struct std::default_delete<MyGame>;

public:
    static MyGame* GetInstance();

    // 初期化、実行、終了
    void Initialize();
    void Finalize();
    void Run();

    // 各シーンから基盤システムへアクセスするためのゲッター
    Input* GetInput() const { return input_.get(); } // .get()で生ポインタを返す
    WinApp* GetWinApp() const { return winApp_.get(); }
    DirectXCommon* GetDxCommon() const { return dxCommon_.get(); }
    Object3dCommon* GetObject3dCommon() const { return object3dCommon_.get(); }
    SkyboxCommon* GetSkyboxCommon() const { return skyboxCommon_.get(); }
    SpriteCommon* GetSpriteCommon() const { return spriteCommon_.get(); }
    VolumetricCloudPass* GetVolumetricCloudPass() const { return volumetricCloudPass_.get(); }

private:
    MyGame() = default;
    ~MyGame() = default;
    MyGame(const MyGame&) = delete;
    MyGame& operator=(const MyGame&) = delete;

    // シングルトンインスタンスも unique_ptr で管理して自動解放
    static std::unique_ptr<MyGame> instance_;

    // 毎フレームの更新と描画
    void Update();
    void Draw();

private:
    // --- 基盤システム (所有権を持つため unique_ptr) ---
    std::unique_ptr<WinApp> winApp_;
    std::unique_ptr<DirectXCommon> dxCommon_;
    std::unique_ptr<ImGuiManager> imguiManager_;
    std::unique_ptr<SpriteCommon> spriteCommon_;
    std::unique_ptr<Object3dCommon> object3dCommon_;
    std::unique_ptr<SkyboxCommon> skyboxCommon_;
    std::unique_ptr<VolumetricCloudPass> volumetricCloudPass_;
    std::unique_ptr<Input> input_;

    // --- マネージャーへの参照 (所有権を持たないため 生ポインタ でOK) ---
    SrvManager* srvManager_ = nullptr;
    TextureManager* texManager_ = nullptr;
    ModelManager* modelManager_ = nullptr;
    ParticleManager* particleManager_ = nullptr;
    Audio* audio_ = nullptr;
};
