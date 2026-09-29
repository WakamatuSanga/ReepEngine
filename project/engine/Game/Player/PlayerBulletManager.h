#pragma once
#include "Engine/math/Matrix4x4.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class AimCorridorTargetingController;
class AimCorridorVisualController;
class Camera;
class EnemyBullet;
class GameViewport;
class LockedWingMissileExhaustController;
class Object3dCommon;
class Player;
class PlayerFanChargeAttack;
struct PlayerFanChargeTrajectory;
class PlayerLockOnTargetProvider;
class ProjectileRailMotionAdapter;
enum class PlayerLockOnTargetKind : uint8_t;

class PlayerBulletManager {
public:
    enum class AimMode {
        CameraForward,
        MouseRay,
        MouseAimPlane,
        AimCorridor,
    };

    enum class VisualDirectionSource {
        AimDirection,
        FinalVelocity,
    };

    enum class PlayerProjectileType : uint8_t {
        NormalShot,
        LockedWingShot,
        FanChargeShot,
    };

    enum class PlayerProjectileKillReason : uint8_t {
        KrakenBodyHit,
        KrakenWeakPointHit,
    };

    struct PlayerBulletCollisionSnapshot {
        uint64_t runtimeId = 0;
        Vector3 worldPosition{};
        Vector3 velocity{};
        float radius = 0.0f;
        float lifeTime = 0.0f;
        float elapsedTime = 0.0f;
        int damage = 1;
        PlayerProjectileType projectileType = PlayerProjectileType::NormalShot;
        std::string lockedTargetId;
        uint8_t launchPhase = 0;
        bool active = false;
        bool killed = false;
        bool homingReady = false;
        bool exhaustEnabled = false;
    };

    enum class WingSide : uint8_t {
        Left,
        Right,
    };

    enum class LockedWingLaunchPhase : uint8_t {
        EjectionDrop,
        PreIgnitionHold,
        IgnitionRamp,
        Cruise,
    };

    PlayerBulletManager();
    ~PlayerBulletManager();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera, Player* player);
    void Finalize();
    void Update(float deltaTime);
    void Draw();
    void DrawImGui();

    void SetGameViewInputActive(bool isActive);
    void SetGameViewport(GameViewport* gameViewport);
    void SetUseLightweightBulletVisual(bool useLightweightVisual);
    void SetAimRuntimeContext(
        AimCorridorVisualController* visualController,
        AimCorridorTargetingController* targetingController,
        const PlayerLockOnTargetProvider* targetProvider,
        bool gameModeActive,
        bool playerAlive);
    void ClearAimCorridorContext();
    void SetProjectileRailMotionAdapter(ProjectileRailMotionAdapter* adapter) { projectileRailMotionAdapter_ = adapter; }
    EnemyBullet* SpawnBullet(const Vector3& position, const Vector3& velocity, int damage);
    void DeleteAllBullets();
    bool CheckHitAndKillFirstEllipsoid(
        const Vector3& center,
        float radius,
        const Vector3& axisScale,
        Vector3* hitPosition,
        int* damage,
        float* lastDistance,
        float* lastRadiusSum,
        float* lastBulletRadius);
    bool CheckHitAndKillFirstSphere(
        const Vector3& center,
        float radius,
        Vector3* hitPosition,
        int* damage,
        float* lastDistance,
        float* lastRadiusSum,
        float* lastBulletRadius);
    std::vector<PlayerBulletCollisionSnapshot> GetActiveCollisionSnapshots() const;
    bool TryKillProjectileByRuntimeId(
        std::uint64_t runtimeId,
        PlayerProjectileKillReason reason);
    size_t GetBulletCount() const;
    size_t GetActiveCount() const;
    float GetChargeTime() const { return chargeTime_; }
    float GetMaxChargeTime() const { return maxChargeTime_; }
    float GetChargeRate() const { return chargeRate_; }
    bool IsChargeMax() const { return isChargeMax_; }
    bool IsChargeInputHeld() const { return lastLeftClickHeld_; }
    size_t GetFiredBulletCount() const { return firedBulletCount_; }
    const Vector3& GetLastAimPoint() const { return lastAimPoint_; }
    const Vector3& GetLastAimDirection() const { return lastAimDirection_; }
    const Vector3& GetLastMuzzlePosition() const { return lastMuzzlePosition_; }
    void DrawKrakenNaturalLockHomingDiagnosticsImGui() const;

