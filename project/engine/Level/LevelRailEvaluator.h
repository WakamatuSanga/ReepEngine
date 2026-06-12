#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstddef>
#include <vector>

enum class RailInterpolationMode {
    Linear,
    CatmullRom,
};

struct LevelRailSampleTable {
    std::vector<Vector3> originalPoints;
    std::vector<Vector3> sampledPoints;
    std::vector<Vector3> sampledForwards;
    std::vector<float> cumulativeDistances;
    std::vector<size_t> sampledSegmentIndices;
    float totalLength = 0.0f;
    bool loop = false;
    RailInterpolationMode interpolationMode = RailInterpolationMode::CatmullRom;
};

struct LevelRailSampleEvaluation {
    bool valid = false;
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    Vector3 forward{ 0.0f, 0.0f, 1.0f };
    size_t segmentIndex = 0;
    float totalLength = 0.0f;
    float distance = 0.0f;
    float t = 0.0f;
};

struct LevelRailClosestPoint {
    bool valid = false;
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    Vector3 forward{ 0.0f, 0.0f, 1.0f };
    size_t segmentIndex = 0;
    float totalLength = 0.0f;
    float distance = 0.0f;
    float t = 0.0f;
    float distanceToPoint = 0.0f;
};

class LevelRailEvaluator {
public:
    static void BuildSampleTable(
        LevelRailSampleTable& table,
        const std::vector<Vector3>& points,
        bool loop,
        RailInterpolationMode interpolationMode,
        int subdivisionsPerSegment);
    static LevelRailSampleEvaluation EvaluateByDistance(
        const LevelRailSampleTable& table,
        float distance,
        bool loopEnabled);
    static LevelRailClosestPoint FindClosestPoint(
        const LevelRailSampleTable& table,
        const Vector3& position,
        bool loopEnabled);
    static const char* GetModeName(RailInterpolationMode mode);
};
