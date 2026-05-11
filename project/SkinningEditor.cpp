#include "SkinningEditor.h"
#include "AnimationClip.h"
#include "Camera.h"
#include "Skeleton.h"
#include "WinApp.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <cstring>
#include <numbers>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#include "externals/imgui/ImGuizmo.h"
#endif

namespace {
#ifdef _DEBUG
    constexpr size_t kMaxUndoHistory = 64;

    const char* kGizmoOperationLabels[] = {
        "Translate",
        "Rotate",
        "Scale",
    };

    const char* kGizmoSpaceLabels[] = {
        "Local",
        "World",
    };

    int FindTrackIndexByJointName(const AnimationClip& clip, const std::string& jointName) {
        for (int trackIndex = 0; trackIndex < static_cast<int>(clip.tracks.size()); ++trackIndex) {
            if (clip.tracks[static_cast<size_t>(trackIndex)].jointName == jointName) {
                return trackIndex;
            }
        }
        return -1;
    }

    int FindKeyIndexByTime(const JointTrack& track, float time, float epsilon) {
        for (int keyIndex = 0; keyIndex < static_cast<int>(track.keys.size()); ++keyIndex) {
            if (std::fabs(track.keys[static_cast<size_t>(keyIndex)].time - time) <= epsilon) {
                return keyIndex;
            }
        }
        return -1;
    }

    int FindJointIndexByName(const Skeleton* skeleton, const std::string& jointName) {
        if (!skeleton) {
            return -1;
        }
        for (int jointIndex = 0; jointIndex < static_cast<int>(skeleton->joints.size()); ++jointIndex) {
            if (skeleton->joints[static_cast<size_t>(jointIndex)].name == jointName) {
                return jointIndex;
            }
        }
        return -1;
    }

    Vector4 TransformPoint(const Vector3& value, const Matrix4x4& matrix) {
        return {
            value.x * matrix.m[0][0] + value.y * matrix.m[1][0] + value.z * matrix.m[2][0] + matrix.m[3][0],
            value.x * matrix.m[0][1] + value.y * matrix.m[1][1] + value.z * matrix.m[2][1] + matrix.m[3][1],
            value.x * matrix.m[0][2] + value.y * matrix.m[1][2] + value.z * matrix.m[2][2] + matrix.m[3][2],
            value.x * matrix.m[0][3] + value.y * matrix.m[1][3] + value.z * matrix.m[2][3] + matrix.m[3][3]
        };
    }

    bool ProjectToScreen(const Vector3& worldPosition, const Camera* camera, ImVec2& outScreen) {
        if (!camera) {
            return false;
        }

        Vector4 clipPosition = TransformPoint(worldPosition, camera->GetViewProjectionMatrix());
        if (clipPosition.w <= 0.0001f) {
            return false;
        }

        float invW = 1.0f / clipPosition.w;
        float ndcX = clipPosition.x * invW;
        float ndcY = clipPosition.y * invW;
        float ndcZ = clipPosition.z * invW;

        if (ndcZ < 0.0f || ndcZ > 1.0f) {
            return false;
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        outScreen.x = viewport->Pos.x + ((ndcX * 0.5f) + 0.5f) * static_cast<float>(WinApp::kClientWidth);
        outScreen.y = viewport->Pos.y + ((-ndcY * 0.5f) + 0.5f) * static_cast<float>(WinApp::kClientHeight);
        return true;
    }

    void DrawJointNode(const Skeleton& skeleton, int jointIndex, int& selectedJointIndex) {
        if (jointIndex < 0 || jointIndex >= static_cast<int>(skeleton.joints.size())) {
            return;
        }

        const Joint& joint = skeleton.joints[static_cast<size_t>(jointIndex)];
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if (joint.children.empty()) {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (selectedJointIndex == jointIndex) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        bool isOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(jointIndex)), flags, "%s", joint.name.c_str());
        if (ImGui::IsItemClicked()) {
            selectedJointIndex = jointIndex;
        }

        if (isOpen) {
            for (int childIndex : joint.children) {
                DrawJointNode(skeleton, childIndex, selectedJointIndex);
            }
            ImGui::TreePop();
        }
    }
#endif
}

SkinningEditor::SkinningEditor() {
    SetBufferText(clipNameBuffer_, currentClip_.name);
    SetPathBufferText("resources/animation/NewClip.json");
}

void SkinningEditor::Update() {
    if (!targets_.empty()) {
        currentTargetIndex_ = std::clamp(currentTargetIndex_, 0, static_cast<int>(targets_.size()) - 1);
        SyncTargetFromIndex();
    }

    if (!targetSkeleton_ || targetSkeleton_->joints.empty()) {
        selectedJointIndex_ = -1;
        selectedTrackIndex_ = -1;
        selectedKeyIndex_ = -1;
        selectedKeys_.clear();
        isDraggingSelectedKey_ = false;
        isSelectionDragHistoryCaptured_ = false;
        isBoxSelecting_ = false;
        isGizmoActive_ = false;
        gizmoActiveJointIndex_ = -1;
        return;
    }

    selectedJointIndex_ = std::clamp(selectedJointIndex_, 0, static_cast<int>(targetSkeleton_->joints.size()) - 1);
    if (gizmoActiveJointIndex_ != selectedJointIndex_) {
        isGizmoActive_ = false;
        gizmoActiveJointIndex_ = -1;
    }
    RefreshSelectionState();

    if (isPlaying_ && !isPaused_ && hasClip_) {
        currentTime_ += (1.0f / 60.0f) * playbackSpeed_;
        float duration = (std::max)(currentClip_.duration, 0.0001f);
        if (currentTime_ > duration) {
            if (isLoop_) {
                currentTime_ = std::fmod(currentTime_, duration);
            } else {
                currentTime_ = duration;
                isPlaying_ = false;
            }
        }
        ApplyCurrentClipAtCurrentTime();
    }
}

