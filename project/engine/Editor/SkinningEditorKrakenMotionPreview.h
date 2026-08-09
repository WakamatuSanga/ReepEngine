#pragma once

#include "Matrix4x4.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class GltfSkinnedModel;
class SkinningEditorKrakenAttackMotion;
struct KrakenTentacleAttackPoseResult;
struct Skeleton;

class SkinningEditorKrakenMotionPreview {
public:
    enum class Mode {
        Manual,
        IdleSway,
        AttackSlamPreview,
    };

    struct Chain {
        std::vector<int> joints;
    };

    struct BoundsSnapshot {
        bool isValid = false;
        Vector3 min{};
        Vector3 max{};
        Vector3 size{};
    };

    struct Diagnostics {
        uint32_t paletteMatrixCount = 0;
        uint32_t nonFinitePaletteMatrixCount = 0;
        uint32_t identityPaletteMatrixCount = 0;
        uint32_t changedPaletteMatrixCount = 0;
        uint32_t weightReferencedJointCount = 0;
        uint32_t skinnedVertexCount = 0;
        uint32_t nonFiniteSkinnedVertexCount = 0;
        uint32_t verticesWithoutWeights = 0;
        uint32_t invalidJointInfluenceCount = 0;
        uint32_t nonFiniteWeightCount = 0;
        uint32_t invalidWeightSumVertexCount = 0;
        uint32_t maxPositiveInfluences = 0;
        BoundsSnapshot sourceBounds{};
        BoundsSnapshot skinnedBounds{};
        bool skeletonEnabled = false;
        bool paletteUpdateSucceeded = false;
        bool skinningUpdateSucceeded = false;
        bool boneOverlaySynchronized = false;
        bool abnormalBoundsDetected = false;
        bool safetyRecoveryOccurred = false;
        uint32_t safetyRecoveryCount = 0;
    };

    ~SkinningEditorKrakenMotionPreview();

    void SetTarget(Skeleton* skeleton, GltfSkinnedModel* model);
    void ClearTarget();
    void Update(
        float unscaledDeltaTime,
        int selectedJointIndex,
        const Matrix4x4& previewWorldMatrix);
    void DrawImGui(int selectedJointIndex);
    void RefreshDiagnosticsAndRecover();
    void ReturnToBindPoseFromEditor();

    bool IsTarget(const Skeleton* skeleton) const;
    bool IsProceduralActive() const;

private:
    struct BindLocalPose {
        Vector3 translate{};
        Vector3 rotate{};
        Vector3 scale{ 1.0f, 1.0f, 1.0f };
    };

    struct AttackTipDiagnostics {
        bool valid = false;
        int jointIndex = -1;
        Vector3 skeletonPosition{};
        Vector3 worldPosition{};
        Vector3 bindSkeletonPosition{};
        Vector3 bindWorldPosition{};
        float distanceFromBind = 0.0f;
    };

    static constexpr const char* kExpectedRootName = "Kraken_Tentacle_Rig_Root";
    static constexpr std::size_t kExpectedJointCount = 41;

    void CaptureBindPose();
    bool DetectChains();
    bool ValidateBindPose() const;
    bool ValidateCurrentPose() const;
    void RestoreBindLocals();
    void ApplyCurrentPose();
    void ApplyManualPose();
    void ApplyIdleSwayPose();
    void ApplyAttackPose();
    void UpdateAttackMotion(float unscaledDeltaTime);
    void DrawAttackMotionImGui();
    void CaptureBindTipPositions();
    void RefreshAttackTipDiagnostics();
    void ReturnToBindPose(bool clearError);
    void SwitchToManual();
    void StartIdleSway();
    void UpdateSelectedChainFromJoint(int selectedJointIndex);
    void ResetIdleSettings();
    void CaptureBindPalette();
    void RefreshDiagnostics();
    uint32_t CountChangedPaletteMatrices(const std::vector<Matrix4x4>& palette) const;
    bool IsBoundsAbnormal(const BoundsSnapshot& source, const BoundsSnapshot& skinned) const;
    void SetHierarchyError(const std::string& message);

    Skeleton* skeleton_ = nullptr;
    GltfSkinnedModel* model_ = nullptr;
    std::vector<BindLocalPose> bindPose_;
    std::vector<Vector3> bindLocalEulerRadians_;
    std::vector<Vector3> manualRotationDegrees_;
    std::vector<Matrix4x4> bindPalette_;
    std::vector<Vector3> bindChainTipSkeletonPositions_;
    std::vector<Chain> chains_;
    std::unique_ptr<SkinningEditorKrakenAttackMotion> attackMotion_;
    std::unique_ptr<KrakenTentacleAttackPoseResult> attackPoseResult_;
    Diagnostics diagnostics_{};
    AttackTipDiagnostics attackTipDiagnostics_{};
    std::string hierarchyError_;
    std::string runtimeError_;
    Mode mode_ = Mode::Manual;
    bool isPaused_ = true;
    bool rootRotationAllowed_ = false;
    bool applyAllChains_ = true;
    bool hierarchyValid_ = false;
    bool targetCompatible_ = false;
    bool recovering_ = false;
    int selectedChainIndex_ = 0;
    float motionTime_ = 0.0f;
    float frequencyHz_ = 0.35f;
    float rootAmplitudeDegrees_ = 3.0f;
    float tipAmplitudeDegrees_ = 15.0f;
    float secondaryAmplitudeDegrees_ = 6.0f;
    float chainPhaseRadians_ = 0.75f;
    float phaseAlongChainRadians_ = 0.55f;
    Matrix4x4 previewWorldMatrix_ = MatrixMath::MakeIdentity4x4();
};
