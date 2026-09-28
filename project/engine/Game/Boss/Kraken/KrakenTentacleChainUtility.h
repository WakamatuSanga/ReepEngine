#pragma once

#include <string>
#include <vector>

struct Skeleton;

struct KrakenTentacleChain {
    std::vector<int> joints;
};

bool DetectKrakenTentacleChains(
    const Skeleton& skeleton,
    std::vector<KrakenTentacleChain>& outChains,
    std::string& outErrorMessage);

// Call once after restoring bind locals, before pose/world/skinning evaluation.
// Only current root translations change; bind data and the shared root stay intact.
bool ApplyKrakenTentaclePlacementToRestoredPose(
    Skeleton& skeleton,
    const std::vector<KrakenTentacleChain>& chains);
