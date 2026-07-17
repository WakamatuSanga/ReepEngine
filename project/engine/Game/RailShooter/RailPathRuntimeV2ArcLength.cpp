#include "RailPathRuntimeV2.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float kMinimumDistance = 0.000001f;

Vector3 Add(const Vector3& a, const Vector3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
Vector3 Subtract(const Vector3& a, const Vector3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
Vector3 Scale(const Vector3& value, float scale) { return { value.x * scale, value.y * scale, value.z * scale }; }
Vector3 Lerp(const Vector3& from, const Vector3& to, float t) { return Add(from, Scale(Subtract(to, from), t)); }
float Length(const Vector3& value) { return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z); }
}

bool RailPathRuntimeV2::BuildArcLengthTable() {
    arcLengthTable_.clear();
    totalLength_ = 0.0f;
    const size_t segmentCount = GetSegmentCount();
    const size_t predictedCount = 1 + segmentCount * static_cast<size_t>(samplesPerSegment_);
    if (segmentCount == 0 || samplesPerSegment_ < 1 || predictedCount > 65536) {
        lastError_ = "Arc Length Table のサンプル数が不正です。";
        return false;
    }

    const Vector3 start = EvaluatePosition(0, 0.0f);
    arcLengthTable_.push_back({ 0.0f, 0, 0.0f, start });
    Vector3 previous = start;
    for (size_t segment = 0; segment < segmentCount; ++segment) {
        for (int step = 1; step <= samplesPerSegment_; ++step) {
            const float t = static_cast<float>(step) / static_cast<float>(samplesPerSegment_);
            const Vector3 position = EvaluatePosition(static_cast<uint32_t>(segment), t);
            if (!IsFinite(position)) {
                lastError_ = "Arc Length Table に有限値でない座標が含まれます。";
                return false;
            }
            const float distance = Length(Subtract(position, previous));
            if (distance <= kMinimumDistance) {
                continue;
            }
            totalLength_ += distance;
            arcLengthTable_.push_back({ totalLength_, static_cast<uint32_t>(segment), t, position });
            previous = position;
        }
    }
    return arcLengthTable_.size() >= 2 && totalLength_ > kMinimumDistance;
}

bool RailPathRuntimeV2::ValidateBuild() {
    arcTableValid_ = arcLengthTable_.size() >= 2 && totalLength_ > kMinimumDistance;
    cumulativeDistanceMonotonic_ = arcTableValid_;
    for (size_t index = 1; index < arcLengthTable_.size(); ++index) {
        if (!(arcLengthTable_[index].cumulativeDistance > arcLengthTable_[index - 1].cumulativeDistance)) {
            cumulativeDistanceMonotonic_ = false;
            break;
        }
    }
    const bool endpointsValid = arcTableValid_
        && Length(Subtract(arcLengthTable_.front().position, nodes_.front().position)) <= 0.001f
        && Length(Subtract(arcLengthTable_.back().position, nodes_.back().position)) <= 0.001f;
    if (!arcTableValid_ || !cumulativeDistanceMonotonic_ || !endpointsValid) {
        lastError_ = "Arc Length Table の検証に失敗しました。";
        return false;
    }
    return true;
}