void SkinningEditor::DrawImGui() {
#ifdef _DEBUG
    if (!isOpen_) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(540.0f, 360.0f), ImGuiCond_Once);
    if (!ImGui::Begin("Skinning Editor", &isOpen_)) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Delete All Models")) {
        ClearTargets();
    }
    ImGui::Separator();

    int deleteTargetIndex = -1;
    if (!targets_.empty()) {
        ImGui::TextUnformatted("Registered Models");
        ImGui::BeginChild("SkinningTargets", ImVec2(0.0f, 110.0f), true);
        for (int targetIndex = 0; targetIndex < static_cast<int>(targets_.size()); ++targetIndex) {
            const TargetEntry& target = targets_[static_cast<size_t>(targetIndex)];
            const bool isCurrentTarget = (targetIndex == currentTargetIndex_);

            ImGui::PushID(targetIndex);
            ImGui::Text("%s%s", isCurrentTarget ? "> " : "", target.label.c_str());
            ImGui::SameLine();
            if (ImGui::Button("Delete")) {
                deleteTargetIndex = targetIndex;
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::Separator();
    }

    if (deleteTargetIndex >= 0) {
        DeleteTargetAt(deleteTargetIndex);
    }

    if (!targets_.empty()) {
        std::vector<const char*> targetLabels;
        targetLabels.reserve(targets_.size());
        for (const TargetEntry& target : targets_) {
            targetLabels.push_back(target.label.c_str());
        }
        ImGui::Combo("Target", &currentTargetIndex_, targetLabels.data(), static_cast<int>(targetLabels.size()));
        SyncTargetFromIndex();
    } else {
        ImGui::Text("Target: %s", targetLabel_.c_str());
    }

    if (!targetSkeleton_) {
        ImGui::TextDisabled("No skinning target is assigned.");
        ImGui::End();
        return;
    }

    ImGui::Text("Skeleton: %s", targetSkeleton_->name.c_str());
    ImGui::Text("Bone Count: %d", static_cast<int>(targetSkeleton_->joints.size()));
    if (ImGui::InputText("Clip Name", clipNameBuffer_.data(), clipNameBuffer_.size())) {
        SyncClipNameFromBuffer();
    }
    ImGui::InputText("JSON Path", jsonPathBuffer_.data(), jsonPathBuffer_.size());

    if (ImGui::Button("Save JSON")) {
        SaveCurrentClipToJson(false);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save As New JSON")) {
        SaveCurrentClipToJson(true);
    }
    ImGui::SameLine();
    if (ImGui::Button("Load JSON")) {
        LoadCurrentClipFromJson();
    }

    if (ImGui::Button("Play")) {
        if (hasClip_) {
            isPlaying_ = true;
            isPaused_ = false;
            ApplyCurrentClipAtCurrentTime();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Pause")) {
        if (hasClip_ && isPlaying_) {
            isPaused_ = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        StopPlayback(true);
    }

    ImGui::Checkbox("Loop", &isLoop_);
    ImGui::DragFloat("Playback Speed", &playbackSpeed_, 0.01f, 0.0f, 4.0f, "%.2f");
    ImGui::Text("Playback State: %s", !hasClip_ ? "No Clip" : (isPlaying_ ? (isPaused_ ? "Paused" : "Playing") : "Stopped"));
    ImGui::TextWrapped("Status: %s", statusMessage_.empty() ? "Ready." : statusMessage_.c_str());
    ImGui::Checkbox("Enable Gizmo", &isTranslateGizmoEnabled_);
    int gizmoOperationIndex = static_cast<int>(gizmoOperation_);
    if (ImGui::Combo("Gizmo Operation", &gizmoOperationIndex, kGizmoOperationLabels, IM_ARRAYSIZE(kGizmoOperationLabels))) {
        gizmoOperation_ = static_cast<GizmoOperation>(gizmoOperationIndex);
    }

    const bool isTranslateOperation = (gizmoOperation_ == GizmoOperation::Translate);
    if (!isTranslateOperation) {
        ImGui::BeginDisabled();
    }
    int gizmoSpaceIndex = static_cast<int>(gizmoSpace_);
    if (ImGui::Combo("Gizmo Space", &gizmoSpaceIndex, kGizmoSpaceLabels, IM_ARRAYSIZE(kGizmoSpaceLabels))) {
        gizmoSpace_ = static_cast<GizmoSpace>(gizmoSpaceIndex);
    }
    if (!isTranslateOperation) {
        ImGui::EndDisabled();
        ImGui::TextDisabled("Rotate / Scale gizmo uses Local space in Step7.");
    }
    ImGui::Separator();

    ImGui::BeginChild("SkinningHierarchy", ImVec2(240.0f, 0.0f), true);
    ImGui::TextUnformatted("Skinning Hierarchy");
    ImGui::Separator();
    for (int jointIndex = 0; jointIndex < static_cast<int>(targetSkeleton_->joints.size()); ++jointIndex) {
        if (targetSkeleton_->joints[static_cast<size_t>(jointIndex)].parentIndex < 0) {
            DrawJointNode(*targetSkeleton_, jointIndex, selectedJointIndex_);
        }
    }
    ImGui::EndChild();
    RefreshSelectionState();

    ImGui::SameLine();

    ImGui::BeginChild("SkinningInspector", ImVec2(0.0f, 0.0f), true);
    ImGui::TextUnformatted("Skinning Inspector");
    ImGui::Separator();
    if (selectedJointIndex_ >= 0 && selectedJointIndex_ < static_cast<int>(targetSkeleton_->joints.size())) {
        Joint& joint = targetSkeleton_->joints[static_cast<size_t>(selectedJointIndex_)];
        ImGui::Text("Selected Index: %d", selectedJointIndex_);
        ImGui::Text("Selected Bone Name: %s", joint.name.c_str());
        ImGui::Text("Gizmo Active: %s", isGizmoActive_ ? "Yes" : "No");
        const char* gizmoOperationLabel = kGizmoOperationLabels[static_cast<int>(gizmoOperation_)];
        ImGui::Text("Gizmo Operation: %s", gizmoOperationLabel);
        const bool isTranslateOperationInInspector = (gizmoOperation_ == GizmoOperation::Translate);
        ImGui::Text(
            "Gizmo Space: %s",
            isTranslateOperationInInspector
            ? (gizmoSpace_ == GizmoSpace::Local ? "Local" : "World")
            : "Local (Rotate / Scale fixed)");
        ImGui::Text("Parent Index: %d", joint.parentIndex);
        ImGui::Text("Children: %d", static_cast<int>(joint.children.size()));
        ImGui::Text("World Pos: %.2f, %.2f, %.2f", joint.worldTranslate.x, joint.worldTranslate.y, joint.worldTranslate.z);
        ImGui::Separator();

        bool isJointEdited = false;
        if (ImGui::DragFloat3("Local Translate", &joint.localTranslate.x, 0.05f)) {
            isJointEdited = true;
        }

        constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>;
        constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
        Vector3 localRotateDegrees = {
            joint.localRotate.x * kRadToDeg,
            joint.localRotate.y * kRadToDeg,
            joint.localRotate.z * kRadToDeg
        };
        if (ImGui::DragFloat3("Local Rotate", &localRotateDegrees.x, 1.0f)) {
            joint.localRotate = {
                localRotateDegrees.x * kDegToRad,
                localRotateDegrees.y * kDegToRad,
                localRotateDegrees.z * kDegToRad
            };
            isJointEdited = true;
        }

        if (ImGui::DragFloat3("Local Scale", &joint.localScale.x, 0.01f, 0.01f, 10.0f, "%.2f")) {
            isJointEdited = true;
        }

        if (isJointEdited) {
            UpdateSkeletonWorldTransforms(*targetSkeleton_);
        }
    } else {
        ImGui::TextDisabled("Select a bone from the hierarchy.");
    }
    ImGui::EndChild();

    ImGui::End();
    DrawTimelineWindow();
#endif
}

void SkinningEditor::DrawTimelineWindow() {
#ifdef _DEBUG
    if (!isOpen_) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(460.0f, 320.0f), ImGuiCond_Once);
    if (!ImGui::Begin("Skinning Timeline")) {
        ImGui::End();
        return;
    }

    ProcessTimelineShortcuts();
    DrawTransport();
    ImGui::Separator();
    DrawTimelineCanvas();
    ImGui::Separator();
    DrawSelectedJointKeys();
    ImGui::Separator();
    DrawSelectedKeyInspector();

    ImGui::End();
#endif
}

void SkinningEditor::DrawTransport() {
#ifdef _DEBUG
    const bool hasValidTarget = (targetSkeleton_ != nullptr);
    if (!hasValidTarget) {
        ImGui::TextDisabled("No target skeleton is assigned.");
        return;
    }

    float clipDuration = GetTimelineDuration();
    if (ImGui::SliderFloat("Current Time", &currentTime_, 0.0f, clipDuration, "%.3f")) {
        if (hasClip_) {
            ApplyCurrentClipAtCurrentTime();
        }
    }

    if (ImGui::Button("Add Key At Current Time")) {
        AddKeyAtCurrentTime();
    }
    ImGui::SameLine();
    const bool canDeleteKey = !selectedKeys_.empty() || (GetSelectedJointTrack() != nullptr && selectedKeyIndex_ >= 0);
    if (!canDeleteKey) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Delete Selected Key")) {
        DeleteSelectedKey();
    }
    if (!canDeleteKey) {
        ImGui::EndDisabled();
    }
#endif
}

void SkinningEditor::ProcessTimelineShortcuts() {
#ifdef _DEBUG
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        DeleteSelectedKey();
        return;
    }

    if (!io.KeyCtrl) {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        UndoTimelineEdit();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        RedoTimelineEdit();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_A, false)) {
        SelectAllKeys();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_C, false)) {
        CopySelectedKeys();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_V, false)) {
        PasteCopiedKeys();
        return;
    }
#endif
}

