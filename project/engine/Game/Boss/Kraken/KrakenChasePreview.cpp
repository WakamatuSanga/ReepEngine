#include "KrakenChasePreview.h"

#include "Engine/Animation/Skeleton.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Game/Camera/RailShooterCameraRig.h"
#include "Engine/Graphics/Model/GltfSkeletonLoader.h"
#include "Engine/Graphics/Model/GltfSkinnedModel.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
constexpr float kRouteEnd = 8.4f;
constexpr int kExitEnd = 13;
constexpr int kRailSamplesPerSecond = 8;
constexpr float kGlide = 6.25f;
constexpr float kModelScale = 0.82f;
constexpr float kRadians = std::numbers::pi_v<float> / 180.0f;
constexpr const char* kHeadPath = "resources/final_boss/kraken_final_head.gltf";
constexpr std::array kTentaclePaths{
    "resources/midboss/parts/kraken_tentacle_variant_01.gltf",
    "resources/midboss/parts/kraken_tentacle_variant_02.gltf",
    "resources/midboss/parts/kraken_tentacle_variant_03.gltf",
    "resources/midboss/parts/kraken_tentacle_variant_04.gltf",
};

float Ease(float from, float to, float time) {
    const float u = std::clamp((time - from) / (to - from), 0.0f, 1.0f);
    return u * u * (3.0f - 2.0f * u);
}

// Integrated speed reduction from the supplied route. The relative gap
// closes by 15.5 units, holds steady, then opens by 8 units.
float ForwardGap(float time) {
    constexpr float start = 2.28f, end = 5.30f;
    constexpr float maxSlowdown = 6.10f, totalLoss = 15.5f;
    constexpr float closeTime = end - start;
    constexpr float rampTime = closeTime - totalLoss / maxSlowdown;
    const auto integral = [](float x) {
        if (x >= rampTime) return maxSlowdown * (x - 0.5f * rampTime);
        const float u = x / rampTime;
        return maxSlowdown * rampTime * u * u * u * (1.0f - 0.5f * u);
    };
    if (time <= start) return 47.0f;
    if (time >= end) return 31.5f + 8.0f * Ease(6.55f, 8.37f, time);
    const float u = time - start;
    const float loss = u <= 0.5f * closeTime ? integral(u) : totalLoss - integral(closeTime - u);
    return 47.0f - loss;
}

Vector3 RouteRotation(float time) {
    const float yaw = -9.0f - 42.0f * Ease(0.6f, 2.15f, time) + 33.0f * Ease(2.95f, 4.4f, time)
        - 24.0f * Ease(4.63f, 5.65f, time) + 24.0f * Ease(6.25f, 7.4f, time);
    const float pitch = 11.0f * (1.0f - Ease(0.7f, 2.3f, time)) + 4.0f * Ease(6.3f, 7.5f, time);
    const float roll = -9.0f * Ease(0.8f, 2.0f, time) * (1.0f - Ease(3.3f, 4.9f, time));
    return { -pitch * kRadians, -yaw * kRadians, roll * kRadians };
}

void ApplyWorldTransform(Object3d& object, const Matrix4x4& world) {
    Matrix4x4 rotation = MatrixMath::MakeIdentity4x4();
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            rotation.m[row][column] = world.m[row][column] / kModelScale;
        }
    }
    object.SetScale({ kModelScale, kModelScale, kModelScale });
    object.SetRotate(MatrixMath::ExtractEulerXYZ(rotation));
    object.SetTranslate({ world.m[3][0], world.m[3][1], world.m[3][2] });
    object.Update();
}

struct SkinnedPart {
    std::unique_ptr<Skeleton> skeleton;
    std::unique_ptr<GltfSkinnedModel> model;
    std::unique_ptr<Object3d> object;

    bool Load(ModelCommon& modelCommon, Object3dCommon& objectCommon, Camera& camera, const char* path) {
        skeleton = GltfSkeletonLoader::LoadFromFile(path);
        if (!skeleton) return false;
        skeleton->UpdateWorldTransforms();
        model = std::make_unique<GltfSkinnedModel>();
        if (!model->Initialize(&modelCommon, skeleton.get(), path)) return false;
        model->SetUseComputeOutputVertices(true);
        object = std::make_unique<Object3d>();
        object->Initialize(&objectCommon);
        if (!object->IsValid()) return false;
        object->SetModel(model->GetModel());
        object->SetCamera(&camera);
        object->SetEnvironmentMapEnabled(false);
        return true;
    }

    void Draw(Object3dCommon& common) const {
        model->DispatchComputeSkinning(common.GetDxCommon()->GetCommandList());
        common.CommonDrawSetting();
        object->Draw();
    }
};

struct Tentacle {
    SkinnedPart part;
    size_t socketIndex{};
    Matrix4x4 meshToSocket{};
    std::vector<Vector3> bindRotations;

