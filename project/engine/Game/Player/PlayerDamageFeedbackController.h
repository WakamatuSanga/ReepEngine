#pragma once

#include "Engine/math/Matrix4x4.h"
#include <cstdint>
#include <memory>

class CombatEffectController;
class Player;
class Sprite;
class SpriteCommon;

class PlayerDamageFeedbackController {
public:
    PlayerDamageFeedbackController();
    ~PlayerDamageFeedbackController();

    void Initialize(SpriteCommon* spriteCommon, Player* player, CombatEffectController* combatEffectController);
    void Finalize();
    void Update(float unscaledDeltaTime);
    void Draw();
    void DrawImGui();

    bool ApplyDamage(const Vector3& position, int damage = 1, bool ignoreInvincible = false);
    bool IsInvincible() const;
    bool IsDead() const { return hp_ <= 0; }
    int GetHp() const { return hp_; }
    int GetMaxHp() const { return maxHp_; }

private:
    void ResetHp();
    void ApplyBlinkAlpha(float alpha);
    void TriggerDamageVisuals(const Vector3& position);

    SpriteCommon* spriteCommon_ = nullptr;
    Player* player_ = nullptr;
    CombatEffectController* combatEffectController_ = nullptr;
    std::unique_ptr<Sprite> flashSprite_;

    bool enableDamageFeedback_ = true;
    bool damageSparkEnabled_ = true;
    bool debugForceInvincible_ = false;
    int maxHp_ = 3;
    int hp_ = 3;
    float invincibleDuration_ = 1.0f;
    float invincibleTimer_ = 0.0f;
    float blinkRate_ = 12.0f;
    float blinkTimer_ = 0.0f;
    float damageFlashDuration_ = 0.25f;
    float damageFlashTimer_ = 0.0f;
    float damageFlashAlpha_ = 0.28f;
    Vector3 lastDamagePosition_{ 0.0f, 0.0f, 0.0f };
    uint64_t damageFeedbackCount_ = 0;
    const char* lastResult_ = "Not damaged";
};