void SkinningEditor::DrawTimelineCanvas() {
#ifdef _DEBUG
    ImGui::TextUnformatted("Timeline Area");

    const ImVec2 canvasPosition = ImGui::GetCursorScreenPos();
    const float canvasWidth = (std::max)(240.0f, ImGui::GetContentRegionAvail().x);
    constexpr float kRowHeight = 24.0f;
    constexpr float kHeaderHeight = 24.0f;
    constexpr float kLabelWidth = 140.0f;
    const int rowCount = targetSkeleton_ ? static_cast<int>(targetSkeleton_->joints.size()) : 0;
    const float canvasHeight = (std::max)(120.0f, kHeaderHeight + 16.0f + kRowHeight * static_cast<float>((std::max)(rowCount, 1)));
    const ImVec2 canvasSize = { canvasWidth, canvasHeight };
    const ImVec2 canvasMax = { canvasPosition.x + canvasSize.x, canvasPosition.y + canvasSize.y };
    const float timelineDuration = GetTimelineDuration();

    ImGui::InvisibleButton("SkinningTimelineCanvas", canvasSize, ImGuiButtonFlags_MouseButtonLeft);
    const bool isHovered = ImGui::IsItemHovered();
    const bool isActive = ImGui::IsItemActive();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImGuiIO& io = ImGui::GetIO();
    const float labelMinX = canvasPosition.x + 10.0f;
    const float labelMaxX = labelMinX + kLabelWidth;
    const float timelineMinX = labelMaxX + 8.0f;
    const float timelineMaxX = canvasMax.x - 10.0f;

    drawList->AddRectFilled(canvasPosition, canvasMax, IM_COL32(22, 22, 28, 255), 4.0f);
    drawList->AddRect(canvasPosition, canvasMax, IM_COL32(80, 80, 96, 255), 4.0f, 0, 1.5f);

    const float headerBottomY = canvasPosition.y + kHeaderHeight;
    drawList->AddRectFilled(canvasPosition, { canvasMax.x, headerBottomY }, IM_COL32(34, 34, 42, 255), 4.0f, ImDrawFlags_RoundCornersTop);
    drawList->AddLine({ labelMaxX, canvasPosition.y }, { labelMaxX, canvasMax.y }, IM_COL32(90, 90, 110, 255), 1.0f);
    drawList->AddText({ labelMinX, canvasPosition.y + 6.0f }, IM_COL32(220, 220, 230, 255), "Bone");
    drawList->AddText({ timelineMinX, canvasPosition.y + 6.0f }, IM_COL32(220, 220, 230, 255), "Timeline");

    constexpr int kTickCount = 8;
    for (int tickIndex = 0; tickIndex <= kTickCount; ++tickIndex) {
        float tickT = static_cast<float>(tickIndex) / static_cast<float>(kTickCount);
        float tickTime = timelineDuration * tickT;
        float tickX = TimeToScreenX(tickTime, timelineMinX, timelineMaxX);
        drawList->AddLine({ tickX, headerBottomY }, { tickX, canvasMax.y - 8.0f }, IM_COL32(65, 65, 80, 120), 1.0f);
        drawList->AddLine({ tickX, headerBottomY - 4.0f }, { tickX, headerBottomY + 4.0f }, IM_COL32(120, 120, 130, 255), 1.0f);

        char timeLabel[32] = {};
        sprintf_s(timeLabel, "%.2f", tickTime);
        drawList->AddText({ tickX - 12.0f, canvasPosition.y + 6.0f }, IM_COL32(170, 170, 180, 255), timeLabel);
    }

    const float currentTimeX = TimeToScreenX(currentTime_, timelineMinX, timelineMaxX);
    drawList->AddLine(
        { currentTimeX, canvasPosition.y + 4.0f },
        { currentTimeX, canvasMax.y - 4.0f },
        IM_COL32(255, 210, 80, 255),
        2.0f);

    struct TimelineKeyVisual {
        int jointIndex = -1;
        int trackIndex = -1;
        int keyIndex = -1;
        float keyTime = 0.0f;
        ImVec2 center{};
        float radius = 0.0f;
    };

    int clickedJointIndex = -1;
    int clickedTrackIndex = -1;
    int clickedKeyIndex = -1;
    float clickedKeyTime = 0.0f;
    bool clickedSelectedKey = false;
    std::vector<TimelineKeyVisual> keyVisuals;
    for (int jointIndex = 0; targetSkeleton_ && jointIndex < static_cast<int>(targetSkeleton_->joints.size()); ++jointIndex) {
        const Joint& joint = targetSkeleton_->joints[static_cast<size_t>(jointIndex)];
        const float rowMinY = headerBottomY + 8.0f + kRowHeight * static_cast<float>(jointIndex);
        const float rowMaxY = rowMinY + kRowHeight;
        const float rowCenterY = rowMinY + kRowHeight * 0.5f;
        const bool isSelectedRow = (jointIndex == selectedJointIndex_);

        drawList->AddRectFilled(
            { canvasPosition.x + 1.0f, rowMinY },
            { canvasMax.x - 1.0f, rowMaxY },
            isSelectedRow ? IM_COL32(52, 52, 64, 255) : ((jointIndex % 2 == 0) ? IM_COL32(28, 28, 34, 255) : IM_COL32(24, 24, 30, 255)));
        drawList->AddLine({ timelineMinX, rowCenterY }, { timelineMaxX, rowCenterY }, IM_COL32(70, 70, 82, 100), 1.0f);
        drawList->AddText({ labelMinX + 4.0f, rowMinY + 4.0f }, isSelectedRow ? IM_COL32(255, 220, 120, 255) : IM_COL32(200, 200, 210, 255), joint.name.c_str());

        int trackIndex = hasClip_ ? FindTrackIndexByJointName(currentClip_, joint.name) : -1;
        if (trackIndex >= 0 && trackIndex < static_cast<int>(currentClip_.tracks.size())) {
            const JointTrack& track = currentClip_.tracks[static_cast<size_t>(trackIndex)];
            for (int keyIndex = 0; keyIndex < static_cast<int>(track.keys.size()); ++keyIndex) {
                const Keyframe& key = track.keys[static_cast<size_t>(keyIndex)];
                const float keyX = TimeToScreenX(key.time, timelineMinX, timelineMaxX);
                const float keyY = rowCenterY;
                const bool isSelectedKey = IsKeySelected(track.jointName, key.time);
                const float radius = isSelectedKey ? 7.0f : 5.0f;
                const ImU32 fillColor = isSelectedKey ? IM_COL32(255, 180, 60, 255) : IM_COL32(120, 210, 255, 255);
                drawList->AddCircleFilled({ keyX, keyY }, radius, fillColor);
                drawList->AddCircle({ keyX, keyY }, radius + 1.0f, IM_COL32(20, 20, 24, 255), 0, 1.5f);
                keyVisuals.push_back({ jointIndex, trackIndex, keyIndex, key.time, { keyX, keyY }, radius });

                if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    const ImVec2 mousePos = io.MousePos;
                    const float dx = mousePos.x - keyX;
                    const float dy = mousePos.y - keyY;
                    if ((dx * dx + dy * dy) <= ((radius + 4.0f) * (radius + 4.0f))) {
                        clickedJointIndex = jointIndex;
                        clickedTrackIndex = trackIndex;
                        clickedKeyIndex = keyIndex;
                        clickedKeyTime = key.time;
                        clickedSelectedKey = isSelectedKey;
                    }
                }
            }
        }

        if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            const ImVec2 mousePos = io.MousePos;
            const bool isInsideRow =
                mousePos.y >= rowMinY && mousePos.y <= rowMaxY &&
                mousePos.x >= canvasPosition.x && mousePos.x <= canvasMax.x;
            if (isInsideRow && clickedKeyIndex < 0) {
                clickedJointIndex = jointIndex;
            }
        }
    }

    const bool isCtrlDown = io.KeyCtrl;

    if (clickedKeyIndex >= 0 && clickedTrackIndex >= 0 && clickedTrackIndex < static_cast<int>(currentClip_.tracks.size())) {
        const std::string& jointName = currentClip_.tracks[static_cast<size_t>(clickedTrackIndex)].jointName;
        if (isCtrlDown) {
            ToggleKeySelection(jointName, clickedKeyTime);
        } else {
            SetSingleKeySelection(jointName, clickedKeyTime);
        }
        selectedJointIndex_ = clickedJointIndex;
        RefreshSelectionState();
        currentTime_ = clickedKeyTime;
        ApplyCurrentClipAtCurrentTime();
        if (!isCtrlDown) {
            isDraggingSelectedKey_ = true;
            draggedSelectionAnchorTime_ = clickedKeyTime;
            draggedSelectionPreviousTime_ = clickedKeyTime;
            isSelectionDragHistoryCaptured_ = false;
        }
    } else if (clickedJointIndex >= 0) {
        selectedJointIndex_ = clickedJointIndex;
        if (!isCtrlDown) {
            ClearKeySelection();
        }
        RefreshSelectionState();
    }

    const bool isTimelineInteraction = isHovered && io.MousePos.x >= timelineMinX;
    if (!isDraggingSelectedKey_ && !isBoxSelecting_ && clickedKeyIndex < 0 && isTimelineInteraction && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        isBoxSelecting_ = true;
        boxSelectionStartX_ = io.MousePos.x;
        boxSelectionStartY_ = io.MousePos.y;
        boxSelectionCurrentX_ = io.MousePos.x;
        boxSelectionCurrentY_ = io.MousePos.y;
    }

    if (isDraggingSelectedKey_ && !io.MouseDown[ImGuiMouseButton_Left]) {
        isDraggingSelectedKey_ = false;
        isSelectionDragHistoryCaptured_ = false;
    }

    if (isDraggingSelectedKey_) {
        float currentDragTime = ScreenXToTime(io.MousePos.x, timelineMinX, timelineMaxX);
        float deltaTime = currentDragTime - draggedSelectionPreviousTime_;
        MoveSelectedKeysByDelta(deltaTime);
        draggedSelectionPreviousTime_ = currentTime_;
    } else if (isBoxSelecting_) {
        boxSelectionCurrentX_ = io.MousePos.x;
        boxSelectionCurrentY_ = io.MousePos.y;

        const float minX = (std::min)(boxSelectionStartX_, boxSelectionCurrentX_);
        const float maxX = (std::max)(boxSelectionStartX_, boxSelectionCurrentX_);
        const float minY = (std::min)(boxSelectionStartY_, boxSelectionCurrentY_);
        const float maxY = (std::max)(boxSelectionStartY_, boxSelectionCurrentY_);
        drawList->AddRectFilled({ minX, minY }, { maxX, maxY }, IM_COL32(100, 160, 255, 40));
        drawList->AddRect({ minX, minY }, { maxX, maxY }, IM_COL32(120, 200, 255, 200), 0.0f, 0, 1.5f);

        if (!io.MouseDown[ImGuiMouseButton_Left]) {
            const bool isBoxSelection = (std::fabs(boxSelectionCurrentX_ - boxSelectionStartX_) > 4.0f) || (std::fabs(boxSelectionCurrentY_ - boxSelectionStartY_) > 4.0f);
            if (isBoxSelection) {
                if (!isCtrlDown) {
                    ClearKeySelection();
                }

                for (const TimelineKeyVisual& keyVisual : keyVisuals) {
                    if (keyVisual.center.x >= minX && keyVisual.center.x <= maxX &&
                        keyVisual.center.y >= minY && keyVisual.center.y <= maxY) {
                        const std::string& jointName = currentClip_.tracks[static_cast<size_t>(keyVisual.trackIndex)].jointName;
                        if (!IsKeySelected(jointName, keyVisual.keyTime)) {
                            selectedKeys_.push_back({ jointName, keyVisual.keyTime });
                        }
                    }
                }
                SyncPrimarySelectionFromSelectedKeys();
                RefreshSelectionState();
            } else {
                currentTime_ = ScreenXToTime(io.MousePos.x, timelineMinX, timelineMaxX);
                if (hasClip_) {
                    ApplyCurrentClipAtCurrentTime();
                }
            }
            isBoxSelecting_ = false;
        }
    } else if (
        ((isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) || (isActive && ImGui::IsMouseDown(ImGuiMouseButton_Left))) &&
        io.MousePos.x >= timelineMinX && clickedKeyIndex < 0) {
        currentTime_ = ScreenXToTime(io.MousePos.x, timelineMinX, timelineMaxX);
        if (hasClip_) {
            ApplyCurrentClipAtCurrentTime();
        }
    }

    if (!targetSkeleton_ || targetSkeleton_->joints.empty()) {
        drawList->AddText(
            { canvasPosition.x + 12.0f, canvasPosition.y + canvasHeight * 0.5f - 8.0f },
            IM_COL32(150, 150, 160, 255),
            "No skeleton target.");
    }
#endif
}

