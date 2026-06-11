#include "RailShooterCameraRig.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Level/LevelRailRuntime.h"
#include <algorithm>
#include <cmath>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kMinVectorLength = 0.00001f;
    constexpr float kMinRailLength = 0.0001f;

    Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }

    Vector3 ScaleVector3(const Vector3& value, float scale) {
        return { value.x * scale, value.y * scale, value.z * scale };
    }

    float Length(const Vector3& value) {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    }

    Vector3 Normalize(const Vector3& value, const Vector3& fallback) {
        const float length = Length(value);
        if (length <= kMinVectorLength ||
            !std::isfinite(value.x) ||
            !std::isfinite(value.y) ||
            !std::isfinite(value.z)) {
            return fallback;
        }
        return { value.x / length, value.y / length, value.z / length };
    }

    float Dot(const Vector3& lhs, const Vector3& rhs) {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
    }

    Vector3 Cross(const Vector3& lhs, const Vector3& rhs) {
        return {
            lhs.y * rhs.z - lhs.z * rhs.y,
            lhs.z * rhs.x - lhs.x * rhs.z,
            lhs.x * rhs.y - lhs.y * rhs.x
        };
    }

    float WrapDistance(float distance, float totalLength) {
        if (totalLength <= kMinRailLength) {
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
        if (totalLength <= kMinRailLength) {
            return 0.0f;
        }
        return std::clamp(distance / totalLength, 0.0f, 1.0f);
    }

    Vector3 GetCameraRight(const Camera& camera) {
        const Matrix4x4& matrix = camera.GetWorldMatrix();
        return Normalize({ matrix.m[0][0], matrix.m[0][1], matrix.m[0][2] }, { 1.0f, 0.0f, 0.0f });
    }

    Vector3 GetCameraUp(const Camera& camera) {
        const Matrix4x4& matrix = camera.GetWorldMatrix();
        return Normalize({ matrix.m[1][0], matrix.m[1][1], matrix.m[1][2] }, { 0.0f, 1.0f, 0.0f });
    }

    Vector3 GetCameraForward(const Camera& camera) {
        const Matrix4x4& matrix = camera.GetWorldMatrix();
        return Normalize({ matrix.m[2][0], matrix.m[2][1], matrix.m[2][2] }, { 0.0f, 0.0f, 1.0f });
    }

    Vector3 MakeCameraRotationFromForward(const Vector3& forward) {
        const Vector3 normalized = Normalize(forward, { 0.0f, 0.0f, 1.0f });
        const float yaw = std::atan2(normalized.x, normalized.z);
        const float horizontal = std::sqrt(normalized.x * normalized.x + normalized.z * normalized.z);
        const float pitch = std::atan2(-normalized.y, horizontal);
        return { pitch, yaw, 0.0f };
    }

    void BuildBasis(const Vector3& forward, const Vector3& preferredUp, Vector3& outRight, Vector3& outUp) {
        const Vector3 safeForward = Normalize(forward, { 0.0f, 0.0f, 1.0f });
        Vector3 up = Normalize(preferredUp, { 0.0f, 1.0f, 0.0f });
        if (std::fabs(Dot(safeForward, up)) > 0.98f) {
            up = { 0.0f, 0.0f, 1.0f };
            if (std::fabs(Dot(safeForward, up)) > 0.98f) {
                up = { 1.0f, 0.0f, 0.0f };
            }
        }

        outRight = Normalize(Cross(up, safeForward), { 1.0f, 0.0f, 0.0f });
        outUp = Normalize(Cross(safeForward, outRight), { 0.0f, 1.0f, 0.0f });
    }

    const char* ToModeLabel(RailShooterCameraRig::Mode mode) {
        switch (mode) {
        case RailShooterCameraRig::Mode::Straight:
            return "Straight";
        case RailShooterCameraRig::Mode::Rail:
            return "Rail";
        case RailShooterCameraRig::Mode::RailFinishedStraight:
            return "RailFinishedStraight";
        case RailShooterCameraRig::Mode::Disabled:
        default:
            return "Manual / Disabled";
        }
    }
}

RailShooterCameraRig::RailShooterCameraRig() = default;

RailShooterCameraRig::~RailShooterCameraRig() = default;

