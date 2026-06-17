#include "CombatEffectController.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Graphics/Effect/PrimitiveEffectSystem.h"

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

CombatEffectController::CombatEffectController() = default;

CombatEffectController::~CombatEffectController() = default;

void CombatEffectController::Initialize(PrimitiveEffectSystem* primitiveEffectSystem, Player* player) {
    primitiveEffectSystem_ = primitiveEffectSystem;
    player_ = player;
    lastEffectResult_ = primitiveEffectSystem_ ? "Initialized" : "PrimitiveEffectSystem missing";
}

void CombatEffectController::Finalize() {
    primitiveEffectSystem_ = nullptr;
    player_ = nullptr;
}

void CombatEffectController::Update([[maybe_unused]] float deltaTime) {
}

void CombatEffectController::DrawImGui() {
#ifdef _DEBUG
    ImGui::SetNextWindowSize(ImVec2(380.0f, 320.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("戦闘エフェクト確認 (Combat Effect Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("戦闘エフェクト有効 (Enable Combat Effects)", &enableCombatEffects_);
    ImGui::Checkbox("再生時にPrimitive Effectsを表示 (Ensure Primitive Effects Visible)", &ensurePrimitiveEffectsVisible_);
    ImGui::TextWrapped("Last Effect Type: %s", lastEffectType_.c_str());
    ImGui::Text("Last Effect Position: %.2f, %.2f, %.2f",
        lastEffectPosition_.x,
        lastEffectPosition_.y,
        lastEffectPosition_.z);
    ImGui::Text("Player Hit Effect Count: %llu", static_cast<unsigned long long>(playerHitEffectCount_));
    ImGui::Text("Enemy Hit Effect Count: %llu", static_cast<unsigned long long>(enemyHitEffectCount_));
    ImGui::Text("Player Death Effect Count: %llu", static_cast<unsigned long long>(playerDeathEffectCount_));
    ImGui::Text("Enemy Death Effect Count: %llu", static_cast<unsigned long long>(enemyDeathEffectCount_));
    ImGui::TextWrapped("Last Effect Result: %s", lastEffectResult_.c_str());

    Vector3 testPosition = { 0.0f, 0.0f, 0.0f };
    if (player_) {
        testPosition = player_->GetWorldPosition();
    }
    if (ImGui::Button("Play Test Player Death Explosion")) {
        PlayPlayerDeathExplosion(testPosition);
    }
    if (ImGui::Button("Play Test Enemy Death Explosion")) {
        PlayEnemyDeathExplosion(testPosition);
    }
    if (ImGui::Button("Play Test Hit At Player")) {
        PlayEnemyBulletHitPlayer(testPosition);
    }
    ImGui::End();
#endif
}

void CombatEffectController::PlayPlayerBulletHitEnemy(const Vector3& position) {
    ++enemyHitEffectCount_;
    PlayHitRing(position, "PlayerBulletHitEnemy");
}

void CombatEffectController::PlayEnemyBulletHitPlayer(const Vector3& position) {
    ++playerHitEffectCount_;
    PlayHitRing(position, "EnemyBulletHitPlayer");
}

void CombatEffectController::PlayPlayerDeathExplosion(const Vector3& position) {
    ++playerDeathEffectCount_;
    PlayExplosion(position, "PlayerDeathExplosion");
}

void CombatEffectController::PlayEnemyDeathExplosion(const Vector3& position) {
    ++enemyDeathEffectCount_;
    PlayExplosion(position, "EnemyDeathExplosion");
}

void CombatEffectController::PlayHitRing(const Vector3& position, const char* effectType) {
    if (!enableCombatEffects_) {
        RecordEffect(effectType, position, "Combat effects disabled");
        return;
    }
    if (!primitiveEffectSystem_) {
        RecordEffect(effectType, position, "PrimitiveEffectSystem missing");
        return;
    }

    if (ensurePrimitiveEffectsVisible_) {
        primitiveEffectSystem_->SetVisible(true);
    }
    primitiveEffectSystem_->PlayHitEffectAt(position);
    primitiveEffectSystem_->PlayRingEffectAt(position);
    RecordEffect(effectType, position, "Played Hit + Ring");
}

void CombatEffectController::PlayExplosion(const Vector3& position, const char* effectType) {
    if (!enableCombatEffects_) {
        RecordEffect(effectType, position, "Combat effects disabled");
        return;
    }
    if (!primitiveEffectSystem_) {
        RecordEffect(effectType, position, "PrimitiveEffectSystem missing");
        return;
    }

    if (ensurePrimitiveEffectsVisible_) {
        primitiveEffectSystem_->SetVisible(true);
    }
    primitiveEffectSystem_->PlayHitEffectAt(position);
    primitiveEffectSystem_->PlayRingEffectAt(position);
    primitiveEffectSystem_->PlayCylinderEffectAt(position);
    RecordEffect(effectType, position, "Played Hit + Ring + Cylinder");
}

void CombatEffectController::RecordEffect(const char* effectType, const Vector3& position, const char* result) {
    lastEffectType_ = effectType ? effectType : "Unknown";
    lastEffectPosition_ = position;
    lastEffectResult_ = result ? result : "Unknown";
}
