#pragma once

#include "Engine/math/Matrix4x4.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

class Camera;
class Model;
class Object3d;
class Object3dCommon;

struct EnemyBreakupPose {
    Transform transform{};
    Vector3 velocity{};
    bool boxVisual = false;
};

class EnemyBreakupEffect {
public:
    EnemyBreakupEffect(Object3dCommon* objects, Camera* camera);
    ~EnemyBreakupEffect();

    bool Prepare();
    bool Spawn(const EnemyBreakupPose& pose);
    void SetWind(const Vector3& direction) { wind_ = direction; }
    void Update(float deltaTime);
    void Draw();
    void DrawImGui();

private:
    static constexpr size_t kFragmentCount = 3;
    static constexpr size_t kCapacity = 24;

    struct Fragment {
        std::unique_ptr<Object3d> object;
        Vector3 position{};
        Vector3 velocity{};
    };

    struct Breakup {
        std::array<Fragment, kFragmentCount> fragments;
        EnemyBreakupPose pose{};
        Matrix4x4 orientation{};
        Vector3 windVelocity{};
        float age = 0.0f;
        bool active = false;
        bool firstUpdate = false;
    };

    void UpdateFragment(Breakup& breakup, size_t index, float previousAge, const Vector3& windDisplacement);

    Object3dCommon* objects_ = nullptr;
    Camera* camera_ = nullptr;
    std::array<Model*, kFragmentCount> shells_{};
    std::array<Model*, kFragmentCount> cutFaces_{};
    Model* box_ = nullptr;
    std::array<Breakup, kCapacity> breakups_{};
    Vector3 wind_{};
    uint64_t droppedCount_ = 0;
    bool prepared_ = false;
};
