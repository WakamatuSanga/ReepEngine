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
