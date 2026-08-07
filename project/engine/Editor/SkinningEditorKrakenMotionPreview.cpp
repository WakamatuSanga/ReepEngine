#include "SkinningEditorKrakenMotionPreview.h"
#include "SkinningEditor.h"
#include "Engine/Animation/Skeleton.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>

namespace {
    constexpr float kDegreesToRadians = std::numbers::pi_v<float> / 180.0f;

    bool IsFiniteVector(const Vector3& value) {
        return std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
    }

    bool IsFiniteMatrix(const Matrix4x4& matrix) {
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                if (!std::isfinite(matrix.m[row][column])) {
                    return false;
                }
            }
        }
        return true;
    }
}

SkinningEditor::~SkinningEditor() = default;

void SkinningEditor::SetKrakenMotionPreviewTarget(
    Skeleton* skeleton,
    GltfSkinnedModel* model) {
    if (!krakenMotionPreview_) {
        krakenMotionPreview_ =
            std::make_unique<SkinningEditorKrakenMotionPreview>();
    }
    krakenMotionPreview_->SetTarget(skeleton, model);
}

void SkinningEditor::RefreshKrakenMotionPreviewDiagnostics() {
    if (krakenMotionPreview_ &&
        krakenMotionPreview_->IsTarget(targetSkeleton_)) {
        krakenMotionPreview_->RefreshDiagnosticsAndRecover();
    }
}

void SkinningEditor::UpdateKrakenMotionPreview(float unscaledDeltaTime) {
    if (krakenMotionPreview_ &&
        krakenMotionPreview_->IsTarget(targetSkeleton_)) {
        krakenMotionPreview_->Update(
            unscaledDeltaTime,
            selectedJointIndex_);
    }
}

void SkinningEditor::ClearKrakenMotionPreviewTarget() {
    if (krakenMotionPreview_) {
        krakenMotionPreview_->ClearTarget();
    }
}

bool SkinningEditor::IsKrakenMotionPreviewTarget() const {
    return krakenMotionPreview_ &&
        krakenMotionPreview_->IsTarget(targetSkeleton_);
}

bool SkinningEditorKrakenMotionPreview::IsTarget(
    const Skeleton* skeleton) const {
    return skeleton_ &&
        skeleton_ == skeleton &&
        targetCompatible_;
}

bool SkinningEditorKrakenMotionPreview::IsProceduralActive() const {
    return IsTarget(skeleton_) && mode_ == Mode::IdleSway;
}

void SkinningEditorKrakenMotionPreview::SetTarget(
    Skeleton* skeleton,
    GltfSkinnedModel* model) {
    ClearTarget();
    skeleton_ = skeleton;
    model_ = model;
    diagnostics_.skeletonEnabled = skeleton_ != nullptr;

    if (!skeleton_ ||
        skeleton_->root < 0 ||
        skeleton_->root >= static_cast<int32_t>(skeleton_->joints.size())) {
        runtimeError_ = "\u6709\u52B9\u306A\u30B9\u30B1\u30EB\u30C8\u30F3\u30EB\u30FC\u30C8\u3092\u53D6\u5F97\u3067\u304D\u307E\u305B\u3093\u3002";
        return;
    }

    const Joint& rootJoint =
        skeleton_->joints[static_cast<std::size_t>(skeleton_->root)];
    targetCompatible_ =
        skeleton_->joints.size() == kExpectedJointCount &&
        rootJoint.name == kExpectedRootName;
    if (!targetCompatible_) {
        runtimeError_ = "\u4E92\u63DB\u89E6\u624B\u30B9\u30B1\u30EB\u30C8\u30F3\u3067\u306F\u3042\u308A\u307E\u305B\u3093\u3002";
        return;
    }

    CaptureBindPose();
    hierarchyValid_ = ValidateBindPose() && DetectChains();
    if (!hierarchyValid_ && hierarchyError_.empty()) {
        SetHierarchyError("\u89E6\u624B\u968E\u5C64\u3092\u691C\u8A3C\u3067\u304D\u307E\u305B\u3093\u3067\u3057\u305F\u3002");
    }

    mode_ = Mode::Manual;
    isPaused_ = true;
    rootRotationAllowed_ = false;
    applyAllChains_ = true;
    selectedChainIndex_ = 0;
    motionTime_ = 0.0f;
    ResetIdleSettings();
    RestoreBindLocals();
    UpdateSkeletonWorldTransforms(*skeleton_);
    CaptureBindPalette();
    RefreshDiagnostics();
}

