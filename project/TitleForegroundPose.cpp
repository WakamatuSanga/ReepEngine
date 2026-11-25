#include "TitleForegroundPose.h"
#include "TitleForegroundMotion.h"
#include "TitleWaitingAircraft.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Animation/Skeleton.h"
#include "Engine/Game/Boss/Kraken/KrakenTentaclePoseEvaluator.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <string>

namespace {
template<class Settings, class F> void LayoutFields(Settings& settings, F visit) {
    for (size_t i = 0; i < settings.sides.size(); ++i) {
        auto& s = settings.sides[i];
        const std::string prefix = i == 0 ? "left" : "right";
        visit(prefix+"CenterX", s.centerX, i == 0 ? 0.03f : 0.88f, i == 0 ? 0.12f : 0.97f);
        visit(prefix+"RootY", s.rootY, 1.04f, 1.20f);
        visit(prefix+"Height", s.height, 0.88f, 1.15f);
        visit(prefix+"Width", s.width, 0.65f, 1.10f);
        visit(prefix+"Yaw", s.yaw, -1.2f, 1.2f);
        visit(prefix+"Lean", s.lean, -0.06f, 0.06f);
    }
}
TitleForegroundLayoutSettings layoutSettings;
}

const TitleForegroundLayoutSettings& GetTitleForegroundLayoutSettings() {
    return layoutSettings;
}

bool SetTitleForegroundLayoutSettings(const TitleForegroundLayoutSettings& settings) {
    bool valid = true;
    LayoutFields(settings, [&](const std::string&, float value, float, float) {
        valid &= std::isfinite(value);
    });
    if (valid) layoutSettings = settings;
    return valid;
}

bool LoadTitleForegroundLayoutSettings(const char* path) {
    std::ifstream file(path);
    if (!file) return false;
    const std::string json((std::istreambuf_iterator<char>(file)), {});
    if (json.find('{') == std::string::npos) return false;
    TitleForegroundLayoutSettings next;
    bool valid = true;
    LayoutFields(next, [&](const std::string& key, float& value, float low, float high) {
        const std::regex pattern("\"" + key + "\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)");
        std::smatch match;
        if (std::regex_search(json, match, pattern)) {
            try { const float v = std::stof(match[1].str()); if (std::isfinite(v)) value = std::clamp(v, low, high); }
            catch (...) { valid = false; }
        }
    });
    return valid && SetTitleForegroundLayoutSettings(next);
}
bool SaveTitleForegroundLayoutSettings(const char* path) {
    std::error_code error;
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, error);
    if (error) return false;
    std::ofstream file(path);
    if (!file) return false;
    file.precision(9);
    file << "{\n  \"version\": 1";
    LayoutFields(GetTitleForegroundLayoutSettings(), [&](const std::string& key, float v, float, float) { file << ",\n  \"" << key << "\": " << v; });
    file << "\n}\n";
    return static_cast<bool>(file);
}

Vector3 PlaceTitleForegroundCloud(const Camera& camera, const Transform& aircraft, const Vector3& halfExtents) {
    using namespace TitleForegroundMotion;
    // Preserve the cloud's existing screen placement when the idle aircraft is reframed.
    // Evaluate its hull margin using the former 75% framing, without changing the draw camera.
    constexpr float kCloudReferenceScreenY = 0.75f;
    const float tanHalfFov = std::tan(camera.GetFovY() * 0.5f);
    const float framingCorrection = std::atan((2.0f * kCloudReferenceScreenY - 1.0f) * tanHalfFov)
        - std::atan((2.0f * TitleWaitingAircraft::kOrbitScreenY - 1.0f) * tanHalfFov);
    const auto placementView = MatrixMath::Multipty(camera.GetViewMatrix(),
        MatrixMath::MakeRotateX(framingCorrection));
    const auto placementViewProjection = MatrixMath::Multipty(placementView, camera.GetProjectionMatrix());
    const auto m = MatrixMath::Multipty(MatrixMath::MakeAffine(aircraft.scale, aircraft.rotate,
        aircraft.translate), placementViewProjection);
    float bottom = 0.75f;
    for (int corner = 0; corner < 8; ++corner) {
        const Vector3 p{ corner & 1 ? TitleWaitingAircraft::kHullMax.x : TitleWaitingAircraft::kHullMin.x,
            corner & 2 ? TitleWaitingAircraft::kHullMax.y : TitleWaitingAircraft::kHullMin.y,
            corner & 4 ? TitleWaitingAircraft::kHullMax.z : TitleWaitingAircraft::kHullMin.z };
        const float w = p.x*m.m[0][3]+p.y*m.m[1][3]+p.z*m.m[2][3]+m.m[3][3];
        if (w > camera.GetNearClip()) {
            const float y = p.x*m.m[0][1]+p.y*m.m[1][1]+p.z*m.m[2][1]+m.m[3][1];
            bottom = (std::max)(bottom, 0.5f - 0.5f*y/w);
        }
    }
    // Translate the unscaled box in camera coordinates. Its far/top edge is the
    // highest projected point; every nearer point projects farther down.
    const float top = (0.5f - bottom - kCloudScreenMargin) *
        2.0f * kCloudFarFace * std::tan(camera.GetFovY()*0.5f);
    return { 0.0f, top - halfExtents.y, kCloudFarFace - halfExtents.z };
}

