#include "TitleForegroundPresentation.h"
#include "TitleForegroundMotion.h"
#include "TitleForegroundPose.h"
#include "Engine/Animation/Skeleton.h"
#include "Engine/Game/Boss/Kraken/KrakenTentacleChainUtility.h"
#include "Engine/Graphics/Model/GltfSkeletonLoader.h"
#include "Engine/Graphics/Model/GltfSkinnedModel.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Cloud/CloudVolume.h"
#include "Engine/Graphics/Cloud/VolumetricCloudPass.h"
#include "Engine/Core/SrvManager.h"
#include "Engine/Core/WinApp.h"
#include "Engine/Utility/Logger.h"
#include <array>
#include <filesystem>

namespace {
    using namespace TitleForegroundMotion;
    std::string AssetPath() {
        for (const char* base : { "", "project/", "../project/", "../../project/", "../../../project/" }) {
            const auto path = std::filesystem::path(base) / "resources/midboss/kraken_midboss_tentacles.gltf";
            if (std::filesystem::exists(path)) return path.generic_string();
        }
        return {};
    }
}

struct TitleForegroundPresentation::Impl {
    Camera* camera = nullptr;
    Object3dCommon* objects = nullptr;
    std::unique_ptr<Skeleton> skeleton;
    std::unique_ptr<GltfSkinnedModel> model;
    std::unique_ptr<Object3d> object;
    std::unique_ptr<CloudVolume> cloud;
    std::unique_ptr<VolumetricCloudPass> cloudPass;
    Camera cloudCamera;
    Matrix4x4 frame = MatrixMath::MakeIdentity4x4();
    float cloudAlpha = 1.0f;
    std::vector<Transform> rest;
    std::vector<KrakenTentacleChain> chains;
    Vector3 framePosition{}, frameRotation{};
    float aspect = 1.0f, fov = 0.65f, time = 0.0f, departure = -1.0f;
    std::array<float, 2> rise{}, departingRise{};
    bool poseVisible = false;

    void CaptureFrame() {
        frame = camera->GetWorldMatrix();
        framePosition = camera->GetTranslate();
        frameRotation = camera->GetRotate();
        aspect = camera->GetAspectRatio();
        fov = camera->GetFovY();
    }

};

TitleForegroundPresentation::TitleForegroundPresentation() = default;
TitleForegroundPresentation::~TitleForegroundPresentation() = default;

bool TitleForegroundPresentation::Initialize(ModelCommon* modelCommon, Object3dCommon* objects,
    Camera* camera, const CloudVolume* background) {
    impl_ = std::make_unique<Impl>();
    auto& p = *impl_;
    if (!modelCommon || !objects || !camera || !background) return false;
    p.camera = camera; p.objects = objects;
    const auto path = AssetPath();
    p.skeleton = GltfSkeletonLoader::LoadFromFile(path);
    std::string error;
    if (!p.skeleton || !DetectKrakenTentacleChains(*p.skeleton, p.chains, error) || p.chains.size() != 4) {
        Logger::Log("[TitleForeground] Skeleton initialization failed: " + error);
        impl_.reset(); return false;
    }
    for (const auto& j : p.skeleton->joints) p.rest.push_back({ j.localScale, j.localRotate, j.localTranslate });
    p.model = std::make_unique<GltfSkinnedModel>();
    if (!p.model->Initialize(modelCommon, p.skeleton.get(), path)) {
        Logger::Log("[TitleForeground] Model initialization failed.");
        impl_.reset(); return false;
    }
    p.model->SetUseComputeOutputVertices(true);
    p.object = std::make_unique<Object3d>();
    p.object->Initialize(objects);
    p.object->SetModel(p.model->GetModel());
    p.object->SetCamera(camera);
    p.object->SetEnvironmentMapEnabled(false);
    p.cloud = std::make_unique<CloudVolume>();
    p.cloudPass = std::make_unique<VolumetricCloudPass>();
    p.cloudPass->Initialize(objects->GetDxCommon(), SrvManager::GetInstance());
    p.cloudPass->ConfigureTitleForegroundCopy(camera, background, p.cloud.get());
    p.cloudAlpha = p.cloud->GetParameters().color.w;
    p.CaptureFrame();
    return true;
}

