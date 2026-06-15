#pragma once
#include "Engine/Scene/IScene.h"
#include <memory>
#include <string>

class Sprite;

class GameOverScene : public IScene {
public:
    GameOverScene();
    ~GameOverScene() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    std::unique_ptr<Sprite> gameOverSprite_;
    std::string gameOverTexturePath_;
    bool hasGameOverTexture_ = false;
};
