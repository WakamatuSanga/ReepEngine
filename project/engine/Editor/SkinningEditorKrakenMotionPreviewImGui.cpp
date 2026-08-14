#include "SkinningEditorKrakenMotionPreview.h"
#include "SkinningEditor.h"
#include "SkinningEditorKrakenAttackMotion.h"
#include "Engine/Animation/Skeleton.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
#ifdef USE_IMGUI
    const char* YesNo(bool value) {
        return value ? "\u306F\u3044" : "\u3044\u3044\u3048";
    }

    void DrawTooltip(const char* text) {
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", text);
        }
    }
#endif
}

void SkinningEditor::DrawKrakenMotionPreviewImGui() {
#ifdef USE_IMGUI
    if (krakenMotionPreview_ &&
        krakenMotionPreview_->IsTarget(targetSkeleton_)) {
        krakenMotionPreview_->DrawImGui(selectedJointIndex_);
    }
#endif
}

void SkinningEditor::BeginKrakenLegacyPoseEditingGuard() const {
#ifdef USE_IMGUI
    if (IsKrakenMotionPreviewTarget()) {
        ImGui::BeginDisabled();
    }
#endif
}

void SkinningEditor::EndKrakenLegacyPoseEditingGuard() const {
#ifdef USE_IMGUI
    if (IsKrakenMotionPreviewTarget()) {
        ImGui::EndDisabled();
    }
#endif
}

