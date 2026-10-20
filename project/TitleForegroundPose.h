#pragma once
#include <array>
#include <vector>

struct Vector3;
struct Transform;
struct Skeleton;
struct KrakenTentacleChain;
class Camera;
Vector3 PlaceTitleForegroundCloud(const Camera& camera, const Transform& aircraft, const Vector3& halfExtents);
// Title-only screen layout; omitted JSON fields use these defaults.
struct TitleForegroundSideSettings {
    float centerX;
    float rootY = 1.10f;
    float height = 1.03f;
    float width = 1.0f;
    float yaw;
    float lean = 0.0f;
};
struct TitleForegroundLayoutSettings {
    std::array<TitleForegroundSideSettings, 2> sides{{
        {0.065f, 1.10f, 1.03f, 1.0f, 0.90f, 0.0f},
        {0.935f, 1.10f, 1.03f, 1.0f, -0.90f, 0.0f}
    }};
};
TitleForegroundLayoutSettings& GetTitleForegroundLayoutSettings();
bool LoadTitleForegroundLayoutSettings(const char* path);
bool SaveTitleForegroundLayoutSettings(const char* path);
// Rest locals are immutable; every pose is evaluated from them, never accumulated.
bool BuildTitleForegroundPose(Skeleton& skeleton, const std::vector<Transform>& rest,
    const std::vector<KrakenTentacleChain>& chains, float time,
    const std::array<float, 2>& rise, float aspect, float fov);
