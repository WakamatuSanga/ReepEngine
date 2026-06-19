#include "CombatEffectController.h"

#include "GpuParticleEffectPlayer.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Effect/PrimitiveEffectSystem.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <cstring>

namespace {
    bool IsDeathEffectName(const char* effectType) {
        return effectType && std::string(effectType).find("Death") != std::string::npos;
    }

    bool IsEnemyDeathEffectName(const char* effectType) {
        return effectType && std::string(effectType).find("EnemyDeath") != std::string::npos;
    }

    bool IsPlayerDeathEffectName(const char* effectType) {
        return effectType && std::string(effectType).find("PlayerDeath") != std::string::npos;
    }

#ifdef USE_IMGUI
    void DrawPathInput(const char* label, std::string& value) {
        char buffer[260]{};
        strncpy_s(buffer, sizeof(buffer), value.c_str(), _TRUNCATE);
        if (ImGui::InputText(label, buffer, sizeof(buffer))) {
            value = buffer;
        }
    }
#endif
}

CombatEffectController::CombatEffectController() = default;

CombatEffectController::~CombatEffectController() = default;

void CombatEffectController::Initialize(
    PrimitiveEffectSystem* primitiveEffectSystem,
    GpuParticleEffectPlayer* gpuParticleEffectPlayer,
    Player* player) {
    primitiveEffectSystem_ = primitiveEffectSystem;
    gpuParticleEffectPlayer_ = gpuParticleEffectPlayer;
    player_ = player;
    lastEffectResult_ = (primitiveEffectSystem_ || gpuParticleEffectPlayer_) ? "Initialized" : "Effect systems missing";
}

void CombatEffectController::Finalize() {
    primitiveEffectSystem_ = nullptr;
    gpuParticleEffectPlayer_ = nullptr;
    player_ = nullptr;
}

void CombatEffectController::Update(float deltaTime, const Camera* camera) {
    if (gpuParticleEffectPlayer_) {
        gpuParticleEffectPlayer_->Update(deltaTime, camera);
    }
}

void CombatEffectController::Draw() {
    if (gpuParticleEffectPlayer_ && enableCombatEffects_ && enableGpuParticleEffects_) {
        gpuParticleEffectPlayer_->Draw();
    }
}