RailPathSample RailPathRuntimeV2::SampleByDistance(float distance) const {
    RailPathSample result;
    if (!valid_ || arcLengthTable_.size() < 2) {
        return result;
    }
    result.distance = std::clamp(distance, 0.0f, totalLength_);
    result.normalizedDistance = totalLength_ > kMinimumDistance ? result.distance / totalLength_ : 0.0f;
    auto upper = std::lower_bound(
        arcLengthTable_.begin(), arcLengthTable_.end(), result.distance,
        [](const RailArcLengthSample& sample, float value) { return sample.cumulativeDistance < value; });
    size_t upperIndex = static_cast<size_t>(std::distance(arcLengthTable_.begin(), upper));
    if (upperIndex == 0) {
        upperIndex = 1;
    } else if (upperIndex >= arcLengthTable_.size()) {
        upperIndex = arcLengthTable_.size() - 1;
    }
    const RailArcLengthSample& lowerSample = arcLengthTable_[upperIndex - 1];
    const RailArcLengthSample& upperSample = arcLengthTable_[upperIndex];
    const float span = upperSample.cumulativeDistance - lowerSample.cumulativeDistance;
    const float ratio = span > kMinimumDistance
        ? std::clamp((result.distance - lowerSample.cumulativeDistance) / span, 0.0f, 1.0f)
        : 0.0f;
    const float lowerGlobal = static_cast<float>(lowerSample.segmentIndex) + lowerSample.segmentT;
    const float upperGlobal = static_cast<float>(upperSample.segmentIndex) + upperSample.segmentT;
    const float global = lowerGlobal + (upperGlobal - lowerGlobal) * ratio;
    size_t segment = static_cast<size_t>(std::floor(global));
    float localT = global - static_cast<float>(segment);
    if (segment >= GetSegmentCount()) {
        segment = GetSegmentCount() - 1;
        localT = 1.0f;
    }
    result.segmentIndex = static_cast<uint32_t>(segment);
    result.segmentT = localT;
    result.position = EvaluatePosition(result.segmentIndex, result.segmentT);
    result.tangent = EvaluateTangent(result.segmentIndex, result.segmentT);
    result.valid = IsFinite(result.position) && IsFinite(result.tangent);
    return result;
}

RailPathSample RailPathRuntimeV2::SampleByNormalizedDistance(float normalizedDistance) const {
    return SampleByDistance(std::clamp(normalizedDistance, 0.0f, 1.0f) * totalLength_);
}

void RailPathRuntimeV2::RunDistanceValidation() {
    validationSampleCount_ = 0;
    validationMinimumStep_ = 0.0f;
    validationMaximumStep_ = 0.0f;
    validationAverageStep_ = 0.0f;
    validationMaximumError_ = 0.0f;
    validationSucceeded_ = false;
    if (!valid_ || validationInterval_ <= kMinimumDistance) {
        return;
    }
    const RailPathSample start = SampleByDistance(0.0f);
    const RailPathSample end = SampleByDistance(totalLength_);
    const RailPathSample clampedStart = SampleByDistance(-1.0f);
    const RailPathSample clampedEnd = SampleByDistance(totalLength_ + 1.0f);
    const RailPathSample normalizedStart = SampleByNormalizedDistance(-1.0f);
    const RailPathSample normalizedEnd = SampleByNormalizedDistance(2.0f);
    if (!start.valid || !end.valid || !clampedStart.valid || !clampedEnd.valid) {
        return;
    }
    float previousDistance = 0.0f;
    Vector3 previousPosition = start.position;
    float sum = 0.0f;
    validationMinimumStep_ = (std::numeric_limits<float>::max)();
    for (float distance = validationInterval_; distance < totalLength_; distance += validationInterval_) {
        const RailPathSample sample = SampleByDistance(distance);
        const float actual = Length(Subtract(sample.position, previousPosition));
        const float expected = distance - previousDistance;
        validationMinimumStep_ = (std::min)(validationMinimumStep_, actual);
        validationMaximumStep_ = (std::max)(validationMaximumStep_, actual);
        validationMaximumError_ = (std::max)(validationMaximumError_, std::fabs(actual - expected));
        sum += actual;
        ++validationSampleCount_;
        previousPosition = sample.position;
        previousDistance = distance;
    }
    const float finalActual = Length(Subtract(end.position, previousPosition));
    const float finalExpected = totalLength_ - previousDistance;
    if (finalExpected > kMinimumDistance) {
        validationMinimumStep_ = (std::min)(validationMinimumStep_, finalActual);
        validationMaximumStep_ = (std::max)(validationMaximumStep_, finalActual);
        validationMaximumError_ = (std::max)(validationMaximumError_, std::fabs(finalActual - finalExpected));
        sum += finalActual;
        ++validationSampleCount_;
    }
    if (validationSampleCount_ > 0) {
        validationAverageStep_ = sum / static_cast<float>(validationSampleCount_);
    } else {
        validationMinimumStep_ = 0.0f;
    }
    const float startTangentLength = Length(start.tangent);
    const float endTangentLength = Length(end.tangent);
    validationSucceeded_ = IsFinite(start.position) && IsFinite(end.position)
        && std::fabs(startTangentLength - 1.0f) <= 0.001f
        && std::fabs(endTangentLength - 1.0f) <= 0.001f
        && clampedStart.distance == 0.0f && normalizedStart.distance == 0.0f
        && std::fabs(clampedEnd.distance - totalLength_) <= kMinimumDistance
        && std::fabs(normalizedEnd.distance - totalLength_) <= kMinimumDistance
        && Length(Subtract(start.position, nodes_.front().position)) <= 0.001f
        && Length(Subtract(end.position, nodes_.back().position)) <= 0.001f;
}

