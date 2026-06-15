#include "RailShooterCameraRig.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Level/LevelRailRuntime.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace {
    constexpr float kMinExternalRailLength = 0.0001f;

    const char* ToStartModeText(RailShooterCameraRig::CameraRailStartMode mode) {
        switch (mode) {
        case RailShooterCameraRig::CameraRailStartMode::FromRailStart:
            return "FromRailStart";
        case RailShooterCameraRig::CameraRailStartMode::ClosestPointFromCurrentCamera:
            return "ClosestPointFromCurrentCamera";
        case RailShooterCameraRig::CameraRailStartMode::BlendFromCurrentCameraToRail:
        default:
            return "BlendFromCurrentCameraToRail";
        }
    }

    float NormalizeExternalT(float distance, float totalLength) {
        if (totalLength <= kMinExternalRailLength) {
            return 0.0f;
        }
        return std::clamp(distance / totalLength, 0.0f, 1.0f);
    }

    float Length(const Vector3& value) {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    }

    Vector3 Normalize(const Vector3& value, const Vector3& fallback) {
        const float length = Length(value);
        if (length <= kMinExternalRailLength ||
            !std::isfinite(value.x) ||
            !std::isfinite(value.y) ||
            !std::isfinite(value.z)) {
            return fallback;
        }
        return { value.x / length, value.y / length, value.z / length };
    }

    Vector3 GetCameraForward(const Camera& camera) {
        const Matrix4x4& matrix = camera.GetWorldMatrix();
        return Normalize({ matrix.m[2][0], matrix.m[2][1], matrix.m[2][2] }, { 0.0f, 0.0f, 1.0f });
    }
}

bool RailShooterCameraRig::StartRailByKey(const std::string& railKey, std::string& resultMessage) {
    return StartRailByKeyWithSource(railKey, railStartMode_, "Manual", resultMessage);
}

bool RailShooterCameraRig::StartRailByKey(
    const std::string& railKey,
    CameraRailStartMode startMode,
    std::string& resultMessage) {
    return StartRailByKeyWithSource(railKey, startMode, "EventFlag", resultMessage);
}

