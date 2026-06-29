#pragma once

#include "InfluenceField.h"

#include <array>
#include <memory>
#include <vector>

class BoostController;
class Camera;
class EnemyManager;
class GpuParticleSystem;
class Model;
class Object3d;
class Object3dCommon;
class Player;
class VolumetricCloudPass;

class InfluenceFieldManager {
public:
    InfluenceFieldManager();
    ~InfluenceFieldManager();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Finalize();
    void SetTargets(Player* player, EnemyManager* enemyManager, BoostController* boostController);
    void SetConsumers(GpuParticleSystem* gpuParticleSystem, VolumetricCloudPass* cloudPass);
    void SetDebugVisualsEnabled(bool enabled);
    void Update(float deltaTime);
    void DrawDebug();
    void DrawImGui();

    const std::vector<InfluenceField>& GetFields() const { return fields_; }
    uint32_t GetFieldCount() const { return fieldCount_; }

private:
    void BuildFields();
    void AddField(const InfluenceField& field);
    void PackFields();
    void PushToConsumers();
    void UpdateDebugObjects();
    void EnsureDebugObjects(size_t count);
    void EnsureTunnelDebugObject();
    uint32_t BuildFlags(const InfluenceField& field) const;
    float GetBoostPower() const;

    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    Player* player_ = nullptr;
    EnemyManager* enemyManager_ = nullptr;
    BoostController* boostController_ = nullptr;
    GpuParticleSystem* gpuParticleSystem_ = nullptr;
    VolumetricCloudPass* cloudPass_ = nullptr;

    std::vector<InfluenceField> fields_;
    std::array<Vector4, kMaxInfluenceFields> centersAndRadius_{};
    std::array<Vector4, kMaxInfluenceFields> params_{};
    uint32_t fieldCount_ = 0;

    Model* debugPlayerSphereModel_ = nullptr;
    Model* debugEnemySphereModel_ = nullptr;
    Model* debugTunnelModel_ = nullptr;
    std::vector<std::unique_ptr<Object3d>> debugSphereObjects_;
    std::unique_ptr<Object3d> debugTunnelObject_;

    bool enabled_ = true;
    bool enablePlayerInfluence_ = true;
    bool enableEnemyInfluence_ = true;
    bool enableParticleInfluence_ = true;
    bool enableCloudInfluence_ = true;
    bool includeSpawningEnemies_ = false;
    bool showInfluenceDebug_ = false;
    bool debugVisualsEnabled_ = false;

    float playerRadius_ = 4.0f;
    float playerParticleRepulsionStrength_ = 8.0f;
    float playerCloudClearStrength_ = 0.65f;
    float playerFalloffPower_ = 2.0f;
    float enemyRadius_ = 3.0f;
    float enemyParticleRepulsionStrength_ = 5.0f;
    float enemyCloudClearStrength_ = 0.45f;
    float enemyFalloffPower_ = 2.0f;
    float boostRadiusMultiplier_ = 1.5f;
    float boostStrengthMultiplier_ = 1.6f;
    float particleInfluenceResponseScale_ = 1.0f;

    bool enableRailParticleFlow_ = true;
    bool useCameraForwardRailFlow_ = true;
    bool showRailFlowDebug_ = false;
    float railFlowSpeed_ = 6.0f;
    float railFlowScale_ = 1.0f;
    float railSpawnAheadDistance_ = 24.0f;
    float railDespawnBehindDistance_ = 8.0f;
    Vector3 fixedRailFlowDirection_{0.0f, 0.0f, -1.0f};
    Vector3 lastRailFlowDirection_{0.0f, 0.0f, -1.0f};

    bool useCameraForwardTunnel_ = true;
    float tunnelLength_ = 18.0f;
    float tunnelRadius_ = 3.0f;
    float tunnelClearStrength_ = 0.45f;
};
