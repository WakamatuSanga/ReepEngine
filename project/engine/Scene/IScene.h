#pragma once

// シーンの基底クラス（Stateパターンのインターフェース）
class IScene {
public:
    virtual ~IScene() = default;

    // シーンの初期化
    virtual void Initialize() = 0;

    // シーンの更新
    virtual void Update() = 0;

    // シーンの描画
    virtual void Draw() = 0;

    // シーンの終了処理
    virtual void Finalize() = 0;
};
