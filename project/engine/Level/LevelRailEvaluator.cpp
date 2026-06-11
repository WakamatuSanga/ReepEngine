#include "LevelRailEvaluator.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr float kMinLength = 0.0001f;

    Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 SubtractVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    }

    Vector3 ScaleVector3(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    Vector3 LerpVector3(const Vector3& from, const Vector3& to, float t) {
        return AddVector3(from, ScaleVector3(SubtractVector3(to, from), t));
    }

    float Length(const Vector3& value) {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    }

    Vector3 NormalizeOr(const Vector3& value, const Vector3& fallback) {
        const float length = Length(value);
        if (length <= kMinLength ||
            !std::isfinite(value.x) ||
            !std::isfinite(value.y) ||
            !std::isfinite(value.z)) {
            return fallback;
        }
        return { value.x / length, value.y / length, value.z / length };
    }

    float WrapDistance(float distance, float totalLength) {
        if (totalLength <= kMinLength) {
            return 0.0f;
        }

        float wrapped = std::fmod(distance, totalLength);
        if (wrapped < 0.0f) {
            wrapped += totalLength;
        }
        return wrapped;
    }

    float ClampDistance(float distance, float totalLength) {
        return std::clamp(distance, 0.0f, (std::max)(0.0f, totalLength));
    }

    float NormalizeT(float distance, float totalLength) {
        if (totalLength <= kMinLength) {
            return 0.0f;
        }
        return std::clamp(distance / totalLength, 0.0f, 1.0f);
    }

    size_t ClampIndex(int index, size_t count) {
        if (count == 0) {
            return 0;
        }
        return static_cast<size_t>(std::clamp(index, 0, static_cast<int>(count) - 1));
    }

    size_t WrapIndex(int index, size_t count) {
        if (count == 0) {
            return 0;
        }
        const int wrapped = ((index % static_cast<int>(count)) + static_cast<int>(count)) % static_cast<int>(count);
        return static_cast<size_t>(wrapped);
    }

    const Vector3& GetControlPoint(const std::vector<Vector3>& points, int index, bool loop) {
        const size_t safeIndex = loop ? WrapIndex(index, points.size()) : ClampIndex(index, points.size());
        return points[safeIndex];
    }

    Vector3 CatmullRom(
        const Vector3& p0,
        const Vector3& p1,
        const Vector3& p2,
        const Vector3& p3,
        float t) {
        const float t2 = t * t;
        const float t3 = t2 * t;
        return {
            0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t +
                (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3),
            0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t +
                (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
                (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3),
            0.5f * ((2.0f * p1.z) + (-p0.z + p2.z) * t +
                (2.0f * p0.z - 5.0f * p1.z + 4.0f * p2.z - p3.z) * t2 +
                (-p0.z + 3.0f * p1.z - 3.0f * p2.z + p3.z) * t3),
        };
    }

    void PushRawSample(
        std::vector<Vector3>& rawPoints,
        std::vector<size_t>& rawSegmentIndices,
        const Vector3& point,
        size_t segmentIndex) {
        rawPoints.push_back(point);
        rawSegmentIndices.push_back(segmentIndex);
    }

    void BuildRawLinearSamples(
        const std::vector<Vector3>& points,
        bool loop,
        std::vector<Vector3>& rawPoints,
        std::vector<size_t>& rawSegmentIndices) {
        if (points.empty()) {
            return;
        }

        for (size_t index = 0; index < points.size(); ++index) {
            PushRawSample(rawPoints, rawSegmentIndices, points[index], index);
        }
        if (loop && points.size() >= 2) {
            PushRawSample(rawPoints, rawSegmentIndices, points.front(), points.size() - 1);
        }
    }

    void BuildRawCatmullRomSamples(
        const std::vector<Vector3>& points,
        bool loop,
        int subdivisionsPerSegment,
        std::vector<Vector3>& rawPoints,
        std::vector<size_t>& rawSegmentIndices) {
        if (points.size() < 3) {
            BuildRawLinearSamples(points, loop, rawPoints, rawSegmentIndices);
            return;
        }

        const int safeSubdivisions = std::clamp(subdivisionsPerSegment, 1, 64);
        const size_t segmentCount = loop ? points.size() : points.size() - 1;
        for (size_t segment = 0; segment < segmentCount; ++segment) {
            const int i = static_cast<int>(segment);
            const Vector3& p0 = GetControlPoint(points, i - 1, loop);
            const Vector3& p1 = GetControlPoint(points, i, loop);
            const Vector3& p2 = GetControlPoint(points, i + 1, loop);
            const Vector3& p3 = GetControlPoint(points, i + 2, loop);
            if (segment == 0) {
                PushRawSample(rawPoints, rawSegmentIndices, p1, segment);
            }
            for (int step = 1; step <= safeSubdivisions; ++step) {
                const float t = static_cast<float>(step) / static_cast<float>(safeSubdivisions);
                PushRawSample(rawPoints, rawSegmentIndices, CatmullRom(p0, p1, p2, p3, t), segment);
            }
        }
    }

    void BuildDistanceTable(
        LevelRailSampleTable& table,
        const std::vector<Vector3>& rawPoints,
        const std::vector<size_t>& rawSegmentIndices) {
        table.sampledPoints.clear();
        table.sampledForwards.clear();
        table.cumulativeDistances.clear();
        table.sampledSegmentIndices.clear();
        table.totalLength = 0.0f;

        if (rawPoints.empty()) {
            return;
        }

        table.sampledPoints.push_back(rawPoints.front());
        table.sampledSegmentIndices.push_back(rawSegmentIndices.empty() ? 0 : rawSegmentIndices.front());
        table.cumulativeDistances.push_back(0.0f);
        for (size_t index = 1; index < rawPoints.size(); ++index) {
            const Vector3 diff = SubtractVector3(rawPoints[index], table.sampledPoints.back());
            const float length = Length(diff);
            if (length <= kMinLength) {
                continue;
            }

            table.totalLength += length;
            table.sampledPoints.push_back(rawPoints[index]);
            table.sampledSegmentIndices.push_back(
                index < rawSegmentIndices.size() ? rawSegmentIndices[index] : table.sampledSegmentIndices.back());
            table.cumulativeDistances.push_back(table.totalLength);
        }

        table.sampledForwards.resize(table.sampledPoints.size(), { 0.0f, 0.0f, 1.0f });
        Vector3 previousForward{ 0.0f, 0.0f, 1.0f };
        for (size_t index = 0; index < table.sampledPoints.size(); ++index) {
            Vector3 diff{};
            if (index + 1 < table.sampledPoints.size()) {
                diff = SubtractVector3(table.sampledPoints[index + 1], table.sampledPoints[index]);
            } else if (index > 0) {
                diff = SubtractVector3(table.sampledPoints[index], table.sampledPoints[index - 1]);
            }
            previousForward = NormalizeOr(diff, previousForward);
            table.sampledForwards[index] = previousForward;
        }
    }
}

void LevelRailEvaluator::BuildSampleTable(
    LevelRailSampleTable& table,
    const std::vector<Vector3>& points,
    bool loop,
    RailInterpolationMode interpolationMode,
    int subdivisionsPerSegment) {
    table = {};
    table.originalPoints = points;
    table.loop = loop;
    table.interpolationMode = interpolationMode;

    std::vector<Vector3> rawPoints;
    std::vector<size_t> rawSegmentIndices;
    if (interpolationMode == RailInterpolationMode::CatmullRom && points.size() >= 3) {
        BuildRawCatmullRomSamples(points, loop, subdivisionsPerSegment, rawPoints, rawSegmentIndices);
    } else {
        BuildRawLinearSamples(points, loop, rawPoints, rawSegmentIndices);
    }
    BuildDistanceTable(table, rawPoints, rawSegmentIndices);
}

LevelRailSampleEvaluation LevelRailEvaluator::EvaluateByDistance(
    const LevelRailSampleTable& table,
    float distance,
    bool loopEnabled) {
    LevelRailSampleEvaluation result;
    result.totalLength = table.totalLength;
    if (table.sampledPoints.empty()) {
        return result;
    }

    result.valid = true;
    if (table.sampledPoints.size() == 1 || table.totalLength <= kMinLength) {
        result.position = table.sampledPoints.front();
        result.forward = table.sampledForwards.empty() ? Vector3{ 0.0f, 0.0f, 1.0f } : table.sampledForwards.front();
        return result;
    }

    result.distance = loopEnabled
        ? WrapDistance(distance, table.totalLength)
        : ClampDistance(distance, table.totalLength);
    result.t = NormalizeT(result.distance, table.totalLength);

    auto upper = std::lower_bound(
        table.cumulativeDistances.begin(),
        table.cumulativeDistances.end(),
        result.distance);
    size_t upperIndex = static_cast<size_t>(std::distance(table.cumulativeDistances.begin(), upper));
    if (upperIndex == 0) {
        result.position = table.sampledPoints.front();
        result.forward = table.sampledForwards.front();
        result.segmentIndex = table.sampledSegmentIndices.empty() ? 0 : table.sampledSegmentIndices.front();
        return result;
    }
    if (upperIndex >= table.sampledPoints.size()) {
        upperIndex = table.sampledPoints.size() - 1;
    }

    const size_t lowerIndex = upperIndex - 1;
    const float lowerDistance = table.cumulativeDistances[lowerIndex];
    const float upperDistance = table.cumulativeDistances[upperIndex];
    const float segmentLength = upperDistance - lowerDistance;
    const float localT = segmentLength > kMinLength
        ? std::clamp((result.distance - lowerDistance) / segmentLength, 0.0f, 1.0f)
        : 0.0f;

    result.position = LerpVector3(table.sampledPoints[lowerIndex], table.sampledPoints[upperIndex], localT);
    result.forward = NormalizeOr(
        LerpVector3(table.sampledForwards[lowerIndex], table.sampledForwards[upperIndex], localT),
        table.sampledForwards[lowerIndex]);
    result.segmentIndex = table.sampledSegmentIndices.empty() ? lowerIndex : table.sampledSegmentIndices[lowerIndex];
    return result;
}

const char* LevelRailEvaluator::GetModeName(RailInterpolationMode mode) {
    switch (mode) {
    case RailInterpolationMode::Linear:
        return "Linear";
    case RailInterpolationMode::CatmullRom:
    default:
        return "CatmullRom";
    }
}
