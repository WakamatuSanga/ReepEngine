#include "SceneManager.h"
#include "TitleCloudTransition.h"
#include "MyGame.h"
#include "Engine/Core/FrameTimer.h"
#include "Engine/Input/Input.h"

SceneManager::SceneManager() = default;
SceneManager::~SceneManager() = default;

std::unique_ptr<SceneManager> SceneManager::instance_ = nullptr;

SceneManager* SceneManager::GetInstance() {
    if (!instance_) {
        instance_.reset(new SceneManager());
    }
    return instance_.get();
}

void SceneManager::ChangeScene(std::unique_ptr<IScene> newScene) {
    // 所有権の移動
    nextScene_ = std::move(newScene);
}

void SceneManager::Update() {
    if (titleTransition_ && titleTransition_->IsFinished()) {
        // Ignore clicks made/held during the transition until release.
        MyGame::GetInstance()->GetInput()->SuppressLeftMouseUntilRelease();
        titleTransition_.reset();
    }
    if (titleTransition_ && titleTransition_->CanSwitch()) {
        nextScene_ = std::move(transitionScene_);
        titleTransition_->OnSceneChanged();
    }
    // シーン切り替え予約があれば切り替える
    if (nextScene_) {
        // 古いシーンの終了処理
        if (currentScene_) {
            currentScene_->Finalize();
            // unique_ptrの書き換えにより、古いシーンは自動でdeleteされます
        }

        // 新しいシーンを現在のシーンにする
        currentScene_ = std::move(nextScene_);

        // 新しいシーンの初期化
        currentScene_->Initialize();
        if (titleTransition_) currentScene_->PrepareForPresentation();
    }

    if (titleTransition_) {
        titleTransition_->Update(FrameTimer::GetInstance().GetGameplayDeltaTime());
        if (!titleTransition_->IsCovering()) return;
    }
    if (currentScene_) {
        currentScene_->Update();
    }
}

void SceneManager::Draw() {
    if (currentScene_) {
        currentScene_->Draw();
    }
}

void SceneManager::Finalize() {
    titleTransition_.reset();
    transitionScene_.reset();
    nextScene_.reset();
    if (currentScene_) {
        currentScene_->Finalize();
        currentScene_.reset(); // 自動delete
    }
}

void SceneManager::ChangeSceneWithTitleClouds(std::unique_ptr<IScene> newScene) {
    if (titleTransition_ || nextScene_ || !newScene) return;
    transitionScene_ = std::move(newScene);
    titleTransition_ = std::make_unique<TitleCloudTransition>();
    titleTransition_->Initialize();
}

void SceneManager::DrawTransitionOverlay() {
    if (currentScene_) currentScene_->DrawOverlay();
    if (titleTransition_) titleTransition_->Draw();
}

void SceneManager::OnFramePresented() {
    // Called only after the existing Present and fence wait; no new GPU wait.
    if (titleTransition_) titleTransition_->OnFramePresented();
}
