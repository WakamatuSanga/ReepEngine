#pragma once

#include "Matrix4x4.h"
#include "SkinningEditorKrakenBoneColliderPhaseControl.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct Skeleton;

enum class KrakenColliderPreviewRole : std::uint8_t {
    Attack,
    Damage,
    WeakPoint,
};

struct KrakenBoneColliderPreviewSettings {
    bool showPreview = true;
    bool showCapsules = true;
    bool showTipSphere = true;
    bool showOnlySelected = false;
    bool depthTest = false;
    bool showAllChains = false;
    bool showAttackColliders = true;
    bool showDamageColliders = true;
    bool showWeakPointColliders = true;
    float globalRadiusScale = 1.0f;
};

struct KrakenBoneColliderPreview {
    std::uint32_t colliderIndex = 0;
    std::uint32_t chainIndex = 0;
    KrakenColliderPreviewRole role = KrakenColliderPreviewRole::Damage;
    int startJointIndex = -1;
    int endJointIndex = -1;
    float normalizedStart = 0.0f;
    float normalizedEnd = 0.0f;
    float recommendedLocalRadius = 0.25f;
    float localRadius = 0.25f;
    float radiusScale = 1.0f;
    float worldRadius = 0.0f;
    Vector3 worldStart{};
    Vector3 worldEnd{};
    Vector3 worldCenter{};
    Vector3 worldDirection{ 0.0f, 1.0f, 0.0f };
    float worldLength = 0.0f;
    bool enabled = true;
    bool valid = false;
    bool previewVisible = true;
    bool phaseActive = false;
    bool gameplayRegistered = false;
    KrakenColliderPhaseReason phaseReason =
        KrakenColliderPhaseReason::MotionStateInvalid;
    std::uint64_t lastPhaseTransitionFrame = ~std::uint64_t{ 0 };
};

struct KrakenTipSphereColliderPreview {
    std::uint32_t chainIndex = 0;
    KrakenColliderPreviewRole role = KrakenColliderPreviewRole::WeakPoint;
    int tipJointIndex = -1;
    float localRadius = 0.30f;
    float radiusScale = 1.0f;
    float worldRadius = 0.0f;
    Vector3 worldPosition{};
    Vector3 bindWorldPosition{};
    float distanceFromBind = 0.0f;
    bool enabled = true;
    bool valid = false;
    bool previewVisible = true;
    bool phaseActive = false;
    bool gameplayRegistered = false;
    KrakenColliderPhaseReason phaseReason =
        KrakenColliderPhaseReason::MotionStateInvalid;
    std::uint64_t lastPhaseTransitionFrame = ~std::uint64_t{ 0 };
};

struct KrakenBoneColliderPreviewDiagnostics {
    std::uint32_t chainCount = 0;
    std::uint32_t selectedChainIndex = 0;
    std::uint32_t chainBoneCount = 0;
    std::uint32_t capsuleCount = 0;
    std::uint32_t tipSphereCount = 0;
    std::uint32_t enabledColliderCount = 0;
    std::uint32_t disabledColliderCount = 0;
    std::uint32_t invalidJointIndexCount = 0;
    std::uint32_t zeroLengthCapsuleCount = 0;
    std::uint32_t nonFinitePositionCount = 0;
    std::uint32_t nonFiniteRadiusCount = 0;
    std::uint32_t nonPositiveRadiusCount = 0;
    std::uint32_t lastDebugDrawShapeCount = 0;
    std::uint64_t currentPoseUpdateCount = 0;
    std::uint64_t debugDrawCount = 0;
    std::uint64_t tipSphereUpdateCount = 0;
    bool currentPoseFollowing = false;
    std::string lastError;
    std::string lastWarning;
};

class SkinningEditorKrakenBoneColliderPreview {
public:
    void Reset();
    void Clear();
    bool Rebuild(
        std::uint32_t detectedChainCount,
        std::uint32_t chainIndex,
        const std::vector<int>& chainJoints,
        int skeletonRootJointIndex,
        std::size_t skeletonJointCount,
        const Vector3& bindTipSkeletonPosition);
    void Update(
        const Skeleton& skeleton,
        const Matrix4x4& previewWorldMatrix);

    bool MatchesChain(
        std::uint32_t chainIndex,
        const std::vector<int>& chainJoints) const;
    bool SetCapsuleJointPair(
        std::size_t colliderIndex,
        int startJointIndex,
        int endJointIndex);
    void ResetRecommendedRadii();
    void SetAllEnabled(bool enabled);
    void SelectFirst();
    void SelectNext();
    void SetSelectedColliderIndex(std::size_t colliderIndex);
    void ResetDiagnostics();
    void RunDiagnostics(const Skeleton& skeleton);
    void RecordDebugDraw(std::uint32_t shapeCount) const;

    KrakenBoneColliderPreviewSettings& GetSettings() { return settings_; }
    const KrakenBoneColliderPreviewSettings& GetSettings() const {
        return settings_;
    }
    std::vector<KrakenBoneColliderPreview>& GetCapsules() {
        return capsules_;
    }
    const std::vector<KrakenBoneColliderPreview>& GetCapsules() const {
        return capsules_;
    }
    KrakenTipSphereColliderPreview& GetTipSphere() { return tipSphere_; }
    const KrakenTipSphereColliderPreview& GetTipSphere() const {
        return tipSphere_;
    }
    const std::vector<int>& GetChainJoints() const { return chainJoints_; }
    const KrakenBoneColliderPreviewDiagnostics& GetDiagnostics() const {
        return diagnostics_;
    }
    std::size_t GetSelectedColliderIndex() const {
        return selectedColliderIndex_;
    }
    std::size_t GetSelectableColliderCount() const;
    bool IsTipSphereSelected() const;

private:
    void RefreshDiagnostics(const Skeleton& skeleton);
    void SetRebuildError(const std::string& message);

    KrakenBoneColliderPreviewSettings settings_{};
    std::vector<KrakenBoneColliderPreview> capsules_;
    KrakenTipSphereColliderPreview tipSphere_{};
    mutable KrakenBoneColliderPreviewDiagnostics diagnostics_{};
    std::vector<int> chainJoints_;
    Vector3 bindTipSkeletonPosition_{};
    int skeletonRootJointIndex_ = -1;
    std::uint32_t detectedChainCount_ = 0;
    std::uint32_t selectedChainIndex_ = 0;
    std::size_t selectedColliderIndex_ = 0;
    bool hasBindTipPosition_ = false;
};

const char* GetKrakenColliderPreviewRoleJapaneseLabel(
    KrakenColliderPreviewRole role);
