#pragma once
#include "AnimationClip.h"
#include <array>
#include <string>
#include <vector>

class Camera;
struct Skeleton;

class SkinningEditor {
public:
    struct BoundsInfo {
        bool isValid = false;
        Vector3 min{ 0.0f, 0.0f, 0.0f };
        Vector3 max{ 0.0f, 0.0f, 0.0f };
        Vector3 size{ 0.0f, 0.0f, 0.0f };
        Vector3 center{ 0.0f, 0.0f, 0.0f };
    };

    struct TargetPreviewInfo {
        BoundsInfo sourceBounds{};
        BoundsInfo skinnedBounds{};
        Vector3 rootNodeScale{ 1.0f, 1.0f, 1.0f };
        Vector3 rootNodeTranslation{ 0.0f, 0.0f, 0.0f };
        Vector3 skeletonRootWorldScale{ 1.0f, 1.0f, 1.0f };
        Vector3 skeletonRootWorldTranslation{ 0.0f, 0.0f, 0.0f };
        float previewScale = 1.0f;
        Vector3 previewRotation{ 0.0f, 0.0f, 0.0f };
        Vector3 defaultPreviewRotation{ 0.0f, 0.0f, 0.0f };
    };

    SkinningEditor();
    void Update();
    void DrawImGui();
    void DrawGizmo(const Camera* camera);
    void DrawDebugOverlay(const Camera* camera) const;

    void SetTarget(const std::string& label, Skeleton* skeleton);
    void RegisterTarget(const std::string& label, Skeleton* skeleton, const AnimationClip* clip = nullptr);
    void RegisterTarget(
        const std::string& label,
        Skeleton* skeleton,
        const AnimationClip* clip,
        const std::string& targetType,
        const std::string& resolvedPath,
        const std::string& loadStatus,
        int boneCount,
        bool skinnedMeshLoaded,
        const TargetPreviewInfo& previewInfo = {});
    bool SelectTargetByLabel(const std::string& label);
    void ClearTarget();
    void ClearTargets();
    void SetClip(const AnimationClip& clip, const std::string& statusMessage = {});
    void SetStatusMessage(const std::string& message);
    void SetGameViewRect(float x, float y, float width, float height);
    void ClearGameViewRect();

    void SetOpen(bool isOpen) { isOpen_ = isOpen; }
    bool IsOpen() const { return isOpen_; }
    int GetSelectedJointIndex() const { return selectedJointIndex_; }
    const Skeleton* GetTargetSkeleton() const { return targetSkeleton_; }
    float GetActivePreviewScale() const;
    Vector3 GetActivePreviewRotation() const;
    Matrix4x4 GetActivePreviewWorldMatrix() const;
    bool IsGizmoInteracting() const { return isGizmoActive_ || isGizmoHovered_; }

private:
    static constexpr float kTimeEpsilon_ = 0.0001f;

    enum class GizmoOperation {
        Translate = 0,
        Rotate = 1,
        Scale = 2,
    };

    enum class GizmoSpace {
        Local = 0,
        World = 1,
    };

    struct TargetEntry {
        std::string label;
        Skeleton* skeleton = nullptr;
        AnimationClip clip{};
        bool hasClip = false;
        std::string targetType = "preview";
        std::string resolvedPath;
        std::string loadStatus;
        int boneCount = 0;
        bool skinnedMeshLoaded = false;
        TargetPreviewInfo previewInfo{};
    };

    struct SelectedKeyHandle {
        std::string jointName;
        float time = 0.0f;
    };

    struct CopiedKeyData {
        std::string jointName;
        Keyframe key{};
        float relativeTime = 0.0f;
    };

    struct TimelineSnapshot {
        AnimationClip clip{};
        bool hasClip = false;
        float currentTime = 0.0f;
        int selectedJointIndex = -1;
        std::vector<SelectedKeyHandle> selectedKeys;
    };