void SkinningEditor::DrawSelectedJointKeys() {
#ifdef _DEBUG
    ImGui::TextUnformatted("Selected Joint Keys");
    if (!targetSkeleton_ || selectedJointIndex_ < 0 || selectedJointIndex_ >= static_cast<int>(targetSkeleton_->joints.size())) {
        ImGui::TextDisabled("Select a bone from Skinning Hierarchy.");
        return;
    }

    const Joint& joint = targetSkeleton_->joints[static_cast<size_t>(selectedJointIndex_)];
    ImGui::Text("Joint: %s", joint.name.c_str());

    JointTrack* track = GetSelectedJointTrack();
    if (!track || track->keys.empty()) {
        ImGui::TextDisabled("No keys on this joint.");
        return;
    }

    ImGui::BeginChild("SelectedJointKeyList", ImVec2(0.0f, 92.0f), true);
    for (int keyIndex = 0; keyIndex < static_cast<int>(track->keys.size()); ++keyIndex) {
        const Keyframe& key = track->keys[static_cast<size_t>(keyIndex)];
        char label[96] = {};
        sprintf_s(label, "Key %d  |  t = %.3f", keyIndex, key.time);
        if (ImGui::Selectable(label, IsKeySelected(joint.name, key.time))) {
            SetSingleKeySelection(joint.name, key.time);
            currentTime_ = key.time;
            ApplyCurrentClipAtCurrentTime();
        }
    }
    ImGui::EndChild();
#endif
}

void SkinningEditor::DrawSelectedKeyInspector() {
#ifdef _DEBUG
    ImGui::TextUnformatted("Selected Key Inspector");
    JointTrack* track = GetSelectedJointTrack();
    if (!track || selectedKeyIndex_ < 0 || selectedKeyIndex_ >= static_cast<int>(track->keys.size())) {
        ImGui::TextDisabled("Select a key from Selected Joint Keys.");
        return;
    }

    Keyframe& key = track->keys[static_cast<size_t>(selectedKeyIndex_)];
    float keyTime = key.time;
    const bool keyTimeChanged = ImGui::DragFloat("Selected Key Time", &keyTime, 0.01f, 0.0f, 600.0f, "%.3f");
    const bool keyTimeActivated = ImGui::IsItemActivated();
    if (keyTimeActivated) {
        PushUndoSnapshot();
    }
    if (keyTimeChanged) {
        MoveSelectedKeyTime((std::max)(0.0f, keyTime));
    }

    ImGui::Text("Translate: %.2f, %.2f, %.2f", key.translate.x, key.translate.y, key.translate.z);
    ImGui::Text("Rotate: %.2f, %.2f, %.2f", key.rotate.x, key.rotate.y, key.rotate.z);
    ImGui::Text("Scale: %.2f, %.2f, %.2f", key.scale.x, key.scale.y, key.scale.z);
#endif
}

void SkinningEditor::PushUndoSnapshot() {
    TimelineSnapshot snapshot{};
    snapshot.clip = currentClip_;
    snapshot.hasClip = hasClip_;
    snapshot.currentTime = currentTime_;
    snapshot.selectedJointIndex = selectedJointIndex_;
    snapshot.selectedKeys = selectedKeys_;
    undoStack_.push_back(std::move(snapshot));
    if (undoStack_.size() > kMaxUndoHistory) {
        undoStack_.erase(undoStack_.begin());
    }
    redoStack_.clear();
}

void SkinningEditor::RestoreTimelineSnapshot(const TimelineSnapshot& snapshot) {
    currentClip_ = snapshot.clip;
    hasClip_ = snapshot.hasClip;
    currentTime_ = snapshot.currentTime;
    selectedJointIndex_ = snapshot.selectedJointIndex;
    selectedTrackIndex_ = -1;
    selectedKeyIndex_ = -1;
    selectedKeys_ = snapshot.selectedKeys;
    isPlaying_ = false;
    isPaused_ = false;
    isDraggingSelectedKey_ = false;
    isSelectionDragHistoryCaptured_ = false;
    isBoxSelecting_ = false;

    SetBufferText(clipNameBuffer_, currentClip_.name.empty() ? "NewClip" : currentClip_.name);
    if (targetSkeleton_ && !targetSkeleton_->joints.empty()) {
        selectedJointIndex_ = std::clamp(selectedJointIndex_, 0, static_cast<int>(targetSkeleton_->joints.size()) - 1);
    } else {
        selectedJointIndex_ = -1;
    }

    RefreshSelectionState();
    if (hasClip_ && targetSkeleton_) {
        currentTime_ = std::clamp(currentTime_, 0.0f, GetTimelineDuration());
        ApplyCurrentClipAtCurrentTime();
    }
}

void SkinningEditor::UndoTimelineEdit() {
    if (undoStack_.empty()) {
        statusMessage_ = "Undo skipped: no history.";
        return;
    }

    TimelineSnapshot currentSnapshot{};
    currentSnapshot.clip = currentClip_;
    currentSnapshot.hasClip = hasClip_;
    currentSnapshot.currentTime = currentTime_;
    currentSnapshot.selectedJointIndex = selectedJointIndex_;
    currentSnapshot.selectedKeys = selectedKeys_;
    redoStack_.push_back(std::move(currentSnapshot));
    if (redoStack_.size() > kMaxUndoHistory) {
        redoStack_.erase(redoStack_.begin());
    }

    TimelineSnapshot snapshot = undoStack_.back();
    undoStack_.pop_back();
    RestoreTimelineSnapshot(snapshot);
    statusMessage_ = "Undo timeline edit.";
}

void SkinningEditor::RedoTimelineEdit() {
    if (redoStack_.empty()) {
        statusMessage_ = "Redo skipped: no history.";
        return;
    }

    TimelineSnapshot currentSnapshot{};
    currentSnapshot.clip = currentClip_;
    currentSnapshot.hasClip = hasClip_;
    currentSnapshot.currentTime = currentTime_;
    currentSnapshot.selectedJointIndex = selectedJointIndex_;
    currentSnapshot.selectedKeys = selectedKeys_;
    undoStack_.push_back(std::move(currentSnapshot));
    if (undoStack_.size() > kMaxUndoHistory) {
        undoStack_.erase(undoStack_.begin());
    }

    TimelineSnapshot snapshot = redoStack_.back();
    redoStack_.pop_back();
    RestoreTimelineSnapshot(snapshot);
    statusMessage_ = "Redo timeline edit.";
}

void SkinningEditor::AddKeyAtCurrentTime() {
    if (!targetSkeleton_ || selectedJointIndex_ < 0 || selectedJointIndex_ >= static_cast<int>(targetSkeleton_->joints.size())) {
        statusMessage_ = "Add key failed: no selected joint.";
        return;
    }

    PushUndoSnapshot();

    SyncClipNameFromBuffer();
    if (!hasClip_) {
        currentClip_.name = clipNameBuffer_.data();
        if (currentClip_.name.empty()) {
            currentClip_.name = "NewClip";
            SetBufferText(clipNameBuffer_, currentClip_.name);
        }
        currentClip_.duration = (std::max)(1.0f, currentTime_);
        currentClip_.tracks.clear();
        hasClip_ = true;
    }

    currentClip_.duration = (std::max)(currentClip_.duration, (std::max)(currentTime_, 0.0001f));

    const Joint& joint = targetSkeleton_->joints[static_cast<size_t>(selectedJointIndex_)];
    JointTrack* track = FindJointTrack(currentClip_, joint.name);
    if (!track) {
        currentClip_.tracks.push_back({ joint.name, {} });
        track = &currentClip_.tracks.back();
    }

    Keyframe key{};
    key.time = currentTime_;
    key.translate = joint.localTranslate;
    key.rotate = joint.localRotate;
    key.scale = joint.localScale;

    int existingKeyIndex = FindKeyIndexByTime(*track, key.time, kTimeEpsilon_);
    if (existingKeyIndex >= 0) {
        track->keys[static_cast<size_t>(existingKeyIndex)] = key;
    } else {
        track->keys.push_back(key);
        SortJointTrackKeys(*track);
    }

    SetSingleKeySelection(joint.name, key.time);
    statusMessage_ = "Added key to joint: " + joint.name;
}

void SkinningEditor::SelectAllKeys() {
    if (!targetSkeleton_ || !hasClip_ || currentClip_.tracks.empty()) {
        statusMessage_ = "Select all skipped: no keys in current clip.";
        return;
    }

    SelectedKeyHandle previousPrimary{};
    bool hasPreviousPrimary = false;
    if (!selectedKeys_.empty()) {
        previousPrimary = selectedKeys_.front();
        hasPreviousPrimary = true;
    } else {
        JointTrack* selectedTrack = GetSelectedJointTrack();
        if (selectedTrack && selectedKeyIndex_ >= 0 && selectedKeyIndex_ < static_cast<int>(selectedTrack->keys.size())) {
            previousPrimary = { selectedTrack->jointName, selectedTrack->keys[static_cast<size_t>(selectedKeyIndex_)].time };
            hasPreviousPrimary = true;
        }
    }

    selectedKeys_.clear();
    for (const Joint& joint : targetSkeleton_->joints) {
        const JointTrack* track = FindJointTrack(currentClip_, joint.name);
        if (!track) {
            continue;
        }
        for (const Keyframe& key : track->keys) {
            selectedKeys_.push_back({ track->jointName, key.time });
        }
    }

    if (hasPreviousPrimary) {
        for (size_t selectionIndex = 0; selectionIndex < selectedKeys_.size(); ++selectionIndex) {
            const SelectedKeyHandle& handle = selectedKeys_[selectionIndex];
            if (handle.jointName == previousPrimary.jointName &&
                std::fabs(handle.time - previousPrimary.time) <= kTimeEpsilon_) {
                std::swap(selectedKeys_.front(), selectedKeys_[selectionIndex]);
                break;
            }
        }
    }

    SyncPrimarySelectionFromSelectedKeys();
    RefreshSelectionState();
    statusMessage_ = "Selected all keys in timeline.";
}