void RailPathRuntimeV2::SetLegacyComparisonData(
    const std::vector<Vector3>& sampledPoints,
    const std::vector<float>& cumulativeDistances,
    float totalLength,
    const std::string& adapterMode) {
    legacySampledPoints_ = sampledPoints;
    legacyCumulativeDistances_ = cumulativeDistances;
    legacyTotalLength_ = (std::max)(totalLength, 0.0f);
    adapterMode_ = adapterMode;
    RunLegacyComparison();
    RebuildDebugObjects();
}

Vector3 RailPathRuntimeV2::SampleLegacyByNormalizedDistance(float normalizedDistance) const {
    if (legacySampledPoints_.empty() || legacyCumulativeDistances_.size() != legacySampledPoints_.size()) {
        return {};
    }
    const float distance = std::clamp(normalizedDistance, 0.0f, 1.0f) * legacyTotalLength_;
    auto upper = std::lower_bound(legacyCumulativeDistances_.begin(), legacyCumulativeDistances_.end(), distance);
    size_t upperIndex = static_cast<size_t>(std::distance(legacyCumulativeDistances_.begin(), upper));
    if (upperIndex == 0) return legacySampledPoints_.front();
    if (upperIndex >= legacySampledPoints_.size()) return legacySampledPoints_.back();
    const float lowerDistance = legacyCumulativeDistances_[upperIndex - 1];
    const float span = legacyCumulativeDistances_[upperIndex] - lowerDistance;
    const float ratio = span > kMinimumDistance ? (distance - lowerDistance) / span : 0.0f;
    return Lerp(legacySampledPoints_[upperIndex - 1], legacySampledPoints_[upperIndex], ratio);
}

void RailPathRuntimeV2::RunLegacyComparison() {
    comparisonStartDifference_ = 0.0f;
    comparisonEndDifference_ = 0.0f;
    comparisonLengthDifference_ = std::fabs(totalLength_ - legacyTotalLength_);
    comparisonMaximumDifference_ = 0.0f;
    comparisonAverageDifference_ = 0.0f;
    if (!valid_ || legacySampledPoints_.empty()) return;
    float sum = 0.0f;
    const int count = (std::max)(comparisonSampleCount_, 2);
    for (int index = 0; index < count; ++index) {
        const float normalized = static_cast<float>(index) / static_cast<float>(count - 1);
        const float difference = Length(Subtract(
            SampleByNormalizedDistance(normalized).position,
            SampleLegacyByNormalizedDistance(normalized)));
        if (index == 0) comparisonStartDifference_ = difference;
        if (index + 1 == count) comparisonEndDifference_ = difference;
        comparisonMaximumDifference_ = (std::max)(comparisonMaximumDifference_, difference);
        sum += difference;
    }
    comparisonAverageDifference_ = sum / static_cast<float>(count);
}
