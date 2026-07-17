#pragma once

#include <string>
#include <vector>

class RailPathRuntimeV2;
struct LevelRailSampleTable;
struct Vector3;

class RailPathRuntimeV2Adapter {
public:
    static bool BuildFromWaypointPositions(
        RailPathRuntimeV2& runtime,
        const std::vector<Vector3>& waypointPositions,
        const LevelRailSampleTable& legacySampleTable,
        const std::string& adapterMode);
};
