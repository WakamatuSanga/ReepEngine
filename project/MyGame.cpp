#include "MyGame.h"
#include "SceneManager.h"
#include "TitleScene.h"

std::unique_ptr<MyGame> MyGame::instance_ = nullptr;

MyGame* MyGame::GetInstance() {
    if (!instance_) {
        instance_.reset(new MyGame());
    }
    return instance_.get();
}

void MyGame::Initialize() {
    winApp_ = std::make_unique<WinApp>();
    winApp_->Initialize();

    dxCommon_ = std::make_unique<DirectXCommon>();
    dxCommon_->Initialize(winApp_.get());

    srvManager_ = SrvManager::GetInstance();
    srvManager_->Initialize(dxCommon_.get());
    dxCommon_->CreateRenderTexture(srvManager_);

    imguiManager_ = std::make_unique<ImGuiManager>();
    imguiManager_->Initialize(winApp_.get(), dxCommon_.get());

    texManager_ = TextureManager::GetInstance();
    texManager_->Initialize(dxCommon_.get(), srvManager_);

    spriteCommon_ = std::make_unique<SpriteCommon>();
    spriteCommon_->Initialize(dxCommon_.get());

    object3dCommon_ = std::make_unique<Object3dCommon>();
    object3dCommon_->Initialize(dxCommon_.get());

    skyboxCommon_ = std::make_unique<SkyboxCommon>();
    skyboxCommon_->Initialize(dxCommon_.get());

    volumetricCloudPass_ = std::make_unique<VolumetricCloudPass>();
    volumetricCloudPass_->Initialize(dxCommon_.get());

    modelManager_ = ModelManager::GetInstance();
    modelManager_->Initialize(dxCommon_.get());

    input_ = std::make_unique<Input>();
    input_->Initialize(winApp_.get());

    particleManager_ = ParticleManager::GetInstance();
    particleManager_->Initialize(dxCommon_.get(), srvManager_);

    audio_ = Audio::GetInstance();
    audio_->Initialize();

    // 事前ロード
    modelManager_->LoadModel("resources/obj/fence/fence.obj");
    modelManager_->LoadModel("resources/obj/sphere/sphere.obj");
    texManager_->LoadTexture("resources/obj/fence/fence.png");
    texManager_->LoadTexture("resources/obj/axis/uvChecker.png");
    texManager_->LoadTexture("resources/postEffect/noise0.png");
    texManager_->LoadTexture("resources/postEffect/noise1.png");
    dxCommon_->SetDissolveNoiseTextureIndex(
        texManager_->GetTextureIndexByFilePath("resources/postEffect/noise0.png"));
    audio_->LoadAudio("resources/sounds/Alarm01.mp3");

    SceneManager::GetInstance()->ChangeScene(std::make_unique<TitleScene>());
}

void MyGame::Finalize() {
    SceneManager::GetInstance()->Finalize();

    texManager_->ReleaseIntermediateResources();
    particleManager_->Finalize();
    imguiManager_->Finalize();
    texManager_->Finalize();
    modelManager_->Finalize();

    if (audio_) {
        audio_->Finalize();
    }
}

void MyGame::Run() {
    while (true) {
        if (winApp_->ProcessMessage()) break;
        Update();
        Draw();
    }
}

void MyGame::Update() {
    imguiManager_->Begin();
    input_->Update();
    SceneManager::GetInstance()->Update();
    imguiManager_->End();
}

void MyGame::Draw() {
    dxCommon_->PreDraw();
    srvManager_->PreDraw();
    SceneManager::GetInstance()->Draw();
    dxCommon_->PrepareSwapChainForImGui();
    dxCommon_->CopyRenderTextureToSwapChain();
    dxCommon_->PrepareRenderTextureForImGui();
    imguiManager_->Draw();
    dxCommon_->RestoreRenderTextureAfterImGui();
    dxCommon_->PostDraw();
}
