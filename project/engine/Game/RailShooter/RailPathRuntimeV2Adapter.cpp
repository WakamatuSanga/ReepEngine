#include "RailPathRuntimeV2Adapter.h"

#include "RailPathRuntimeV2.h"
#include "Engine/Level/LevelRailEvaluator.h"

bool RailPathRuntimeV2Adapter::BuildFromWaypointPositions(
    RailPathRuntimeV2& runtime,
    const std::vector<Vector3>& waypointPositions,
    const LevelRailSampleTable& legacySampleTable,
    const std::string& adapterMode) {
    std::vector<RailSplineNode> nodes;
    nodes.reserve(waypointPositions.size());
    for (const Vector3& position : waypointPositions) {
        nodes.push_back({ position });
    }
    const bool built = runtime.Build(nodes);
    runtime.SetLegacyComparisonData(
        legacySampleTable.sampledPoints,
        legacySampleTable.cumulativeDistances,
        legacySampleTable.totalLength,
        adapterMode);
    return built;
}