bool RailShooterCameraRig::StartRailByKeyWithSource(
    const std::string& railKey,
    CameraRailStartMode startMode,
    const std::string& startSource,
    std::string& resultMessage) {
    lastStartCameraRailTarget_ = railKey.empty() ? "(empty)" : railKey;
    lastResolvedRailId_ = "(none)";
    lastResolvedRailName_ = "(none)";
    lastStartMode_ = ToStartModeText(startMode);
    lastRequestedStartMode_ = ToStartModeText(startMode);
    lastActualStartMode_ = ToStartModeText(startMode);
    lastStartSource_ = startSource.empty() ? "(unknown)" : startSource;
    lastClosestValid_ = false;
    lastBlendValid_ = false;
    lastStartEvaluationValid_ = false;
    lastPreviousRailDistance_ = railDistance_;
    lastStartDistance_ = 0.0f;
    lastClosestRailDistance_ = 0.0f;
    lastClosestRailPathDistance_ = 0.0f;
    lastCameraToRailDistance_ = 0.0f;
    lastClosestSampledPointCount_ = 0;
    lastClosestWorldPosition_ = { 0.0f, 0.0f, 0.0f };
    lastBlendStartCameraPosition_ = { 0.0f, 0.0f, 0.0f };
    lastBlendTargetRailPosition_ = { 0.0f, 0.0f, 0.0f };
    lastBlendTargetDistance_ = 0.0f;
    lastBlendProgress_ = 0.0f;
    lastBlendTime_ = railBlendTime_;
    lastStartDistanceRatio_ = 0.0f;
    railEndReached_ = false;
    lastStartedNearEnd_ = false;
    lastFallbackUsed_ = false;
    lastClosestTooFar_ = false;
    lastRailDirectionReversed_ = false;
    lastStartWarning_ = "(none)";

    auto failStart = [&](const std::string& message) {
        resultMessage = message;
        lastStartResult_ = message;
        return false;
        };

    if (railKey.empty()) {
        return failStart("Rail key is empty.");
    }
    if (!railRuntime_) {
        return failStart("RailRuntime is missing.");
    }
    if (!camera_) {
        return failStart("Camera is missing.");
    }

    const size_t railCount = railRuntime_->GetRailCount();
    if (railCount == 0) {
        return failStart("RailRuntime has no rails.");
    }

    LevelRailRuntimeRailInfo matchedInfo;
    bool found = false;
    size_t matchedIndex = 0;
    for (size_t index = 0; index < railCount; ++index) {
        LevelRailRuntimeRailInfo info;
        if (!railRuntime_->GetRailInfo(index, info)) {
            continue;
        }
        if (info.railId == railKey || info.name == railKey) {
            matchedInfo = info;
            matchedIndex = index;
            found = true;
            break;
        }
    }

    if (!found) {
        return failStart("Rail not found: " + railKey);
    }
    lastResolvedRailId_ = matchedInfo.railId.empty() ? "(empty)" : matchedInfo.railId;
    lastResolvedRailName_ = matchedInfo.name.empty() ? "(empty)" : matchedInfo.name;
    lastClosestSampledPointCount_ = matchedInfo.sampledPointCount;
    lastRailDirectionReversed_ = matchedInfo.reverseDirection;
    if (matchedInfo.pointCount < 2 || matchedInfo.sampledPointCount < 2) {
        return failStart("Rail has too few points: " + railKey);
    }
    if (matchedInfo.totalLength <= kMinExternalRailLength) {
        return failStart("invalid rail length: " + railKey);
    }

    const std::string resolvedRailKey = matchedInfo.railId.empty() ? matchedInfo.name : matchedInfo.railId;
    if (resolvedRailKey.empty()) {
        return failStart("Resolved rail id/name is empty: " + railKey);
    }

    const Vector3 startCameraPosition = camera_->GetTranslate();
    const Vector3 startCameraForward = GetCameraForward(*camera_);
    float startDistance = 0.0f;
    CameraRailStartMode actualStartMode = startMode;
    if (startMode != CameraRailStartMode::FromRailStart) {
        const LevelRailEvaluation closest =
            railRuntime_->FindClosestEvaluation(resolvedRailKey, startCameraPosition, matchedInfo.loop);
        lastClosestValid_ = closest.valid;
        if (!closest.valid || !std::isfinite(closest.distance)) {
            if (closestPointFallback_ == ClosestPointFallback::DoNotStart) {
                return failStart("Closest rail evaluation failed: " + railKey);
            }
            lastFallbackUsed_ = true;
            lastStartWarning_ = "Closest rail evaluation failed; fallback to rail start.";
            startDistance = 0.0f;
            actualStartMode = CameraRailStartMode::FromRailStart;
        } else {
            startDistance = closest.distance;
            lastClosestRailPathDistance_ = closest.distance;
            lastClosestRailDistance_ = closest.distanceToPoint;
            lastCameraToRailDistance_ = closest.distanceToPoint;
            lastClosestWorldPosition_ = closest.position;
            lastClosestTooFar_ = maxAttachDistance_ > 0.0f && closest.distanceToPoint > maxAttachDistance_;
            if (lastClosestTooFar_) {
                const std::string tooFarMessage =
                    "Camera To Rail Distance Too Large: " + std::to_string(closest.distanceToPoint) +
                    " > " + std::to_string(maxAttachDistance_);
                if (closestPointFallback_ == ClosestPointFallback::DoNotStart) {
                    return failStart(tooFarMessage);
                }
                lastFallbackUsed_ = true;
                lastStartWarning_ = tooFarMessage + "; fallback to rail start.";
                startDistance = 0.0f;
                actualStartMode = CameraRailStartMode::FromRailStart;
            }
        }
    }

    const float safeStartDistance = std::clamp(startDistance, 0.0f, matchedInfo.totalLength);
    lastStartDistanceRatio_ = NormalizeExternalT(safeStartDistance, matchedInfo.totalLength);
    const bool detectedNearEndStart = lastStartDistanceRatio_ >= nearEndWarningRatio_;
    lastStartedNearEnd_ = detectedNearEndStart;
    if (detectedNearEndStart) {
        const std::string nearEndMessage =
            "Start Distance Near Rail End: ratio=" + std::to_string(lastStartDistanceRatio_);
        if (closestPointFallback_ == ClosestPointFallback::FromRailStart && startMode != CameraRailStartMode::FromRailStart) {
            lastFallbackUsed_ = true;
            actualStartMode = CameraRailStartMode::FromRailStart;
            startDistance = 0.0f;
            lastStartWarning_ = lastStartWarning_ == "(none)"
                ? nearEndMessage + "; fallback to rail start."
                : lastStartWarning_ + " " + nearEndMessage + "; fallback to rail start.";
        } else {
            lastStartWarning_ = lastStartWarning_ == "(none)"
                ? nearEndMessage
                : lastStartWarning_ + " " + nearEndMessage;
        }
    }
    const float finalStartDistance = std::clamp(startDistance, 0.0f, matchedInfo.totalLength);
    lastStartDistanceRatio_ = NormalizeExternalT(finalStartDistance, matchedInfo.totalLength);
    lastStartedNearEnd_ = detectedNearEndStart || lastStartDistanceRatio_ >= nearEndWarningRatio_;
    lastActualStartMode_ = ToStartModeText(actualStartMode);
    const LevelRailEvaluation startEvaluation =
        railRuntime_->EvaluateByDistance(resolvedRailKey, finalStartDistance, matchedInfo.loop);
    lastStartEvaluationValid_ = startEvaluation.valid;
    if (!startEvaluation.valid) {
        return failStart("Failed to evaluate rail: " + railKey);
    }

    SaveDebugHomeCameraIfNeeded();
    selectedRailIndex_ = static_cast<int>(matchedIndex);
    selectedRailId_ = resolvedRailKey;
    selectedRailName_ = matchedInfo.name;
    selectedRailType_ = matchedInfo.railType;
    selectedRailLoop_ = matchedInfo.loop;
    selectedRailReverseDirection_ = matchedInfo.reverseDirection;
    selectedRailPointCount_ = matchedInfo.pointCount;
    selectedRailSampledPointCount_ = matchedInfo.sampledPointCount;
    selectedRailTotalLength_ = matchedInfo.totalLength;
    railSpeed_ = matchedInfo.speed;
    railCount_ = railCount;
    railDistance_ = finalStartDistance;
    railT_ = NormalizeExternalT(railDistance_, selectedRailTotalLength_);
    lastStartDistance_ = railDistance_;
    lastStartDistanceRatio_ = railT_;
    previousRailId_ = selectedRailId_;
    previousSelectedRailIndex_ = selectedRailIndex_;
    currentSegmentIndex_ = startEvaluation.segmentIndex;
    currentEvaluationValid_ = true;
    railEndReached_ = false;
    autoPlay_ = true;
    enableCameraRig_ = true;
    useCameraRig_ = true;
    mode_ = Mode::Rail;
    hasSmoothedForward_ = false;
    railBlendActive_ = false;
    railBlendElapsed_ = 0.0f;

    if (actualStartMode == CameraRailStartMode::BlendFromCurrentCameraToRail) {
        blendStartPosition_ = startCameraPosition;
        blendStartForward_ = startCameraForward;
        blendTargetPosition_ = startEvaluation.position;
        blendTargetForward_ = startEvaluation.forward;
        lastBlendStartCameraPosition_ = blendStartPosition_;
        lastBlendTargetRailPosition_ = blendTargetPosition_;
        lastBlendTargetDistance_ = railDistance_;
        lastBlendValid_ = true;
        railBlendElapsed_ = 0.0f;
        railBlendActive_ = true;
        currentPosition_ = blendStartPosition_;
        currentForward_ = blendStartForward_;
        currentEvaluationValid_ = true;
    } else {
        railBlendActive_ = false;
        currentPosition_ = startEvaluation.position;
        currentForward_ = startEvaluation.forward;
        currentSegmentIndex_ = startEvaluation.segmentIndex;
        currentEvaluationValid_ = true;
        ApplyToCamera();
    }

    resultMessage = "Started camera rail: " +
        (selectedRailName_.empty() ? selectedRailId_ : selectedRailName_) +
        " (" + selectedRailId_ + "), requestedMode=" + ToStartModeText(startMode) +
        ", actualMode=" + ToStartModeText(actualStartMode) +
        ", source=" + lastStartSource_ +
        ", distance=" + std::to_string(lastStartDistance_) +
        ", totalLength=" + std::to_string(selectedRailTotalLength_) +
        ", startRatio=" + std::to_string(lastStartDistanceRatio_) +
        ", previousDistance=" + std::to_string(lastPreviousRailDistance_) +
        ", closestDistance=" + std::to_string(lastClosestRailPathDistance_) +
        ", cameraToRail=" + std::to_string(lastCameraToRailDistance_) +
        ", sampledPoints=" + std::to_string(lastClosestSampledPointCount_) +
        ", nearEnd=" + std::string(lastStartedNearEnd_ ? "true" : "false") +
        ", fallbackUsed=" + std::string(lastFallbackUsed_ ? "true" : "false") +
        ", reversed=" + std::string(lastRailDirectionReversed_ ? "true" : "false") +
        ", warning=" + lastStartWarning_;
    lastStartResult_ = resultMessage;
    return true;
}

void RailShooterCameraRig::StopAndRestoreCamera() {
    mode_ = Mode::Disabled;
    autoPlay_ = false;
    currentEvaluationValid_ = false;
    railEndReached_ = false;
    railBlendActive_ = false;
    railBlendElapsed_ = 0.0f;
    RestoreDebugHomeCamera();
    wasCameraRigActive_ = false;
}
