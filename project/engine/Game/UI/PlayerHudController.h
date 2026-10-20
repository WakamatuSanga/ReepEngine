#pragma once
#include "Engine/math/Matrix4x4.h"
#include <array>
#include <memory>
#include <string>
#include <unordered_map>
class BoostController;
class PlayerBulletManager;
class PlayerDamageFeedbackController;
class StageCombatState;
class StageStartSequenceController;
class EnemyWaveManager;
class RailShooterCameraRig;
class Sprite;
class SpriteCommon;

// Read-only presentation. Score, ammunition, HP, progress and cooldown stay with their owners.
class PlayerHudController {
public:
    PlayerHudController();
    ~PlayerHudController();
    void Initialize(SpriteCommon*, PlayerDamageFeedbackController*, PlayerBulletManager*, BoostController*);
    void BindState(StageCombatState*, EnemyWaveManager*, RailShooterCameraRig*);
    void SetStartSequence(const StageStartSequenceController* sequence) { sequence_ = sequence; }
    void Finalize();
    void Reset();
    void SetGameModeActive(bool active) { gameMode_ = active; }
    void Update(float unscaledDeltaTime);
    void Draw();
    void DrawInRect(float left, float top, float width, float height);
    void DrawImGui();
private:
    struct Part { uint32_t texture = 0; Vector2 position{}; Vector2 size{}; };
    void DrawPart(const std::string&, int group, float alpha, float fill = 1.0f);
    void DrawItem(uint32_t texture, Vector2 position, Vector2 size, int group, float alpha,
        Vector2 uvOrigin = {}, Vector2 uvSize = {}, bool frame = false);
    void DrawNumber(const std::string&, Vector2 baseline, bool smallDigits, int group, bool right = false);
    void DrawGroup(int group);
    Vector2 Map(Vector2 position, int group) const;
    SpriteCommon* sprites_ = nullptr;
    PlayerDamageFeedbackController* damage_ = nullptr;
    PlayerBulletManager* bullets_ = nullptr;
    BoostController* boost_ = nullptr;
    StageCombatState* combat_ = nullptr;
    EnemyWaveManager* waves_ = nullptr;
    RailShooterCameraRig* rig_ = nullptr;
    const StageStartSequenceController* sequence_ = nullptr;
    std::unordered_map<std::string, Part> parts_;
    // Every submitted draw owns its constants/vertices until the existing frame fence.
    std::array<std::unique_ptr<Sprite>, 96> pool_;
    size_t used_ = 0;
    uint32_t framesTexture_ = 0;
    uint32_t whiteTexture_ = 0;
    bool enabled_ = true;
    bool reducedMotion_ = false;
    bool gameMode_ = true;
    float panelScale_ = 1.0f;
    float frameAlpha_ = 1.0f;
    float textAlpha_ = 1.0f;
    float topReveal_ = 0;
    float bottomReveal_ = 0;
    float ammoPulse_ = 0;
    int previousAmmo_ = 5;
    float left_ = 0, top_ = 0, width_ = 1280, height_ = 720, scale_ = 1;
    float groupAlpha_ = 1;
    float jitter_ = 0;
};
