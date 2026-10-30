#pragma once
#include <memory>
class Camera;
class EnemyBullet;
class Object3dCommon;

// Presentation only. The bullet managers still own motion, collisions and lifetime.
class ProjectileBeamVisual {
public:
    ProjectileBeamVisual();
    ~ProjectileBeamVisual();
    bool Initialize(Object3dCommon* common,const Camera* camera);
    bool IsReady() const;
    void Begin();
    void Add(const EnemyBullet& bullet,bool player);
    void Draw(bool player);
private:
    struct State;
    std::unique_ptr<State> state_;
};
