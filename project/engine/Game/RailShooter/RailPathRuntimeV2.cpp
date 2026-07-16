#include "RailPathRuntimeV2.h"

#include "Engine/Utility/Logger.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float kMinimumDistance = 0.000001f;

Vector3 Add(const Vector3& a, const Vector3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
Vector3 Subtract(const Vector3& a, const Vector3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
Vector3 Scale(const Vector3& value, float scale) { return { value.x * scale, value.y * scale, value.z * scale }; }
float Length(const Vector3& value) { return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z); }

Vector3 Lerp(const Vector3& from, const Vector3& to, float t) {
    return Add(from, Scale(Subtract(to, from), t));
}

Vector3 SafeKnotLerp(const Vector3& from, const Vector3& to, float knotFrom, float knotTo, float knot) {
    const float denominator = knotTo - knotFrom;
    if (std::fabs(denominator) <= kMinimumDistance) {
        return from;
    }
    return Lerp(from, to, (knot - knotFrom) / denominator);
}
}

bool RailPathRuntimeV2::Build(const std::vector<RailSplineNode>& nodes) {
    ClearDebugObjects();
    sourceNodes_ = nodes;
    nodes_.clear();
    arcLengthTable_.clear();
    totalLength_ = 0.0f;
    duplicateNodeSkipCount_ = 0;
    valid_ = false;
    arcTableValid_ = false;
    cumulativeDistanceMonotonic_ = false;
    validationSucceeded_ = false;
    lastError_.clear();

    for (size_t index = 0; index < nodes.size(); ++index) {
        if (!IsFinite(nodes[index].position)) {
            lastError_ = "有限値でないノードを検出しました。";
            lastBuildResult_ = "構築失敗";
            return false;
        }
        if (!nodes_.empty() && Length(Subtract(nodes[index].position, nodes_.back().position)) <= duplicateNodeDistanceEpsilon_) {
            ++duplicateNodeSkipCount_;
            Logger::Log("[RailPathV2] Consecutive duplicate node skipped: index=" + std::to_string(index) + "\n");
            continue;
        }
        nodes_.push_back(nodes[index]);
    }

    if (nodes_.size() < 2) {
        lastError_ = "有効なノードが2点未満です。";
        lastBuildResult_ = "構築失敗";
        return false;
    }
    if (!BuildArcLengthTable() || !ValidateBuild()) {
        lastBuildResult_ = "構築失敗";
        return false;
    }

    valid_ = true;
    lastBuildResult_ = nodes_.size() == 2 ? "構築成功（直線補間）" : "構築成功";
    previewDistance_ = std::clamp(previewDistance_, 0.0f, totalLength_);
    previewNormalizedDistance_ = totalLength_ > kMinimumDistance ? previewDistance_ / totalLength_ : 0.0f;
    previewSample_ = SampleByDistance(previewDistance_);
    RunDistanceValidation();
    RunLegacyComparison();
    RebuildDebugObjects();
    return true;
}

bool RailPathRuntimeV2::Rebuild() {
    const std::vector<RailSplineNode> sourceCopy = sourceNodes_;
    return Build(sourceCopy);
}

void RailPathRuntimeV2::Clear() {
    ClearDebugObjects();
    sourceNodes_.clear();
    nodes_.clear();
    arcLengthTable_.clear();
    legacySampledPoints_.clear();
    legacyCumulativeDistances_.clear();
    valid_ = false;
    arcTableValid_ = false;
    cumulativeDistanceMonotonic_ = false;
    totalLength_ = 0.0f;
    legacyTotalLength_ = 0.0f;
    duplicateNodeSkipCount_ = 0;
    previewDistance_ = 0.0f;
    previewNormalizedDistance_ = 0.0f;
    previewSample_ = {};
    validationSampleCount_ = 0;
    validationMinimumStep_ = 0.0f;
    validationMaximumStep_ = 0.0f;
    validationAverageStep_ = 0.0f;
    validationMaximumError_ = 0.0f;
    validationSucceeded_ = false;
    comparisonStartDifference_ = 0.0f;
    comparisonEndDifference_ = 0.0f;
    comparisonLengthDifference_ = 0.0f;
    comparisonMaximumDifference_ = 0.0f;
    comparisonAverageDifference_ = 0.0f;
    lastBuildResult_ = "未構築";
    lastError_.clear();
    adapterMode_ = "未接続";
}

