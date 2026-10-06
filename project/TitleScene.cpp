#include "TitleScene.h"
#include "SceneManager.h"
#include "GameScene.h" 
#include "MyGame.h"    
#include "Engine/Input/Input.h"
#include "Engine/Core/FrameTimer.h"
#include "Engine/Game/Player/PlayerJetExhaustController.h"
#include "TitleSortieEffects.h"
#include "TitleLogoController.h"
#include "TitleWaitingAircraft.h"
#include "TitleForegroundPresentation.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Cloud/CloudVolume.h"
#include "Engine/Graphics/Cloud/VolumetricCloudPass.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Skybox/Skybox.h"
#include <array>
#include <cmath>
#include <filesystem>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
    // Preserve the aircraft's initial world pose; only the title camera orbits it.
    constexpr float kAircraftScale = 1.5f;
    constexpr float kGameplayModelScale = 0.25f; // Calibration of the existing nozzle offsets.
    constexpr Vector3 kCameraPosition{ 0.0f, 2.4f, -8.0f };
    constexpr float kCameraFov = 0.65f;
    constexpr float kCameraNear = 0.1f;
    constexpr float kCameraFar = 100.0f;
    std::string ResolvePlayerModelPath() {
        const std::array<std::filesystem::path, 6> roots = {
            "", "project", "../project", "../../project", "../../../project", "../../../../project"
        };
        for (const auto& root : roots) {
            const auto path = root / "resources/Player/player.obj";
            if (std::filesystem::exists(path)) return path.lexically_normal().generic_string();
        }
        return {};
    }
}

TitleScene::TitleScene() = default;
TitleScene::~TitleScene() = default;

void TitleScene::Initialize() {
    sequence_ = {};
    transitionRequested_ = false;
    aircraftVisibility_ = 0.0f;
    auto* game = MyGame::GetInstance();
    logo_ = std::make_unique<TitleLogoController>();
    logo_->Initialize(game->GetSpriteCommon());
    camera_ = std::make_unique<Camera>();
    camera_->SetTranslate(kCameraPosition);
    camera_->SetRotate({ 0.0f, sequence_.yaw, 0.0f });
    camera_->SetFovY(kCameraFov);
    camera_->SetNearClip(kCameraNear);
    camera_->SetFarClip(kCameraFar);
    const auto height = game->GetDxCommon()->GetRenderTextureHeight();
    if (height != 0) camera_->SetAspectRatio(
        static_cast<float>(game->GetDxCommon()->GetRenderTextureWidth()) / height);
    aircraftStart_ = TitleWaitingAircraft::MakeInitialPose(kCameraPosition, sequence_.yaw,
        camera_->GetAspectRatio(), kCameraFov, kAircraftScale);

    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(game->GetSkyboxCommon());
    skybox_->SetCamera(camera_.get());
    skybox_->SetScale({ 100.0f, 100.0f, 100.0f });
    skybox_->SetTexture("resources/skybox/skybox.dds");
    skybox_->SetBlueFramingStrength(0.16f);

    cloudVolume_ = std::make_unique<CloudVolume>();
    cloudVolume_->GetParameters() = CloudVolume::RecommendedDefaults();
    cloudVolume_->GetParameters().windSpeed = 0.0f;
    cloudPass_ = std::make_unique<VolumetricCloudPass>();
    cloudPass_->Initialize(game->GetDxCommon(), SrvManager::GetInstance());
    cloudPass_->ConfigureStillTitleBackground();
    sortieEffects_ = std::make_unique<TitleSortieEffects>();
    sortieEffects_->Initialize(game->GetDxCommon(), camera_.get());

    const std::string path = ResolvePlayerModelPath();
    auto* modelManager = ModelManager::GetInstance();
    modelManager->LoadModel(path);
    model_ = modelManager->FindModel(path);
    if (model_) {
        // Gameplay may have faded this shared material. Restore it before leaving the title.
        if (auto* material = model_->GetMaterialData()) {
            savedModelAlpha_ = material->color.w;
            material->color.w = 1.0f;
        }
        aircraft_ = std::make_unique<Object3d>();
        aircraft_->Initialize(game->GetObject3dCommon());
        aircraft_->SetCamera(camera_.get());
        aircraft_->SetModel(model_);
        aircraft_->SetEnvironmentMapEnabled(false);
        aircraft_->GetTransform() = aircraftStart_;
        exhaust_ = std::make_unique<PlayerJetExhaustController>();
        if (!exhaust_->Initialize(game->GetObject3dCommon(), camera_.get(), nullptr, nullptr,
            game->GetDxCommon(), SrvManager::GetInstance())) {
            exhaust_->Finalize();
            exhaust_.reset();
        }
    }
    foreground_ = std::make_unique<TitleForegroundPresentation>();
    if (!foreground_->Initialize(modelManager->GetModelCommon(), game->GetObject3dCommon(),
        camera_.get(), cloudVolume_.get())) foreground_.reset();
    UpdateDisplay(0.0f);
}

void TitleScene::Finalize() {
    foreground_.reset();
    logo_.reset();
    sortieEffects_.reset();
    if (exhaust_) exhaust_->Finalize();
    exhaust_.reset();
    aircraft_.reset();
    skybox_.reset();
    cloudPass_.reset();
    cloudVolume_.reset();
    camera_.reset();
    if (model_) {
        if (auto* material = model_->GetMaterialData()) material->color.w = savedModelAlpha_;
    }
    model_ = nullptr;
    sequence_ = {};
    transitionRequested_ = false;
    aircraftStart_ = {};
    aircraftVisibility_ = 0.0f;
}