void SkinningEditor::CopySelectedKeys() {
    if (selectedKeys_.empty()) {
        JointTrack* track = GetSelectedJointTrack();
        if (track && selectedKeyIndex_ >= 0 && selectedKeyIndex_ < static_cast<int>(track->keys.size())) {
            SetSingleKeySelection(track->jointName, track->keys[static_cast<size_t>(selectedKeyIndex_)].time);
        }
    }

    if (selectedKeys_.empty()) {
        statusMessage_ = "Copy failed: no selected key.";
        return;
    }

    float baseTime = selectedKeys_.front().time;
    for (const SelectedKeyHandle& handle : selectedKeys_) {
        baseTime = (std::min)(baseTime, handle.time);
    }

    copiedKeys_.clear();
    copiedKeys_.reserve(selectedKeys_.size());
    for (const SelectedKeyHandle& handle : selectedKeys_) {
        int trackIndex = FindTrackIndexByJointName(currentClip_, handle.jointName);
        if (trackIndex < 0 || trackIndex >= static_cast<int>(currentClip_.tracks.size())) {
            continue;
        }

        const JointTrack& track = currentClip_.tracks[static_cast<size_t>(trackIndex)];
        int keyIndex = FindKeyIndexByTime(track, handle.time, kTimeEpsilon_);
        if (keyIndex < 0 || keyIndex >= static_cast<int>(track.keys.size())) {
            continue;
        }

        CopiedKeyData copiedKey{};
        copiedKey.jointName = handle.jointName;
        copiedKey.key = track.keys[static_cast<size_t>(keyIndex)];
        copiedKey.relativeTime = copiedKey.key.time - baseTime;
        copiedKeys_.push_back(copiedKey);
    }

    std::sort(copiedKeys_.begin(), copiedKeys_.end(), [](const CopiedKeyData& lhs, const CopiedKeyData& rhs) {
        if (std::fabs(lhs.relativeTime - rhs.relativeTime) > 0.0001f) {
            return lhs.relativeTime < rhs.relativeTime;
        }
        return lhs.jointName < rhs.jointName;
        });

    statusMessage_ = "Copied " + std::to_string(copiedKeys_.size()) + " key(s).";
}

void SkinningEditor::PasteCopiedKeys() {
    if (!targetSkeleton_) {
        statusMessage_ = "Paste failed: target skeleton is not assigned.";
        return;
    }
    if (copiedKeys_.empty()) {
        statusMessage_ = "Paste failed: clipboard is empty.";
        return;
    }

    SyncClipNameFromBuffer();
    if (!hasClip_) {
        currentClip_.name = clipNameBuffer_.data();
        if (currentClip_.name.empty()) {
            currentClip_.name = "NewClip";
            SetBufferText(clipNameBuffer_, currentClip_.name);
        }
        currentClip_.duration = (std::max)(1.0f, currentTime_);
        currentClip_.tracks.clear();
        hasClip_ = true;
    }

    PushUndoSnapshot();

    std::vector<SelectedKeyHandle> pastedSelection;
    pastedSelection.reserve(copiedKeys_.size());
    float maxPastedTime = currentClip_.duration;

    for (const CopiedKeyData& copiedKey : copiedKeys_) {
        if (FindJointIndexByName(targetSkeleton_, copiedKey.jointName) < 0) {
            continue;
        }

        JointTrack* track = FindJointTrack(currentClip_, copiedKey.jointName);
        if (!track) {
            currentClip_.tracks.push_back({ copiedKey.jointName, {} });
            track = &currentClip_.tracks.back();
        }

        Keyframe pastedKey = copiedKey.key;
        pastedKey.time = (std::max)(0.0f, currentTime_ + copiedKey.relativeTime);
        maxPastedTime = (std::max)(maxPastedTime, pastedKey.time);

        int duplicateIndex = FindKeyIndexByTime(*track, pastedKey.time, kTimeEpsilon_);
        if (duplicateIndex >= 0) {
            track->keys[static_cast<size_t>(duplicateIndex)] = pastedKey;
        } else {
            track->keys.push_back(pastedKey);
        }
        SortJointTrackKeys(*track);
        pastedSelection.push_back({ copiedKey.jointName, pastedKey.time });
    }

    if (pastedSelection.empty()) {
        statusMessage_ = "Paste skipped: no compatible joints for copied keys.";
        return;
    }

    currentClip_.duration = (std::max)(1.0f, maxPastedTime);
    selectedKeys_ = std::move(pastedSelection);
    SyncPrimarySelectionFromSelectedKeys();
    RefreshSelectionState();
    ApplyCurrentClipAtCurrentTime();
    statusMessage_ = "Pasted " + std::to_string(selectedKeys_.size()) + " key(s).";
}

void SkinningEditor::DeleteSelectedKey() {
    if (selectedKeys_.empty()) {
        JointTrack* track = GetSelectedJointTrack();
        if (track && selectedKeyIndex_ >= 0 && selectedKeyIndex_ < static_cast<int>(track->keys.size())) {
            SetSingleKeySelection(track->jointName, track->keys[static_cast<size_t>(selectedKeyIndex_)].time);
        }
    }

    if (selectedKeys_.empty()) {
        statusMessage_ = "Delete key failed: no selected key.";
        return;
    }

    PushUndoSnapshot();

    for (const SelectedKeyHandle& handle : selectedKeys_) {
        int trackIndex = FindTrackIndexByJointName(currentClip_, handle.jointName);
        if (trackIndex < 0 || trackIndex >= static_cast<int>(currentClip_.tracks.size())) {
            continue;
        }

        JointTrack& track = currentClip_.tracks[static_cast<size_t>(trackIndex)];
        int keyIndex = FindKeyIndexByTime(track, handle.time, kTimeEpsilon_);
        if (keyIndex >= 0 && keyIndex < static_cast<int>(track.keys.size())) {
            track.keys.erase(track.keys.begin() + keyIndex);
        }
    }

    for (int trackIndex = static_cast<int>(currentClip_.tracks.size()) - 1; trackIndex >= 0; --trackIndex) {
        if (currentClip_.tracks[static_cast<size_t>(trackIndex)].keys.empty()) {
            currentClip_.tracks.erase(currentClip_.tracks.begin() + trackIndex);
        }
    }

    ClearKeySelection();

    float maxKeyTime = 0.0f;
    for (const JointTrack& clipTrack : currentClip_.tracks) {
        for (const Keyframe& key : clipTrack.keys) {
            maxKeyTime = (std::max)(maxKeyTime, key.time);
        }
    }
    currentClip_.duration = (std::max)(1.0f, maxKeyTime);
    currentTime_ = std::clamp(currentTime_, 0.0f, currentClip_.duration);
    RefreshSelectionState();
    ApplyCurrentClipAtCurrentTime();
    statusMessage_ = "Deleted selected key(s).";
}

void SkinningEditor::MoveSelectedKeyTime(float newTime) {
    JointTrack* track = GetSelectedJointTrack();
    if (!track || selectedKeyIndex_ < 0 || selectedKeyIndex_ >= static_cast<int>(track->keys.size())) {
        return;
    }

    Keyframe movedKey = track->keys[static_cast<size_t>(selectedKeyIndex_)];
    movedKey.time = std::clamp(newTime, 0.0f, GetTimelineDuration());
    track->keys.erase(track->keys.begin() + selectedKeyIndex_);

    int duplicateIndex = FindKeyIndexByTime(*track, movedKey.time, kTimeEpsilon_);
    if (duplicateIndex >= 0) {
        track->keys[static_cast<size_t>(duplicateIndex)] = movedKey;
    } else {
        track->keys.push_back(movedKey);
    }

    SortJointTrackKeys(*track);
    selectedKeyIndex_ = FindKeyIndexByTime(*track, movedKey.time, kTimeEpsilon_);
    SetSingleKeySelection(track->jointName, movedKey.time);
    currentClip_.duration = (std::max)(currentClip_.duration, movedKey.time);
    currentTime_ = movedKey.time;
    RefreshSelectionState();
    ApplyCurrentClipAtCurrentTime();
}

void SkinningEditor::RefreshSelectionState() {
    if (!hasClip_ || !targetSkeleton_ || selectedJointIndex_ < 0 || selectedJointIndex_ >= static_cast<int>(targetSkeleton_->joints.size())) {
        selectedKeys_.clear();
        selectedTrackIndex_ = -1;
        selectedKeyIndex_ = -1;
        return;
    }

    const std::string& selectedJointName = targetSkeleton_->joints[static_cast<size_t>(selectedJointIndex_)].name;
    std::vector<SelectedKeyHandle> validSelection;
    validSelection.reserve(selectedKeys_.size());
    for (const SelectedKeyHandle& handle : selectedKeys_) {
        int trackIndex = FindTrackIndexByJointName(currentClip_, handle.jointName);
        if (trackIndex < 0 || trackIndex >= static_cast<int>(currentClip_.tracks.size())) {
            continue;
        }
        JointTrack& track = currentClip_.tracks[static_cast<size_t>(trackIndex)];
        SortJointTrackKeys(track);
        int keyIndex = FindKeyIndexByTime(track, handle.time, kTimeEpsilon_);
        if (keyIndex >= 0) {
            validSelection.push_back(handle);
        }
    }
    selectedKeys_ = std::move(validSelection);

    if (!selectedKeys_.empty()) {
        SyncPrimarySelectionFromSelectedKeys();
        return;
    }

    const int previousTrackIndex = selectedTrackIndex_;
    selectedTrackIndex_ = FindTrackIndexByJointName(currentClip_, selectedJointName);
    if (selectedTrackIndex_ != previousTrackIndex) {
        selectedKeyIndex_ = -1;
    }

    if (selectedTrackIndex_ < 0 || selectedTrackIndex_ >= static_cast<int>(currentClip_.tracks.size())) {
        selectedTrackIndex_ = -1;
        selectedKeyIndex_ = -1;
        return;
    }

    JointTrack& track = currentClip_.tracks[static_cast<size_t>(selectedTrackIndex_)];
    SortJointTrackKeys(track);
    if (track.keys.empty()) {
        selectedKeyIndex_ = -1;
        return;
    }

    if (selectedKeyIndex_ >= static_cast<int>(track.keys.size())) {
        selectedKeyIndex_ = static_cast<int>(track.keys.size()) - 1;
    }
}

