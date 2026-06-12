#include "RailShooterCameraRig.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Level/LevelRailRuntime.h"
#include <algorithm>

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
}

bool RailShooterCameraRig::StartRailByKey(const std::string& railKey, std::string& resultMessage) {
    if (railKey.empty()) {
        resultMessage = "Rail key is empty.";
        lastStartResult_ = resultMessage;
        return false;
    }
    if (!railRuntime_) {
        resultMessage = "RailRuntime is missing.";
        lastStartResult_ = resultMessage;
        return false;
    }

    const size_t railCount = railRuntime_->GetRailCount();
    if (railCount == 0) {
        resultMessage = "RailRuntime has no rails.";
        lastStartResult_ = resultMessage;
        return false;
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
        resultMessage = "Rail not found: " + railKey;
        lastStartResult_ = resultMessage;
        return false;
    }
    if (matchedInfo.pointCount < 2 || matchedInfo.sampledPointCount < 2) {
        resultMessage = "Rail has too few points: " + railKey;
        lastStartResult_ = resultMessage;
        return false;
    }
    if (matchedInfo.totalLength <= kMinExternalRailLength) {
        resultMessage = "Rail totalLength is zero: " + railKey;
        lastStartResult_ = resultMessage;
        return false;
    }

    SaveDebugHomeCameraIfNeeded();
    CaptureCameraPose();
    selectedRailIndex_ = static_cast<int>(matchedIndex);
    previousRailId_.clear();
    previousSelectedRailIndex_ = -1;
    currentSegmentIndex_ = 0;
    currentEvaluationValid_ = false;
    autoPlay_ = true;
    enableCameraRig_ = true;
    useCameraRig_ = true;
    mode_ = Mode::Rail;
    hasSmoothedForward_ = false;
    railBlendActive_ = false;
    railBlendElapsed_ = 0.0f;

    railCount_ = railCount;
    if (!FetchSelectedRailInfo()) {
        resultMessage = "Failed to select rail: " + railKey;
        currentEvaluationValid_ = false;
        lastStartResult_ = resultMessage;
        return false;
    }

    SyncSelectedRailDefaults();
    float startDistance = 0.0f;
    lastClosestRailDistance_ = 0.0f;
    if (railStartMode_ != CameraRailStartMode::FromRailStart) {
        const LevelRailEvaluation closest =
            railRuntime_->FindClosestEvaluation(selectedRailId_, currentPosition_, selectedRailLoop_);
        if (closest.valid) {
            startDistance = closest.distance;
            lastClosestRailDistance_ = closest.distanceToPoint;
        }
    }

    railDistance_ = std::clamp(startDistance, 0.0f, selectedRailTotalLength_);
    railT_ = NormalizeExternalT(railDistance_, selectedRailTotalLength_);
    lastStartDistance_ = railDistance_;
    autoPlay_ = true;
    const LevelRailEvaluation startEvaluation =
        railRuntime_->EvaluateByDistance(selectedRailId_, railDistance_, selectedRailLoop_);
    if (!startEvaluation.valid) {
        resultMessage = "Failed to evaluate rail: " + railKey;
        lastStartResult_ = resultMessage;
        return false;
    }

    if (railStartMode_ == CameraRailStartMode::BlendFromCurrentCameraToRail) {
        blendStartPosition_ = currentPosition_;
        blendStartForward_ = currentForward_;
        blendTargetPosition_ = startEvaluation.position;
        blendTargetForward_ = startEvaluation.forward;
        railBlendElapsed_ = 0.0f;
        railBlendActive_ = true;
        currentPosition_ = blendStartPosition_;
        currentForward_ = blendStartForward_;
        currentEvaluationValid_ = true;
        ApplyToCamera();
    } else {
        railBlendActive_ = false;
        UpdateRail(0.0f);
        if (!currentEvaluationValid_) {
            resultMessage = "Failed to evaluate rail: " + railKey;
            lastStartResult_ = resultMessage;
            return false;
        }
    }

    resultMessage = "Started camera rail: " +
        (selectedRailName_.empty() ? selectedRailId_ : selectedRailName_) +
        " (" + selectedRailId_ + "), mode=" + ToStartModeText(railStartMode_) +
        ", distance=" + std::to_string(lastStartDistance_) +
        ", closestDelta=" + std::to_string(lastClosestRailDistance_);
    lastStartResult_ = resultMessage;
    return true;
}

void RailShooterCameraRig::StopAndRestoreCamera() {
    mode_ = Mode::Disabled;
    autoPlay_ = false;
    currentEvaluationValid_ = false;
    railBlendActive_ = false;
    railBlendElapsed_ = 0.0f;
    RestoreDebugHomeCamera();
    wasCameraRigActive_ = false;
}
