#include "ScreenSpaceFakeShadowPass.h"

#include "Engine/Game/Enemy/Enemy.h"
#include "Engine/Game/Enemy/EnemyManager.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Sprite/Sprite.h"
#include "Engine/Graphics/Sprite/SpriteCommon.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr float kMinViewportSize = 1.0f;
    constexpr float kClipMargin = 0.35f;

    Vector4 TransformPoint(const Vector3& value, const Matrix4x4& matrix) {
        return {
            value.x * matrix.m[0][0] + value.y * matrix.m[1][0] + value.z * matrix.m[2][0] + matrix.m[3][0],
            value.x * matrix.m[0][1] + value.y * matrix.m[1][1] + value.z * matrix.m[2][1] + matrix.m[3][1],
            value.x * matrix.m[0][2] + value.y * matrix.m[1][2] + value.z * matrix.m[2][2] + matrix.m[3][2],
            value.x * matrix.m[0][3] + value.y * matrix.m[1][3] + value.z * matrix.m[2][3] + matrix.m[3][3],
        };
    }

    float SmoothStep(float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
}

ScreenSpaceFakeShadowPass::ScreenSpaceFakeShadowPass() = default;

ScreenSpaceFakeShadowPass::~ScreenSpaceFakeShadowPass() = default;

void ScreenSpaceFakeShadowPass::Initialize(SpriteCommon* spriteCommon) {
    spriteCommon_ = spriteCommon;
    if (!spriteCommon_) {
        lastReason_ = "SpriteCommon is null";
        return;
    }

    blobSprite_ = std::make_unique<Sprite>();
    blobSprite_->Initialize(spriteCommon_);
    blobSprite_->SetTexture("resources/particle/circle2.png");
    blobSprite_->SetColor(shadowColor_);
    blobSprite_->Update();
}

void ScreenSpaceFakeShadowPass::SetTargets(const Player* player, const EnemyManager* enemyManager) {
    player_ = player;
    enemyManager_ = enemyManager;
}

void ScreenSpaceFakeShadowPass::SetRenderTargetSize(float width, float height) {
    renderTargetSize_.x = (std::max)(kMinViewportSize, width);
    renderTargetSize_.y = (std::max)(kMinViewportSize, height);
}

void ScreenSpaceFakeShadowPass::SetDebugMode(bool isDebugMode) {
    isDebugMode_ = isDebugMode;
}

void ScreenSpaceFakeShadowPass::Draw(const Camera* camera) {
    visibleShadowCount_ = 0;
    clippedShadowCount_ = 0;

    if (!enabled_ || !camera || !spriteCommon_ || !blobSprite_) {
        lastReason_ = "Disabled or missing dependency";
        return;
    }
    if (useGameModeOnly_ && isDebugMode_) {
        lastReason_ = "Use Game Mode Only is enabled";
        return;
    }
    if (shadowMode_ == ShadowMode::Off) {
        lastReason_ = "Shadow mode is Off";
        return;
    }

    spriteCommon_->CommonDrawSetting();
    if ((targetMode_ == TargetMode::Player || targetMode_ == TargetMode::Both) && player_) {
        DrawTargetShadow(camera, player_->GetWorldPosition(), 1.0f);
    }

    if ((targetMode_ == TargetMode::Enemy || targetMode_ == TargetMode::Both) && enemyManager_) {
        const std::vector<Enemy*> enemies = enemyManager_->GetActiveEnemies();
        for (const Enemy* enemy : enemies) {
            if (!enemy || enemy->IsDead()) {
                continue;
            }
            DrawTargetShadow(camera, enemy->GetPosition(), enemyShadowScale_);
        }
    }

    lastReason_ = visibleShadowCount_ > 0 ? "Drawn" : "All targets clipped";
}

void ScreenSpaceFakeShadowPass::DrawTargetShadow(const Camera* camera, const Vector3& worldPosition, float targetScale) {
    ProjectedShadow projected = ProjectWorldPosition(camera, worldPosition);
    if (!projected.visible) {
        ++clippedShadowCount_;
        return;
    }

    DrawBlob(projected, targetScale);
    ++visibleShadowCount_;
}

ScreenSpaceFakeShadowPass::ProjectedShadow ScreenSpaceFakeShadowPass::ProjectWorldPosition(
    const Camera* camera,
    const Vector3& worldPosition) const {
    ProjectedShadow result{};
    if (!camera || renderTargetSize_.x <= kMinViewportSize || renderTargetSize_.y <= kMinViewportSize) {
        return result;
    }

    const Vector4 clip = TransformPoint(worldPosition, camera->GetViewProjectionMatrix());
    if (clip.w <= 0.0001f || !std::isfinite(clip.w)) {
        return result;
    }

    const float invW = 1.0f / clip.w;
    const float ndcX = clip.x * invW;
    const float ndcY = clip.y * invW;
    if (!std::isfinite(ndcX) || !std::isfinite(ndcY)) {
        return result;
    }
    if (ndcX < -1.0f - kClipMargin || ndcX > 1.0f + kClipMargin ||
        ndcY < -1.0f - kClipMargin || ndcY > 1.0f + kClipMargin) {
        return result;
    }

    result.center.x = (ndcX * 0.5f + 0.5f) * renderTargetSize_.x + shadowOffset_.x;
    result.center.y = (-ndcY * 0.5f + 0.5f) * renderTargetSize_.y + shadowOffset_.y;
    result.alpha = 1.0f;
    if (fadeNearScreenEdge_) {
        const float distanceToEdge = (std::min)({
            result.center.x,
            result.center.y,
            renderTargetSize_.x - result.center.x,
            renderTargetSize_.y - result.center.y
            });
        result.alpha = SmoothStep((distanceToEdge + edgeFadePixels_) / (edgeFadePixels_ * 2.0f));
    }
    result.alpha = std::clamp(result.alpha, 0.0f, 1.0f);
    result.visible = result.alpha > 0.001f;
    return result;
}

