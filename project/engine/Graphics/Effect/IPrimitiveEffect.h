#pragma once

class Camera;

class IPrimitiveEffect {
public:
    virtual ~IPrimitiveEffect() = default;

    virtual void Update(float deltaTime, const Camera* camera) = 0;
    virtual void Draw() = 0;
};