void TitleScene::UpdateDisplay(float deltaTime) {
    const auto previousPhase = sequence_.phase;
    sequence_.Update(deltaTime);
    if (!camera_) return;
    auto* dxCommon = MyGame::GetInstance()->GetDxCommon();
    const auto height = dxCommon->GetRenderTextureHeight();
    if (height != 0) {
        camera_->SetAspectRatio(static_cast<float>(dxCommon->GetRenderTextureWidth()) / height);
    }
    if (sequence_.phase == TitleLaunchSequence::Phase::Idle ||
        sequence_.phase == TitleLaunchSequence::Phase::Align || previousPhase == TitleLaunchSequence::Phase::Align) {
        // Arc around the stationary aircraft; flatten and center the view before flight.
        const auto orbit = TitleWaitingAircraft::MakeOrbitCamera(aircraftStart_.translate,
            sequence_.yaw, camera_->GetAspectRatio(), kCameraFov, kAircraftScale, sequence_.GetAlignmentProgress());
        camera_->SetTranslate(orbit.translate);
        camera_->SetRotate(orbit.rotate);
    }
    camera_->Update();

    // Freeze both wind displacement AND raw time used by the far-cloud/sea noise.
    if (cloudVolume_) cloudVolume_->Update(0.0f);
    if (skybox_) {
        skybox_->SetTranslate(camera_->GetTranslate());
        skybox_->Update();
    }
    if (aircraft_) {
        const auto basis = MatrixMath::MakeAffine({ 1, 1, 1 }, aircraftStart_.rotate, aircraftStart_.translate);
        const float distance = sequence_.GetFlightDistance();
        aircraft_->SetTranslate({ aircraftStart_.translate.x + basis.m[2][0] * distance,
            aircraftStart_.translate.y + basis.m[2][1] * distance,
            aircraftStart_.translate.z + basis.m[2][2] * distance });
        // Same model, orientation, opacity and nozzle frame across Align -> Fly.
        aircraftVisibility_ = 1.0f;
        if (auto* material = model_->GetMaterialData()) material->color.w = 1.0f;
        aircraft_->Update();
        if (exhaust_) exhaust_->UpdateDisplay(deltaTime, aircraft_->GetTransform(), kGameplayModelScale,
            sequence_.phase == TitleLaunchSequence::Phase::Fly || sequence_.phase == TitleLaunchSequence::Phase::Cover
                ? 1.0f : 0.0f, 1.0f, true);
    }
    if (foreground_) foreground_->Update(deltaTime, aircraftStart_);
    if (sortieEffects_) sortieEffects_->Update(deltaTime,
        aircraft_ ? &aircraft_->GetTransform() : nullptr,
        sequence_.phase == TitleLaunchSequence::Phase::Fly, aircraftVisibility_, sequence_.GetFlightProgress());
}
void TitleScene::Update() {
    Input* input = MyGame::GetInstance()->GetInput();
    bool startClicked = input && input->MouseLeftClientTrigger();
#ifdef USE_IMGUI
    startClicked = startClicked && !ImGui::GetIO().WantCaptureMouse;
#endif
    if (sequence_.phase == TitleLaunchSequence::Phase::Idle && startClicked) {
        if (aircraft_) aircraftStart_ = aircraft_->GetTransform();
        if (foreground_) foreground_->BeginDeparture();
        sequence_.Start(aircraftStart_.rotate.y);
        input->SuppressLeftMouseUntilRelease();
    }
    const float deltaTime = FrameTimer::GetInstance().GetGameplayDeltaTime();
    if (logo_) logo_->Update(deltaTime, sequence_.phase != TitleLaunchSequence::Phase::Idle);
    UpdateDisplay(deltaTime);
    if (sequence_.phase == TitleLaunchSequence::Phase::Cover && !transitionRequested_) {
        transitionRequested_ = true;
        SceneManager::GetInstance()->ChangeSceneWithTitleClouds(std::make_unique<GameScene>());
    }
#ifdef USE_IMGUI
    if (sequence_.phase == TitleLaunchSequence::Phase::Idle) {
#ifdef IMGUI_HAS_DOCK
    // The title's dockspace is kept alive without rendering its docked windows.
    ImGui::Begin("Title Scene", nullptr, ImGuiWindowFlags_NoDocking);
#else
    ImGui::Begin("Title Scene");
#endif
    ImGui::Text("This is Title Scene.");
    ImGui::Text("Left click to Start Game!");
    ImGui::End();
    }
#endif
}

void TitleScene::Draw() {
    if (skybox_) {
        MyGame::GetInstance()->GetSkyboxCommon()->CommonDrawSetting();
        skybox_->Draw();
    }
    if (aircraft_ && aircraftVisibility_ > 0.0f) {
        MyGame::GetInstance()->GetObject3dCommon()->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
        aircraft_->Draw();
    }
    if (exhaust_ && aircraftVisibility_ > 0.0f) exhaust_->Draw();
    if (foreground_) foreground_->DrawTentacles();
    if (cloudPass_ && cloudVolume_) {
        auto* dx = MyGame::GetInstance()->GetDxCommon();
        const auto bounds = cloudPass_->BuildTitleBackgroundBounds(camera_.get(), cloudVolume_.get());
        if (bounds.isVisible && !bounds.isPassSkipped) {
            dx->TransitionDepthBuffer(D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            cloudPass_->Render(camera_.get(), cloudVolume_.get(), bounds);
            dx->TransitionDepthBuffer(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            const auto rtv = dx->GetRenderTextureRTV();
            const auto dsv = dx->GetDepthStencilView();
            dx->GetCommandList()->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        }
    }
    if (foreground_) foreground_->DrawClouds();
    if (exhaust_ && aircraftVisibility_ > 0.0f) {
        exhaust_->DrawAfterCloud();
    }
    if (sortieEffects_) sortieEffects_->Draw();
}

void TitleScene::DrawOverlay() {
    if (logo_) logo_->Draw();
}