Vector3 RailPathRuntimeV2::EvaluatePosition(uint32_t segmentIndex, float segmentT) const {
    if (nodes_.size() < 2) {
        return nodes_.empty() ? Vector3{} : nodes_.front().position;
    }
    const size_t segmentCount = nodes_.size() - 1;
    const size_t segment = (std::min)(static_cast<size_t>(segmentIndex), segmentCount - 1);
    const float t = std::clamp(segmentT, 0.0f, 1.0f);
    const Vector3& p1 = nodes_[segment].position;
    const Vector3& p2 = nodes_[segment + 1].position;
    if (nodes_.size() == 2) {
        return Lerp(p1, p2, t);
    }

    const Vector3 p0 = segment > 0
        ? nodes_[segment - 1].position
        : Subtract(Scale(p1, 2.0f), p2);
    const Vector3 p3 = segment + 2 < nodes_.size()
        ? nodes_[segment + 2].position
        : Subtract(Scale(p2, 2.0f), p1);
    const auto knotStep = [this](const Vector3& a, const Vector3& b) {
        return std::pow((std::max)(Length(Subtract(b, a)), kMinimumDistance), alpha_);
    };
    const float t0 = 0.0f;
    const float t1 = t0 + knotStep(p0, p1);
    const float t2 = t1 + knotStep(p1, p2);
    const float t3 = t2 + knotStep(p2, p3);
    const float knot = t1 + (t2 - t1) * t;
    const Vector3 a1 = SafeKnotLerp(p0, p1, t0, t1, knot);
    const Vector3 a2 = SafeKnotLerp(p1, p2, t1, t2, knot);
    const Vector3 a3 = SafeKnotLerp(p2, p3, t2, t3, knot);
    const Vector3 b1 = SafeKnotLerp(a1, a2, t0, t2, knot);
    const Vector3 b2 = SafeKnotLerp(a2, a3, t1, t3, knot);
    const Vector3 result = SafeKnotLerp(b1, b2, t1, t2, knot);
    return IsFinite(result) ? result : Lerp(p1, p2, t);
}

Vector3 RailPathRuntimeV2::EvaluateGlobalParameter(float globalParameter) const {
    const size_t segmentCount = GetSegmentCount();
    if (segmentCount == 0) {
        return nodes_.empty() ? Vector3{} : nodes_.front().position;
    }
    const float clamped = std::clamp(globalParameter, 0.0f, static_cast<float>(segmentCount));
    size_t segment = static_cast<size_t>(std::floor(clamped));
    float localT = clamped - static_cast<float>(segment);
    if (segment >= segmentCount) {
        segment = segmentCount - 1;
        localT = 1.0f;
    }
    return EvaluatePosition(static_cast<uint32_t>(segment), localT);
}

Vector3 RailPathRuntimeV2::EvaluateTangent(uint32_t segmentIndex, float segmentT) const {
    if (nodes_.size() < 2) {
        return { 0.0f, 0.0f, 1.0f };
    }
    const float global = static_cast<float>((std::min)(static_cast<size_t>(segmentIndex), GetSegmentCount() - 1))
        + std::clamp(segmentT, 0.0f, 1.0f);
    constexpr float kTangentStep = 0.001f;
    const Vector3 from = EvaluateGlobalParameter(global - kTangentStep);
    const Vector3 to = EvaluateGlobalParameter(global + kTangentStep);
    Vector3 tangent = Subtract(to, from);
    float length = Length(tangent);
    if (length <= kMinimumDistance || !IsFinite(tangent)) {
        const size_t segment = (std::min)(static_cast<size_t>(segmentIndex), GetSegmentCount() - 1);
        tangent = Subtract(nodes_[segment + 1].position, nodes_[segment].position);
        length = Length(tangent);
    }
    if (length <= kMinimumDistance || !IsFinite(tangent)) {
        return { 0.0f, 0.0f, 1.0f };
    }
    return Scale(tangent, 1.0f / length);
}

bool RailPathRuntimeV2::IsFinite(const Vector3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