void RailShooterCameraRig::Initialize(Camera* camera, LevelRailRuntime* railRuntime) {
    camera_ = camera;
    railRuntime_ = railRuntime;
    CaptureCameraPose();
}

void RailShooterCameraRig::Finalize() {
    camera_ = nullptr;
    railRuntime_ = nullptr;
}

void RailShooterCameraRig::Update(float deltaTime) {
    railCount_ = railRuntime_ ? railRuntime_->GetRailCount() : 0;
    if (!IsCameraRigActive()) {
        currentEvaluationValid_ = false;
        if (wasCameraRigActive_ && restoreCameraOnDisable_) {
            RestoreDebugHomeCamera();
        }
        wasCameraRigActive_ = false;
        return;
    }

    if (!wasCameraRigActive_ && mode_ != Mode::Rail) {
        SaveDebugHomeCameraIfNeeded();
        CaptureCameraPose();
    }
    wasCameraRigActive_ = true;

    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 15.0f);
    switch (mode_) {
    case Mode::Straight:
        UpdateStraight(safeDeltaTime);
        break;
    case Mode::Rail:
        UpdateRail(safeDeltaTime);
        break;
    case Mode::RailFinishedStraight:
        UpdateRailFinishedStraight(safeDeltaTime);
        break;
    case Mode::Disabled:
    default:
        break;
    }

    if (IsCameraRigActive() && mode_ != Mode::Rail && mode_ != Mode::Disabled) {
        ApplyToCamera();
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

    int modeIndex = static_cast<int>(mode_);
    const char* modeNames[] = { "Manual / Disabled", "Straight", "Rail", "RailFinishedStraight" };
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
    ImGui::Text("Selected Rail ID: %s", selectedRailId_.empty() ? "(none)" : selectedRailId_.c_str());
    ImGui::Text("Selected Rail Type: %s", selectedRailType_.empty() ? "(none)" : selectedRailType_.c_str());
    ImGui::Text("Rail Point Count: %zu", selectedRailPointCount_);
    ImGui::Text("Sampled Point Count: %zu", selectedRailSampledPointCount_);
    ImGui::Text("Rail Total Length: %.3f", selectedRailTotalLength_);
    ImGui::Text("Rail Loop: %s", selectedRailLoop_ ? "true" : "false");
    ImGui::Text("Current Segment Index: %zu", currentSegmentIndex_);
    ImGui::Text("Evaluation Valid: %s", currentEvaluationValid_ ? "true" : "false");
    ImGui::Text("Current Position: %.3f, %.3f, %.3f", currentPosition_.x, currentPosition_.y, currentPosition_.z);
    ImGui::Text("Current Forward: %.3f, %.3f, %.3f", currentForward_.x, currentForward_.y, currentForward_.z);
    ImGui::Text("Current Up: %.3f, %.3f, %.3f", currentUp_.x, currentUp_.y, currentUp_.z);
    ImGui::Text("Saved Position: %.3f, %.3f, %.3f", savedCameraPosition_.x, savedCameraPosition_.y, savedCameraPosition_.z);
    ImGui::Text("Saved Rotation: %.3f, %.3f, %.3f", savedCameraRotation_.x, savedCameraRotation_.y, savedCameraRotation_.z);
    ImGui::End();
#endif
}

bool RailShooterCameraRig::IsCameraRigActive() const {
    return camera_ && enableCameraRig_ && useCameraRig_ && mode_ != Mode::Disabled;
}

void RailShooterCameraRig::SyncSelectedRailDefaults() {
    if (selectedRailId_ == previousRailId_ && selectedRailIndex_ == previousSelectedRailIndex_) {
        return;
    }

    railDistance_ = 0.0f;
    railT_ = 0.0f;
    railSpeed_ = selectedRailTotalLength_ > kMinRailLength ? railSpeed_ : 5.0f;
    debugLoop_ = selectedRailLoop_;
    previousRailId_ = selectedRailId_;
    previousSelectedRailIndex_ = selectedRailIndex_;
}