JointTrack* SkinningEditor::GetSelectedJointTrack() {
    if (selectedTrackIndex_ < 0 || selectedTrackIndex_ >= static_cast<int>(currentClip_.tracks.size())) {
        return nullptr;
    }
    return &currentClip_.tracks[static_cast<size_t>(selectedTrackIndex_)];
}

const JointTrack* SkinningEditor::GetSelectedJointTrack() const {
    if (selectedTrackIndex_ < 0 || selectedTrackIndex_ >= static_cast<int>(currentClip_.tracks.size())) {
        return nullptr;
    }
    return &currentClip_.tracks[static_cast<size_t>(selectedTrackIndex_)];
}

float SkinningEditor::GetTimelineDuration() const {
    return hasClip_ ? (std::max)(currentClip_.duration, 1.0f) : 1.0f;
}

float SkinningEditor::TimeToScreenX(float time, float minX, float maxX) const {
    float duration = GetTimelineDuration();
    float normalizedTime = (duration > kTimeEpsilon_) ? std::clamp(time / duration, 0.0f, 1.0f) : 0.0f;
    return minX + (maxX - minX) * normalizedTime;
}

float SkinningEditor::ScreenXToTime(float screenX, float minX, float maxX) const {
    float width = (std::max)(maxX - minX, 1.0f);
    float normalizedTime = std::clamp((screenX - minX) / width, 0.0f, 1.0f);
    return GetTimelineDuration() * normalizedTime;
}

bool SkinningEditor::IsKeySelected(const std::string& jointName, float time) const {
    for (const SelectedKeyHandle& handle : selectedKeys_) {
        if (handle.jointName == jointName && std::fabs(handle.time - time) <= kTimeEpsilon_) {
            return true;
        }
    }
    return false;
}

void SkinningEditor::ClearKeySelection() {
    selectedKeys_.clear();
    selectedTrackIndex_ = -1;
    selectedKeyIndex_ = -1;
    isDraggingSelectedKey_ = false;
}

void SkinningEditor::SetSingleKeySelection(const std::string& jointName, float time) {
    selectedKeys_.clear();
    selectedKeys_.push_back({ jointName, time });
    SyncPrimarySelectionFromSelectedKeys();
}

void SkinningEditor::ToggleKeySelection(const std::string& jointName, float time) {
    for (size_t selectionIndex = 0; selectionIndex < selectedKeys_.size(); ++selectionIndex) {
        const SelectedKeyHandle& handle = selectedKeys_[selectionIndex];
        if (handle.jointName == jointName && std::fabs(handle.time - time) <= kTimeEpsilon_) {
            selectedKeys_.erase(selectedKeys_.begin() + static_cast<long long>(selectionIndex));
            SyncPrimarySelectionFromSelectedKeys();
            return;
        }
    }

    selectedKeys_.insert(selectedKeys_.begin(), { jointName, time });
    SyncPrimarySelectionFromSelectedKeys();
}

void SkinningEditor::SyncPrimarySelectionFromSelectedKeys() {
    if (selectedKeys_.empty()) {
        selectedTrackIndex_ = -1;
        selectedKeyIndex_ = -1;
        return;
    }

    const SelectedKeyHandle& primary = selectedKeys_.front();
    selectedTrackIndex_ = FindTrackIndexByJointName(currentClip_, primary.jointName);
    selectedJointIndex_ = FindJointIndexByName(targetSkeleton_, primary.jointName);
    if (selectedTrackIndex_ < 0 || selectedTrackIndex_ >= static_cast<int>(currentClip_.tracks.size())) {
        selectedTrackIndex_ = -1;
        selectedKeyIndex_ = -1;
        return;
    }

    const JointTrack& track = currentClip_.tracks[static_cast<size_t>(selectedTrackIndex_)];
    selectedKeyIndex_ = FindKeyIndexByTime(track, primary.time, kTimeEpsilon_);
}

void SkinningEditor::MoveSelectedKeysByDelta(float deltaTime) {
    if (selectedKeys_.empty()) {
        return;
    }

    const float duration = GetTimelineDuration();
    float minSelectedTime = duration;
    float maxSelectedTime = 0.0f;
    for (const SelectedKeyHandle& handle : selectedKeys_) {
        minSelectedTime = (std::min)(minSelectedTime, handle.time);
        maxSelectedTime = (std::max)(maxSelectedTime, handle.time);
    }

    deltaTime = std::clamp(deltaTime, -minSelectedTime, duration - maxSelectedTime);
    if (std::fabs(deltaTime) <= kTimeEpsilon_) {
        return;
    }

    if (!isSelectionDragHistoryCaptured_) {
        PushUndoSnapshot();
        isSelectionDragHistoryCaptured_ = true;
    }

    std::vector<SelectedKeyHandle> movedSelection;
    movedSelection.reserve(selectedKeys_.size());
    std::vector<std::pair<std::string, Keyframe>> movedKeys;
    movedKeys.reserve(selectedKeys_.size());

    for (const SelectedKeyHandle& handle : selectedKeys_) {
        int trackIndex = FindTrackIndexByJointName(currentClip_, handle.jointName);
        if (trackIndex < 0 || trackIndex >= static_cast<int>(currentClip_.tracks.size())) {
            continue;
        }

        JointTrack& track = currentClip_.tracks[static_cast<size_t>(trackIndex)];
        int keyIndex = FindKeyIndexByTime(track, handle.time, kTimeEpsilon_);
        if (keyIndex < 0 || keyIndex >= static_cast<int>(track.keys.size())) {
            continue;
        }

        Keyframe movedKey = track.keys[static_cast<size_t>(keyIndex)];
        movedKey.time = std::clamp(movedKey.time + deltaTime, 0.0f, duration);
        movedKeys.push_back({ handle.jointName, movedKey });
    }

    for (const SelectedKeyHandle& handle : selectedKeys_) {
        int trackIndex = FindTrackIndexByJointName(currentClip_, handle.jointName);
        if (trackIndex < 0 || trackIndex >= static_cast<int>(currentClip_.tracks.size())) {
            continue;
        }

        JointTrack& track = currentClip_.tracks[static_cast<size_t>(trackIndex)];
        int keyIndex = FindKeyIndexByTime(track, handle.time, kTimeEpsilon_);
        if (keyIndex >= 0 && keyIndex < static_cast<int>(track.keys.size())) {
            track.keys.erase(track.keys.begin() + keyIndex);
        }
    }

    for (const auto& moved : movedKeys) {
        int trackIndex = FindTrackIndexByJointName(currentClip_, moved.first);
        if (trackIndex < 0 || trackIndex >= static_cast<int>(currentClip_.tracks.size())) {
            continue;
        }

        JointTrack& track = currentClip_.tracks[static_cast<size_t>(trackIndex)];
        int duplicateIndex = FindKeyIndexByTime(track, moved.second.time, kTimeEpsilon_);
        if (duplicateIndex >= 0) {
            track.keys[static_cast<size_t>(duplicateIndex)] = moved.second;
        } else {
            track.keys.push_back(moved.second);
        }
        SortJointTrackKeys(track);
        movedSelection.push_back({ moved.first, moved.second.time });
    }

    selectedKeys_ = std::move(movedSelection);
    SyncPrimarySelectionFromSelectedKeys();
    if (!selectedKeys_.empty()) {
        currentTime_ = selectedKeys_.front().time;
    }
    ApplyCurrentClipAtCurrentTime();
}

