#pragma once

#include "SkinningEditorKrakenBoneColliderPreview.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

struct Skeleton;

class SkinningEditorKrakenBoneColliderPreviewCollection {
public:
    ~SkinningEditorKrakenBoneColliderPreviewCollection();

    void Reset();
    void Clear();
    bool Rebuild(
        const std::vector<std::vector<int>>& chainJoints,
        int skeletonRootJointIndex,
        std::size_t skeletonJointCount,
        const std::vector<Vector3>& bindTipSkeletonPositions);
    void Update(
        const Skeleton& skeleton,
        const Matrix4x4& previewWorldMatrix);
    bool MatchesChains(
        const std::vector<std::vector<int>>& chainJoints) const;

    void SetDisplayChainIndex(std::size_t chainIndex);
    std::size_t GetDisplayChainIndex() const {
        return displayChainIndex_;
    }
    std::size_t GetChainCount() const { return chainPreviews_.size(); }
    bool IsConnected() const { return connected_; }

    KrakenBoneColliderPreviewSettings& GetSettings() { return settings_; }
    const KrakenBoneColliderPreviewSettings& GetSettings() const {
        return settings_;
    }
    std::vector<std::unique_ptr<
        SkinningEditorKrakenBoneColliderPreview>>& GetChainPreviews() {
        return chainPreviews_;
    }
    const std::vector<std::unique_ptr<
        SkinningEditorKrakenBoneColliderPreview>>& GetChainPreviews() const {
        return chainPreviews_;
    }
    const KrakenBoneColliderPreviewDiagnostics& GetDiagnostics() const {
        return diagnostics_;
    }

    void ResetRecommendedRadii();
    void SetAllEnabled(bool enabled);
    void RunDiagnostics(const Skeleton& skeleton);
    void ResetDiagnostics();
    void RecordDebugDraw(std::uint32_t shapeCount) const;

    void SelectFirstForDisplayChain();
    void SelectNext();
    void Select(
        std::size_t chainIndex,
        std::size_t localIndex,
        bool tip);
    bool IsSelected(
        std::size_t chainIndex,
        std::size_t localIndex,
        bool tip) const;
    void ResynchronizeSelection();

    std::size_t GetSelectedShapeChainIndex() const {
        return selectedShapeChainIndex_;
    }
    std::size_t GetSelectedLocalIndex() const {
        return selectedLocalIndex_;
    }
    bool IsSelectedShapeTip() const { return selectedShapeIsTip_; }
    KrakenBoneColliderPreview* GetSelectedCapsule();
    const KrakenBoneColliderPreview* GetSelectedCapsule() const;
    KrakenTipSphereColliderPreview* GetSelectedTipSphere();
    const KrakenTipSphereColliderPreview* GetSelectedTipSphere() const;

private:
    void RefreshAggregatedDiagnostics();
    void ClampSelection();

    KrakenBoneColliderPreviewSettings settings_{};
    std::vector<std::unique_ptr<
        SkinningEditorKrakenBoneColliderPreview>> chainPreviews_;
    mutable KrakenBoneColliderPreviewDiagnostics diagnostics_{};
    std::size_t displayChainIndex_ = 0;
    std::size_t selectedShapeChainIndex_ = 0;
    std::size_t selectedLocalIndex_ = 0;
    bool selectedShapeIsTip_ = false;
    bool connected_ = false;
};
