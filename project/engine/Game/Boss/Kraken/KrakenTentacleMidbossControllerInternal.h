#pragma once

#include "Engine/Game/Boss/Kraken/KrakenTentacleColliderEvaluator.h"
#include "Engine/Game/Boss/Kraken/KrakenTentacleMidbossController.h"
#include "Engine/Game/Boss/Kraken/KrakenTentaclePoseEvaluator.h"
#include "Matrix4x4.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Camera;
class GltfSkinnedModel;
class ModelCommon;
class Object3d;
class Object3dCommon;
class Player;
class PlayerBulletManager;
struct Skeleton;

struct KrakenTentacleMidbossBindLocalPose {
    Vector3 translate{};
    Vector3 rotate{};
    Vector3 scale{ 1.0f, 1.0f, 1.0f };
};

struct KrakenTentacleMidbossBoneSnapshot {
    int jointIndex = -1;
    int parentIndex = -1;
    std::size_t chainIndex = static_cast<std::size_t>(-1);
    Vector3 worldPosition{};
    bool isRoot = false;
    bool valid = false;
};

struct KrakenTentacleMidbossCapsuleSnapshot {
    std::uint64_t colliderId = 0;
    std::uint64_t lastRegisteredFrame = 0;
    std::uint64_t registrationGeneration = 0;
    std::uint32_t chainIndex = 0;
    std::uint32_t colliderIndex = 0;
    KrakenColliderPreviewRole role = KrakenColliderPreviewRole::Damage;
    int startJointIndex = -1;
    int endJointIndex = -1;
    Vector3 worldStart{};
    Vector3 worldEnd{};
    Vector3 worldCenter{};
    float worldRadius = 0.0f;
    float worldLength = 0.0f;
    bool enabled = true;
    bool valid = false;
    bool zeroLength = false;
    bool phaseActive = false;
    bool requestedRegistration = false;
    bool gameplayRegistered = false;
    bool registrationFailed = false;
    KrakenColliderPhaseReason phaseReason =
        KrakenColliderPhaseReason::MotionStateInvalid;
};

struct KrakenTentacleMidbossTipSnapshot {
    std::uint64_t colliderId = 0;
    std::uint64_t lastRegisteredFrame = 0;
    std::uint64_t registrationGeneration = 0;
    std::uint32_t chainIndex = 0;
    KrakenColliderPreviewRole role = KrakenColliderPreviewRole::WeakPoint;
    int jointIndex = -1;
    Vector3 worldPosition{};
    Vector3 bindWorldPosition{};
    float worldRadius = 0.0f;
    float distanceFromBind = 0.0f;
    bool enabled = true;
    bool valid = false;
    bool phaseActive = false;
    bool requestedRegistration = false;
    bool gameplayRegistered = false;
    bool registrationFailed = false;
    KrakenColliderPhaseReason phaseReason =
        KrakenColliderPhaseReason::MotionStateInvalid;
};

struct KrakenTentacleMidbossBoundsSnapshot {
    bool valid = false;
    Vector3 minimum{};
    Vector3 maximum{};
    Vector3 size{};
    Vector3 center{};
};

enum class KrakenTentacleCollisionTargetKind : std::uint8_t {
    Player,
    PlayerBullet,
};

enum class KrakenTentacleCollisionTransition : std::uint8_t {
    Enter,
    Stay,
    Exit,
};

struct KrakenTentacleCollisionPairKey {
    KrakenColliderPreviewRole role = KrakenColliderPreviewRole::Damage;
    KrakenTentacleCollisionTargetKind targetKind =
        KrakenTentacleCollisionTargetKind::PlayerBullet;
    std::uint32_t chainIndex = 0;
    std::uint32_t colliderIndex = 0;
    std::uint64_t targetRuntimeId = 0;
};

struct KrakenTentacleCollisionPairSnapshot {
    KrakenTentacleCollisionPairKey key{};
    Vector3 colliderClosestPosition{};
    Vector3 targetWorldPosition{};
    float centerDistance = 0.0f;
    float radiusSum = 0.0f;
};

struct KrakenTentacleCollisionEventSnapshot {
    KrakenTentacleCollisionPairSnapshot pair{};
    KrakenTentacleCollisionTransition transition =
        KrakenTentacleCollisionTransition::Enter;
};

struct KrakenTentacleCollisionRoleDiagnostics {
    std::size_t currentIntersectionCount = 0;
    std::size_t frameIntersectionCount = 0;
    std::size_t frameEnterCount = 0;
    std::size_t frameStayCount = 0;
    std::size_t frameExitCount = 0;
    std::uint64_t totalIntersectionCount = 0;
    std::uint64_t cumulativeIntersectionFrameCount = 0;
    std::uint64_t totalEnterCount = 0;
    std::uint64_t totalStayCount = 0;
    std::uint64_t totalExitCount = 0;
    std::uint64_t lastTargetRuntimeId = 0;
    std::uint64_t lastIntersectionFrameIndex = 0;
    std::uint32_t lastColliderId = 0;
    std::uint32_t lastChainIndex = 0;
    float lastIntersectionRuntimeTime = 0.0f;
    bool hasLastIntersection = false;
};

