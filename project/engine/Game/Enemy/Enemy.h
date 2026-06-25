#pragma once
#include "Engine/math/Matrix4x4.h"
#include <memory>
#include <string>

class Camera;
class Model;
class Object3d;
class Object3dCommon;

class Enemy {
public:
    enum class State {
        Spawning,
        AligningToPlayer,
        Active,
        Dead,
    };

    enum class SpawnSpinAxisMode {
        AroundForward,
        AroundWorldY,
    };

    enum class AlignSmoothType {
        Linear,
        SmoothStep,
        EaseOut,
    };

    Enemy();
    ~Enemy();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera, const std::string& enemyId);
    void Finalize();
    void Update(float deltaTime);
    void Draw();
    void DrawImGui();

    void SetEnemyId(const std::string& enemyId);
    void SetEnemyType(const std::string& enemyType);
    void SetPosition(const Vector3& position);
    void SetRotation(const Vector3& rotation);
    void SetScale(const Vector3& scale);
    void SetVelocity(const Vector3& velocity);
    void SetForward(const Vector3& forward);
    void LookAt(const Vector3& targetPosition);
    void SetHitRadius(float hitRadius);
    void SetHitScale(const Vector3& hitScale);
    void SetUseEllipsoidHitShape(bool enabled);
    void SetUseLightweightVisual(bool useLightweightVisual);
    void SetModelPath(const std::string& modelPath);
    void SetSpawnPresentationOptions(
        bool faceDownDuringSpawn,
        bool facePlayerOnComplete,
        bool resetRollOnActive,
        bool resetPitchOnActive,
        bool enableCollisionDuringSpawn,
        SpawnSpinAxisMode spinAxisMode,
        bool facePlayerDuringSpawn,
        float facePlayerStartT,
        float facePlayerEndT,
        float spinFadeStartT,
        float spinFadeEndT,
        bool alignAfterSpawn,
        float alignDuration,
        AlignSmoothType alignSmoothType,
        const Vector3& lookTarget);
    void StartSpawnAnimation(
        const Vector3& targetPosition,
        float spawnHeight,
        float spawnDuration,
        float spawnSpinSpeedDegrees,
        float spawnAttackDelay);
    void StartSpawnAnimationFrom(
        const Vector3& startPosition,
        const Vector3& targetPosition,
        float spawnDuration,
        float spawnSpinSpeedDegrees,
        float spawnAttackDelay);

    void Damage(int amount);
    void Kill();
    void Revive(int hp);

    bool IsActive() const { return isActive_; }
    bool IsDead() const { return isDead_; }
    bool CanAttack() const { return state_ == State::Active && isActive_ && !isDead_; }
    bool CanReceivePlayerBullet() const {
        return isActive_ && !isDead_ && (state_ == State::Active || (state_ == State::Spawning && enableCollisionDuringSpawn_));
    }
    State GetState() const { return state_; }
    const std::string& GetEnemyId() const { return enemyId_; }
    const std::string& GetEnemyType() const { return enemyType_; }
    const Vector3& GetPosition() const { return position_; }
    const Vector3& GetForward() const { return desiredForward_; }
    float GetHitRadius() const { return hitRadius_; }
    const Vector3& GetHitScale() const { return hitScale_; }
    bool IsUsingEllipsoidHitShape() const { return useEllipsoidHitShape_; }
    int GetHp() const { return hp_; }

private:
    void LoadModel();
    void UpdateObjectTransform();
    void UpdateSpawnAnimation(float deltaTime);
    void BeginAlignToPlayer();
    void UpdateAlignToPlayer(float deltaTime);
    void ApplySpawnCompleteFacing();
    Vector3 MakeSpawnFacingDownVisualRotation(float spinAngle) const;
    Vector3 ComputePlayerFacingRotation() const;
    float ApplyAlignCurve(float t) const;

    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    std::unique_ptr<Object3d> object_;
    Model* model_ = nullptr;

    std::string enemyId_;
    std::string enemyType_ = "Default";
    std::string modelPath_ = "resources/Enemy/Enemy.obj";
    std::string resolvedModelPath_;
    std::string texturePath_;
    std::string loadStatus_ = "Not initialized";

    Vector3 spawnStartPosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 spawnTargetPosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 spawnLookTarget_{ 0.0f, 0.0f, 0.0f };
    Vector3 spawnVisualRotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 alignStartRotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 alignTargetRotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 activeFinalRotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 position_{ 0.0f, 0.0f, 10.0f };
    Vector3 rotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 finalSpawnRotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 modelRotationOffset_{ 0.0f, 4.71238899f, 0.0f };
    Vector3 visualModelRotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 scale_{ 0.8f, 0.8f, 0.8f };
    Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
    Vector3 desiredForward_{ 0.0f, 0.0f, -1.0f };
    float hitRadius_ = 0.6f;
    Vector3 hitScale_{ 1.6f, 1.0f, 1.0f };
    bool useEllipsoidHitShape_ = true;
    bool showHitShapeDebug_ = false;
    float spawnElapsed_ = 0.0f;
    float spawnDuration_ = 1.0f;
    float spawnSpinSpeedRadians_ = 12.5663706f;
    float spawnAttackDelay_ = 1.0f;
    float currentSpawnT_ = 0.0f;
    float spawnFacePlayerStartT_ = 0.35f;
    float spawnFacePlayerEndT_ = 0.95f;
    float spawnSpinFadeStartT_ = 0.25f;
    float spawnSpinFadeEndT_ = 0.85f;
    float currentSpawnFacingWeight_ = 0.0f;
    float currentSpinWeight_ = 1.0f;
    float spawnSpinAngle_ = 0.0f;
    float alignElapsed_ = 0.0f;
    float alignDuration_ = 0.2f;
    float currentAlignT_ = 0.0f;
    float spawnGlideArcHeight_ = 1.0f;
    int hp_ = 10;
    bool isActive_ = true;
    bool isDead_ = false;
    bool useFallbackModel_ = false;
    bool useLightweightVisual_ = false;
    bool spawnFaceDownDuringSpawn_ = true;
    bool spawnFacePlayerOnComplete_ = true;
    bool spawnResetRollOnActive_ = true;
    bool spawnResetPitchOnActive_ = true;
    bool enableCollisionDuringSpawn_ = false;
    bool facePlayerDuringSpawn_ = true;
    bool alignAfterSpawn_ = true;
    SpawnSpinAxisMode spawnSpinAxisMode_ = SpawnSpinAxisMode::AroundForward;
    AlignSmoothType alignSmoothType_ = AlignSmoothType::SmoothStep;
    State state_ = State::Active;
};