void SkinningEditorKrakenMotionPreview::ClearTarget() {
    if (skeleton_ && bindPose_.size() == skeleton_->joints.size()) {
        RestoreBindLocals();
        UpdateSkeletonWorldTransforms(*skeleton_);
    }

    skeleton_ = nullptr;
    model_ = nullptr;
    bindPose_.clear();
    manualRotationDegrees_.clear();
    bindPalette_.clear();
    chains_.clear();
    diagnostics_ = {};
    hierarchyError_.clear();
    runtimeError_.clear();
    mode_ = Mode::Manual;
    isPaused_ = true;
    rootRotationAllowed_ = false;
    applyAllChains_ = true;
    hierarchyValid_ = false;
    targetCompatible_ = false;
    recovering_ = false;
    selectedChainIndex_ = 0;
    motionTime_ = 0.0f;
    ResetIdleSettings();
}

void SkinningEditorKrakenMotionPreview::CaptureBindPose() {
    bindPose_.clear();
    manualRotationDegrees_.clear();
    if (!skeleton_) {
        return;
    }

    bindPose_.reserve(skeleton_->joints.size());
    manualRotationDegrees_.resize(skeleton_->joints.size());
    for (const Joint& joint : skeleton_->joints) {
        bindPose_.push_back({
            joint.localTranslate,
            joint.localRotate,
            joint.localScale,
            });
    }
}

bool SkinningEditorKrakenMotionPreview::ValidateBindPose() const {
    if (!skeleton_ || bindPose_.size() != skeleton_->joints.size()) {
        return false;
    }
    for (const BindLocalPose& pose : bindPose_) {
        if (!IsFiniteVector(pose.translate) ||
            !IsFiniteVector(pose.rotate) ||
            !IsFiniteVector(pose.scale)) {
            return false;
        }
    }
    return true;
}