void CombatEffectController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(440.0f, 560.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("戦闘エフェクト確認 (Combat Effect Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("戦闘エフェクト有効 (Enable Combat Effects)", &enableCombatEffects_);
    ImGui::Checkbox("再生時にPrimitive Effectsを表示 (Ensure Primitive Effects Visible)", &ensurePrimitiveEffectsVisible_);

    ImGui::SeparatorText("Primitive");
    ImGui::Checkbox("Primitive Ringを使う (Enable Primitive Ring)", &enablePrimitiveRing_);
    ImGui::Checkbox("Primitive Hitを使う (Enable Primitive Hit)", &enablePrimitiveHit_);
    ImGui::Checkbox("Primitive Cylinderを使う (Enable Primitive Cylinder)", &enablePrimitiveCylinder_);
    ImGui::Checkbox("敵ヒットRingを使う (Enemy Hit Ring Enabled)", &enableEnemyHitRing_);
    ImGui::DragFloat("敵ヒットRing倍率 (Enemy Hit Ring Scale)", &enemyHitRingScale_, 0.01f, 0.05f, 3.0f, "%.2f");
    ImGui::DragFloat("敵ヒットRing透明度 (Enemy Hit Ring Alpha)", &enemyHitRingAlpha_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("敵撃破Effect倍率 (Enemy Death Effect Scale)", &enemyDeathEffectScale_, 0.01f, 0.1f, 5.0f, "%.2f");
    ImGui::DragFloat("PlayerヒットRing倍率 (Player Hit Ring Scale)", &playerHitRingScale_, 0.01f, 0.1f, 5.0f, "%.2f");
    ImGui::DragFloat("Player死亡爆発倍率 (Player Death Explosion Scale)", &playerDeathExplosionScale_, 0.01f, 0.1f, 5.0f, "%.2f");

    ImGui::SeparatorText("GPU Particle");
    ImGui::Checkbox("GPU Particle Effectsを使う (Enable GPU Particle Effects)", &enableGpuParticleEffects_);
    ImGui::Checkbox("Hit Sparksを使う (Enable Hit Sparks)", &enableHitSparks_);
    ImGui::Checkbox("Small Explosionを使う (Enable Small Explosion)", &enableSmallExplosion_);
    ImGui::Checkbox("Death Explosionを使う (Enable Death Explosion)", &enableDeathExplosion_);
    DrawPathInput("Hit Sparks JSON Path", hitSparksJsonPath_);
    DrawPathInput("Small Explosion JSON Path", smallExplosionJsonPath_);
    DrawPathInput("Death Explosion JSON Path", deathExplosionJsonPath_);
    if (ImGui::Button("GPU Particle JSONを再読み込み (Reload GPU Particle Effects)")) {
        bool reloaded = gpuParticleEffectPlayer_ != nullptr;
        if (gpuParticleEffectPlayer_) {
            reloaded &= gpuParticleEffectPlayer_->ReloadEffect(hitSparksJsonPath_);
            reloaded &= gpuParticleEffectPlayer_->ReloadEffect(smallExplosionJsonPath_);
            reloaded &= gpuParticleEffectPlayer_->ReloadEffect(deathExplosionJsonPath_);
        }
        lastEffectResult_ = reloaded ? "Reloaded GPU particle effects" : "Failed to reload one or more GPU particle effects";
    }

    ImGui::SeparatorText("Status");
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
    if (ImGui::Button("Test Hit Sparks At Player")) {
        PlayGpuEffectAt(hitSparksJsonPath_, testPosition, "TestHitSparks");
    }
    if (ImGui::Button("Test Small Explosion At Player")) {
        PlayGpuEffectAt(smallExplosionJsonPath_, testPosition, "TestSmallExplosion");
    }
    if (ImGui::Button("Test Death Explosion At Player")) {
        PlayGpuEffectAt(deathExplosionJsonPath_, testPosition, "TestDeathExplosion");
    }
    if (gpuParticleEffectPlayer_) {
        gpuParticleEffectPlayer_->DrawImGui();
    }

    ImGui::End();
#endif
}

void CombatEffectController::PlayPlayerBulletHitEnemy(const Vector3& position) {
    PlayPlayerBulletHitEnemy(position, false);
}

void CombatEffectController::PlayPlayerBulletHitEnemy(const Vector3& position, bool lethalHit) {
    ++enemyHitEffectCount_;
    PlayHitRing(
        position,
        lethalHit ? "PlayerBulletHitEnemyLethal" : "PlayerBulletHitEnemy",
        lethalHit ? enemyDeathEffectScale_ : enemyHitRingScale_,
        lethalHit ? 1.0f : enemyHitRingAlpha_,
        lethalHit ? true : enableEnemyHitRing_);
}

void CombatEffectController::PlayEnemyBulletHitPlayer(const Vector3& position) {
    ++playerHitEffectCount_;
    PlayHitRing(position, "EnemyBulletHitPlayer", playerHitRingScale_, 1.0f, true);
}

void CombatEffectController::PlayPlayerDeathExplosion(const Vector3& position) {
    ++playerDeathEffectCount_;
    PlayExplosion(position, "PlayerDeathExplosion");
}

void CombatEffectController::PlayEnemyDeathExplosion(const Vector3& position) {
    ++enemyDeathEffectCount_;
    PlayExplosion(position, "EnemyDeathExplosion");
}

void CombatEffectController::PlayHitRing(
    const Vector3& position,
    const char* effectType,
    float ringScale,
    float ringAlpha,
    bool ringEnabled) {
    if (!enableCombatEffects_) {
        RecordEffect(effectType, position, "Combat effects disabled");
        return;
    }

    bool playedSomething = false;
    std::string result;
    const bool shouldPlayRing = enablePrimitiveRing_ && ringEnabled;
    if (primitiveEffectSystem_ && (shouldPlayRing || enablePrimitiveHit_)) {
        if (ensurePrimitiveEffectsVisible_) {
            primitiveEffectSystem_->SetVisible(true);
        }
        if (enablePrimitiveHit_) {
            primitiveEffectSystem_->PlayHitEffectAt(position);
            result += "Primitive Hit ";
            playedSomething = true;
        }
        if (shouldPlayRing) {
            primitiveEffectSystem_->PlayRingEffectAt(position, ringScale, ringAlpha);
            result += "Primitive Ring ";
            playedSomething = true;
        }
    } else if (!primitiveEffectSystem_ && (shouldPlayRing || enablePrimitiveHit_)) {
        result += "PrimitiveEffectSystem missing ";
    }

    if (enableGpuParticleEffects_ && enableHitSparks_) {
        playedSomething = PlayGpuEffectAt(hitSparksJsonPath_, position, effectType) || playedSomething;
        result += gpuParticleEffectPlayer_ ? "GPU HitSparks requested " : "GPU player missing ";
    }

    RecordEffect(effectType, position, playedSomething ? result.c_str() : "No hit effect enabled");
}

void CombatEffectController::PlayExplosion(const Vector3& position, const char* effectType) {
    if (!enableCombatEffects_) {
        RecordEffect(effectType, position, "Combat effects disabled");
        return;
    }

    bool playedSomething = false;
    std::string result;
    if (primitiveEffectSystem_ && (enablePrimitiveRing_ || enablePrimitiveHit_ || enablePrimitiveCylinder_)) {
        if (ensurePrimitiveEffectsVisible_) {
            primitiveEffectSystem_->SetVisible(true);
        }
        if (enablePrimitiveHit_) {
            primitiveEffectSystem_->PlayHitEffectAt(position);
            result += "Primitive Hit ";
            playedSomething = true;
        }
        if (enablePrimitiveRing_) {
            float ringScale = 1.0f;
            if (IsEnemyDeathEffectName(effectType)) {
                ringScale = enemyDeathEffectScale_;
            } else if (IsPlayerDeathEffectName(effectType)) {
                ringScale = playerDeathExplosionScale_;
            }
            primitiveEffectSystem_->PlayRingEffectAt(position, ringScale, 1.0f);
            result += "Primitive Ring ";
            playedSomething = true;
        }
        if (enablePrimitiveCylinder_) {
            primitiveEffectSystem_->PlayCylinderEffectAt(position);
            result += "Primitive Cylinder ";
            playedSomething = true;
        }
    } else if (!primitiveEffectSystem_ && (enablePrimitiveRing_ || enablePrimitiveHit_ || enablePrimitiveCylinder_)) {
        result += "PrimitiveEffectSystem missing ";
    }

    const bool isDeath = IsDeathEffectName(effectType);
    const bool gpuEnabled = isDeath ? enableDeathExplosion_ : enableSmallExplosion_;
    const std::string& gpuPath = isDeath ? deathExplosionJsonPath_ : smallExplosionJsonPath_;
    if (enableGpuParticleEffects_ && gpuEnabled) {
        playedSomething = PlayGpuEffectAt(gpuPath, position, effectType) || playedSomething;
        result += gpuParticleEffectPlayer_ ? "GPU Explosion requested " : "GPU player missing ";
    }

    RecordEffect(effectType, position, playedSomething ? result.c_str() : "No explosion effect enabled");
}

bool CombatEffectController::PlayGpuEffectAt(const std::string& jsonPath, const Vector3& position, [[maybe_unused]] const char* label) {
    if (!enableGpuParticleEffects_ || !gpuParticleEffectPlayer_) {
        return false;
    }
    return gpuParticleEffectPlayer_->PlayGpuParticleEffectAt(jsonPath, position);
}

void CombatEffectController::RecordEffect(const char* effectType, const Vector3& position, const char* result) {
    lastEffectType_ = effectType ? effectType : "Unknown";
    lastEffectPosition_ = position;
    lastEffectResult_ = result ? result : "Unknown";
}