void SkinningEditorKrakenMotionPreview::DrawImGui(
    int selectedJointIndex) {
#ifdef USE_IMGUI
    if (!IsTarget(skeleton_)) {
        return;
    }

    UpdateSelectedChainFromJoint(selectedJointIndex);
    ImGui::SeparatorText(
        "\u89E6\u624B\u30B9\u30AD\u30CB\u30F3\u30B0\u52D5\u4F5C\u30D7\u30EC\u30D3\u30E5\u30FC##KrakenMotionPreview");

    const int jointCount =
        static_cast<int>(skeleton_->joints.size());
    const int rootIndex = skeleton_->root;
    const char* rootName =
        rootIndex >= 0 && rootIndex < jointCount
        ? skeleton_->joints[static_cast<std::size_t>(rootIndex)].name.c_str()
        : "\u306A\u3057";
    const char* selectedBoneName =
        selectedJointIndex >= 0 && selectedJointIndex < jointCount
        ? skeleton_->joints[
            static_cast<std::size_t>(selectedJointIndex)].name.c_str()
        : "\u306A\u3057";
    const char* modeName =
        mode_ == Mode::Manual
        ? "\u624B\u52D5\u30DD\u30FC\u30BA"
        : (mode_ == Mode::IdleSway
            ? "\u30A2\u30A4\u30C9\u30EB\u30B9\u30A6\u30A7\u30A4"
            : "\u89E6\u624B\u653B\u6483\u30D7\u30EC\u30D3\u30E5\u30FC");
    const bool isPlaying =
        (mode_ == Mode::IdleSway && !isPaused_) ||
        (mode_ == Mode::AttackSlamPreview &&
            attackMotion_ && attackMotion_->IsPlaying() &&
            !attackMotion_->IsPaused());
    const float displayedMotionTime =
        mode_ == Mode::AttackSlamPreview && attackMotion_
        ? attackMotion_->GetElapsedTime()
        : motionTime_;
    const bool manualOffsetActive = std::any_of(
        manualRotationDegrees_.begin(),
        manualRotationDegrees_.end(),
        [](const Vector3& value) {
            return std::fabs(value.x) > 0.0001f ||
                std::fabs(value.y) > 0.0001f ||
                std::fabs(value.z) > 0.0001f;
        });

    ImGui::Text("\u30D7\u30EC\u30D3\u30E5\u30FC\u30E2\u30C7\u30EB: \u4E92\u63DB\u30AF\u30E9\u30FC\u30B1\u30F3\u89E6\u624B");
    ImGui::Text("\u30B9\u30B1\u30EB\u30C8\u30F3\u6709\u52B9: %s", YesNo(diagnostics_.skeletonEnabled));
    ImGui::Text("\u30B8\u30E7\u30A4\u30F3\u30C8\u6570: %d", jointCount);
    ImGui::Text("\u30EB\u30FC\u30C8\u30B8\u30E7\u30A4\u30F3\u30C8: %s", rootName);
    ImGui::Text("\u691C\u51FA\u3057\u305F\u89E6\u624B\u30C1\u30A7\u30FC\u30F3\u6570: %d",
        static_cast<int>(chains_.size()));
    ImGui::Text("\u9078\u629E\u30DC\u30FC\u30F3: %s", selectedBoneName);
    ImGui::Text("\u73FE\u5728\u306E\u52D5\u4F5C\u30E2\u30FC\u30C9: %s", modeName);
    ImGui::Text("\u52D5\u4F5C\u518D\u751F\u4E2D: %s", YesNo(isPlaying));
    ImGui::Text("\u52D5\u4F5C\u6642\u9593: %.3f \u79D2", displayedMotionTime);

    if (hierarchyValid_ && !chains_.empty()) {
        for (std::size_t chainIndex = 0;
            chainIndex < chains_.size();
            ++chainIndex) {
            ImGui::Text(
                "\u89E6\u624B %d: %d \u30DC\u30FC\u30F3",
                static_cast<int>(chainIndex) + 1,
                static_cast<int>(chains_[chainIndex].joints.size()));
        }

        const Chain& selectedChain = chains_[
            static_cast<std::size_t>(selectedChainIndex_)];
        const Joint& chainRoot = skeleton_->joints[
            static_cast<std::size_t>(selectedChain.joints.front())];
        const Joint& chainTip = skeleton_->joints[
            static_cast<std::size_t>(selectedChain.joints.back())];
        ImGui::Text("\u9078\u629E\u30C1\u30A7\u30FC\u30F3: \u89E6\u624B %d",
            selectedChainIndex_ + 1);
        ImGui::Text("\u9078\u629E\u30C1\u30A7\u30FC\u30F3\u306E\u6839\u5143: %s",
            chainRoot.name.c_str());
        ImGui::Text("\u9078\u629E\u30C1\u30A7\u30FC\u30F3\u306E\u5148\u7AEF: %s",
            chainTip.name.c_str());
    }

    if (!hierarchyError_.empty()) {
        ImGui::TextColored(
            ImVec4(1.0f, 0.35f, 0.25f, 1.0f),
            "\u968E\u5C64\u30A8\u30E9\u30FC: %s",
            hierarchyError_.c_str());
    }
    if (!runtimeError_.empty()) {
        ImGui::TextColored(
            ImVec4(1.0f, 0.55f, 0.25f, 1.0f),
            "\u5B89\u5168\u8A3A\u65AD: %s",
            runtimeError_.c_str());
    }

    if (ImGui::Button("\u624B\u52D5\u30DD\u30FC\u30BA\u3078\u5207\u308A\u66FF\u3048##SwitchManual")) {
        SwitchToManual();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!hierarchyValid_);
    if (ImGui::Button("\u30A2\u30A4\u30C9\u30EB\u30B9\u30A6\u30A7\u30A4\u3092\u518D\u751F##StartIdle")) {
        StartIdleSway();
    }
    ImGui::EndDisabled();

    ImGui::BeginDisabled(mode_ != Mode::IdleSway || isPaused_);
    if (ImGui::Button("\u4E00\u6642\u505C\u6B62##PauseMotion")) {
        isPaused_ = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(mode_ != Mode::IdleSway || !isPaused_);
    if (ImGui::Button("\u518D\u958B##ResumeMotion")) {
        isPaused_ = false;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(mode_ == Mode::AttackSlamPreview);
    if (ImGui::Button("\u6642\u9593\u30920\u3078\u623B\u3059##ResetMotionTime")) {
        motionTime_ = 0.0f;
        ApplyCurrentPose();
    }
    ImGui::EndDisabled();

    if (ImGui::Button("\u30D0\u30A4\u30F3\u30C9\u30DD\u30FC\u30BA\u3078\u623B\u3059##ReturnBindPose")) {
        ReturnToBindPose(true);
    }
    DrawTooltip("\u5168\u624B\u52D5\u56DE\u8EE2\u3068\u81EA\u52D5\u52D5\u4F5C\u3092\u6D88\u53BB\u3057\u3001\u8AAD\u8FBC\u6642\u306E\u59FF\u52E2\u3078\u623B\u3057\u307E\u3059\u3002");
    ImGui::SameLine();
    ImGui::BeginDisabled(mode_ != Mode::Manual);
    if (ImGui::Button("\u5168\u624B\u52D5\u56DE\u8EE2\u3092\u6D88\u53BB##ClearAllManual")) {
        std::fill(
            manualRotationDegrees_.begin(),
            manualRotationDegrees_.end(),
            Vector3{});
        ApplyCurrentPose();
    }
    ImGui::EndDisabled();

    ImGui::SeparatorText("\u624B\u52D5\u30DC\u30FC\u30F3\u56DE\u8EE2##ManualBoneRotation");
    bool allowRootRotation = rootRotationAllowed_;
    ImGui::BeginDisabled(mode_ != Mode::Manual);
    if (ImGui::Checkbox(
        "\u30EB\u30FC\u30C8\u56DE\u8EE2\u3092\u8A31\u53EF##AllowRootRotation",
        &allowRootRotation)) {
        rootRotationAllowed_ = allowRootRotation;
        if (!rootRotationAllowed_ &&
            rootIndex >= 0 &&
            rootIndex < static_cast<int>(manualRotationDegrees_.size())) {
            manualRotationDegrees_[
                static_cast<std::size_t>(rootIndex)] = {};
        }
        ApplyCurrentPose();
    }
    ImGui::EndDisabled();
    DrawTooltip("\u65E2\u5B9A\u3067\u306F\u30EB\u30FC\u30C8\u3092\u56FA\u5B9A\u3057\u3001\u89E6\u624B\u90E8\u5206\u3060\u3051\u3092\u78BA\u8A8D\u3057\u307E\u3059\u3002");

    const bool hasSelectedBone =
        selectedJointIndex >= 0 &&
        selectedJointIndex < static_cast<int>(
            manualRotationDegrees_.size());
    const bool rootLocked =
        hasSelectedBone &&
        selectedJointIndex == rootIndex &&
        !rootRotationAllowed_;
    const bool disableManual =
        mode_ != Mode::Manual ||
        !hasSelectedBone ||
        rootLocked;
    ImGui::BeginDisabled(disableManual);
    if (hasSelectedBone) {
        Vector3& degrees = manualRotationDegrees_[
            static_cast<std::size_t>(selectedJointIndex)];
        bool changed = false;
        changed |= ImGui::DragFloat(
            "\u30ED\u30FC\u30AB\u30EB\u56DE\u8EE2 X\uFF08\u5EA6\uFF09##ManualRotationX",
            &degrees.x,
            0.5f,
            -180.0f,
            180.0f,
            "%.1f");
        changed |= ImGui::DragFloat(
            "\u30ED\u30FC\u30AB\u30EB\u56DE\u8EE2 Y\uFF08\u5EA6\uFF09##ManualRotationY",
            &degrees.y,
            0.5f,
            -180.0f,
            180.0f,
            "%.1f");
        changed |= ImGui::DragFloat(
            "\u30ED\u30FC\u30AB\u30EB\u56DE\u8EE2 Z\uFF08\u5EA6\uFF09##ManualRotationZ",
            &degrees.z,
            0.5f,
            -180.0f,
            180.0f,
            "%.1f");
        degrees.x = std::clamp(degrees.x, -180.0f, 180.0f);
        degrees.y = std::clamp(degrees.y, -180.0f, 180.0f);
        degrees.z = std::clamp(degrees.z, -180.0f, 180.0f);
        if (changed) {
            ApplyCurrentPose();
        }

        if (ImGui::Button(
            "\u9078\u629E\u30DC\u30FC\u30F3\u30920\u3078\u623B\u3059##ResetSelectedBone")) {
            degrees = {};
            ApplyCurrentPose();
        }
    }
    ImGui::EndDisabled();
    if (mode_ != Mode::Manual) {
        ImGui::TextDisabled(
            "\u81EA\u52D5\u52D5\u4F5C\u4E2D\u306F\u624B\u52D5\u56DE\u8EE2\u3092\u7DE8\u96C6\u3067\u304D\u307E\u305B\u3093\u3002");
    } else if (rootLocked) {
        ImGui::TextDisabled(
            "\u30EB\u30FC\u30C8\u56DE\u8EE2\u306F\u30ED\u30C3\u30AF\u3055\u308C\u3066\u3044\u307E\u3059\u3002");
    }
    ImGui::Text("\u624B\u52D5\u56DE\u8EE2\u9069\u7528\u4E2D: %s",
        YesNo(mode_ == Mode::Manual && manualOffsetActive));
    ImGui::TextDisabled(
        "\u89E6\u624B\u30D7\u30EC\u30D3\u30E5\u30FC\u4E2D\u306F\u65E2\u5B58\u306E\u7D76\u5BFE\u5909\u63DB\u7DE8\u96C6\u3068\u30AE\u30BA\u30E2\u3092\u7121\u52B9\u306B\u3057\u307E\u3059\u3002");

    ImGui::SeparatorText(
        "\u30A2\u30A4\u30C9\u30EB\u30B9\u30A6\u30A7\u30A4\u8A2D\u5B9A##IdleSwaySettings");
    ImGui::BeginDisabled(mode_ == Mode::IdleSway && !isPaused_);
    ImGui::DragFloat(
        "\u518D\u751F\u5468\u6CE2\u6570\uFF08Hz\uFF09##IdleFrequency",
        &frequencyHz_,
        0.01f,
        0.05f,
        2.0f,
        "%.2f");
    DrawTooltip("\u6BCE\u79D2\u306E\u63FA\u308C\u56DE\u6570\u3067\u3059\u3002");
    ImGui::DragFloat(
        "\u6839\u5143\u5074\u306E\u6700\u5927\u89D2\uFF08\u5EA6\uFF09##IdleRootAmplitude",
        &rootAmplitudeDegrees_,
        0.25f,
        0.0f,
        30.0f,
        "%.2f");
    ImGui::DragFloat(
        "\u5148\u7AEF\u5074\u306E\u6700\u5927\u89D2\uFF08\u5EA6\uFF09##IdleTipAmplitude",
        &tipAmplitudeDegrees_,
        0.25f,
        0.0f,
        45.0f,
        "%.2f");
    ImGui::DragFloat(
        "\u7B2C2\u8EF8\u306E\u89D2\u5EA6\uFF08\u5EA6\uFF09##IdleSecondaryAmplitude",
        &secondaryAmplitudeDegrees_,
        0.25f,
        0.0f,
        30.0f,
        "%.2f");
    ImGui::DragFloat(
        "\u30C1\u30A7\u30FC\u30F3\u4F4D\u76F8\u5DEE\uFF08\u30E9\u30B8\u30A2\u30F3\uFF09##IdleChainPhase",
        &chainPhaseRadians_,
        0.01f,
        0.0f,
        3.14f,
        "%.2f");
    ImGui::DragFloat(
        "\u30C1\u30A7\u30FC\u30F3\u5185\u4F4D\u76F8\u5DEE\uFF08\u30E9\u30B8\u30A2\u30F3\uFF09##IdleAlongPhase",
        &phaseAlongChainRadians_,
        0.01f,
        0.0f,
        3.14f,
        "%.2f");
    ImGui::EndDisabled();

    if (ImGui::RadioButton(
        "\u5168\u30C1\u30A7\u30FC\u30F3\u3078\u9069\u7528##ApplyAllChains",
        applyAllChains_)) {
        applyAllChains_ = true;
        ApplyCurrentPose();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton(
        "\u9078\u629E\u30C1\u30A7\u30FC\u30F3\u3060\u3051\u3078\u9069\u7528##ApplySelectedChain",
        !applyAllChains_)) {
        applyAllChains_ = false;
        ApplyCurrentPose();
    }
    if (ImGui::Button("\u63A8\u5968\u8A2D\u5B9A\u3078\u623B\u3059##ResetIdleSettings")) {
        ResetIdleSettings();
        ApplyCurrentPose();
    }

    DrawAttackMotionImGui();
    DrawBoneColliderPreviewImGui();

    ImGui::SeparatorText(
        "\u30B9\u30AD\u30CB\u30F3\u30B0\u8A3A\u65AD##SkinningDiagnostics");
    ImGui::Text("\u30B9\u30AD\u30CB\u30F3\u30B0\u66F4\u65B0\u6210\u529F: %s",
        YesNo(diagnostics_.skinningUpdateSucceeded));
    ImGui::Text("\u30D1\u30EC\u30C3\u30C8\u66F4\u65B0\u6210\u529F: %s",
        YesNo(diagnostics_.paletteUpdateSucceeded));
    ImGui::Text("\u30DC\u30FC\u30F3\u8868\u793A\u540C\u671F: %s",
        YesNo(diagnostics_.boneOverlaySynchronized));
    ImGui::Text("\u30D1\u30EC\u30C3\u30C8\u884C\u5217\u6570: %u",
        diagnostics_.paletteMatrixCount);
    ImGui::Text("\u975E\u6709\u9650\u30D1\u30EC\u30C3\u30C8\u884C\u5217\u6570: %u",
        diagnostics_.nonFinitePaletteMatrixCount);
    ImGui::Text("\u5358\u4F4D\u30D1\u30EC\u30C3\u30C8\u884C\u5217\u6570: %u",
        diagnostics_.identityPaletteMatrixCount);
    ImGui::Text("\u5909\u5316\u3057\u305F\u30D1\u30EC\u30C3\u30C8\u884C\u5217\u6570: %u",
        diagnostics_.changedPaletteMatrixCount);
    ImGui::Text("\u30A6\u30A7\u30A4\u30C8\u53C2\u7167\u30B8\u30E7\u30A4\u30F3\u30C8\u6570: %u",
        diagnostics_.weightReferencedJointCount);
    ImGui::Text("\u30B9\u30AD\u30CB\u30F3\u30B0\u5BFE\u8C61\u9802\u70B9\u6570: %u",
        diagnostics_.skinnedVertexCount);
    ImGui::Text("\u975E\u6709\u9650\u30B9\u30AD\u30CB\u30F3\u30B0\u9802\u70B9\u6570: %u",
        diagnostics_.nonFiniteSkinnedVertexCount);
    ImGui::Text("\u30A6\u30A7\u30A4\u30C8\u306A\u3057\u9802\u70B9\u6570: %u",
        diagnostics_.verticesWithoutWeights);
    ImGui::Text("\u7121\u52B9\u30B8\u30E7\u30A4\u30F3\u30C8\u53C2\u7167\u6570: %u",
        diagnostics_.invalidJointInfluenceCount);
    ImGui::Text("\u975E\u6709\u9650\u30A6\u30A7\u30A4\u30C8\u6570: %u",
        diagnostics_.nonFiniteWeightCount);
    ImGui::Text("\u30A6\u30A7\u30A4\u30C8\u5408\u8A08\u7570\u5E38\u9802\u70B9\u6570: %u",
        diagnostics_.invalidWeightSumVertexCount);
    ImGui::Text("\u6700\u5927\u5F71\u97FF\u30DC\u30FC\u30F3\u6570: %u",
        diagnostics_.maxPositiveInfluences);
    ImGui::Text("\u7570\u5E38\u306A\u5883\u754C\u3092\u691C\u51FA: %s",
        YesNo(diagnostics_.abnormalBoundsDetected));
    ImGui::Text("\u5B89\u5168\u5FA9\u5E30\u3092\u5B9F\u884C: %s",
        YesNo(diagnostics_.safetyRecoveryOccurred));

    if (diagnostics_.skinnedBounds.isValid) {
        const BoundsSnapshot& bounds = diagnostics_.skinnedBounds;
        ImGui::Text(
            "\u5909\u5F62\u5F8C\u306E\u6700\u5C0F\u5EA7\u6A19: %.3f, %.3f, %.3f",
            bounds.min.x,
            bounds.min.y,
            bounds.min.z);
        ImGui::Text(
            "\u5909\u5F62\u5F8C\u306E\u6700\u5927\u5EA7\u6A19: %.3f, %.3f, %.3f",
            bounds.max.x,
            bounds.max.y,
            bounds.max.z);
        ImGui::Text(
            "\u5909\u5F62\u5F8C\u306E\u5927\u304D\u3055: %.3f, %.3f, %.3f",
            bounds.size.x,
            bounds.size.y,
            bounds.size.z);
    }
#else
    (void)selectedJointIndex;
#endif
}
