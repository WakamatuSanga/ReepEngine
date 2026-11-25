#include "SceneManager.h"
#include "TitleCloudTransition.h"
#include "MyGame.h"
#include "GameScene.h"
#include "ParticleContestDemo.h"
#include "TitleScene.h"
#include "Engine/Utility/Logger.h"
#include <exception>
#include "Engine/Core/FrameTimer.h"
#include "Engine/Input/Input.h"

SceneManager::SceneManager() : particleContestDemo_(std::make_unique<ParticleContestDemo>()) {}
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
    presentationRevealPending_ = false;
    nextScene_ = std::move(newScene);
}

void SceneManager::Update() {
    UpdateParticleContestDemo();
    if (!BlocksDebugUiForNextUpdate() && titleTransition_ && titleTransition_->DrawImGui() && !nextScene_) {
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
        try {
            currentScene_->Initialize();
            currentScene_->PrepareForPresentation();
        } catch (const std::exception& error) {
            lastPreparationError_ = error.what();
            Logger::Log("[Stage preparation failed] " + lastPreparationError_ + "\n");
            // Recorded DIRECT-list commands may still refer to the failed scene.
            // Release it only after this frame's existing submission/fence.
            failedPreparationScene_ = std::move(currentScene_);
            currentScene_ = std::make_unique<TitleScene>();
            currentScene_->Initialize();
            currentScene_->PrepareForPresentation();
        }
        presentationRevealPending_ = true;
        if (exchanged) {
            // CPU preparation succeeded (or a fresh title was restored). The next
            // fully covered Draw + Present + DIRECT queue fence gates reveal.
            titleTransition_->OnSceneChanged();
        }
    }
    if (IsTitleTransitionActive()) {
        if (!titleTransition_->IsCovering() || testTransition_) return;
    }
    if (currentScene_) currentScene_->Update();
}

void SceneManager::UpdateParticleContestDemo() {
    using Screen = ParticleContestDemo::Screen;
    using Action = ParticleContestDemo::SceneAction;
    auto* title = dynamic_cast<TitleScene*>(currentScene_.get());
    const auto* game = dynamic_cast<const GameScene*>(currentScene_.get());
    Screen screen = Screen::Other;
    if (nextScene_ || presentationRevealPending_ || IsTitleTransitionActive()) {
        screen = Screen::Busy;
    } else if (title) {
        screen = title->CanStartLaunch() ? Screen::Title : Screen::TitleDeparture;
    } else if (game && game->IsParticleContestScene()) {
        screen = game->IsParticleContestReady() ? Screen::Game : Screen::GameIntro;
    }

    auto* app = MyGame::GetInstance();
    auto* input = app->GetInput();
    const bool wasSuppressingInput = particleContestDemo_->BlocksManualInput();
    const auto action = particleContestDemo_->UpdateScreen(screen, app->GetWinApp()->WasF8Pressed(),
        FrameTimer::GetInstance().GetRawDeltaTime(), !lastPreparationError_.empty());
    const bool suppressInput = particleContestDemo_->BlocksManualInput();
    input->SetManualInputSuppressed(suppressInput);
    if (wasSuppressingInput && !suppressInput) input->SuppressLeftMouseUntilRelease();
    if (action == Action::StartFromTitle && title) {
        title->StartLaunch(true);
    } else if (action == Action::ReturnToTitle) {
        input->SuppressLeftMouseUntilRelease();
        ChangeScene(std::make_unique<TitleScene>());
    }
}

bool SceneManager::BlocksDebugUiForNextUpdate() const {
    const IScene* scene = nextScene_ ? nextScene_.get() : currentScene_.get();
    return particleContestDemo_->BlocksManualInput() || (scene && scene->BlocksDebugUiForPresentation());
}

void SceneManager::Draw() {
    if (currentScene_) {
        currentScene_->Draw();
    }
}

void SceneManager::Finalize() {
    *particleContestDemo_ = ParticleContestDemo{};
    if (failedPreparationScene_) failedPreparationScene_->Finalize();
    failedPreparationScene_.reset();
    lastPreparationError_.clear();
    presentationRevealPending_ = false;
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
    lastPreparationError_.clear();
    transitionScene_ = std::move(newScene);
    testTransition_ = false;
}

void SceneManager::DrawTransitionOverlay() {
    if (currentScene_) currentScene_->DrawOverlay();
    if (titleTransition_) titleTransition_->Draw();
}

void SceneManager::OnFramePresented() {
    // Called only after the existing Present and fence wait; no new GPU wait.
    if (failedPreparationScene_) {
        failedPreparationScene_->Finalize();
        failedPreparationScene_.reset();
    }
    if (titleTransition_) {
        const bool wasActive = titleTransition_->IsActive();
        titleTransition_->OnFramePresented();
        if (wasActive && !titleTransition_->IsActive()) {
            MyGame::GetInstance()->GetInput()->SuppressLeftMouseUntilRelease();
            testTransition_ = false;
        }
    }
    if (presentationRevealPending_ && !IsTitleTransitionActive() && currentScene_) {
        presentationRevealPending_ = false;
        currentScene_->OnPresentationRevealed();
    }
    if (currentScene_) currentScene_->OnFramePresented();

}