bool RailShooterCameraRig::FetchSelectedRailInfo() {
    if (!railRuntime_ || railCount_ == 0) {
        selectedRailId_.clear();
        selectedRailName_.clear();
        selectedRailType_.clear();
        selectedRailLoop_ = false;
        selectedRailPointCount_ = 0;
        selectedRailSampledPointCount_ = 0;
        selectedRailTotalLength_ = 0.0f;
        return false;
    }

    selectedRailIndex_ = std::clamp(selectedRailIndex_, 0, (std::max)(0, static_cast<int>(railCount_) - 1));
    LevelRailRuntimeRailInfo info;
    if (!railRuntime_->GetRailInfo(static_cast<size_t>(selectedRailIndex_), info)) {
        return false;
    }

    selectedRailId_ = info.railId;
    selectedRailName_ = info.name;
    selectedRailType_ = info.railType;
    selectedRailLoop_ = info.loop;
    selectedRailPointCount_ = info.pointCount;
    selectedRailSampledPointCount_ = info.sampledPointCount;
    selectedRailTotalLength_ = info.totalLength;
    if (previousRailId_ != selectedRailId_ || previousSelectedRailIndex_ != selectedRailIndex_) {
        railSpeed_ = info.speed;
    }
    return true;
}

void RailShooterCameraRig::UpdateStraight(float deltaTime) {
    currentEvaluationValid_ = false;
    currentPosition_ = AddVector3(currentPosition_, ScaleVector3(currentForward_, straightSpeed_ * deltaTime));
    BuildBasis(currentForward_, currentUp_, currentRight_, currentUp_);
}

void RailShooterCameraRig::UpdateRail(float deltaTime) {
    if (!FetchSelectedRailInfo()) {
        currentEvaluationValid_ = false;
        return;
    }
    SyncSelectedRailDefaults();

    const bool shouldLoop = debugLoop_ || selectedRailLoop_;
    if (autoPlay_ && selectedRailTotalLength_ > kMinRailLength) {
        railDistance_ += deltaTime * railSpeed_;
        if (shouldLoop) {
            railDistance_ = WrapDistance(railDistance_, selectedRailTotalLength_);
        } else if (railDistance_ >= selectedRailTotalLength_) {
            railDistance_ = selectedRailTotalLength_;
            railT_ = 1.0f;
            const LevelRailEvaluation endEvaluation = railRuntime_->EvaluateByDistance(selectedRailId_, railDistance_, false);
            if (endEvaluation.valid) {
                currentPosition_ = endEvaluation.position;
                currentForward_ = SmoothCameraForward(endEvaluation.forward, deltaTime);
                currentSegmentIndex_ = endEvaluation.segmentIndex;
                currentEvaluationValid_ = true;
                BuildBasis(currentForward_, { 0.0f, 1.0f, 0.0f }, currentRight_, currentUp_);
                ApplyToCamera();
            }
            mode_ = Mode::RailFinishedStraight;
            return;
        } else if (railDistance_ < 0.0f) {
            railDistance_ = 0.0f;
            autoPlay_ = false;
        }
        railT_ = NormalizeT(railDistance_, selectedRailTotalLength_);
    }

    railDistance_ = shouldLoop
        ? WrapDistance(railDistance_, selectedRailTotalLength_)
        : ClampDistance(railDistance_, selectedRailTotalLength_);
    railT_ = NormalizeT(railDistance_, selectedRailTotalLength_);

    const LevelRailEvaluation evaluation = railRuntime_->EvaluateByDistance(selectedRailId_, railDistance_, shouldLoop);
    currentEvaluationValid_ = evaluation.valid;
    if (!evaluation.valid) {
        return;
    }

    currentPosition_ = evaluation.position;
    currentForward_ = SmoothCameraForward(evaluation.forward, deltaTime);
    currentSegmentIndex_ = evaluation.segmentIndex;
    railDistance_ = evaluation.distance;
    railT_ = evaluation.t;
    BuildBasis(currentForward_, { 0.0f, 1.0f, 0.0f }, currentRight_, currentUp_);
    ApplyToCamera();
}

void RailShooterCameraRig::UpdateRailFinishedStraight(float deltaTime) {
    currentEvaluationValid_ = false;
    currentPosition_ = AddVector3(currentPosition_, ScaleVector3(currentForward_, straightSpeed_ * deltaTime));
    BuildBasis(currentForward_, currentUp_, currentRight_, currentUp_);
}