bool SkinningEditorKrakenMotionPreview::DetectChains() {
    chains_.clear();
    hierarchyError_.clear();
    if (!skeleton_ ||
        skeleton_->root < 0 ||
        skeleton_->root >= static_cast<int32_t>(skeleton_->joints.size())) {
        SetHierarchyError("\u30EB\u30FC\u30C8\u30B8\u30E7\u30A4\u30F3\u30C8\u304C\u4E0D\u6B63\u3067\u3059\u3002");
        return false;
    }

    const int jointCount = static_cast<int>(skeleton_->joints.size());
    const int rootIndex = skeleton_->root;
    std::vector<int> incoming(static_cast<std::size_t>(jointCount), 0);
    for (int parentIndex = 0; parentIndex < jointCount; ++parentIndex) {
        const Joint& parent =
            skeleton_->joints[static_cast<std::size_t>(parentIndex)];
        for (int childIndex : parent.children) {
            if (childIndex < 0 || childIndex >= jointCount) {
                SetHierarchyError("\u7BC4\u56F2\u5916\u306E\u5B50\u30B8\u30E7\u30A4\u30F3\u30C8\u3092\u691C\u51FA\u3057\u307E\u3057\u305F\u3002");
                return false;
            }
            ++incoming[static_cast<std::size_t>(childIndex)];
            if (skeleton_->joints[
                static_cast<std::size_t>(childIndex)].parentIndex !=
                parentIndex) {
                SetHierarchyError("\u89AA\u5B50\u30B8\u30E7\u30A4\u30F3\u30C8\u306E\u5BFE\u5FDC\u304C\u4E00\u81F4\u3057\u307E\u305B\u3093\u3002");
                return false;
            }
        }
    }

    for (int jointIndex = 0; jointIndex < jointCount; ++jointIndex) {
        const int expectedIncoming = jointIndex == rootIndex ? 0 : 1;
        if (incoming[static_cast<std::size_t>(jointIndex)] !=
            expectedIncoming) {
            SetHierarchyError(
                jointIndex == rootIndex
                ? "\u30EB\u30FC\u30C8\u306B\u89AA\u53C2\u7167\u304C\u3042\u308A\u307E\u3059\u3002"
                : "\u8907\u6570\u306E\u89AA\u307E\u305F\u306F\u89AA\u306A\u3057\u30B8\u30E7\u30A4\u30F3\u30C8\u3092\u691C\u51FA\u3057\u307E\u3057\u305F\u3002");
            return false;
        }
    }

    const Joint& root =
        skeleton_->joints[static_cast<std::size_t>(rootIndex)];
    if (root.children.empty()) {
        SetHierarchyError("\u89E6\u624B\u30C1\u30A7\u30FC\u30F3\u304C0\u672C\u3067\u3059\u3002");
        return false;
    }

    std::vector<bool> visited(
        static_cast<std::size_t>(jointCount),
        false);
    visited[static_cast<std::size_t>(rootIndex)] = true;
    for (int startIndex : root.children) {
        Chain chain{};
        int jointIndex = startIndex;
        while (true) {
            if (jointIndex < 0 ||
                jointIndex >= jointCount ||
                visited[static_cast<std::size_t>(jointIndex)]) {
                SetHierarchyError("\u5FAA\u74B0\u307E\u305F\u306F\u91CD\u8907\u30B8\u30E7\u30A4\u30F3\u30C8\u3092\u691C\u51FA\u3057\u307E\u3057\u305F\u3002");
                return false;
            }

            visited[static_cast<std::size_t>(jointIndex)] = true;
            chain.joints.push_back(jointIndex);
            const Joint& joint =
                skeleton_->joints[static_cast<std::size_t>(jointIndex)];
            if (joint.children.empty()) {
                break;
            }
            if (joint.children.size() != 1) {
                SetHierarchyError("\u89E6\u624B\u30C1\u30A7\u30FC\u30F3\u9014\u4E2D\u306E\u5206\u5C90\u3092\u691C\u51FA\u3057\u307E\u3057\u305F\u3002");
                return false;
            }
            jointIndex = joint.children.front();
        }
        if (chain.joints.empty()) {
            SetHierarchyError("\u7A7A\u306E\u89E6\u624B\u30C1\u30A7\u30FC\u30F3\u3092\u691C\u51FA\u3057\u307E\u3057\u305F\u3002");
            return false;
        }
        chains_.push_back(std::move(chain));
    }

    if (!std::all_of(
        visited.begin(),
        visited.end(),
        [](bool value) { return value; })) {
        SetHierarchyError("\u30EB\u30FC\u30C8\u914D\u4E0B\u3067\u306A\u3044\u30B8\u30E7\u30A4\u30F3\u30C8\u3092\u691C\u51FA\u3057\u307E\u3057\u305F\u3002");
        return false;
    }
    return !chains_.empty();
}

void SkinningEditorKrakenMotionPreview::SetHierarchyError(
    const std::string& message) {
    hierarchyValid_ = false;
    hierarchyError_ = message;
    mode_ = Mode::Manual;
    isPaused_ = true;
}