    void Swim(float time, float phaseOffset) {
        const float stroke = 0.055f + 0.08f * Ease(6.4f, 7.7f, time);
        const auto& joints = part.skeleton->GetJoints();
        // The chain root stays in its bind pose. Each successive joint
        // samples an older stroke and turn, so the tips trail the body.
        for (size_t joint = 1; joint < joints.size(); ++joint) {
            const float along = static_cast<float>(joint) / static_cast<float>(joints.size() - 1);
            const float delayedTime = (std::max)(0.0f, time - 0.6f * along);
            const float phase = delayedTime * 4.1f + phaseOffset;
            const float turnLag = RouteRotation(delayedTime).y - RouteRotation(time).y;
            const auto& bind = bindRotations[joint];
            part.skeleton->SetJointLocalRotation(joint, {
                bind.x + along * (stroke * std::sin(phase) - 0.11f * (1.0f - Ease(0.4f, 2.2f, delayedTime))),
                bind.y + along * 0.02f * std::sin(phase + 1.0f),
                bind.z + along * (stroke * 0.55f * std::cos(phase) + turnLag * 0.28f),
            });
        }
        part.skeleton->UpdateWorldTransforms();
        part.model->UpdateSkinning();
    }
};
}

struct KrakenChasePreview::Impl {
    SkinnedPart head;
    std::array<Tentacle, 8> tentacles;
    Object3dCommon* objectCommon = nullptr;
    const char* loadError = nullptr;
    bool loadAttempted = false;
    bool active = false;
    float time = 0.0f;
    float opacity = 0.0f;
    float speed = 0.0f;
    float playerForwardAtStart = 0.0f;
    std::array<Matrix4x4, kExitEnd * kRailSamplesPerSecond + 1> nominalRailFrames{};

    bool Load(ModelCommon& modelCommon, Object3dCommon& common, Camera& camera) {
        loadAttempted = true;
        if (!head.Load(modelCommon, common, camera, kHeadPath)) {
            loadError = kHeadPath;
            return false;
        }
        for (size_t i = 0; i < tentacles.size(); ++i) {
            auto& tentacle = tentacles[i];
            const int socket = head.skeleton->FindJointIndex("TentacleSocket_0" + std::to_string(i + 1));
            if (socket < 0) {
                loadError = "TentacleSocket_01–08";
                return false;
            }
            tentacle.socketIndex = static_cast<size_t>(socket);
            const char* path = kTentaclePaths[i % kTentaclePaths.size()];
            if (!tentacle.part.Load(modelCommon, common, camera, path)) {
                loadError = path;
                return false;
            }
            const int root = tentacle.part.skeleton->FindJointIndex("Tentacle_Root");
            const auto& joints = tentacle.part.skeleton->GetJoints();
            if (root != 0 || joints.size() < 2) {
                loadError = path;
                return false;
            }
            const auto& attachment = joints[root].worldTranslate;
            tentacle.meshToSocket = MatrixMath::MakeTranslate({ -attachment.x, -attachment.y, -attachment.z });
            tentacle.bindRotations.reserve(joints.size());
            for (const auto& joint : joints) tentacle.bindRotations.push_back(joint.localRotate);
        }
        objectCommon = &common;
        return true;
    }

    Matrix4x4 SampleNominalRailFrame() const {
        const float sample = time * kRailSamplesPerSecond;
        const size_t before = (std::min)(static_cast<size_t>(sample), nominalRailFrames.size() - 2);
        const float fraction = sample - static_cast<float>(before);
        const auto& a = nominalRailFrames[before].m;
        const auto& b = nominalRailFrames[before + 1].m;
        const Vector3 forward{
            std::lerp(a[2][0], b[2][0], fraction), std::lerp(a[2][1], b[2][1], fraction),
            std::lerp(a[2][2], b[2][2], fraction),
        };
        const Vector3 rotation{
            std::atan2(-forward.y, std::hypot(forward.x, forward.z)), std::atan2(forward.x, forward.z), 0.0f,
        };
        const Vector3 position{
            std::lerp(a[3][0], b[3][0], fraction), std::lerp(a[3][1], b[3][1], fraction),
            std::lerp(a[3][2], b[3][2], fraction),
        };
        return MatrixMath::MakeAffine({ 1.0f, 1.0f, 1.0f }, rotation, position);
    }