void RailShooterCameraRig::ApplyToCamera() {
    if (!camera_) {
        return;
    }

    currentForward_ = Normalize(currentForward_, { 0.0f, 0.0f, 1.0f });
    BuildBasis(currentForward_, currentUp_, currentRight_, currentUp_);
    camera_->SetTranslate(currentPosition_);
    camera_->SetRotate(MakeCameraRotationFromForward(currentForward_));
}

void RailShooterCameraRig::CaptureCameraPose() {
    if (!camera_) {
        return;
    }

    currentPosition_ = camera_->GetTranslate();
    currentForward_ = GetCameraForward(*camera_);
    currentUp_ = GetCameraUp(*camera_);
    currentRight_ = GetCameraRight(*camera_);
    hasSmoothedForward_ = true;
}

void RailShooterCameraRig::CaptureDebugHomeCamera() {
    if (!camera_) {
        return;
    }

    savedCameraPosition_ = camera_->GetTranslate();
    savedCameraRotation_ = camera_->GetRotate();
    hasSavedCameraPose_ = true;
}

void RailShooterCameraRig::RestoreDebugHomeCamera() {
    if (!camera_ || !hasSavedCameraPose_) {
        return;
    }

    camera_->SetTranslate(savedCameraPosition_);
    camera_->SetRotate(savedCameraRotation_);
    camera_->Update();
    CaptureCameraPose();
}

void RailShooterCameraRig::SaveDebugHomeCameraIfNeeded() {
    if (!hasSavedCameraPose_) {
        CaptureDebugHomeCamera();
    }
}

Vector3 RailShooterCameraRig::SmoothCameraForward(const Vector3& targetForward, float deltaTime) {
    const Vector3 safeTarget = Normalize(targetForward, currentForward_);
    if (!enableCameraForwardSmoothing_ || forwardSmoothStrength_ <= 0.0f || !hasSmoothedForward_ || deltaTime <= 0.0f) {
        hasSmoothedForward_ = true;
        return safeTarget;
    }

    const float alpha = std::clamp(1.0f - std::exp(-forwardSmoothStrength_ * (std::max)(0.0f, deltaTime)), 0.0f, 1.0f);
    return Normalize({
        currentForward_.x + (safeTarget.x - currentForward_.x) * alpha,
        currentForward_.y + (safeTarget.y - currentForward_.y) * alpha,
        currentForward_.z + (safeTarget.z - currentForward_.z) * alpha,
        }, safeTarget);
}

void RailShooterCameraRig::ResetCameraRig() {
    railDistance_ = 0.0f;
    railT_ = 0.0f;
    currentSegmentIndex_ = 0;
    currentEvaluationValid_ = false;
    autoPlay_ = false;
    previousRailId_.clear();
    previousSelectedRailIndex_ = -1;
    hasSmoothedForward_ = false;
    CaptureCameraPose();
}

void RailShooterCameraRig::ResetCameraRigAndRestoreCamera() {
    railDistance_ = 0.0f;
    railT_ = 0.0f;
    currentSegmentIndex_ = 0;
    currentEvaluationValid_ = false;
    autoPlay_ = false;
    mode_ = Mode::Disabled;
    enableCameraRig_ = false;
    previousRailId_.clear();
    previousSelectedRailIndex_ = -1;
    hasSmoothedForward_ = false;
    RestoreDebugHomeCamera();
    wasCameraRigActive_ = false;
}

void RailShooterCameraRig::StartRail() {
    SaveDebugHomeCameraIfNeeded();
    enableCameraRig_ = true;
    useCameraRig_ = true;
    mode_ = Mode::Rail;
    hasSmoothedForward_ = false;
    railCount_ = railRuntime_ ? railRuntime_->GetRailCount() : 0;
    if (!FetchSelectedRailInfo()) {
        currentEvaluationValid_ = false;
        return;
    }
    SyncSelectedRailDefaults();
    UpdateRail(0.0f);
}

void RailShooterCameraRig::StopRail() {
    mode_ = Mode::Disabled;
    autoPlay_ = false;
    currentEvaluationValid_ = false;
    if (restoreCameraOnStop_) {
        RestoreDebugHomeCamera();
    } else {
        CaptureCameraPose();
    }
    wasCameraRigActive_ = false;
}
