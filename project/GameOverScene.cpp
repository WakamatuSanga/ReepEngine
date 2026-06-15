#include "GameOverScene.h"
#include "MyGame.h"
#include "SceneManager.h"
#include "TitleScene.h"
#include "Engine/Core/WinApp.h"
#include "Engine/Graphics/Sprite/Sprite.h"
#include "Engine/Graphics/Sprite/SpriteCommon.h"
#include "Engine/Input/Input.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    std::string ToGenericString(const std::filesystem::path& path) {
        return path.lexically_normal().generic_string();
    }

    std::string ResolveResourcePath(const std::string& path) {
        const std::array<std::filesystem::path, 6> basePaths = {
            std::filesystem::path{},
            std::filesystem::path{ "project" },
            std::filesystem::path{ ".." } / "project",
            std::filesystem::path{ ".." } / ".." / "project",
            std::filesystem::path{ ".." } / ".." / ".." / "project",
            std::filesystem::path{ ".." } / ".." / ".." / ".." / "project",
        };

        for (const std::filesystem::path& basePath : basePaths) {
            const std::filesystem::path candidate = basePath.empty()
                ? std::filesystem::path(path)
                : basePath / path;
            if (std::filesystem::exists(candidate)) {
                return ToGenericString(candidate);
            }
        }
        return {};
    }

    bool IsImGuiCapturingMouse() {
#ifdef _DEBUG
        const ImGuiIO& io = ImGui::GetIO();
        return io.WantCaptureMouse || ImGui::IsAnyItemActive();
#else
        return false;
#endif
    }
}

GameOverScene::GameOverScene() = default;

GameOverScene::~GameOverScene() = default;

void GameOverScene::Initialize() {
    if (DirectXCommon* dxCommon = MyGame::GetInstance()->GetDxCommon()) {
        DirectXCommon::PostEffectParameters& params = dxCommon->GetPostEffectParameters();
        params.grayscaleEnabled = 0;
        params.grayscaleIntensity = 1.0f;
    }

    SpriteCommon* spriteCommon = MyGame::GetInstance()->GetSpriteCommon();
    gameOverSprite_ = std::make_unique<Sprite>();
    gameOverSprite_->Initialize(spriteCommon);

    gameOverTexturePath_ = ResolveResourcePath("resources/ui/gameover.png");
    hasGameOverTexture_ = !gameOverTexturePath_.empty();
    if (!hasGameOverTexture_) {
        gameOverTexturePath_ = ResolveResourcePath("resources/obj/axis/uvChecker.png");
    }

    if (!gameOverTexturePath_.empty()) {
        gameOverSprite_->SetTexture(gameOverTexturePath_);
    }

    const Vector2 textureSize = gameOverSprite_->GetTextureSize();
    const float windowWidth = static_cast<float>(WinApp::kClientWidth);
    const float windowHeight = static_cast<float>(WinApp::kClientHeight);
    const float fitScale = (textureSize.x > 0.0f && textureSize.y > 0.0f)
        ? (std::min)(windowWidth / textureSize.x, windowHeight / textureSize.y)
        : 1.0f;
    const Vector2 displaySize = {
        textureSize.x * fitScale,
        textureSize.y * fitScale,
    };
    gameOverSprite_->SetSize(displaySize);
    gameOverSprite_->SetPosition({
        (windowWidth - displaySize.x) * 0.5f,
        (windowHeight - displaySize.y) * 0.5f,
    });
    gameOverSprite_->SetColor(hasGameOverTexture_ ? Vector4{ 1.0f, 1.0f, 1.0f, 1.0f } : Vector4{ 0.8f, 0.15f, 0.15f, 1.0f });
    gameOverSprite_->Update();
}

void GameOverScene::Update() {
#ifdef _DEBUG
    ImGui::Begin("GameOver Scene");
    ImGui::Text("Left click to return Title.");
    ImGui::Text("gameover.png: %s", hasGameOverTexture_ ? "loaded" : "missing fallback");
    ImGui::End();
#endif

    Input* input = MyGame::GetInstance()->GetInput();
    if (input && input->MouseTrigger(Input::MouseLeft) && !IsImGuiCapturingMouse()) {
        SceneManager::GetInstance()->ChangeScene(std::make_unique<TitleScene>());
    }
}

void GameOverScene::Draw() {
    if (!gameOverSprite_) {
        return;
    }

    SpriteCommon* spriteCommon = MyGame::GetInstance()->GetSpriteCommon();
    spriteCommon->CommonDrawSetting();
    gameOverSprite_->Draw();
}

void GameOverScene::Finalize() {
    gameOverSprite_.reset();
}
