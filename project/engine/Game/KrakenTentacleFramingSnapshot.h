#pragma once

#include "Matrix4x4.h"

#include <array>
#include <cstddef>

struct KrakenTentacleFramingSnapshot {
    static constexpr std::size_t kChainCapacity = 4;

    Vector3 screenMinimum{};
    Vector3 screenMaximum{};
    std::array<Vector3, kChainCapacity> tipWorldPositions{};
    std::array<Vector3, kChainCapacity> upperMidpointWorldPositions{};
    std::array<bool, kChainCapacity> tipValid{};
    std::array<bool, kChainCapacity> upperMidpointValid{};
    float screenHeightOccupancy = 0.0f;
    bool screenBoundsValid = false;
    bool rootSideHidden = false;
    bool nearPlaneWarning = false;
};
