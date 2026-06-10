#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstdint>
#include <memory>
#include <string>

class Camera;
class Model;
class Object3d;
class Object3dCommon;

class Player {
public:
    Player();
    ~Player();

    void Initialize(Object3dCommon* object3dCommon, Camera* camera);
    void Finalize();
    void Update(float deltaTime);
    void Draw();
    void DrawImGui();

    void SetGameViewInputActive(bool isActive);
    bool UsesWASDInput() const;

private:
    void LoadModel();
    void ResetPosition();
    void UpdateWorldPosition();
    void UpdateObjectTransform();

    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    std::unique_ptr<Object3d> object_;
    Model* model_ = nullptr;
    std::string modelPath_ = "resources/Player/player.obj";
    std::string resolvedModelPath_;
    std::string texturePath_;
    std::string loadStatus_;
    Vector3 basePosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 worldPosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 modelScale_{ 0.25f, 0.25f, 0.25f };
    Vector3 modelRotation_{ 0.0f, 0.0f, 0.0f };
    float localOffsetX_ = 0.0f;
    float localOffsetY_ = 0.0f;
    float moveLimitX_ = 2.5f;
    float moveLimitY_ = 1.4f;
    float moveSpeed_ = 3.0f;
    float distanceFromCamera_ = 6.0f;
    bool enablePlayer_ = true;
    bool showPlayer_ = true;
    bool useFallbackModel_ = false;
    bool gameViewInputActive_ = false;
    uint64_t updateCount_ = 0;
    std::string inputBlockedReason_ = "Not updated";
    bool lastWPressed_ = false;
    bool lastAPressed_ = false;
    bool lastSPressed_ = false;
    bool lastDPressed_ = false;
    bool lastInputApplied_ = false;
    Vector3 lastRawMoveInput_{ 0.0f, 0.0f, 0.0f };
};