struct KrakenTentacleMidbossDiagnostics {
    std::size_t meshCount = 0;
    std::size_t primitiveCount = 0;
    std::size_t materialCount = 0;
    std::size_t vertexCount = 0;
    std::size_t indexCount = 0;
    std::size_t triangleCount = 0;
    std::size_t jointCount = 0;
    std::size_t paletteCount = 0;
    std::size_t drawCallCount = 0;
    std::size_t materialBindingCount = 0;
    std::size_t cpuSkinningUpdateCount = 0;
    std::size_t computeDispatchCount = 0;
    std::size_t nonFinitePaletteCount = 0;
    std::size_t nonFiniteSkinnedVertexCount = 0;
    std::size_t weightlessVertexCount = 0;
    std::size_t invalidJointInfluenceCount = 0;
    std::size_t colliderCount = 0;
    std::size_t attackColliderCount = 0;
    std::size_t damageColliderCount = 0;
    std::size_t weakPointCount = 0;
    std::size_t phaseActiveCount = 0;
    std::size_t gameplayRegisteredCount = 0;
    std::size_t invalidColliderJointCount = 0;
    std::size_t zeroLengthColliderCount = 0;
    std::size_t nonFiniteColliderCount = 0;
    std::size_t playerCollisionSnapshotCount = 0;
    std::size_t playerBulletCollisionSnapshotCount = 0;
    std::size_t collisionRegistrationRequestedCount = 0;
    std::size_t collisionRegistrationFailureCount = 0;
    std::size_t registeredAttackColliderCount = 0;
    std::size_t registeredDamageColliderCount = 0;
    std::size_t registeredWeakPointCount = 0;
    std::size_t collisionQueryTestCount = 0;
    std::size_t invalidCollisionQueryCount = 0;
    std::size_t currentCollisionPairCount = 0;
    std::size_t currentAttackPlayerPairCount = 0;
    std::size_t currentDamageBulletPairCount = 0;
    std::size_t currentWeakPointBulletPairCount = 0;
    std::size_t collisionEnterCount = 0;
    std::size_t collisionStayCount = 0;
    std::size_t collisionExitCount = 0;
    std::size_t bodyAndWeakPointSameBulletCount = 0;
    std::uint64_t collisionQueryFrameCount = 0;
    std::uint64_t totalCollisionQueryTestCount = 0;
    std::uint64_t totalCollisionEnterCount = 0;
    std::uint64_t totalCollisionStayCount = 0;
    std::uint64_t totalCollisionExitCount = 0;
    std::uint64_t totalBodyAndWeakPointSameBulletCount = 0;
    std::uint64_t duplicateCollisionQueryCount = 0;
    std::uint64_t lastCollisionQueryFrameIndex = ~std::uint64_t{ 0 };
    KrakenTentacleCollisionRoleDiagnostics attackPlayerCollision{};
    KrakenTentacleCollisionRoleDiagnostics damageBulletCollision{};
    KrakenTentacleCollisionRoleDiagnostics weakPointBulletCollision{};
    std::size_t safetyRecoveryCount = 0;
    std::size_t modelLoadFailureCount = 0;
    std::size_t attackStartRejectedCount = 0;
    std::size_t outOfRangeChainCount = 0;
    KrakenColliderPhaseReason lastPhaseReason =
        KrakenColliderPhaseReason::MotionStateInvalid;
    KrakenTentacleMidbossBoundsSnapshot sourceBounds{};
    KrakenTentacleMidbossBoundsSnapshot skinnedBounds{};
    bool boundsAbnormal = false;
    bool collisionQueryContextConnected = false;
    bool playerCollisionSnapshotValid = false;
};

enum class KrakenTentacleMidbossPendingCommand : std::uint8_t {
    None,
    Show,
    Hide,
    ReturnToIdle,
    StartAttack,
    StopAttack,
    ReturnToBindPose,
    ResetState,
    ResetRuntime,
};

struct KrakenTentacleMidbossController::Impl {
    Impl();
    ~Impl();

    bool Initialize(ModelCommon* modelCommon, Object3dCommon* object3dCommon);
    bool FailInitialization(const std::string& message);
    void SetCamera(Camera* camera);
    void SetCollisionQueryContext(
        const Player* playerValue,
        const PlayerBulletManager* playerBulletManagerValue);
    void Update(float scaledDeltaTime);
    void UpdateCollisionQuery();
    void Draw();
    void DrawDebug(float viewX, float viewY, float viewWidth, float viewHeight) const;
    void DrawImGui();
    void Reset();
    void Finalize();

