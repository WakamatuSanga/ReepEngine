#pragma once
#include "Engine/math/Matrix4x4.h"

#include <memory>

class Camera;
class EnemyManager;
class Player;
class Sprite;
class SpriteCommon;

class ScreenSpaceFakeShadowPass {
public:
    enum class ShadowMode {
        Off,
        Blob,
        ScreenSpaceSilhouette,
        DebugArtifactPreview,
    };

    enum class TargetMode {
        Player,
        Enemy,
        Both,
    };

    ScreenSpaceFakeShadowPass();
    ~ScreenSpaceFakeShadowPass();

    void Initialize(SpriteCommon* spriteCommon);
    void SetTargets(const Player* player, const EnemyManager* enemyManager);
    void SetRenderTargetSize(float width, float height);
    void SetDebugMode(bool isDebugMode);
    void Draw(const Camera* camera);
    void DrawImGui();

    bool IsEnabled() const { return enabled_; }

private:
    struct ProjectedShadow {
        Vector2 center{};
        float alpha = 0.0f;
        bool visible = false;
    };

    ProjectedShadow ProjectWorldPosition(const Camera* camera, const Vector3& worldPosition) const;
    void DrawBlob(const ProjectedShadow& projected, float targetScale);
    void DrawTargetShadow(const Camera* camera, const Vector3& worldPosition, float targetScale);
    const char* GetModeName() const;
    const char* GetTargetName() const;

    SpriteCommon* spriteCommon_ = nullptr;
    const Player* player_ = nullptr;
    const EnemyManager* enemyManager_ = nullptr;
    std::unique_ptr<Sprite> blobSprite_;

    bool enabled_ = true;
    bool useGameModeOnly_ = false;
    bool fadeNearScreenEdge_ = true;
    bool showShadowDebug_ = false;
    bool isDebugMode_ = true;
    ShadowMode shadowMode_ = ShadowMode::Blob;
    TargetMode targetMode_ = TargetMode::Player;
    Vector2 renderTargetSize_{ 1280.0f, 720.0f };
    Vector2 shadowOffset_{ 22.0f, 28.0f };
    Vector2 shadowBaseSize_{ 150.0f, 58.0f };
    Vector4 shadowColor_{ 0.02f, 0.04f, 0.08f, 0.38f };
    float shadowScale_ = 1.0f;
    float enemyShadowScale_ = 0.82f;
    float shadowAlpha_ = 0.55f;
    float shadowSoftness_ = 0.65f;
    float edgeFadePixels_ = 110.0f;
    int visibleShadowCount_ = 0;
    int clippedShadowCount_ = 0;
    Vector2 lastPlayerShadowPosition_{};
    float lastPlayerShadowAlpha_ = 0.0f;
    const char* lastReason_ = "Not drawn yet";
};