    void DrawTimelineWindow();
    void DrawTransport();
    void DrawTimelineCanvas();
    void DrawSelectedJointKeys();
    void DrawSelectedKeyInspector();
    void ProcessTimelineShortcuts();
    void PushUndoSnapshot();
    void RestoreTimelineSnapshot(const TimelineSnapshot& snapshot);
    void UndoTimelineEdit();
    void RedoTimelineEdit();
    void AddKeyAtCurrentTime();
    void DeleteSelectedKey();
    void MoveSelectedKeyTime(float newTime);
    void MoveSelectedKeysByDelta(float deltaTime);
    void RefreshSelectionState();
    JointTrack* GetSelectedJointTrack();
    const JointTrack* GetSelectedJointTrack() const;
    float GetTimelineDuration() const;
    float TimeToScreenX(float time, float minX, float maxX) const;
    float ScreenXToTime(float screenX, float minX, float maxX) const;
    bool IsKeySelected(const std::string& jointName, float time) const;
    void ClearKeySelection();
    void SetSingleKeySelection(const std::string& jointName, float time);
    void ToggleKeySelection(const std::string& jointName, float time);
    void SyncPrimarySelectionFromSelectedKeys();
    void SelectAllKeys();
    void CopySelectedKeys();
    void PasteCopiedKeys();

    void CaptureCurrentPoseAsClip();
    void ApplyCurrentClipAtCurrentTime();
    void StopPlayback(bool resetTime);
    void SyncClipNameFromBuffer();
    std::string GetJsonPathOrDefault() const;
    bool SaveCurrentClipToJson(bool treatAsNewFile);
    bool LoadCurrentClipFromJson();
    void SetBufferText(std::array<char, 128>& buffer, const std::string& text);
    void SetPathBufferText(const std::string& text);
    void DeleteTargetAt(int targetIndex);
    void SyncTargetFromIndex();
    void StoreCurrentClipToCurrentTarget();
    std::string BuildTargetStatusMessage(const TargetEntry& target) const;

    bool isOpen_ = true;
    bool isTranslateGizmoEnabled_ = true;
    bool isGizmoActive_ = false;
    bool isGizmoHovered_ = false;
    bool showSkeletonDebugJoints_ = false;
    bool showSkeletonDebugLines_ = false;
    bool showJointLabels_ = false;
    bool showSelectedJointPanel_ = false;
    bool hasGameViewRect_ = false;
    float gameViewX_ = 0.0f;
    float gameViewY_ = 0.0f;
    float gameViewWidth_ = 0.0f;
    float gameViewHeight_ = 0.0f;
    GizmoOperation gizmoOperation_ = GizmoOperation::Translate;
    GizmoSpace gizmoSpace_ = GizmoSpace::World;
    int gizmoActiveJointIndex_ = -1;
    std::vector<TargetEntry> targets_;
    int currentTargetIndex_ = -1;
    int syncedTargetIndex_ = -1;
    Skeleton* targetSkeleton_ = nullptr;
    std::string targetLabel_ = "None";
    int selectedJointIndex_ = -1;
    int selectedTrackIndex_ = -1;
    int selectedKeyIndex_ = -1;
    bool isDraggingSelectedKey_ = false;
    bool isBoxSelecting_ = false;
    float boxSelectionStartX_ = 0.0f;
    float boxSelectionStartY_ = 0.0f;
    float boxSelectionCurrentX_ = 0.0f;
    float boxSelectionCurrentY_ = 0.0f;
    float draggedSelectionAnchorTime_ = 0.0f;
    float draggedSelectionPreviousTime_ = 0.0f;
    bool isSelectionDragHistoryCaptured_ = false;
    std::vector<SelectedKeyHandle> selectedKeys_;
    std::vector<CopiedKeyData> copiedKeys_;
    std::vector<TimelineSnapshot> undoStack_;
    std::vector<TimelineSnapshot> redoStack_;
    AnimationClip currentClip_{};
    bool hasClip_ = false;
    bool isPlaying_ = false;
    bool isPaused_ = false;
    bool isLoop_ = true;
    float playbackSpeed_ = 1.0f;
    float currentTime_ = 0.0f;
    std::array<char, 128> clipNameBuffer_{};
    std::array<char, 260> jsonPathBuffer_{};
    std::string statusMessage_;
};