    bool ValidateLoadedAsset();
    bool CaptureBindPoseAndChains();
    bool RebuildColliderDefinitions();
    bool RestoreBindPose();
    bool ApplyCurrentPose();
    bool ValidateCurrentPose() const;
    bool UpdateCurrentPoseAndSkinning();
    void UpdateObjectTransform();
    void RefreshBoneSnapshots();
    void RefreshColliderSnapshots();
    void RefreshCollisionRegistrationState();
    void ResetCollisionQueryState(bool resetCumulativeDiagnostics);
    void DrawCollisionQueryImGui();
    void RefreshSkinningDiagnostics();
    void RefreshDrawDiagnostics();
    void ProcessPendingCommand();
    void AdvanceState(float deltaTime);
    void EnterState(KrakenTentacleMidbossState state);
    void EnterHidden(const std::string& errorMessage, bool safetyRecovery);
    bool Show();
    void Hide();
    bool StartAttack();
    void StopAttack();
    void ReturnToIdle();
    void ReturnToBindPose();
    void ResetStateOnly();
    bool PlaceInFrontOfCamera();

    KrakenTentacleAttackPreviewPhase GetAttackPhase() const;
    KrakenTentacleColliderAttackPhase GetColliderAttackPhase() const;
    float GetCurrentStateDuration() const;
    float GetSlamProgress() const;
    bool IsAttackState() const;
    bool IsVisible() const;

    // Ownership order is intentional. Reverse destruction releases Object,
    // then Model, then the Skeleton borrowed by the model.
    std::unique_ptr<Skeleton> skeleton;
    std::unique_ptr<GltfSkinnedModel> model;
    std::unique_ptr<Object3d> object;

    Object3dCommon* object3dCommon = nullptr;
    Camera* camera = nullptr;
    const Player* collisionPlayer = nullptr;
    const PlayerBulletManager* collisionPlayerBulletManager = nullptr;

    std::vector<KrakenTentacleMidbossBindLocalPose> bindPose;
    std::vector<Vector3> bindLocalEulerRadians;
    std::vector<KrakenTentacleChain> chains;
    std::vector<KrakenTentacleColliderDefinitionResult> colliderDefinitions;
    std::vector<KrakenTentacleMidbossBoneSnapshot> boneSnapshots;
    std::vector<KrakenTentacleMidbossCapsuleSnapshot> capsuleSnapshots;
    std::vector<KrakenTentacleMidbossTipSnapshot> tipSnapshots;
    std::vector<KrakenTentacleCollisionPairSnapshot>
        previousCollisionPairs;
    std::vector<KrakenTentacleCollisionPairSnapshot>
        currentCollisionPairs;
    std::vector<KrakenTentacleCollisionEventSnapshot>
        collisionFrameEvents;

    KrakenTentacleIdlePoseSettings idleSettings{};
    KrakenTentacleAttackSettings attackSettings{};
    KrakenTentacleColliderPhaseSettings colliderPhaseSettings{};
    KrakenTentacleMidbossDiagnostics diagnostics{};
    KrakenTentacleMidbossPendingCommand pendingCommand =
        KrakenTentacleMidbossPendingCommand::None;

    KrakenTentacleMidbossState state = KrakenTentacleMidbossState::Hidden;
    std::size_t selectedAttackChainIndex = 0;
    float stateElapsedTime = 0.0f;
    float totalActiveTime = 0.0f;
    float idleTime = 0.0f;
    float attackElapsedTime = 0.0f;
    float lastScaledDeltaTime = 0.0f;

    Vector3 worldPosition{};
    Vector3 worldRotation{};
    Vector3 worldScale{ 0.5f, 0.5f, 0.5f };
    Matrix4x4 worldMatrix{};
    float cameraForwardOffset = 35.0f;
    float cameraRightOffset = 0.0f;
    float cameraUpOffset = -2.0f;
    float colliderRadiusScale = 1.0f;
    float colliderGlobalRadiusScale = 1.0f;
    std::uint64_t collisionRegistrationGeneration = 1;

    std::string requestedAssetPath;
    std::string resolvedAssetPath;
    std::string rootName;
    std::string lastError;
    std::string lastWarning;
    std::string lastCollisionQueryWarning;

    bool initialized = false;
    bool modelLoaded = false;
    bool skeletonValid = false;
    bool safetyStopped = false;
    bool idleSwayEnabled = true;
    bool showBones = false;
    bool showJoints = false;
    bool showColliders = false;
    bool showAttackColliders = true;
    bool showDamageColliders = true;
    bool showWeakPoints = true;
    bool showSelectedChainOnly = false;
    bool collisionQueryEnabled = true;
};