void SkinningEditorKrakenMotionPreview::RestoreBindLocals() {
    if (!skeleton_ || bindPose_.size() != skeleton_->joints.size()) {
        return;
    }
    for (std::size_t jointIndex = 0;
        jointIndex < skeleton_->joints.size();
        ++jointIndex) {
        Joint& joint = skeleton_->joints[jointIndex];
        const BindLocalPose& pose = bindPose_[jointIndex];
        joint.localTranslate = pose.translate;
        joint.localRotate = pose.rotate;
        joint.localScale = pose.scale;
    }
}

void SkinningEditorKrakenMotionPreview::ApplyManualPose() {
    if (!skeleton_ ||
        manualRotationDegrees_.size() != skeleton_->joints.size()) {
        return;
    }
    for (std::size_t jointIndex = 0;
        jointIndex < skeleton_->joints.size();
        ++jointIndex) {
        if (static_cast<int>(jointIndex) == skeleton_->root &&
            !rootRotationAllowed_) {
            continue;
        }
        const Vector3 offset = manualRotationDegrees_[jointIndex];
        Joint& joint = skeleton_->joints[jointIndex];
        joint.localRotate.x += offset.x * kDegreesToRadians;
        joint.localRotate.y += offset.y * kDegreesToRadians;
        joint.localRotate.z += offset.z * kDegreesToRadians;
    }
}

void SkinningEditorKrakenMotionPreview::ApplyIdleSwayPose() {
    if (!skeleton_ || !hierarchyValid_ || chains_.empty()) {
        return;
    }

    const float angularSpeed =
        2.0f * std::numbers::pi_v<float> * frequencyHz_;
    constexpr float kBlendInSeconds = 0.25f;
    const float startupBlend = std::clamp(
        motionTime_ / kBlendInSeconds, 0.0f, 1.0f);
    for (std::size_t chainIndex = 0;
        chainIndex < chains_.size();
        ++chainIndex) {
        if (!applyAllChains_ &&
            static_cast<int>(chainIndex) != selectedChainIndex_) {
            continue;
        }

        const Chain& chain = chains_[chainIndex];
        const int denominator =
            (std::max)(static_cast<int>(chain.joints.size()) - 1, 1);
        const float chainPhase =
            static_cast<float>(chainIndex) * chainPhaseRadians_;
        for (std::size_t boneIndex = 0;
            boneIndex < chain.joints.size();
            ++boneIndex) {
            const float chainT =
                static_cast<float>(boneIndex) /
                static_cast<float>(denominator);
            const float amplitudeDegrees =
                rootAmplitudeDegrees_ +
                (tipAmplitudeDegrees_ - rootAmplitudeDegrees_) * chainT;
            const float angleA =
                std::sin(
                    motionTime_ * angularSpeed +
                    chainPhase +
                    chainT * phaseAlongChainRadians_) *
                amplitudeDegrees * startupBlend;
            const float angleB =
                std::cos(
                    motionTime_ * angularSpeed * 0.73f +
                    chainPhase) *
                secondaryAmplitudeDegrees_ *
                chainT * startupBlend;

            Joint& joint = skeleton_->joints[
                static_cast<std::size_t>(chain.joints[boneIndex])];
            joint.localRotate.x += angleA * kDegreesToRadians;
            joint.localRotate.z += angleB * kDegreesToRadians;
        }
    }
}

void SkinningEditorKrakenMotionPreview::ApplyCurrentPose() {
    if (!IsTarget(skeleton_)) {
        return;
    }

    RestoreBindLocals();
    if (mode_ == Mode::Manual) {
        ApplyManualPose();
    } else if (hierarchyValid_) {
        ApplyIdleSwayPose();
    }

    if (!ValidateCurrentPose()) {
        runtimeError_ =
            "\u975E\u6709\u9650\u306E\u30DD\u30FC\u30BA\u3092\u691C\u51FA\u3057\u305F\u305F\u3081\u3001\u30D0\u30A4\u30F3\u30C9\u30DD\u30FC\u30BA\u3078\u623B\u3057\u307E\u3057\u305F\u3002";
        ReturnToBindPose(false);
        return;
    }
    UpdateSkeletonWorldTransforms(*skeleton_);
    if (!ValidateCurrentPose()) {
        runtimeError_ =
            "\u975E\u6709\u9650\u306E\u968E\u5C64\u884C\u5217\u3092\u691C\u51FA\u3057\u305F\u305F\u3081\u3001\u30D0\u30A4\u30F3\u30C9\u30DD\u30FC\u30BA\u3078\u623B\u3057\u307E\u3057\u305F\u3002";
        ReturnToBindPose(false);
    }
}

