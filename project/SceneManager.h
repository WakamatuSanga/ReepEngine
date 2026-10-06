#pragma once
#include "Engine/Scene/IScene.h"
#include <memory>
class TitleCloudTransition;

// シーンを管理するクラス
class SceneManager {
    friend struct std::default_delete<SceneManager>;
public:
    static SceneManager* GetInstance();

    void Update();
    void Draw();
    void Finalize();
    void DrawTransitionOverlay();
    void OnFramePresented();
    void ChangeSceneWithTitleClouds(std::unique_ptr<IScene> newScene);

    // 次のシーンを予約する (unique_ptrで所有権ごと受け取る)
    void ChangeScene(std::unique_ptr<IScene> newScene);

    bool UsesFullscreenScenePresentationForNextUpdate() const {
        if (titleTransition_) return true;
        // ImGui begins before Update applies a pending scene change.
        const IScene* scene = nextScene_ ? nextScene_.get() : currentScene_.get();
        return scene && scene->UsesFullscreenScenePresentation();
    }

private:
    SceneManager();
    ~SceneManager();
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

private:
    static std::unique_ptr<SceneManager> instance_;

    std::unique_ptr<IScene> currentScene_; // 現在のシーン
    std::unique_ptr<IScene> nextScene_;    // 次のシーン(切り替え予約用)
    std::unique_ptr<IScene> transitionScene_;
    std::unique_ptr<TitleCloudTransition> titleTransition_;
};
