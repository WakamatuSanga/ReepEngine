#pragma once
#include "Engine/Scene/IScene.h"
#include <memory>

// シーンを管理するクラス
class SceneManager {
    friend struct std::default_delete<SceneManager>;
public:
    static SceneManager* GetInstance();

    void Update();
    void Draw();
    void Finalize();

    // 次のシーンを予約する (unique_ptrで所有権ごと受け取る)
    void ChangeScene(std::unique_ptr<IScene> newScene);

private:
    SceneManager() = default;
    ~SceneManager() = default;
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

private:
    static std::unique_ptr<SceneManager> instance_;

    std::unique_ptr<IScene> currentScene_; // 現在のシーン
    std::unique_ptr<IScene> nextScene_;    // 次のシーン(切り替え予約用)
};
