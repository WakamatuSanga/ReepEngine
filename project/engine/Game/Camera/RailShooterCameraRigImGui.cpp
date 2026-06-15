#include "RailShooterCameraRig.h"
#include "Engine/Level/LevelRailRuntime.h"
#include "Engine/Level/LevelSceneRuntime.h"
#include <algorithm>
#include <cmath>
#include <string>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kMinRailLength = 0.0001f;
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

    template <size_t N>
    void CopyTextToBuffer(std::array<char, N>& buffer, const std::string& text) {
        buffer.fill('\0');
        const size_t copyCount = (std::min)(text.size(), buffer.size() - 1);
        std::copy_n(text.data(), copyCount, buffer.data());
    }

    float ClampDistance(float distance, float totalLength) {
        return std::clamp(distance, 0.0f, (std::max)(0.0f, totalLength));
    }

    float NormalizeT(float distance, float totalLength) {
        if (totalLength <= kMinRailLength) {
            return 0.0f;
        }
        return std::clamp(distance / totalLength, 0.0f, 1.0f);
    }

    float Length(const Vector3& value) {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    }

    Vector3 Normalize(const Vector3& value, const Vector3& fallback) {
        const float length = Length(value);
        if (length <= 0.00001f ||
            !std::isfinite(value.x) ||
            !std::isfinite(value.y) ||
            !std::isfinite(value.z)) {
            return fallback;
        }
        return { value.x / length, value.y / length, value.z / length };
    }

    Vector3 MakeForwardFromRotationDegrees(const Vector3& rotationDegrees) {
        const float pitch = rotationDegrees.x * kDegToRad;
        const float yaw = rotationDegrees.y * kDegToRad;
        const float horizontal = std::cos(pitch);
        return Normalize({
            std::sin(yaw) * horizontal,
            -std::sin(pitch),
            std::cos(yaw) * horizontal,
        }, { 0.0f, 0.0f, 1.0f });
    }

    const char* ToModeLabel(RailShooterCameraRig::Mode mode) {
        switch (mode) {
        case RailShooterCameraRig::Mode::Straight:
            return "Straight";
        case RailShooterCameraRig::Mode::Rail:
            return "Rail";
        case RailShooterCameraRig::Mode::RailFinishedStraight:
            return "RailFinishedStraight";
        case RailShooterCameraRig::Mode::RailEndStopped:
            return "RailEndStopped";
        case RailShooterCameraRig::Mode::Disabled:
        default:
            return "Manual / Disabled";
        }
    }

    const char* ToStartModeLabel(RailShooterCameraRig::CameraRailStartMode mode) {
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

    const char* ToRailEndBehaviorLabel(RailShooterCameraRig::RailEndBehavior behavior) {
        switch (behavior) {
        case RailShooterCameraRig::RailEndBehavior::ContinueStraight:
            return "ContinueStraight";
        case RailShooterCameraRig::RailEndBehavior::RestoreCamera:
            return "RestoreCamera";
        case RailShooterCameraRig::RailEndBehavior::StopAtEnd:
        default:
            return "StopAtEnd";
        }
    }

    const char* ToClosestFallbackLabel(RailShooterCameraRig::ClosestPointFallback fallback) {
        switch (fallback) {
        case RailShooterCameraRig::ClosestPointFallback::DoNotStart:
            return "DoNotStart";
        case RailShooterCameraRig::ClosestPointFallback::FromRailStart:
        default:
            return "FromRailStart";
        }
    }
}

