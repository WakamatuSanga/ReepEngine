#pragma once
#include "AnimationClip.h"
#include <array>
#include <string>
#include <vector>

class Camera;
struct Skeleton;

class SkinningEditor {
public:
    SkinningEditor();
    void Update();
    void DrawImGui();
    void DrawGizmo(const Camera* camera);
    void DrawDebugOverlay(const Camera* camera) const;

    void SetTarget(const std::string& label, Skeleton* skeleton);
    void RegisterTarget(const std::string& label, Skeleton* skeleton, const AnimationClip* clip = nullptr);
    void ClearTarget();
    void ClearTargets();
    void SetClip(const AnimationClip& clip, const std::string& statusMessage = {});

    void SetOpen(bool isOpen) { isOpen_ = isOpen; }
    bool IsOpen() const { return isOpen_; }
    int GetSelectedJointIndex() const { return selectedJointIndex_; }
    const Skeleton* GetTargetSkeleton() const { return targetSkeleton_; }

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

    bool isOpen_ = true;
    bool isTranslateGizmoEnabled_ = true;
    bool isGizmoActive_ = false;
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