void SkinningEditor::DrawGizmo(const Camera* camera) {
#ifdef _DEBUG
    if (!isOpen_ || !isTranslateGizmoEnabled_ || !targetSkeleton_ || !camera) {
        isGizmoActive_ = false;
        gizmoActiveJointIndex_ = -1;
        return;
    }
    if (selectedJointIndex_ < 0 || selectedJointIndex_ >= static_cast<int>(targetSkeleton_->joints.size())) {
        isGizmoActive_ = false;
        gizmoActiveJointIndex_ = -1;
        return;
    }

    Joint& joint = targetSkeleton_->joints[static_cast<size_t>(selectedJointIndex_)];
    Matrix4x4 editedWorldMatrix = joint.worldMatrix;

    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
    ImGuizmo::BeginFrame();
    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGuizmo::SetRect(viewport->Pos.x, viewport->Pos.y, viewport->Size.x, viewport->Size.y);
    ImGuizmo::SetOrthographic(false);

    const bool isTranslateOperation = (gizmoOperation_ == GizmoOperation::Translate);
    const bool isRotateOperation = (gizmoOperation_ == GizmoOperation::Rotate);
    const ImGuizmo::OPERATION gizmoOperation = isTranslateOperation
        ? ImGuizmo::TRANSLATE
        : (isRotateOperation ? ImGuizmo::ROTATE : ImGuizmo::SCALE);
    const ImGuizmo::MODE gizmoMode = isTranslateOperation
        ? ((gizmoSpace_ == GizmoSpace::Local) ? ImGuizmo::LOCAL : ImGuizmo::WORLD)
        : ImGuizmo::LOCAL;

    ImGuizmo::Manipulate(
        &camera->GetViewMatrix().m[0][0],
        &camera->GetProjectionMatrix().m[0][0],
        gizmoOperation,
        gizmoMode,
        &editedWorldMatrix.m[0][0]);

    isGizmoActive_ = ImGuizmo::IsUsing();
    gizmoActiveJointIndex_ = isGizmoActive_ ? selectedJointIndex_ : -1;

    if (isGizmoActive_) {
        Matrix4x4 localMatrix = editedWorldMatrix;
        if (joint.parentIndex >= 0 && joint.parentIndex < static_cast<int>(targetSkeleton_->joints.size())) {
            const Matrix4x4& parentWorldMatrix = targetSkeleton_->joints[static_cast<size_t>(joint.parentIndex)].worldMatrix;
            localMatrix = MatrixMath::Multipty(
                editedWorldMatrix,
                MatrixMath::Inverse(parentWorldMatrix));
        }

        if (isTranslateOperation) {
            joint.localTranslate = {
                localMatrix.m[3][0],
                localMatrix.m[3][1],
                localMatrix.m[3][2]
            };
        } else {
            float matrixTranslation[3] = {};
            float matrixRotation[3] = {};
            float matrixScale[3] = {};
            ImGuizmo::DecomposeMatrixToComponents(
                &localMatrix.m[0][0],
                matrixTranslation,
                matrixRotation,
                matrixScale);

            constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
            joint.localTranslate = { matrixTranslation[0], matrixTranslation[1], matrixTranslation[2] };
            joint.localRotate = {
                matrixRotation[0] * kDegToRad,
                matrixRotation[1] * kDegToRad,
                matrixRotation[2] * kDegToRad
            };
            joint.localScale = { matrixScale[0], matrixScale[1], matrixScale[2] };
        }
        UpdateSkeletonWorldTransforms(*targetSkeleton_);
    }
#endif
}

void SkinningEditor::DrawDebugOverlay(const Camera* camera) const {
#ifdef _DEBUG
    if (!isOpen_ || !targetSkeleton_ || !camera) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImU32 jointColor = IM_COL32(90, 200, 255, 120);
    const ImU32 selectedJointColor = IM_COL32(255, 180, 60, 255);
    const ImU32 activeJointColor = IM_COL32(120, 255, 120, 255);
    const ImU32 lineColor = IM_COL32(150, 220, 255, 80);
    const ImU32 selectedLineColor = IM_COL32(255, 200, 80, 220);
    const ImU32 activeLineColor = IM_COL32(120, 255, 120, 240);
    ImVec2 selectedJointScreen{};
    bool hasSelectedJointScreen = false;

    for (int jointIndex = 0; jointIndex < static_cast<int>(targetSkeleton_->joints.size()); ++jointIndex) {
        const Joint& joint = targetSkeleton_->joints[static_cast<size_t>(jointIndex)];
        ImVec2 jointScreen{};
        if (!ProjectToScreen(joint.worldTranslate, camera, jointScreen)) {
            continue;
        }

        const bool isSelected = (jointIndex == selectedJointIndex_);
        const bool isGizmoActiveJoint = isGizmoActive_ && (jointIndex == gizmoActiveJointIndex_);
        if (isSelected) {
            selectedJointScreen = jointScreen;
            hasSelectedJointScreen = true;
        }

        if (joint.parentIndex >= 0 && joint.parentIndex < static_cast<int>(targetSkeleton_->joints.size())) {
            const Joint& parentJoint = targetSkeleton_->joints[static_cast<size_t>(joint.parentIndex)];
            ImVec2 parentScreen{};
            if (ProjectToScreen(parentJoint.worldTranslate, camera, parentScreen)) {
                const bool isSelectedLink = isSelected || (joint.parentIndex == selectedJointIndex_);
                const bool isActiveLink = isGizmoActiveJoint || (isGizmoActive_ && joint.parentIndex == gizmoActiveJointIndex_);
                ImU32 drawColor = lineColor;
                float thickness = 1.5f;
                if (isSelectedLink) {
                    drawColor = selectedLineColor;
                    thickness = 3.0f;
                }
                if (isActiveLink) {
                    drawColor = activeLineColor;
                    thickness = 4.0f;
                }
                drawList->AddLine(parentScreen, jointScreen, drawColor, thickness);
            }
        }

        ImU32 drawJointColor = jointColor;
        float radius = 3.5f;
        if (isSelected) {
            drawJointColor = selectedJointColor;
            radius = 7.0f;
        }
        if (isGizmoActiveJoint) {
            drawJointColor = activeJointColor;
            radius = 9.0f;
        }
        drawList->AddCircleFilled(jointScreen, radius, drawJointColor);
        if (isSelected) {
            const char* activeOperationLabel = gizmoOperation_ == GizmoOperation::Translate
                ? "[Translate] Selected Bone"
                : (gizmoOperation_ == GizmoOperation::Rotate ? "[Rotate] Selected Bone" : "[Scale] Selected Bone");
            const char* selectedLabel = isGizmoActiveJoint ? activeOperationLabel : "Selected Bone";
            drawList->AddText(ImVec2(jointScreen.x + 12.0f, jointScreen.y - 26.0f), IM_COL32(20, 20, 20, 220), selectedLabel);
            drawList->AddText(
                ImVec2(jointScreen.x + 11.0f, jointScreen.y - 27.0f),
                isGizmoActiveJoint ? activeJointColor : selectedJointColor,
                joint.name.c_str());
        }
    }

    if (hasSelectedJointScreen && selectedJointIndex_ >= 0 && selectedJointIndex_ < static_cast<int>(targetSkeleton_->joints.size())) {
        const Joint& selectedJoint = targetSkeleton_->joints[static_cast<size_t>(selectedJointIndex_)];
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 panelMin = { viewport->Pos.x + 16.0f, viewport->Pos.y + 16.0f };
        ImVec2 panelMax = { panelMin.x + 250.0f, panelMin.y + 52.0f };
        drawList->AddRectFilled(panelMin, panelMax, IM_COL32(0, 0, 0, 140), 6.0f);
        drawList->AddRect(panelMin, panelMax, isGizmoActive_ ? activeLineColor : selectedLineColor, 6.0f, 0, 2.0f);

        std::string title = "Selected: " + selectedJoint.name;
        const bool isTranslateOperation = (gizmoOperation_ == GizmoOperation::Translate);
        const char* stateLabel = gizmoOperation_ == GizmoOperation::Translate
            ? (isGizmoActive_ ? "Translate Gizmo Active" : "Translate Gizmo Idle")
            : (gizmoOperation_ == GizmoOperation::Rotate
                ? (isGizmoActive_ ? "Rotate Gizmo Active" : "Rotate Gizmo Idle")
                : (isGizmoActive_ ? "Scale Gizmo Active" : "Scale Gizmo Idle"));
        const char* spaceLabel = isTranslateOperation
            ? (gizmoSpace_ == GizmoSpace::Local ? "Space: Local" : "Space: World")
            : "Space: Local";
        drawList->AddText({ panelMin.x + 10.0f, panelMin.y + 8.0f }, IM_COL32(255, 255, 255, 255), title.c_str());
        drawList->AddText({ panelMin.x + 10.0f, panelMin.y + 28.0f }, isGizmoActive_ ? activeJointColor : selectedJointColor, stateLabel);
        drawList->AddText({ panelMin.x + 140.0f, panelMin.y + 28.0f }, IM_COL32(220, 220, 220, 255), spaceLabel);

        drawList->AddLine(
            { panelMin.x + 250.0f, panelMin.y + 26.0f },
            { selectedJointScreen.x - 10.0f, selectedJointScreen.y },
            isGizmoActive_ ? activeLineColor : selectedLineColor,
            2.0f);
    }
#endif
}

void SkinningEditor::SetTarget(const std::string& label, Skeleton* skeleton) {
    StoreCurrentClipToCurrentTarget();
    targetLabel_ = label;
    targetSkeleton_ = skeleton;
    syncedTargetIndex_ = -1;
    selectedJointIndex_ = (targetSkeleton_ && !targetSkeleton_->joints.empty()) ? 0 : -1;
    selectedTrackIndex_ = -1;
    selectedKeyIndex_ = -1;
    RefreshSelectionState();
    if (hasClip_ && targetSkeleton_) {
        ApplyCurrentClipAtCurrentTime();
    }
}

void SkinningEditor::RegisterTarget(const std::string& label, Skeleton* skeleton, const AnimationClip* clip) {
    TargetEntry entry{};
    entry.label = label;
    entry.skeleton = skeleton;
    if (clip) {
        entry.clip = *clip;
        entry.clip.duration = (std::max)(entry.clip.duration, 0.0001f);
        entry.hasClip = true;
    }
    targets_.push_back(entry);
    if (currentTargetIndex_ < 0 && !targets_.empty()) {
        currentTargetIndex_ = 0;
        SyncTargetFromIndex();
    }
}

