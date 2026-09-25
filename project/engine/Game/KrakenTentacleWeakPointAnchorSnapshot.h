#pragma once

#include "Matrix4x4.h"

#include <cstdint>
#include <vector>

struct KrakenTentacleWeakPointAnchorSnapshot {
    std::uint64_t colliderId = 0;
    std::uint32_t chainIndex = 0;
    Vector3 worldCenter{};
    float worldRadius = 0.0f;
    bool enabled = false;
    bool valid = false;
};

// Selection/display geometry is separate from the weak-point homing anchor.
struct KrakenTentacleLockOnSegment {
    Vector3 worldStart{};
    Vector3 worldEnd{};
    float worldRadius = 0.0f;
};

struct KrakenTentacleLockOnGeometrySnapshot {
    std::vector<KrakenTentacleLockOnSegment> segments;
    Vector3 markerWorldPosition{};
};
