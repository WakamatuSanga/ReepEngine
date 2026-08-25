#pragma once

#include "Matrix4x4.h"

#include <cstdint>

struct KrakenTentacleWeakPointAnchorSnapshot {
    std::uint32_t chainIndex = 0;
    Vector3 worldCenter{};
    float worldRadius = 0.0f;
    bool enabled = false;
    bool valid = false;
};