    void ApplyPose() {
        const float exitTime = (std::max)(0.0f, time - kRouteEnd);
        const float gapScale = (std::min)(1.35f, speed / kGlide * 0.85f);
        const float lateral = -0.6f + 8.2f * Ease(0.75f, 3.45f, time) - 2.8f * Ease(3.7f, 5.8f, time)
            + 4.1f * Ease(6.55f, kRouteEnd, time) + 1.5f * exitTime * exitTime;
        const float height = -15.0f + 19.0f * Ease(0.0f, 1.95f, time) - 0.9f * Ease(4.4f, 5.85f, time)
            + 1.1f * Ease(6.45f, kRouteEnd, time) + 0.38f * std::sin(3.5f * time);
        const float forward = playerForwardAtStart + 24.0f
            + (ForwardGap(time) - 31.5f) * gapScale + 1.8f * exitTime * exitTime;
        auto orientation = RouteRotation(time);
        const float flank = Ease(4.15f, 5.15f, time) * (1.0f - Ease(6.55f, 7.5f, time));
        orientation.y += kRadians * (14.0f * flank + 10.0f * Ease(kRouteEnd, 10.4f, time));

        // The exported model's mantle points along +Y. Rotate it into the
        // engine's +Z flight axis around the center of the head.
        const auto centerModel = MatrixMath::MakeTranslate({ 0.0f, -5.1f, 0.0f });
        const auto bankTowardPlayer = MatrixMath::MakeRotateY(36.0f * flank * kRadians);
        const auto modelAxis = MatrixMath::MakeAffine({ kModelScale, kModelScale, kModelScale },
            { (90.0f + 8.0f * flank) * kRadians, 0.0f, 0.0f }, {});
        const auto modelToFlight = MatrixMath::Multipty(MatrixMath::Multipty(centerModel, bankTowardPlayer), modelAxis);
        const auto flight = MatrixMath::MakeAffine({ 1.0f, 1.0f, 1.0f }, orientation,
            { 1.35f * lateral, height, forward });
        const auto bodyWorld = MatrixMath::Multipty(MatrixMath::Multipty(modelToFlight, flight), SampleNominalRailFrame());
        opacity = Ease(0.25f, 1.05f, time) * (1.0f - Ease(11.0f, kExitEnd, time));
        head.model->SetOpacity(opacity);
        ApplyWorldTransform(*head.object, bodyWorld);
        for (size_t i = 0; i < tentacles.size(); ++i) {
            auto& tentacle = tentacles[i];
            tentacle.Swim(time, static_cast<float>(i) * 0.78f);
            tentacle.part.model->SetOpacity(opacity);
            const auto& socket = head.skeleton->GetJoints()[tentacle.socketIndex].skeletonSpaceMatrix;
            const auto socketWorld = MatrixMath::Multipty(socket, bodyWorld);
            ApplyWorldTransform(*tentacle.part.object, MatrixMath::Multipty(tentacle.meshToSocket, socketWorld));
        }
    }
};

KrakenChasePreview::KrakenChasePreview() : impl_(std::make_unique<Impl>()) {}
KrakenChasePreview::~KrakenChasePreview() = default;

bool KrakenChasePreview::Start(ModelCommon& modelCommon, Object3dCommon& objectCommon,
    Camera& camera, const RailShooterCameraRig& rail, const Vector3& playerPosition) {
    auto& state = *impl_;
    const auto frame = rail.GetProjectileRailFrame();
    const float railSpeed = rail.GetBaseRailSpeed() * rail.GetExistingRailSpeedScale();
    if (!frame.valid || !frame.gameModeActive || !(railSpeed > 0.0f)) return false;
    if (!state.loadAttempted) state.Load(modelCommon, objectCommon, camera);
    if (!state.objectCommon) return false;

    // Snapshot the nominal rail path once, including its bends. Subsequent
    // player input, boosts and camera offsets cannot change this trajectory.
    for (size_t i = 0; i < state.nominalRailFrames.size(); ++i) {
        const float distance = frame.railDistance + railSpeed * static_cast<float>(i) / kRailSamplesPerSecond;
        state.nominalRailFrames[i] = rail.SampleSceneryFrame(distance);
    }
    const auto& basis = state.nominalRailFrames.front().m;
    state.playerForwardAtStart = (playerPosition.x - basis[3][0]) * basis[2][0]
        + (playerPosition.y - basis[3][1]) * basis[2][1] + (playerPosition.z - basis[3][2]) * basis[2][2];
    state.speed = railSpeed;
    state.time = 0.0f;
    state.active = true;
    state.ApplyPose();
    return true;
}

void KrakenChasePreview::Update(float deltaTime) {
    auto& state = *impl_;
    if (!state.active || state.time >= kExitEnd) return;
    state.time = (std::min)(state.time + deltaTime, static_cast<float>(kExitEnd));
    if (state.time < kExitEnd) state.ApplyPose();
}

void KrakenChasePreview::Draw() const {
    const auto& state = *impl_;
    if (!state.active || state.time >= kExitEnd || state.opacity <= 0.0f) return;
    state.head.Draw(*state.objectCommon);
    for (const auto& tentacle : state.tentacles) tentacle.part.Draw(*state.objectCommon);
}

void KrakenChasePreview::DrawImGui() const {
#ifdef USE_IMGUI
    if (impl_->loadError) ImGui::Text("本体アセットの読み込みに失敗しました (%s)", impl_->loadError);
    if (!impl_->active) return;
    const float time = impl_->time;
    const char* phase = time < 1.95f ? "浮上" : time < 4.4f ? "旋回・追撃" : time < 6.6f ? "並走"
        : time < kRouteEnd ? "再加速" : time < kExitEnd ? "離脱" : "検証終了・レール飛行中";
    ImGui::Text("%s  %.2f 秒", phase, time);
#endif
}

bool KrakenChasePreview::IsActive() const { return impl_->active; }