private:
    enum class LockedWingShotResult : uint8_t {
        NotRequested,
        FallbackToNormal,
        Spawned,
        SpawnFailed,
    };

    struct LockedTargetValidation {
        std::string targetId;
        bool targetingControllerAvailable = false;
        bool lockStateLocked = false;
        bool targetIdValid = false;
        bool alive = false;
        bool targetable = false;
        bool cameraFront = false;
        bool valid = false;
    };

    struct LockOnTargetProviderDiagnostics {
        std::string targetId{};
        Vector3 worldPosition{};
        PlayerLockOnTargetKind kind{};
        bool querySucceeded = false;
        bool alive = false;
        bool targetable = false;
        bool valid = false;
    };

    enum class LockedWingTargetLostReason : uint8_t {
        None,
        MissingTargetId,
        TargetProviderUnavailable,
        TargetRemoved,
        TargetDead,
        TargetInactive,
        NonFiniteTarget,
        NonFiniteDirection,
    };

    struct LockedWingLaunchState {
        std::string lockedTargetId;
        Vector3 currentFlightDirection{ 0.0f, 0.0f, 1.0f };
        Vector3 currentEjectionDownDirection{ 0.0f, -1.0f, 0.0f };
        Vector3 targetWorldPosition{};
        Vector3 desiredDirection{ 0.0f, 0.0f, 1.0f };
        uint64_t sequence = 0;
        uint64_t exhaustHandle = 0;
        float totalElapsed = 0.0f;
        float ejectionDropDuration = 0.06f;
        float ejectionDropDistance = 0.20f;
        float preIgnitionHoldDuration = 0.24f;
        float ignitionRampDuration = 0.15f;
        float baseBulletSpeed = 0.0f;
        float currentSpeedRate = 0.0f;
        float targetDistance = -1.0f;
        float directionDot = 1.0f;
        float angleErrorDegrees = 0.0f;
        float frameTurnDegrees = 0.0f;
        float maximumFrameTurnDegrees = 0.0f;
        WingSide launchWing = WingSide::Left;
        LockedWingLaunchPhase phase = LockedWingLaunchPhase::EjectionDrop;
        LockedWingTargetLostReason targetLostReason =
            LockedWingTargetLostReason::None;
        bool exhaustEnabled = false;
        bool ignitionStarted = false;
        bool homingReady = false;
        bool homingEnabled = false;
        bool homingStarted = false;
        bool targetQuerySucceeded = false;
        bool targetAlive = false;
        bool targetLost = false;
        bool targetLostStraightRecorded = false;
        bool exhaustFollowingDirection = false;
    };

    struct PlayerBulletInstance {
        PlayerBulletInstance();
        ~PlayerBulletInstance();
        PlayerBulletInstance(PlayerBulletInstance&&) noexcept;
        PlayerBulletInstance& operator=(PlayerBulletInstance&&) noexcept;
        std::unique_ptr<EnemyBullet> bullet;
        std::unique_ptr<LockedWingLaunchState> lockedWingLaunch;
        std::unique_ptr<PlayerFanChargeTrajectory> fanTrajectory;
        uint64_t runtimeId = 0;
        int damage = 1;
        PlayerProjectileType projectileType = PlayerProjectileType::NormalShot;
    };

    void FireFromPlayer();
    uint64_t AllocateBulletRuntimeId();
    LockedWingShotResult TrySpawnLockedWingShot();
    LockedWingShotResult SpawnLockedWingShot(
        WingSide wing, const std::string& targetId, bool forcedTest);
    LockedTargetValidation ValidateLockedTarget() const;
    Vector3 ResolveLockedWingLaunchDirection() const;
    Vector3 ResolveLockedWingEjectionDownDirection() const;
    void UpdateLockedWingShot(PlayerBulletInstance& instance, float scaledDeltaTime);
    void UpdateLockedWingHoming(
        PlayerBulletInstance& instance,
        float cruiseDeltaTime);
    void UpdateLockedWingMissileExhaust(PlayerBulletInstance& instance);
    void UpdateLockedWingShotDiagnostics();
    void ResetLockedWingHomingDiagnostics(bool resetStatistics);
    void RefreshLockOnTargetProviderDiagnostics();
    void RecordLockedWingHomingHit(const PlayerBulletInstance& instance);
    void ResetLockedWingShotState(bool resetStatistics);
    void ClearLockedWingShotForceState();
    void DrawLockedWingShotImGui();
    void DrawLockedWingMissileLaunchImGui();
    void DrawLockedWingMissileIgnitionImGui();
    void DrawLockOnTargetProviderImGui();
    void DrawLockedWingHomingImGui();
    void RemoveDeadBullets();
    void SyncModelPathBuffer();
    void UpdateCameraVelocity(float deltaTime);
    void UpdateViewportDebugState();
    void ApplyModelRotationOffsetToBullets();
    bool UpdateChargeState(float deltaTime, bool inputBlocked);
    void ResetFanCharge();
    void FireFanChargeVolley();
    void UpdateFanChargeShot(PlayerBulletInstance& instance, float deltaTime);
    Vector3 ResolveAimDirection(const Vector3& muzzleBasePosition, const Vector3& cameraForward);
    Vector3 ResolveFinalShotDirection(
        const Vector3& muzzleWorldPosition,
        const Vector3& fallbackDirection);
    void DrawAimImGui();
    void ResetAimDiagnostics();
    void RecordAimShot();
    bool ShouldBlockFireInput();

    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    Player* player_ = nullptr;
    GameViewport* gameViewport_ = nullptr;
    ProjectileRailMotionAdapter* projectileRailMotionAdapter_ = nullptr;
    std::unique_ptr<LockedWingMissileExhaustController> lockedWingMissileExhaustController_;
    std::unique_ptr<PlayerFanChargeAttack> fanChargeAttack_;
    const PlayerLockOnTargetProvider* targetProvider_ = nullptr;
    AimCorridorVisualController* aimCorridorVisualController_ = nullptr;
    AimCorridorTargetingController* aimCorridorTargetingController_ = nullptr;
    std::vector<PlayerBulletInstance> bullets_;
    std::array<char, 260> modelPathBuffer_{};
    std::string modelPath_ = "resources/EnemyBullet/EnemyBullet.obj";
    std::string inputBlockedReason_ = "Not initialized";
    std::string lastShotFallbackReason_ = "なし";
    Vector3 defaultScale_{ 0.35f, 0.35f, 0.35f };
    Vector3 defaultRotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 playerBulletModelRotationOffset_{ 0.0f, 4.71238899f, 0.0f };
    Vector3 lastFirePosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 lastFireDirection_{ 0.0f, 0.0f, 1.0f };
    Vector3 lastShotVelocity_{ 0.0f, 0.0f, 0.0f };
    Vector3 lastVisualDirection_{ 0.0f, 0.0f, 1.0f };
    Vector3 lastAimPoint_{ 0.0f, 0.0f, 0.0f };
    Vector3 lastAimDirection_{ 0.0f, 0.0f, 1.0f };
    Vector3 lastMuzzlePosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 cameraVelocity_{ 0.0f, 0.0f, 0.0f };
    Vector3 previousCameraPosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 currentBulletRotation_{ 0.0f, 0.0f, 0.0f };
    Vector2 mouseNdc_{ 0.0f, 0.0f };
    Vector2 mouseNormalized_{ 0.0f, 0.0f };
    float bulletSpeed_ = 28.0f;
    float bulletRadius_ = 0.16f;
    float bulletLifeTime_ = 3.0f;
    float fireInterval_ = 0.16f;
    float fireTimer_ = 0.0f;
    float muzzleOffset_ = 0.5f;
    float aimDistance_ = 30.0f;
    float inheritCameraVelocityFactor_ = 0.5f;
    float chargeTime_ = 0.0f;
    float maxChargeTime_ = 1.5f;
    float chargeRate_ = 0.0f;
    float lastShotDirectionLength_ = 0.0f;
    int bulletDamage_ = 1;
    int selectedBulletIndex_ = -1;
    AimMode aimMode_ = AimMode::AimCorridor;
    AimMode lastUsedAimMode_ = AimMode::AimCorridor;
    VisualDirectionSource visualDirectionSource_ = VisualDirectionSource::FinalVelocity;
    bool enablePlayerShot_ = true;
    bool gameViewInputActive_ = false;
    bool showBulletCollisionRadius_ = false;
    bool useLightweightBulletVisual_ = false;
    bool autoRemoveDeadBullets_ = true;
    bool inheritCameraVelocity_ = true;
    bool enableChargeFeedbackInput_ = true;
    bool isChargeMax_ = false;
    bool mouseInGameView_ = false;
    bool hasPreviousCameraPosition_ = false;
    bool lastLeftClickPressed_ = false;
    bool lastLeftClickHeld_ = false;
    bool lastCanFire_ = false;
    bool lastImGuiTextInputActive_ = false;
    bool aimRuntimeStateInitialized_ = false;
    bool aimGameModeActive_ = false;
    bool aimPlayerAlive_ = true;
    bool debugAimModeForced_ = false;
    bool lastAimFallbackUsed_ = false;
    bool lastShotSpawnDataValid_ = false;
    size_t firedBulletCount_ = 0;
    uint64_t nextBulletRuntimeId_ = 1;
    size_t cursorAimVisualUseCount_ = 0;
    size_t cursorAimStandardShotCount_ = 0;
    size_t aimCorridorShotCount_ = 0;
    size_t straightForwardShotCount_ = 0;

    LockedTargetValidation lockedTargetValidation_{};
    LockOnTargetProviderDiagnostics lockOnTargetProviderDiagnostics_{};
    Vector3 leftLockedWingLocalOffset_{ -1.0f, -0.35f, -0.33f };
    Vector3 rightLockedWingLocalOffset_{ 1.0f, -0.35f, -0.33f };
    Vector3 leftLockedWingWorldPosition_{};
    Vector3 rightLockedWingWorldPosition_{};
    Vector3 lastLockedWingLaunchDirection_{ 0.0f, 0.0f, 1.0f };
    Vector3 lastLockedWingEjectionDownDirection_{ 0.0f, -1.0f, 0.0f };
    Vector3 lastLockedWingRelativeVelocity_{};
    std::string lastLockedWingTargetId_;
    std::string lastLockedWingStatus_ = "未実行";
    float lockedWingEjectionDropDuration_ = 0.06f;
    float lockedWingEjectionDropDistance_ = 0.20f;
    float lockedWingPreIgnitionHoldDuration_ = 0.24f;
    float lockedWingIgnitionRampDuration_ = 0.15f;
    float lastLockedWingLaunchElapsed_ = 0.0f;
    float lastLockedWingSpeedRate_ = 0.0f;
    float lastLockedWingCurrentSpeed_ = 0.0f;
    WingSide nextLockedShotWing_ = WingSide::Left;
    WingSide lastLockedShotWing_ = WingSide::Left;
    WingSide forcedLockedShotWing_ = WingSide::Left;
    LockedWingLaunchPhase lastLockedWingLaunchPhase_ = LockedWingLaunchPhase::EjectionDrop;
    bool lockedWingShotEnabled_ = true;
    bool hasLastLockedShotWing_ = false;
    bool lockedWingForceTestPending_ = false;
    bool hasLastLockedWingLaunchPhase_ = false;
    bool lastLockedWingExhaustEnabled_ = false;
    bool lastLockedWingIgnitionStarted_ = false;
    bool lastLockedWingHomingReady_ = false;
    bool lastLockedWingHomingEnabled_ = false;
    bool lockedWingHomingEnabled_ = true;
    float lockedWingHomingTurnRateDegreesPerSecond_ = 360.0f;
    float lockedWingHomingMinimumTargetDistance_ = 0.05f;
    uint64_t lockedWingSequenceCounter_ = 0;
    uint64_t lastLockedWingShotSequence_ = 0;
    size_t lockedWingShotCount_ = 0;
    size_t leftLockedWingShotCount_ = 0;
    size_t rightLockedWingShotCount_ = 0;
    size_t lockedWingTargetInvalidFallbackCount_ = 0;
    size_t lockedWingSpawnFailureCount_ = 0;
    size_t lockedWingNormalFallbackCount_ = 0;
    size_t lockedWingAlternationErrorCount_ = 0;
    size_t lockedWingEjectionDropStartCount_ = 0;
    size_t lockedWingPreIgnitionHoldStartCount_ = 0;
    size_t lockedWingIgnitionStartCount_ = 0;
    size_t lockedWingCruiseTransitionCount_ = 0;
    size_t lockedWingDirectionFallbackCount_ = 0;
    size_t lockedWingNonFiniteVelocityCount_ = 0;
    size_t lockedWingHomingStartCount_ = 0;
    size_t lockedWingHomingUpdateCount_ = 0;
    size_t lockedWingTargetQuerySuccessCount_ = 0;
    size_t lockedWingTargetQueryFailureCount_ = 0;
    size_t lockedWingTargetDeathCount_ = 0;
    size_t lockedWingTargetRemovalCount_ = 0;
    size_t lockedWingNonFiniteTargetCount_ = 0;
    size_t lockedWingNonFiniteDirectionCount_ = 0;
    size_t lockedWingOppositeFallbackCount_ = 0;
    size_t lockedWingMaximumTurnClampCount_ = 0;
    size_t lockedWingHomingHitCount_ = 0;
    size_t lockedWingTargetLostStraightCount_ = 0;
    size_t lockedWingTargetSwitchCount_ = 0;
    size_t normalShotHomingApplyCount_ = 0;
};