void RailShooterCameraRig::DrawImGui() {
#ifdef _DEBUG
    ImGui::SetNextWindowSize(ImVec2(430.0f, 520.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("レールシューティングカメラ確認 (Rail Shooter Camera Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("CameraRig有効 (Enable Camera Rig)", &enableCameraRig_);
    ImGui::Checkbox("CameraRigを使用 (Use Camera Rig)", &useCameraRig_);
    ImGui::Checkbox("Stop時にカメラ復元 (Restore Camera On Stop)", &restoreCameraOnStop_);
    ImGui::Checkbox("Disable時にカメラ復元 (Restore Camera On Disable)", &restoreCameraOnDisable_);
    ImGui::Checkbox("Gameplay Preview Mode", &gameplayPreviewMode_);
    ImGui::Checkbox("CameraRig中はRail Debugを隠す (Hide Rail Debug While Camera Rig Active)", &hideRailDebugWhileActive_);
    ImGui::Checkbox("CameraRig中はRail点を隠す (Hide Rail Points While Camera Rig Active)", &hideRailPointsWhileActive_);
    ImGui::Checkbox("CameraRig中はEvent Debugを隠す (Hide Event Debug While Camera Rig Active)", &hideEventDebugWhileActive_);

    ImGui::SeparatorText("Initial Camera");
    if (ImGui::Button("初期カメラを適用 (Apply Initial Camera)")) {
        ApplyInitialCameraFromLevel();
    }
    ImGui::Checkbox("読込後に初期カメラ自動適用 (Auto Apply Initial Camera On Load)", &autoApplyInitialCameraOnLoad_);
    const LevelCameraStart* cameraStart = levelSceneRuntime_ ? levelSceneRuntime_->GetEngineCameraStart() : nullptr;
    ImGui::Text("Camera Start Exists: %s", cameraStart ? "true" : "false");
    if (cameraStart) {
        const Vector3 startForward = MakeForwardFromRotationDegrees(cameraStart->transform.rotation);
        ImGui::Text("Camera Start Position: %.3f, %.3f, %.3f",
            cameraStart->transform.translation.x,
            cameraStart->transform.translation.y,
            cameraStart->transform.translation.z);
        ImGui::Text("Camera Start Forward: %.3f, %.3f, %.3f", startForward.x, startForward.y, startForward.z);
    }
    ImGui::TextWrapped("Initial Camera Result: %s", lastInitialCameraResult_.c_str());

    int modeIndex = static_cast<int>(mode_);
    const char* modeNames[] = { "Manual / Disabled", "Straight", "Rail", "RailFinishedStraight", "RailEndStopped" };
    if (ImGui::Combo("Mode", &modeIndex, modeNames, IM_ARRAYSIZE(modeNames))) {
        mode_ = static_cast<Mode>(modeIndex);
        if (mode_ == Mode::Straight) {
            SaveDebugHomeCameraIfNeeded();
            CaptureCameraPose();
        } else if (mode_ == Mode::Rail) {
            StartRail();
        }
    }

    if (ImGui::Button("Start Rail")) {
        StartRail();
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop Rail")) {
        StopRail();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Camera Rig")) {
        ResetCameraRig();
    }
    if (ImGui::Button("Capture Debug Home Camera")) {
        CaptureDebugHomeCamera();
    }
    ImGui::SameLine();
    if (ImGui::Button("Restore Debug Home Camera")) {
        RestoreDebugHomeCamera();
    }
    if (ImGui::Button("Reset Camera Rig And Restore Camera")) {
        ResetCameraRigAndRestoreCamera();
    }

    ImGui::DragFloat("Straight Speed", &straightSpeed_, 0.05f, -100.0f, 100.0f, "%.2f");
    ImGui::DragFloat("Rail Speed", &railSpeed_, 0.05f, -100.0f, 100.0f, "%.2f");
    int endBehaviorIndex = static_cast<int>(railEndBehavior_);
    const char* railEndBehaviorNames[] = { "StopAtEnd", "ContinueStraight", "RestoreCamera" };
    if (ImGui::Combo("Rail End Behavior", &endBehaviorIndex, railEndBehaviorNames, IM_ARRAYSIZE(railEndBehaviorNames))) {
        railEndBehavior_ = static_cast<RailEndBehavior>(endBehaviorIndex);
    }
    int startModeIndex = static_cast<int>(railStartMode_);
    const char* startModeNames[] = { "FromRailStart", "ClosestPointFromCurrentCamera", "BlendFromCurrentCameraToRail" };
    if (ImGui::Combo("Camera Rail Start Mode", &startModeIndex, startModeNames, IM_ARRAYSIZE(startModeNames))) {
        railStartMode_ = static_cast<CameraRailStartMode>(startModeIndex);
    }
    bool startFromClosest = railStartMode_ != CameraRailStartMode::FromRailStart;
    if (ImGui::Checkbox("現在位置に近いRailから開始 (Start From Closest Point)", &startFromClosest)) {
        railStartMode_ = startFromClosest
            ? CameraRailStartMode::ClosestPointFromCurrentCamera
            : CameraRailStartMode::FromRailStart;
    }
    ImGui::DragFloat("Blend Time", &railBlendTime_, 0.01f, 0.05f, 5.0f, "%.2f");
    ImGui::DragFloat("Max Attach Distance", &maxAttachDistance_, 0.1f, 0.0f, 10000.0f, "%.2f");
    ImGui::DragFloat("Near End Warning Ratio", &nearEndWarningRatio_, 0.01f, 0.5f, 1.0f, "%.2f");
    nearEndWarningRatio_ = std::clamp(nearEndWarningRatio_, 0.5f, 1.0f);
    int fallbackIndex = static_cast<int>(closestPointFallback_);
    const char* fallbackNames[] = { "DoNotStart", "FromRailStart" };
    if (ImGui::Combo("Closest Failed Fallback", &fallbackIndex, fallbackNames, IM_ARRAYSIZE(fallbackNames))) {
        closestPointFallback_ = static_cast<ClosestPointFallback>(fallbackIndex);
    }
    ImGui::SeparatorText("StartCameraRail Key Test");
    ImGui::InputText("Manual Start Rail Key", manualStartRailKeyBuffer_.data(), manualStartRailKeyBuffer_.size());
    if (ImGui::Button("Use Selected Rail ID")) {
        CopyTextToBuffer(manualStartRailKeyBuffer_, selectedRailId_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Use Selected Rail Name")) {
        CopyTextToBuffer(manualStartRailKeyBuffer_, selectedRailName_);
    }
    if (ImGui::Button("StartRailByKey Test")) {
        std::string result;
        StartRailByKey(std::string(manualStartRailKeyBuffer_.data()), result);
    }
    ImGui::Checkbox("Camera Forward Smoothingを使う (Enable Camera Forward Smoothing)", &enableCameraForwardSmoothing_);
    ImGui::DragFloat("Forward Smooth Strength", &forwardSmoothStrength_, 0.1f, 0.0f, 60.0f, "%.2f");
    ImGui::Checkbox("Auto Play", &autoPlay_);
    ImGui::Checkbox("Loop", &debugLoop_);
    ImGui::Text("Rail Count: %zu", railCount_);

    if (railRuntime_ && railCount_ > 0) {
        selectedRailIndex_ = std::clamp(selectedRailIndex_, 0, (std::max)(0, static_cast<int>(railCount_) - 1));
        LevelRailRuntimeRailInfo selectedInfo;
        const bool hasSelectedInfo = railRuntime_->GetRailInfo(static_cast<size_t>(selectedRailIndex_), selectedInfo);
        const std::string currentLabel = hasSelectedInfo
            ? ((selectedInfo.name.empty() ? selectedInfo.railId : selectedInfo.name) + "##camera_rig_current")
            : "(none)";

        if (ImGui::BeginCombo("Selected Rail", currentLabel.c_str())) {
            for (size_t index = 0; index < railCount_; ++index) {
                LevelRailRuntimeRailInfo info;
                if (!railRuntime_->GetRailInfo(index, info)) {
                    continue;
                }
                const std::string label =
                    (info.name.empty() ? info.railId : info.name) +
                    " [" + std::to_string(info.pointCount) + " pts]##camera_rig_rail_" + std::to_string(index);
                const bool selected = selectedRailIndex_ == static_cast<int>(index);
                if (ImGui::Selectable(label.c_str(), selected)) {
                    selectedRailIndex_ = static_cast<int>(index);
                    previousRailId_.clear();
                    FetchSelectedRailInfo();
                    SyncSelectedRailDefaults();
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        FetchSelectedRailInfo();
        const float maxDistance = (std::max)(selectedRailTotalLength_, 0.001f);
        if (ImGui::SliderFloat("Rail Distance", &railDistance_, 0.0f, maxDistance, "%.3f")) {
            railDistance_ = ClampDistance(railDistance_, selectedRailTotalLength_);
            railT_ = NormalizeT(railDistance_, selectedRailTotalLength_);
            if (mode_ == Mode::Rail && IsCameraRigActive()) {
                UpdateRail(0.0f);
            }
        }
        if (ImGui::SliderFloat("Rail T", &railT_, 0.0f, 1.0f, "%.3f")) {
            railT_ = std::clamp(railT_, 0.0f, 1.0f);
            railDistance_ = railT_ * selectedRailTotalLength_;
            if (mode_ == Mode::Rail && IsCameraRigActive()) {
                UpdateRail(0.0f);
            }
        }
    } else {
        ImGui::TextDisabled("レールがありません。 (No rails.)");
    }

    ImGui::SeparatorText("Camera Rig State");
    ImGui::Text("Active: %s", IsCameraRigActive() ? "true" : "false");
    ImGui::Text("Has Saved Camera Pose: %s", hasSavedCameraPose_ ? "true" : "false");
    ImGui::Text("Mode: %s", ToModeLabel(mode_));
    ImGui::Text("Current Mode: %s", ToModeLabel(mode_));
    ImGui::Text("Camera Rail Start Mode: %s", ToStartModeLabel(railStartMode_));
    ImGui::Text("Rail End Behavior: %s", ToRailEndBehaviorLabel(railEndBehavior_));
    ImGui::Text("Closest Failed Fallback: %s", ToClosestFallbackLabel(closestPointFallback_));
    ImGui::Text("Requested Start Mode: %s", lastRequestedStartMode_.c_str());
    ImGui::Text("Actual Start Mode: %s", lastActualStartMode_.c_str());
    ImGui::Text("Start Source: %s", lastStartSource_.c_str());
    ImGui::TextWrapped("Last StartCameraRail Target: %s", lastStartCameraRailTarget_.c_str());
    ImGui::TextWrapped("Last StartCameraRail Result: %s", lastStartResult_.c_str());
    ImGui::Text("Last Resolved Rail ID: %s", lastResolvedRailId_.c_str());
    ImGui::Text("Last Resolved Rail Name: %s", lastResolvedRailName_.c_str());
    ImGui::Text("Last Start Mode: %s", lastStartMode_.c_str());
    ImGui::Text("Last Start Distance: %.3f", lastStartDistance_);
    ImGui::Text("Last Start Distance Ratio: %.3f", lastStartDistanceRatio_);
    ImGui::Text("Previous Rail Distance: %.3f", lastPreviousRailDistance_);
    ImGui::Text("Last Closest Distance: %.3f", lastClosestRailPathDistance_);
    ImGui::Text("Last Camera To Rail Distance: %.3f", lastCameraToRailDistance_);
    ImGui::Text("Max Attach Distance: %.3f", maxAttachDistance_);
    ImGui::Text("Started Near End: %s", lastStartedNearEnd_ ? "true" : "false");
    ImGui::Text("Fallback Used: %s", lastFallbackUsed_ ? "true" : "false");
    ImGui::Text("Rail Direction Reversed: %s", lastRailDirectionReversed_ ? "true" : "false");
    ImGui::Text("Camera To Rail Distance Too Large: %s", lastClosestTooFar_ ? "true" : "false");
    ImGui::TextWrapped("Last Start Warning: %s", lastStartWarning_.c_str());
    ImGui::Text("Last Closest Valid: %s", lastClosestValid_ ? "true" : "false");
    ImGui::Text("Last Closest Sampled Point Count: %zu", lastClosestSampledPointCount_);
    ImGui::Text("Last Closest Position: %.3f, %.3f, %.3f",
        lastClosestWorldPosition_.x,
        lastClosestWorldPosition_.y,
        lastClosestWorldPosition_.z);
    ImGui::Text("Blend Valid: %s", lastBlendValid_ ? "true" : "false");
    ImGui::Text("Blend Progress / Time: %.3f / %.3f", lastBlendProgress_, lastBlendTime_);
    ImGui::Text("Blend Target Distance: %.3f", lastBlendTargetDistance_);
    ImGui::Text("Blend Start Camera Position: %.3f, %.3f, %.3f",
        lastBlendStartCameraPosition_.x,
        lastBlendStartCameraPosition_.y,
        lastBlendStartCameraPosition_.z);
    ImGui::Text("Blend Target Rail Position: %.3f, %.3f, %.3f",
        lastBlendTargetRailPosition_.x,
        lastBlendTargetRailPosition_.y,
        lastBlendTargetRailPosition_.z);
    ImGui::Text("Start Evaluation Valid: %s", lastStartEvaluationValid_ ? "true" : "false");
    ImGui::TextWrapped("Last Start Result: %s", lastStartResult_.c_str());
    ImGui::Text("Selected Rail ID: %s", selectedRailId_.empty() ? "(none)" : selectedRailId_.c_str());
    ImGui::Text("Selected Rail Name: %s", selectedRailName_.empty() ? "(none)" : selectedRailName_.c_str());
    ImGui::Text("Selected Rail Type: %s", selectedRailType_.empty() ? "(none)" : selectedRailType_.c_str());
    ImGui::Text("Rail Point Count: %zu", selectedRailPointCount_);
    ImGui::Text("Sampled Point Count: %zu", selectedRailSampledPointCount_);
    ImGui::Text("Rail Total Length: %.3f", selectedRailTotalLength_);
    ImGui::Text("Rail Loop: %s", selectedRailLoop_ ? "true" : "false");
    ImGui::Text("Rail Reverse Direction: %s", selectedRailReverseDirection_ ? "true" : "false");
    ImGui::Text("Current Segment Index: %zu", currentSegmentIndex_);
    ImGui::Text("Rail Distance: %.3f", railDistance_);
    ImGui::Text("Rail Total Length: %.3f", selectedRailTotalLength_);
    ImGui::Text("Rail End Reached: %s", railEndReached_ ? "true" : "false");
    ImGui::Text("Evaluation Valid: %s", currentEvaluationValid_ ? "true" : "false");
    ImGui::Text("Current Position: %.3f, %.3f, %.3f", currentPosition_.x, currentPosition_.y, currentPosition_.z);
    ImGui::Text("Current Forward: %.3f, %.3f, %.3f", currentForward_.x, currentForward_.y, currentForward_.z);
    ImGui::Text("Current Up: %.3f, %.3f, %.3f", currentUp_.x, currentUp_.y, currentUp_.z);
    ImGui::Text("Saved Position: %.3f, %.3f, %.3f", savedCameraPosition_.x, savedCameraPosition_.y, savedCameraPosition_.z);
    ImGui::Text("Saved Rotation: %.3f, %.3f, %.3f", savedCameraRotation_.x, savedCameraRotation_.y, savedCameraRotation_.z);
    ImGui::End();
#endif
}
