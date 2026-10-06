#include "TitleForegroundPose.h"
#include "TitleForegroundMotion.h"
#include "TitleWaitingAircraft.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Animation/Skeleton.h"
#include "Engine/Game/Boss/Kraken/KrakenTentaclePoseEvaluator.h"

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
    if (chains.size() != 4 || rest.size() != skeleton.joints.size()) return false;
    for (size_t i = 0; i < rest.size(); ++i) {
        auto& joint = skeleton.joints[i];
        joint.localScale = rest[i].scale;
        joint.localRotate = rest[i].rotate;
        joint.localTranslate = rest[i].translate;
    }
    KrakenTentacleIdlePoseSettings sway;
    sway.frequencyHz = 0.16f;
    sway.rootAmplitudeDegrees = 0.5f;
    sway.tipAmplitudeDegrees = 3.0f;
    sway.secondaryAmplitudeDegrees = 1.0f;
    KrakenTentacleIdlePoseResult pose;
    if (!BuildKrakenTentacleIdlePose(sway, time, chains, true, 0,
        skeleton.joints.size(), skeleton.root, pose)) return false;
    for (const auto& offset : pose.joints) {
        auto& r = skeleton.joints[offset.jointIndex].localRotate;
        r.x += offset.localEulerOffsetRadians.x;
        r.y += offset.localEulerOffsetRadians.y;
        r.z += offset.localEulerOffsetRadians.z;
    }
    const float height = 2.0f * kDepth * std::tan(fov * 0.5f);
    // Source mesh is approximately 10.8 units tall. Roots remain below the frame.
    const float scale = kHeight * height / 10.8f;
    for (size_t chain = 0; chain < chains.size(); ++chain) {
        if (chains[chain].joints.empty()) return false;
        auto& root = skeleton.joints[chains[chain].joints.front()];
        // Unused chains are wholly below the frustum; no bind data/mesh edits.
        root.localTranslate = { 0.0f, -1000.0f, kDepth };
        root.localScale = { scale, scale, scale };
        if (chain != 0 && chain != 3) continue;
        const int side = chain == 0 ? 0 : 1;
        root.localTranslate = { (CenterX(time, side) - 0.5f) * height * aspect,
            (0.5f - kRootY - (1.0f - rise[side]) * kStorageDrop) * height, kDepth };
    }
    UpdateSkeletonWorldTransforms(skeleton);
    return true;
}
