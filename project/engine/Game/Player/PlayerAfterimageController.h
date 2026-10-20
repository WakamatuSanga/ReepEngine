#pragma once
#include <cstdint>
#include <memory>
#include <vector>
class Camera;
class Model;
class Object3dCommon;
struct Transform;

class PlayerAfterimageController {
public:
    PlayerAfterimageController();
    ~PlayerAfterimageController();
    void Initialize(Object3dCommon* common, Camera* camera);
    void Update(float gameDeltaTime, bool active, bool rolling, Model* model, const Transform& pose);
    void RefreshCamera();
    void Draw();
    uint32_t GetDrawCount() const;
private:
    struct Slot;
    std::vector<std::unique_ptr<Slot>> slots_;
    Object3dCommon* common_ = nullptr;
    float sampleTimer_ = 0;
    bool wasRolling_ = false;
};