void SkinningEditor::ClearTarget() {
    targetLabel_ = "None";
    targetSkeleton_ = nullptr;
    selectedJointIndex_ = -1;
    selectedTrackIndex_ = -1;
    selectedKeyIndex_ = -1;
    selectedKeys_.clear();
    isGizmoActive_ = false;
    gizmoActiveJointIndex_ = -1;
    syncedTargetIndex_ = -1;
    isDraggingSelectedKey_ = false;
    isSelectionDragHistoryCaptured_ = false;
    isBoxSelecting_ = false;
}

void SkinningEditor::ClearTargets() {
    targets_.clear();
    currentTargetIndex_ = -1;
    syncedTargetIndex_ = -1;
    ClearTarget();
}

void SkinningEditor::SetClip(const AnimationClip& clip, const std::string& statusMessage) {
    currentClip_ = clip;
    currentClip_.duration = (std::max)(currentClip_.duration, 0.0001f);
    hasClip_ = true;
    isPlaying_ = false;
    isPaused_ = false;
    currentTime_ = 0.0f;
    selectedTrackIndex_ = -1;
    selectedKeyIndex_ = -1;
    selectedKeys_.clear();
    undoStack_.clear();
    redoStack_.clear();
    SetBufferText(clipNameBuffer_, currentClip_.name);
    StoreCurrentClipToCurrentTarget();
    RefreshSelectionState();
    ApplyCurrentClipAtCurrentTime();

    if (!statusMessage.empty()) {
        statusMessage_ = statusMessage;
    }
}

void SkinningEditor::DeleteTargetAt(int targetIndex) {
    if (targetIndex < 0 || targetIndex >= static_cast<int>(targets_.size())) {
        return;
    }

    if (targetIndex != currentTargetIndex_) {
        StoreCurrentClipToCurrentTarget();
    }
    targets_.erase(targets_.begin() + targetIndex);
    syncedTargetIndex_ = -1;
    if (targets_.empty()) {
        currentTargetIndex_ = -1;
        ClearTarget();
        return;
    }

    if (currentTargetIndex_ == targetIndex) {
        currentTargetIndex_ = (targetIndex < static_cast<int>(targets_.size()))
            ? targetIndex
            : (static_cast<int>(targets_.size()) - 1);
        selectedJointIndex_ = -1;
        selectedTrackIndex_ = -1;
        selectedKeyIndex_ = -1;
        isGizmoActive_ = false;
        gizmoActiveJointIndex_ = -1;
    } else if (currentTargetIndex_ > targetIndex) {
        currentTargetIndex_--;
    }

    SyncTargetFromIndex();
}

void SkinningEditor::SyncTargetFromIndex() {
    if (currentTargetIndex_ < 0 || currentTargetIndex_ >= static_cast<int>(targets_.size())) {
        ClearTarget();
        return;
    }

    const bool targetChanged = (syncedTargetIndex_ != currentTargetIndex_);
    if (targetChanged &&
        hasClip_ &&
        syncedTargetIndex_ >= 0 &&
        syncedTargetIndex_ < static_cast<int>(targets_.size())) {
        TargetEntry& previousTarget = targets_[static_cast<size_t>(syncedTargetIndex_)];
        previousTarget.clip = currentClip_;
        previousTarget.clip.duration = (std::max)(previousTarget.clip.duration, 0.0001f);
        previousTarget.hasClip = true;
    }

    const TargetEntry& target = targets_[static_cast<size_t>(currentTargetIndex_)];
    targetLabel_ = target.label;
    targetSkeleton_ = target.skeleton;
    if (targetChanged) {
        selectedJointIndex_ = (targetSkeleton_ && !targetSkeleton_->joints.empty()) ? 0 : -1;
        selectedTrackIndex_ = -1;
        selectedKeyIndex_ = -1;
        selectedKeys_.clear();
        copiedKeys_.clear();
        undoStack_.clear();
        redoStack_.clear();
        isPlaying_ = false;
        isPaused_ = false;
        isGizmoActive_ = false;
        gizmoActiveJointIndex_ = -1;
        isDraggingSelectedKey_ = false;
        isSelectionDragHistoryCaptured_ = false;
        isBoxSelecting_ = false;
        currentTime_ = 0.0f;

        if (target.hasClip) {
            currentClip_ = target.clip;
            currentClip_.duration = (std::max)(currentClip_.duration, 0.0001f);
            hasClip_ = true;
            SetBufferText(clipNameBuffer_, currentClip_.name);
            statusMessage_ = "Loaded target clip: " + currentClip_.name;
        } else {
            currentClip_ = AnimationClip{};
            hasClip_ = false;
            SetBufferText(clipNameBuffer_, currentClip_.name);
        }
        syncedTargetIndex_ = currentTargetIndex_;
    } else {
        selectedJointIndex_ = (targetSkeleton_ && !targetSkeleton_->joints.empty())
            ? std::clamp(selectedJointIndex_, 0, static_cast<int>(targetSkeleton_->joints.size()) - 1)
            : -1;
    }
    RefreshSelectionState();
    if (targetChanged && hasClip_ && targetSkeleton_) {
        ApplyCurrentClipAtCurrentTime();
    }
}

void SkinningEditor::StoreCurrentClipToCurrentTarget() {
    if (!hasClip_ ||
        currentTargetIndex_ < 0 ||
        currentTargetIndex_ >= static_cast<int>(targets_.size())) {
        return;
    }

    TargetEntry& target = targets_[static_cast<size_t>(currentTargetIndex_)];
    target.clip = currentClip_;
    target.clip.duration = (std::max)(target.clip.duration, 0.0001f);
    target.hasClip = true;
}

void SkinningEditor::CaptureCurrentPoseAsClip() {
    if (!targetSkeleton_) {
        return;
    }

    SyncClipNameFromBuffer();
    float duration = hasClip_ ? (std::max)(currentClip_.duration, 1.0f) : 1.0f;
    currentClip_ = CaptureSkeletonPoseAsClip(*targetSkeleton_, currentClip_.name, duration);
    hasClip_ = true;
    selectedTrackIndex_ = -1;
    selectedKeyIndex_ = -1;
    RefreshSelectionState();
}

void SkinningEditor::ApplyCurrentClipAtCurrentTime() {
    if (!hasClip_ || !targetSkeleton_) {
        return;
    }

    ApplyAnimationClipAtTime(currentClip_, *targetSkeleton_, currentTime_);
}

void SkinningEditor::StopPlayback(bool resetTime) {
    isPlaying_ = false;
    isPaused_ = false;
    if (resetTime) {
        currentTime_ = 0.0f;
        ApplyCurrentClipAtCurrentTime();
    }
}

void SkinningEditor::SyncClipNameFromBuffer() {
    currentClip_.name = clipNameBuffer_.data();
    if (currentClip_.name.empty()) {
        currentClip_.name = "NewClip";
        SetBufferText(clipNameBuffer_, currentClip_.name);
    }
}

std::string SkinningEditor::GetJsonPathOrDefault() const {
    std::string path = jsonPathBuffer_.data();
    if (!path.empty()) {
        return path;
    }

    std::string clipName = clipNameBuffer_.data();
    if (clipName.empty()) {
        clipName = "NewClip";
    }
    for (char& ch : clipName) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            ch = '_';
        }
    }
    return "resources/animation/" + clipName + ".json";
}

bool SkinningEditor::SaveCurrentClipToJson(bool treatAsNewFile) {
    if (!targetSkeleton_) {
        statusMessage_ = "Save failed: target skeleton is not assigned.";
        return false;
    }

    SyncClipNameFromBuffer();
    if (!hasClip_) {
        CaptureCurrentPoseAsClip();
    } else {
        currentClip_.name = clipNameBuffer_.data();
        if (currentClip_.name.empty()) {
            currentClip_.name = "NewClip";
            SetBufferText(clipNameBuffer_, currentClip_.name);
        }
        currentClip_.duration = (std::max)(currentClip_.duration, 0.0001f);
    }
    std::string path = GetJsonPathOrDefault();
    if (treatAsNewFile && std::string(jsonPathBuffer_.data()).empty()) {
        SetPathBufferText(path);
    }
    if (!SaveAnimationClipToJson(currentClip_, path)) {
        statusMessage_ = "Save failed: could not write JSON.";
        return false;
    }

    SetPathBufferText(path);
    StoreCurrentClipToCurrentTarget();
    statusMessage_ = "Saved clip JSON: " + path;
    return true;
}

bool SkinningEditor::LoadCurrentClipFromJson() {
    std::string path = jsonPathBuffer_.data();
    if (path.empty()) {
        statusMessage_ = "Load failed: JSON path is empty.";
        return false;
    }

    AnimationClip loadedClip{};
    if (!LoadAnimationClipFromJson(path, loadedClip)) {
        statusMessage_ = "Load failed: could not parse JSON.";
        return false;
    }

    currentClip_ = std::move(loadedClip);
    hasClip_ = true;
    isPlaying_ = false;
    isPaused_ = false;
    currentTime_ = 0.0f;
    SetBufferText(clipNameBuffer_, currentClip_.name);
    selectedTrackIndex_ = -1;
    selectedKeyIndex_ = -1;
    RefreshSelectionState();
    StoreCurrentClipToCurrentTarget();
    ApplyCurrentClipAtCurrentTime();
    statusMessage_ = "Loaded clip JSON: " + path;
    return true;
}

void SkinningEditor::SetBufferText(std::array<char, 128>& buffer, const std::string& text) {
    buffer.fill('\0');
    strncpy_s(buffer.data(), buffer.size(), text.c_str(), _TRUNCATE);
}

void SkinningEditor::SetPathBufferText(const std::string& text) {
    jsonPathBuffer_.fill('\0');
    strncpy_s(jsonPathBuffer_.data(), jsonPathBuffer_.size(), text.c_str(), _TRUNCATE);
}
