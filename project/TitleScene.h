#pragma once
#include "Engine/Scene/IScene.h"

// タイトル画面のシーン
class TitleScene : public IScene {
public:
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;
};
