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
    if (!newScene) return;
    // A direct Restart/end request cancels the outstanding title transition.
    if (titleTransition_) titleTransition_->Cancel();
    transitionScene_.reset();
    testTransition_ = false;
    nextScene_ = std::move(newScene);
}

void SceneManager::Update() {
    if (titleTransition_ && titleTransition_->DrawImGui() && !nextScene_) {
        testTransition_ = titleTransition_->Start();
    }
    const bool wasActive = IsTitleTransitionActive();
    if (wasActive) titleTransition_->Update(FrameTimer::GetInstance().GetRawDeltaTime());
    bool exchanged = false;
    if (wasActive && titleTransition_->CanSwitch()) {
        if (testTransition_) {
            if (titleTransition_->CanFinishTestLoad()) titleTransition_->OnSceneChanged();
        } else {
            nextScene_ = std::move(transitionScene_);
            exchanged = static_cast<bool>(nextScene_);
        }
    }
    if (nextScene_) {
        if (currentScene_) currentScene_->Finalize();
        currentScene_ = std::move(nextScene_);
        currentScene_->Initialize();
        if (exchanged) {
            currentScene_->PrepareForPresentation();
            // Initialization is synchronous; the first new-scene Present also gates exit.
            titleTransition_->OnSceneChanged();
        }
    }
    if (IsTitleTransitionActive()) {
        if (!titleTransition_->IsCovering() || testTransition_) return;
    }
    if (currentScene_) currentScene_->Update();
}

void SceneManager::Draw() {
    if (currentScene_) {
        currentScene_->Draw();
    }
}

void SceneManager::Finalize() {
    titleTransition_.reset();
    testTransition_ = false;
    transitionScene_.reset();
    nextScene_.reset();
    if (currentScene_) {
        currentScene_->Finalize();
        currentScene_.reset(); // 自動delete
    }
}

bool SceneManager::IsTitleTransitionActive() const {
    return titleTransition_ && titleTransition_->IsActive();
}
void SceneManager::PreloadTitleCloudTransition() {
    if (!titleTransition_) {
        titleTransition_ = std::make_unique<TitleCloudTransition>();
        titleTransition_->Initialize(MyGame::GetInstance()->GetSpriteCommon());
    }
}
void SceneManager::ShowTitleCloudTransitionSettings() { TitleCloudTransition::ShowSettings(); }
void SceneManager::ChangeSceneWithTitleClouds(std::unique_ptr<IScene> newScene) {
    if (IsTitleTransitionActive() || nextScene_ || !newScene) return;
    PreloadTitleCloudTransition();
    if (!titleTransition_->Start()) return;
    transitionScene_ = std::move(newScene);
    testTransition_ = false;
}

void SceneManager::DrawTransitionOverlay() {
    if (currentScene_) currentScene_->DrawOverlay();
    if (titleTransition_) titleTransition_->Draw();
}

void SceneManager::OnFramePresented() {
    // Called only after the existing Present and fence wait; no new GPU wait.
    if (titleTransition_) {
        const bool wasActive = titleTransition_->IsActive();
        titleTransition_->OnFramePresented();
        if (wasActive && !titleTransition_->IsActive()) {
            MyGame::GetInstance()->GetInput()->SuppressLeftMouseUntilRelease();
            testTransition_ = false;
        }
    }
}
