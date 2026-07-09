#pragma once
#include "Engine/math/Matrix4x4.h"
#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class Camera;
class EnemyBullet;
class GameViewport;
class Object3dCommon;
class Player;

class PlayerBulletManager {
public:
    enum class AimMode {
        CameraForward,
        MouseRay,
        MouseAimPlane,
    };

    enum class VisualDirectionSource {
        AimDirection,
        FinalVelocity,
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
    size_t GetBulletCount() const;
    size_t GetActiveCount() const;
    float GetChargeTime() const { return chargeTime_; }
    float GetMaxChargeTime() const { return maxChargeTime_; }
    float GetChargeRate() const { return chargeRate_; }
    bool IsChargeMax() const { return isChargeMax_; }
    bool IsChargeInputHeld() const { return lastLeftClickHeld_; }
    const Vector3& GetLastAimPoint() const { return lastAimPoint_; }
    const Vector3& GetLastAimDirection() const { return lastAimDirection_; }
    const Vector3& GetLastMuzzlePosition() const { return lastMuzzlePosition_; }

private:
    struct PlayerBulletInstance {
        std::unique_ptr<EnemyBullet> bullet;
        int damage = 1;
    };

    void FireFromPlayer();
    void RemoveDeadBullets();
    void SyncModelPathBuffer();
    void UpdateCameraVelocity(float deltaTime);
    void UpdateViewportDebugState();
    void ApplyModelRotationOffsetToBullets();
    void UpdateChargeState(float deltaTime, bool inputBlocked);
    Vector3 ResolveAimDirection(const Vector3& muzzleBasePosition, const Vector3& cameraForward);
    bool ShouldBlockFireInput();

    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    Player* player_ = nullptr;
    GameViewport* gameViewport_ = nullptr;
    std::vector<PlayerBulletInstance> bullets_;
    std::array<char, 260> modelPathBuffer_{};
    std::string modelPath_ = "resources/EnemyBullet/EnemyBullet.obj";
    std::string inputBlockedReason_ = "Not initialized";
    std::string lastShotFallbackReason_ = "None";
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
    float maxChargeTime_ = 1.2f;
    float chargeRate_ = 0.0f;
    int bulletDamage_ = 1;
    int selectedBulletIndex_ = -1;
    AimMode aimMode_ = AimMode::MouseAimPlane;
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
    size_t firedBulletCount_ = 0;
};