void TitleForegroundPresentation::BeginDeparture() {
    if (!impl_ || impl_->departure >= 0.0f) return;
    // Freeze before the camera moves. The model does not chase it.
    impl_->CaptureFrame();
    impl_->departure = 0.0f;
    impl_->departingRise = impl_->rise;
}

void TitleForegroundPresentation::Update(float deltaTime, const Transform& aircraft) {
    if (!impl_) return;
    auto& p = *impl_;
    const float dt = std::isfinite(deltaTime) ? (std::max)(0.0f, deltaTime) : 0.0f;
    if (p.departure >= 0.0f) p.departure += dt;
    else { p.time += dt; p.CaptureFrame(); }
    auto& cloud = p.cloud->GetParameters();
    // Reconstruct the existing depth texture in this volume's coordinate frame.
    // Freeze the frame on click, so neither tentacles nor clouds chase the camera.
    p.cloudCamera.SetRenderCoordinateFrame(*p.camera, p.frame);
    if (p.departure < 0.0f) {
        cloud.center = PlaceTitleForegroundCloud(*p.camera, aircraft, cloud.halfExtents);
        const auto& origin = p.cloudCamera.GetTranslate();
        cloud.center = { cloud.center.x+origin.x, cloud.center.y+origin.y, cloud.center.z+origin.z };
    }
    cloud.color.w = p.cloudAlpha * (p.departure < 0.0f ? 1.0f : 1.0f - Smooth(p.departure / kCloudExitSeconds));
    for (int side = 0; side < 2; ++side) p.rise[side] = p.departure < 0.0f ? Rise(p.time, side)
        : p.departingRise[side] * (1.0f - Smooth(p.departure / kRetreatSeconds));
    const float opacity = p.departure < 0.0f ? 1.0f : 1.0f - Smooth(p.departure / kTentacleFadeSeconds);
    p.model->SetOpacity(opacity);
    p.poseVisible = opacity > 0.0f && (p.rise[0] > 0.0f || p.rise[1] > 0.0f);
    if (p.poseVisible) {
        p.poseVisible = BuildTitleForegroundPose(*p.skeleton, p.rest, p.chains, p.time, p.rise, p.aspect, p.fov);
        if (p.poseVisible) {
            p.model->UpdateSkinning();
            p.object->SetTranslate(p.framePosition);
            p.object->SetRotate(p.frameRotation);
            p.object->Update();
        }
    }
}

void TitleForegroundPresentation::DrawTentacles() {
    if (!impl_ || !impl_->poseVisible) return;
    auto& p = *impl_;
    p.model->DispatchComputeSkinning(p.objects->GetDxCommon()->GetCommandList());
    p.objects->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
    p.object->Draw();
}

void TitleForegroundPresentation::DrawClouds() {
    if (!impl_ || impl_->cloud->GetParameters().color.w <= 0.0f) return;
    auto& p = *impl_;
    auto bounds = p.cloudPass->BuildProjectedBounds(&p.cloudCamera, p.cloud.get());
    if (!bounds.isVisible || bounds.isPassSkipped) return;
    if (p.departure < 0.0f) {
        // This translated box crosses the near plane, so the general AABB path
        // falls back to fullscreen. Its known top edge gives a tighter safe rect.
        const auto& volume = p.cloud->GetParameters();
        const float top = volume.center.y + volume.halfExtents.y - p.cloudCamera.GetTranslate().y;
        const float edge = 0.5f - top / (2.0f*kCloudFarFace*std::tan(p.fov*0.5f));
        const LONG firstRow = std::clamp(static_cast<LONG>(std::floor(edge*WinApp::kClientHeight))-2,
            0L, static_cast<LONG>(WinApp::kClientHeight-1));
        bounds.scissorRect = { 0, firstRow, WinApp::kClientWidth, WinApp::kClientHeight };
        bounds.useFullScreenScissor = false;
        bounds.scissorAreaRatio = 1.0f - static_cast<float>(firstRow)/WinApp::kClientHeight;
    }
    auto* dx = p.objects->GetDxCommon();
    dx->TransitionDepthBuffer(D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    p.cloudPass->Render(&p.cloudCamera, p.cloud.get(), bounds);
    dx->TransitionDepthBuffer(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    const auto rtv = dx->GetRenderTextureRTV();
    const auto dsv = dx->GetDepthStencilView();
    dx->GetCommandList()->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
}
