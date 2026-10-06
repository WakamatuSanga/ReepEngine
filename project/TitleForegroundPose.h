#pragma once
#include "Engine/math/Matrix4x4.h"
#include <array>
#include <vector>

struct Skeleton;
struct KrakenTentacleChain;
class Camera;
Vector3 PlaceTitleForegroundCloud(const Camera& camera, const Transform& aircraft, const Vector3& halfExtents);
// Rest locals are immutable; every pose is evaluated from them, never accumulated.
bool BuildTitleForegroundPose(Skeleton& skeleton, const std::vector<Transform>& rest,
    const std::vector<KrakenTentacleChain>& chains, float time,
    const std::array<float, 2>& rise, float aspect, float fov);
