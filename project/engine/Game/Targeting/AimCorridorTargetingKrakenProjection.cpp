#include "AimCorridorTargetingController.h"
#include "PlayerLockOnTargetProvider.h"
#include "Engine/Graphics/Camera/Camera.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {
Vector3 Lerp(const Vector3& a, const Vector3& b, float t) {
    return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t };
}

float ClosestParameter(const Vector2& point, const Vector2& a, const Vector2& b) {
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float lengthSquared = dx * dx + dy * dy;
    return lengthSquared > 0.0000001f
        ? std::clamp(((point.x - a.x) * dx + (point.y - a.y) * dy) /
            lengthSquared, 0.0f, 1.0f) : 0.0f;
}

float PointSegmentDistanceSquared(const Vector2& p, const Vector2& a, const Vector2& b) {
    const float t = ClosestParameter(p, a, b);
    const float dx = p.x - (a.x + (b.x - a.x) * t);
    const float dy = p.y - (a.y + (b.y - a.y) * t);
    return dx * dx + dy * dy;
}

bool ClipAxis(float start, float end, float minimum, float maximum, float& lo, float& hi) {
    const float delta = end - start;
    if (std::fabs(delta) < 0.0000001f) {
        return start >= minimum && start <= maximum;
    }
    const float a = (minimum - start) / delta, b = (maximum - start) / delta;
    lo = (std::max)(lo, (std::min)(a, b));
    hi = (std::min)(hi, (std::max)(a, b));
    return lo <= hi;
}

// The projected radius varies with depth. Test the swept discs section by section,
// so a near endpoint cannot enlarge the entire bone into a wide selection rectangle.
bool CapsuleOverlapsRect(Vector2 a, Vector2 b, Vector2 radiusA, Vector2 radiusB,
    Vector2 minimum, Vector2 maximum) {
    const float aspect = radiusA.x / radiusA.y;
    a.x /= aspect; b.x /= aspect;
    minimum.x /= aspect; maximum.x /= aspect;
    const float vx = b.x - a.x, vy = b.y - a.y;
    const float r0 = radiusA.y, vr = radiusB.y - radiusA.y;
    std::array<float, 6> cuts{0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    if (std::fabs(vx) > 0.0000001f) {
        cuts[2] = std::clamp((minimum.x - a.x) / vx, 0.0f, 1.0f);
        cuts[3] = std::clamp((maximum.x - a.x) / vx, 0.0f, 1.0f);
    }
    if (std::fabs(vy) > 0.0000001f) {
        cuts[4] = std::clamp((minimum.y - a.y) / vy, 0.0f, 1.0f);
        cuts[5] = std::clamp((maximum.y - a.y) / vy, 0.0f, 1.0f);
    }
    std::sort(cuts.begin(), cuts.end());
    const auto intersectsAt = [&](float t) {
        const float x = a.x + vx * t, y = a.y + vy * t;
        const float dx = x - std::clamp(x, minimum.x, maximum.x);
        const float dy = y - std::clamp(y, minimum.y, maximum.y);
        const float radius = r0 + vr * t;
        return dx * dx + dy * dy <= radius * radius + 0.00000001f;
    };
    for (std::size_t i = 0; i + 1 < cuts.size(); ++i) {
        const float lo = cuts[i], hi = cuts[i + 1], mid = (lo + hi) * 0.5f;
        if (intersectsAt(lo) || intersectsAt(hi)) {
            return true;
        }
        const auto coefficients = [mid](float start, float velocity, float low, float high) {
            const float p = start + velocity * mid;
            return p < low ? Vector2{start - low, velocity}
                : (p > high ? Vector2{start - high, velocity} : Vector2{});
        };
        const Vector2 x = coefficients(a.x, vx, minimum.x, maximum.x);
        const Vector2 y = coefficients(a.y, vy, minimum.y, maximum.y);
        const float quadratic = x.y * x.y + y.y * y.y - vr * vr;
        const float linear = 2.0f * (x.x * x.y + y.x * y.y - r0 * vr);
        if (quadratic > 0.0f && intersectsAt(std::clamp(-linear / (2.0f * quadratic), lo, hi))) {
            return true;
        }
    }
    return false;
}
}

