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
    // Screen-space UI after scene post effects, before the transition cover.
    virtual void DrawOverlay() {}

    // シーンの終了処理
    virtual void Finalize() = 0;
    virtual void PrepareForPresentation() {}
    virtual void OnPresentationRevealed() {}
    virtual void OnFramePresented() {}
    virtual bool BlocksDebugUiForPresentation() const { return false; }

    // Fullscreen scenes are presented behind their overlay UI instead of a docked Game View.
    virtual bool UsesFullscreenScenePresentation() const { return false; }
};
