#pragma once

#include "Engine/math/Matrix4x4.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class AimCorridorTargetMarkerRenderer;
class AimCorridorVisualController;
class Camera;
class DirectXCommon;
class EnemyManager;

class AimCorridorTargetingController {
public:
    enum class AimLockState : uint8_t {
        None,
        Candidate,
        Acquiring,
        Locked,
    };

    AimCorridorTargetingController();
    ~AimCorridorTargetingController();

    bool Initialize(
        DirectXCommon* dxCommon,
        EnemyManager* enemyManager,
        Camera* camera,
        AimCorridorVisualController* visualController);
    void Finalize();
    void Reset();
    void SetGameModeActive(bool active);
    void SetPlayerAlive(bool alive);
    void Update(float scaledDeltaTime, float unscaledDeltaTime);
    void Draw();
    void DrawImGui();

    bool HasCandidate() const { return !candidateTargetId_.empty(); }
    bool HasLockedTarget() const { return !lockedTargetId_.empty() && lockState_ == AimLockState::Locked; }
    const std::string& GetCandidateTargetId() const { return candidateTargetId_; }
    const std::string& GetLockedTargetId() const { return lockedTargetId_; }
    float GetLockProgress() const { return lockProgress_; }
    AimLockState GetLockState() const { return lockState_; }
    const Vector3& GetLockedTargetWorldPosition() const { return lockedTargetWorldPosition_; }
    const Vector3& GetLockedTargetAimPosition() const { return lockedTargetAimPosition_; }

private:
    struct ScreenRect {
        Vector2 center{};
        Vector2 halfSize{};
        Vector2 minimum{};
        Vector2 maximum{};
        bool valid = false;
    };

    struct ProjectedTarget {
        std::string runtimeId;
        std::string enemyType;
        Vector3 worldPosition{};
        Vector2 screenUv{};
        Vector2 screenRadius{};
        Vector2 boundsMinimum{};
        Vector2 boundsMaximum{};
        float clipW = 0.0f;
        float cameraDepth = 0.0f;
        float score = 0.0f;
        bool overlapsVisibleRect = false;
        bool overlapsSoftRect = false;
        bool projectionValid = false;
    };

    void ClampParameters();
    void ProjectTargets();
    bool ProjectWorldToScreen(
        const Vector3& worldPosition,
        Vector2& screenUv,
        float& clipW) const;
    void UpdateSelection(float scaledDeltaTime);
    void SwitchCandidate(const ProjectedTarget& target, const char* reason);
    void ClearTarget(bool countLockBreak, const char* reason);
    void PublishVisualState(float unscaledDeltaTime);
    const ProjectedTarget* FindProjectedTarget(const std::string& runtimeId) const;
    const ProjectedTarget* FindBestCandidate() const;
    static bool RectsOverlap(
        const Vector2& lhsMinimum,
        const Vector2& lhsMaximum,
        const Vector2& rhsMinimum,
        const Vector2& rhsMaximum);

    DirectXCommon* dxCommon_ = nullptr;
    EnemyManager* enemyManager_ = nullptr;
    Camera* camera_ = nullptr;
    AimCorridorVisualController* visualController_ = nullptr;
    std::unique_ptr<AimCorridorTargetMarkerRenderer> markerRenderer_;

    std::vector<ProjectedTarget> projectedTargets_;
    ScreenRect visibleRect_{};
    ScreenRect softRect_{};
    ProjectedTarget currentTarget_{};
    bool currentTargetValid_ = false;

    std::string candidateTargetId_;
    std::string lockedTargetId_;
    std::string bestCandidateId_;
    std::string lastSwitchReason_ = "対象なし";
    Vector3 lockedTargetWorldPosition_{};
    Vector3 lockedTargetAimPosition_{};
    AimLockState lockState_ = AimLockState::None;
    float currentCandidateScore_ = 0.0f;
    float bestCandidateScore_ = 0.0f;
    float lockElapsed_ = 0.0f;
    float lockProgress_ = 0.0f;
    float targetHoldElapsed_ = 0.0f;
    float breakGraceElapsed_ = 0.0f;

    float softAssistScale_ = 1.60f;
    float fallbackWorldRadius_ = 1.50f;
    float minimumScreenRadius_ = 0.008f;
    float maximumScreenRadius_ = 0.25f;
    float minimumTargetDepth_ = 0.50f;
    float maximumTargetDepth_ = 250.0f;
    float depthScoreWeight_ = 0.12f;
    float visibleRectBonus_ = 0.15f;
    float targetHoldTime_ = 0.20f;
    float targetSwitchMargin_ = 0.15f;
    float lockAcquireTime_ = 0.85f;
    float lockBreakGraceTime_ = 0.25f;
    int maximumCandidateCount_ = 32;
    int candidateCount_ = 0;
    uint32_t lockCompletedCount_ = 0;
    uint32_t lockBreakCount_ = 0;

    int debugForcedState_ = -1;
    bool showCandidateBounds_ = false;
    bool showVisibleRect_ = false;
    bool showSoftRect_ = false;
    bool showTargetCenter_ = false;
    bool showTargetRadius_ = false;
    bool showDecisionReason_ = true;
    bool considerEnemyBounds_ = true;
    bool enabled_ = true;
    bool gameModeActive_ = false;
    bool playerAlive_ = true;
    bool initialized_ = false;
};