bool AimCorridorTargetingController::ProjectKrakenTarget(
    const PlayerLockOnTargetSnapshot& source, ProjectedTarget& target,
    KrakenNaturalLockTargetDiagnostic& diagnostic) const {
    const auto& geometry = source.krakenGeometry;
    const Matrix4x4& world = camera_->GetWorldMatrix();
    const Vector3 forward{ world.m[2][0], world.m[2][1], world.m[2][2] };
    const Vector3 right{ world.m[0][0], world.m[0][1], world.m[0][2] };
    const Vector3 up{ world.m[1][0], world.m[1][1], world.m[1][2] };
    const Vector3 viewPosition = camera_->GetViewTranslate();
    const auto depth = [&](const Vector3& p) {
        return (p.x - viewPosition.x) * forward.x + (p.y - viewPosition.y) * forward.y +
            (p.z - viewPosition.z) * forward.z;
    };
    const auto projectedRadius = [&](const Vector3& p, float radius, Vector2 center) {
        Vector2 rightUv{}, upUv{};
        float w = 0.0f;
        ProjectWorldToScreen({ p.x + right.x * radius, p.y + right.y * radius,
            p.z + right.z * radius }, rightUv, w);
        ProjectWorldToScreen({ p.x + up.x * radius, p.y + up.y * radius,
            p.z + up.z * radius }, upUv, w);
        return Vector2{ (std::max)(std::fabs(rightUv.x - center.x), 0.000001f),
            (std::max)(std::fabs(upUv.y - center.y), 0.000001f) };
    };
    target.markerWorldPosition = geometry.markerWorldPosition;
    target.cameraDepth = depth(target.markerWorldPosition);
    Vector2 markerUv{};
    ProjectWorldToScreen(target.markerWorldPosition, markerUv, target.clipW);
    target.screenRadius = { minimumScreenRadius_, minimumScreenRadius_ };
    target.boundsMinimum = { (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)() };
    target.boundsMaximum = { -(std::numeric_limits<float>::max)(), -(std::numeric_limits<float>::max)() };
    target.score = (std::numeric_limits<float>::max)();
    bool anyProjected = false;
    const float halfX = (std::max)(softRect_.halfSize.x, 0.00001f);
    const float halfY = (std::max)(softRect_.halfSize.y, 0.00001f);
    for (const auto& segment : geometry.segments) {
        if (!std::isfinite(segment.worldRadius) || segment.worldRadius <= 0.0f) {
            continue;
        }
        const float startDepth = depth(segment.worldStart), endDepth = depth(segment.worldEnd);
        float lo = 0.0f, hi = 1.0f;
        if (!std::isfinite(startDepth) || !std::isfinite(endDepth) ||
            !ClipAxis(startDepth, endDepth, minimumTargetDepth_, maximumTargetDepth_, lo, hi)) {
            continue;
        }
        const Vector3 a = Lerp(segment.worldStart, segment.worldEnd, lo);
        const Vector3 b = Lerp(segment.worldStart, segment.worldEnd, hi);
        Vector2 aUv{}, bUv{};
        float w = 0.0f;
        if (!ProjectWorldToScreen(a, aUv, w) || !ProjectWorldToScreen(b, bUv, w)) {
            continue;
        }
        const Vector2 aRadius = projectedRadius(a, segment.worldRadius, aUv);
        const Vector2 bRadius = projectedRadius(b, segment.worldRadius, bUv);
        const Vector2 radius{ (std::max)(aRadius.x, bRadius.x), (std::max)(aRadius.y, bRadius.y) };
        anyProjected = true;
        const bool visible = CapsuleOverlapsRect(aUv, bUv, aRadius, bRadius,
            visibleRect_.minimum, visibleRect_.maximum);
        target.overlapsVisibleRect |= visible;
        target.overlapsSoftRect |= CapsuleOverlapsRect(aUv, bUv, aRadius, bRadius,
            softRect_.minimum, softRect_.maximum);
        target.boundsMinimum.x = (std::min)(target.boundsMinimum.x, (std::min)(aUv.x, bUv.x) - radius.x);
        target.boundsMinimum.y = (std::min)(target.boundsMinimum.y, (std::min)(aUv.y, bUv.y) - radius.y);
        target.boundsMaximum.x = (std::max)(target.boundsMaximum.x, (std::max)(aUv.x, bUv.x) + radius.x);
        target.boundsMaximum.y = (std::max)(target.boundsMaximum.y, (std::max)(aUv.y, bUv.y) + radius.y);
        const Vector2 normalizedA{ (aUv.x - visibleRect_.center.x) / halfX,
            (aUv.y - visibleRect_.center.y) / halfY };
        const Vector2 normalizedB{ (bUv.x - visibleRect_.center.x) / halfX,
            (bUv.y - visibleRect_.center.y) / halfY };
        const float t = ClosestParameter({}, normalizedA, normalizedB);
        const float nearestDepth = depth(a) + (depth(b) - depth(a)) * t;
        const float depthScore = std::clamp((nearestDepth - minimumTargetDepth_) /
            (maximumTargetDepth_ - minimumTargetDepth_), 0.0f, 1.0f);
        const float score = std::sqrt(PointSegmentDistanceSquared({}, normalizedA, normalizedB)) +
            depthScore * depthScoreWeight_ - (visible ? visibleRectBonus_ : 0.0f);
        if (score < target.score) {
            target.score = score;
            target.screenUv = { aUv.x + (bUv.x - aUv.x) * t, aUv.y + (bUv.y - aUv.y) * t };
            const float worldT = (t / depth(b)) / ((1.0f - t) / depth(a) + t / depth(b));
            const Vector3 point = Lerp(a, b, worldT);
            diagnostic.worldPosition = point;
            diagnostic.worldRadius = segment.worldRadius;
            diagnostic.cameraSpacePosition = {
                (point.x - viewPosition.x) * right.x + (point.y - viewPosition.y) * right.y +
                    (point.z - viewPosition.z) * right.z,
                (point.x - viewPosition.x) * up.x + (point.y - viewPosition.y) * up.y +
                    (point.z - viewPosition.z) * up.z, depth(point) };
            diagnostic.viewDepth = depth(point);
            const auto& vp = camera_->GetViewProjectionMatrix();
            diagnostic.clipPosition = {
                point.x * vp.m[0][0] + point.y * vp.m[1][0] + point.z * vp.m[2][0] + vp.m[3][0],
                point.x * vp.m[0][1] + point.y * vp.m[1][1] + point.z * vp.m[2][1] + vp.m[3][1],
                point.x * vp.m[0][2] + point.y * vp.m[1][2] + point.z * vp.m[2][2] + vp.m[3][2] };
            diagnostic.clipW = point.x * vp.m[0][3] + point.y * vp.m[1][3] +
                point.z * vp.m[2][3] + vp.m[3][3];
        }
    }
    if (!anyProjected) {
        return false;
    }
    // Marker dimensions come from the middle section, independent of the selected bone/tip.
    if (!geometry.segments.empty() && target.cameraDepth > 0.0f) {
        const auto radius = projectedRadius(target.markerWorldPosition,
            geometry.segments[(geometry.segments.size() - 1) / 2].worldRadius, markerUv);
        target.screenRadius = {
            std::clamp(radius.x, minimumScreenRadius_, maximumScreenRadius_),
            std::clamp(radius.y, minimumScreenRadius_, maximumScreenRadius_) };
    }
    diagnostic.cameraFront = true;
    diagnostic.viewportInside = target.screenUv.x >= 0.0f && target.screenUv.x <= 1.0f &&
        target.screenUv.y >= 0.0f && target.screenUv.y <= 1.0f;
    diagnostic.projectionValid = true;
    diagnostic.screenUv = target.screenUv;
    diagnostic.ndcPosition = { target.screenUv.x * 2.0f - 1.0f, 1.0f - target.screenUv.y * 2.0f };
    target.projectionValid = true;
    return true;
}