bool BuildTitleForegroundPose(Skeleton& skeleton, const std::vector<Transform>& rest,
    const std::vector<KrakenTentacleChain>& chains, float time,
    const std::array<float, 2>& rise, float aspect, float fov) {
    using namespace TitleForegroundMotion;
    if (chains.size() != 4 || rest.size() != skeleton.GetJoints().size()) return false;
    for (size_t i = 0; i < rest.size(); ++i) {
        skeleton.SetJointLocalTransform(i, rest[i]);
    }
    KrakenTentacleIdlePoseSettings sway;
    sway.frequencyHz = 0.16f;
    sway.rootAmplitudeDegrees = 0.5f;
    sway.tipAmplitudeDegrees = 3.0f;
    sway.secondaryAmplitudeDegrees = 1.0f;
    KrakenTentacleIdlePoseResult pose;
    if (!BuildKrakenTentacleIdlePose(sway, time, chains, true, 0,
        skeleton.GetJoints().size(), skeleton.GetRootIndex(), pose)) return false;
    for (const auto& offset : pose.joints) {
        auto r = skeleton.GetJoints()[offset.jointIndex].localRotate;
        r.x += offset.localEulerOffsetRadians.x;
        r.y += offset.localEulerOffsetRadians.y;
        r.z += offset.localEulerOffsetRadians.z;
        skeleton.SetJointLocalRotation(offset.jointIndex, r);
    }
    const float height = 2.0f * kDepth * std::tan(fov * 0.5f);
    // Preserve height on narrow screens; compress only the title's horizontal cross-section.
    const float aspectWidth = (std::min)(1.0f, (std::max)(aspect, 0.1f) / (16.0f/9.0f));
    const auto& layout = GetTitleForegroundLayoutSettings();
    for (size_t chain = 0; chain < chains.size(); ++chain) {
        if (chains[chain].joints.empty()) return false;
        const auto rootIndex = chains[chain].joints.front();
        auto rotation = skeleton.GetJoints()[rootIndex].localRotate;
        // Unused chains are wholly below the frustum; no bind data/mesh edits.
        if (chain != 0 && chain != 3) {
            skeleton.SetJointLocalTransform(rootIndex, {{1, 1, 1}, rotation, {0.0f, -1000.0f, kDepth}});
            continue;
        }
        const int side = chain == 0 ? 0 : 1;
        const auto& settings = layout.sides[side];
        const float scale = settings.height * height / 10.8f;
        rotation.y += settings.yaw;
        rotation.z += settings.lean;
        skeleton.SetJointLocalTransform(rootIndex, {{scale, scale, scale}, rotation, { (settings.centerX + SwayX(time, side) - 0.5f) * height * aspect,
            (0.5f - settings.rootY - (1.0f - rise[side]) * kStorageDrop) * height, kDepth }});
    }
    skeleton.UpdateWorldTransforms();
    // Width is a title camera-space adjustment, applied after the rest-bone
    // orientation and sway. Scaling bone-local axes also shortens the tips.
    // Keep each root fixed and update the matrices consumed by skinning together.
    for (int side = 0; side < 2; ++side) {
        const auto& chain = chains[side == 0 ? 0 : 3];
        const float rootX = skeleton.GetJoints()[chain.joints.front()].worldTranslate.x;
        const float width = layout.sides[side].width * aspectWidth;
        const auto adjustment = MatrixMath::Multipty(
            MatrixMath::Multipty(MatrixMath::MakeTranslate({ -rootX, 0, 0 }),
                MatrixMath::MakeScale({ width, 1, 1 })),
            MatrixMath::MakeTranslate({ rootX, 0, 0 }));
        for (const auto index : chain.joints) {
            skeleton.ApplyJointWorldAdjustment(index, adjustment);
        }
    }
    return true;
}