bool SkinningEditorKrakenMotionPreview::ValidateCurrentPose() const {
    if (!skeleton_) {
        return false;
    }
    for (const Joint& joint : skeleton_->joints) {
        if (!IsFiniteVector(joint.localTranslate) ||
            !IsFiniteVector(joint.localRotate) ||
            !IsFiniteVector(joint.localScale) ||
            !IsFiniteMatrix(joint.worldMatrix)) {
            return false;
        }
    }
    return true;
}

void SkinningEditorKrakenMotionPreview::Update(
    float unscaledDeltaTime,
    int selectedJointIndex) {
    if (!IsTarget(skeleton_)) {
        return;
    }

    UpdateSelectedChainFromJoint(selectedJointIndex);
    if (mode_ == Mode::IdleSway) {
        if (!hierarchyValid_) {
            SwitchToManual();
        } else if (!isPaused_) {
            const float safeDeltaTime = std::clamp(
                std::isfinite(unscaledDeltaTime)
                ? unscaledDeltaTime
                : 0.0f,
                0.0f,
                0.1f);
            motionTime_ += safeDeltaTime;
        }
    }
    ApplyCurrentPose();
}

void SkinningEditorKrakenMotionPreview::UpdateSelectedChainFromJoint(
    int selectedJointIndex) {
    for (std::size_t chainIndex = 0;
        chainIndex < chains_.size();
        ++chainIndex) {
        const std::vector<int>& joints = chains_[chainIndex].joints;
        if (std::find(
            joints.begin(),
            joints.end(),
            selectedJointIndex) != joints.end()) {
            selectedChainIndex_ = static_cast<int>(chainIndex);
            return;
        }
    }
    if (!chains_.empty()) {
        selectedChainIndex_ = std::clamp(
            selectedChainIndex_,
            0,
            static_cast<int>(chains_.size()) - 1);
    }
}

void SkinningEditorKrakenMotionPreview::SwitchToManual() {
    mode_ = Mode::Manual;
    isPaused_ = true;
    motionTime_ = 0.0f;
    ApplyCurrentPose();
}

void SkinningEditorKrakenMotionPreview::StartIdleSway() {
    if (!hierarchyValid_) {
        return;
    }
    mode_ = Mode::IdleSway;
    isPaused_ = false;
    motionTime_ = 0.0f;
    ApplyCurrentPose();
}

void SkinningEditorKrakenMotionPreview::ReturnToBindPose(
    bool clearError) {
    std::fill(
        manualRotationDegrees_.begin(),
        manualRotationDegrees_.end(),
        Vector3{});
    mode_ = Mode::Manual;
    isPaused_ = true;
    motionTime_ = 0.0f;
    RestoreBindLocals();
    if (skeleton_) {
        UpdateSkeletonWorldTransforms(*skeleton_);
    }
    if (clearError) {
        runtimeError_.clear();
        diagnostics_.safetyRecoveryOccurred = false;
    }
}

void SkinningEditorKrakenMotionPreview::ResetIdleSettings() {
    frequencyHz_ = 0.35f;
    rootAmplitudeDegrees_ = 3.0f;
    tipAmplitudeDegrees_ = 15.0f;
    secondaryAmplitudeDegrees_ = 6.0f;
    chainPhaseRadians_ = 0.75f;
    phaseAlongChainRadians_ = 0.55f;
}