void ScreenSpaceFakeShadowPass::DrawBlob(const ProjectedShadow& projected, float targetScale) {
    if (!blobSprite_ || !projected.visible) {
        return;
    }

    const float softnessScale = 1.0f + std::clamp(shadowSoftness_, 0.0f, 2.0f) * 0.35f;
    const Vector2 size = {
        shadowBaseSize_.x * shadowScale_ * targetScale * softnessScale,
        shadowBaseSize_.y * shadowScale_ * targetScale * softnessScale,
    };
    const float alpha = std::clamp(shadowColor_.w * shadowAlpha_ * projected.alpha, 0.0f, 1.0f);
    blobSprite_->SetPosition({ projected.center.x - size.x * 0.5f, projected.center.y - size.y * 0.5f });
    blobSprite_->SetSize(size);
    blobSprite_->SetColor({ shadowColor_.x, shadowColor_.y, shadowColor_.z, alpha });
    blobSprite_->Update();
    blobSprite_->Draw();

    lastPlayerShadowPosition_ = projected.center;
    lastPlayerShadowAlpha_ = alpha;
}

const char* ScreenSpaceFakeShadowPass::GetModeName() const {
    switch (shadowMode_) {
    case ShadowMode::Off:
        return "Off";
    case ShadowMode::ScreenSpaceSilhouette:
        return "ScreenSpaceSilhouette (Blob fallback)";
    case ShadowMode::DebugArtifactPreview:
        return "DebugArtifactPreview (Blob fallback)";
    case ShadowMode::Blob:
    default:
        return "Blob";
    }
}

const char* ScreenSpaceFakeShadowPass::GetTargetName() const {
    switch (targetMode_) {
    case TargetMode::Player:
        return "Player";
    case TargetMode::Enemy:
        return "Enemy";
    case TargetMode::Both:
    default:
        return "Both";
    }
}

void ScreenSpaceFakeShadowPass::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(420.0f, 500.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("スクリーンスペース簡易影 (Screen Space Fake Shadow Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Fake Shadowを有効化 (Enable Fake Shadow)", &enabled_);
    int modeIndex = static_cast<int>(shadowMode_);
    const char* modeItems[] = { "Off", "Blob", "ScreenSpaceSilhouette", "DebugArtifactPreview" };
    if (ImGui::Combo("影モード (Shadow Mode)", &modeIndex, modeItems, IM_ARRAYSIZE(modeItems))) {
        shadowMode_ = static_cast<ShadowMode>(modeIndex);
    }
    int targetIndex = static_cast<int>(targetMode_);
    const char* targetItems[] = { "Player", "Enemy", "Both" };
    if (ImGui::Combo("対象 (Target)", &targetIndex, targetItems, IM_ARRAYSIZE(targetItems))) {
        targetMode_ = static_cast<TargetMode>(targetIndex);
    }

    ImGui::DragFloat2("影オフセット (Shadow Offset X/Y)", &shadowOffset_.x, 0.5f, -300.0f, 300.0f, "%.1f");
    ImGui::DragFloat2("影サイズ (Shadow Base Size)", &shadowBaseSize_.x, 1.0f, 1.0f, 600.0f, "%.1f");
    ImGui::SliderFloat("影スケール (Shadow Scale)", &shadowScale_, 0.1f, 3.0f, "%.2f");
    ImGui::SliderFloat("敵影スケール (Enemy Shadow Scale)", &enemyShadowScale_, 0.1f, 2.0f, "%.2f");
    ImGui::SliderFloat("影アルファ (Shadow Alpha)", &shadowAlpha_, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("ぼかし風の広がり (Shadow Softness)", &shadowSoftness_, 0.0f, 2.0f, "%.2f");
    ImGui::ColorEdit4("影色 (Shadow Color)", &shadowColor_.x);
    ImGui::Checkbox("画面端フェード (Fade Near Screen Edge)", &fadeNearScreenEdge_);
    ImGui::DragFloat("画面端フェード幅 (Edge Fade Pixels)", &edgeFadePixels_, 1.0f, 1.0f, 400.0f, "%.1f");
    ImGui::Checkbox("Game Modeだけで使う (Use Game Mode Only)", &useGameModeOnly_);
    ImGui::Checkbox("影デバッグ表示 (Show Shadow Debug)", &showShadowDebug_);

    ImGui::SeparatorText("状態 (Status)");
    ImGui::Text("Mode: %s", GetModeName());
    ImGui::Text("Target: %s", GetTargetName());
    ImGui::Text("Visible / Clipped: %d / %d", visibleShadowCount_, clippedShadowCount_);
    ImGui::Text("Render Target Size: %.1f x %.1f", renderTargetSize_.x, renderTargetSize_.y);
    ImGui::Text("Last Shadow Pos: %.1f, %.1f", lastPlayerShadowPosition_.x, lastPlayerShadowPosition_.y);
    ImGui::Text("Last Shadow Alpha: %.3f", lastPlayerShadowAlpha_);
    ImGui::TextWrapped("Last Reason: %s", lastReason_);
    ImGui::TextWrapped("現在はBlob方式です。Silhouette/Artifact Previewは将来の低解像度マスク+Blur用の比較モードとして残しています。");
    ImGui::End();
#endif
}
