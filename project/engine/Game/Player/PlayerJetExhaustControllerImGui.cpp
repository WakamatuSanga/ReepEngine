#include "PlayerJetExhaustController.h"

#include "PlayerJetExhaustBeamCore.h"
#include "Engine/Graphics/Particle/GpuParticleSystem.h"
#include "Engine/Graphics/Particle/GpuParticleTypes.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>

void PlayerJetExhaustController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(500.0f, 720.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("プレイヤージェット排気確認 (Player Jet Exhaust Debug)")) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("ジェット排気を表示 (Enable Jet Exhaust)", &enableJetExhaust_);
    ImGui::Checkbox("Player死亡中は隠す (Hide When Player Dead)", &hideWhenPlayerDead_);
    ImGui::Checkbox("排気デバッグ表示 (Show Exhaust Debug)", &showDebugVisuals_);
    ImGui::TextWrapped("Preset: %s", presetPath_.c_str());
    ImGui::TextWrapped("Status: %s", loadStatus_.c_str());
    if (ImGui::Button("排気プリセット再読込 (Reload Jet Preset)")) {
        LoadPreset();
    }
    ImGui::SameLine();
    if (ImGui::Button("軽量プリセット (Lightweight Preset)")) {
        ApplyLightweightPreset();
    }
    if (ImGui::Button("現行チューニング値を適用 (Apply Current Tuned Preset)")) {
        ApplyCurrentTunedPreset();
    }

    ImGui::SeparatorText("ノズル位置 (Nozzle Offset)");
    ImGui::DragFloat("後方オフセット (Back Offset)", &nozzleBackOffset_, 0.01f, -3.0f, 3.0f, "%.2f");
    ImGui::DragFloat("上下オフセット (Up Offset)", &nozzleUpOffset_, 0.01f, -2.0f, 2.0f, "%.2f");
    ImGui::DragFloat("左右オフセット (Side Offset)", &nozzleSideOffset_, 0.01f, -2.0f, 2.0f, "%.2f");
    ImGui::Checkbox("排気方向を反転 (Invert Exhaust Direction)", &invertExhaustDirection_);
    ImGui::Text("Nozzle: %.2f, %.2f, %.2f", currentNozzlePosition_.x, currentNozzlePosition_.y, currentNozzlePosition_.z);
    ImGui::Text("Direction: %.2f, %.2f, %.2f", currentExhaustDirection_.x, currentExhaustDirection_.y, currentExhaustDirection_.z);
    if (beamCore_) {
        beamCore_->DrawImGui();
    }

    ImGui::SeparatorText("Jet Exhaust Visibility");
    ImGui::Checkbox("雲の後に排気を描画 (Draw Jet Exhaust After Cloud)", &drawAfterCloud_);
    ImGui::Checkbox("AfterCloud Debug表示 (Show After Cloud Layer Debug)", &showAfterCloudLayerDebug_);
    ImGui::DragFloat("After Cloud Alpha Scale", &afterCloudAlphaScale_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("After Cloud Brightness Scale", &afterCloudBrightnessScale_, 0.01f, 0.0f, 2.0f, "%.2f");
    if (showAfterCloudLayerDebug_) {
        ImGui::Text("Current Layer: %s", drawAfterCloud_ ? "After Cloud" : "Before Cloud");
    }

    ImGui::SeparatorText("GPUパーティクルプール (Dead / Free List)");
    ImGui::TextWrapped("排気Particleは既存GPU ParticleのDead List / Free Listを使って再利用します。高Emission時は上限を超えた発生を捨て、穴や上書きを防ぎます。");
    ImGui::Checkbox("Dead Listを使う (Use Dead List)", &useDeadList_);
    ImGui::Checkbox("死亡Particleを自動再利用 (Auto Reuse Dead Particles)", &autoReuseDeadParticles_);
    ImGui::Checkbox("未使用リストを生成 (Generate Unused List)", &generateUnusedList_);
    ImGui::Checkbox("GPUカウンタ自動読込 (Auto Readback Counters)", &autoReadbackPoolCounters_);
    ImGui::Checkbox("プール警告を表示 (Show Pool Warning)", &showPoolWarning_);
    int maxActive = static_cast<int>(maxActiveExhaustParticles_);
    if (ImGui::DragInt("最大Active排気数 (Max Active Exhaust Particles)", &maxActive, 1.0f, 1, static_cast<int>(GpuParticle::kParticleCount))) {
        maxActiveExhaustParticles_ = static_cast<uint32_t>(std::clamp(maxActive, 1, static_cast<int>(GpuParticle::kParticleCount)));
    }
    int maxEmit = static_cast<int>(maxEmitPerFrame_);
    if (ImGui::DragInt("最大発生数/Frame (Max Emit Per Frame)", &maxEmit, 1.0f, 1, static_cast<int>(GpuParticle::kParticleCount))) {
        maxEmitPerFrame_ = static_cast<uint32_t>(std::clamp(maxEmit, 1, static_cast<int>(GpuParticle::kParticleCount)));
    }
    int maxOuter = static_cast<int>(maxOuterEmitPerFrame_);
    if (ImGui::DragInt("Outer最大発生数/Frame (Max Outer Emit Per Frame)", &maxOuter, 1.0f, 1, 256)) {
        maxOuterEmitPerFrame_ = static_cast<uint32_t>(std::clamp(maxOuter, 1, 256));
    }
    int maxCore = static_cast<int>(maxCoreEmitPerFrame_);
    if (ImGui::DragInt("Core最大発生数/Frame (Max Core Emit Per Frame)", &maxCore, 1.0f, 1, 128)) {
        maxCoreEmitPerFrame_ = static_cast<uint32_t>(std::clamp(maxCore, 1, 128));
    }

    const GpuParticle::State* state = particleSystem_ ? &particleSystem_->GetState() : nullptr;
    const uint32_t poolCapacity = particleSystem_ ? particleSystem_->GetPoolCapacity() : 0u;
    const uint32_t activeCount = state ? state->activeCountEstimate : 0u;
    const uint32_t deadCount = state ? state->deadListCountEstimate : 0u;
    const uint32_t freeCount = state ? state->freeListRemainingEstimate : 0u;
    const uint32_t requestedEmit = state ? state->lastRequestedEmitCount : 0u;
    const uint32_t actualEmit = state ? state->lastActualEmitCount : 0u;
    const uint32_t skippedEmit = state ? state->lastSkippedEmitCount : 0u;
    const uint32_t reusedCount = state ? state->lastReusedCount : 0u;
    const float poolUsage = maxActiveExhaustParticles_ > 0 ? static_cast<float>(activeCount) / static_cast<float>(maxActiveExhaustParticles_) : 0.0f;
    ImGui::Text("Pool Capacity: %u", poolCapacity);
    ImGui::Text("Active Count: %u", activeCount);
    ImGui::Text("Dead Count: %u", deadCount);
    ImGui::Text("Free Count: %u", freeCount);
    ImGui::Text("Unused Count: %u", freeCount);
    ImGui::Text("Requested / Actual / Skipped Emit: %u / %u / %u", requestedEmit, actualEmit, skippedEmit);
    ImGui::Text("Reused Count: %u", reusedCount);
    ImGui::Text("Current Core Spawn Rate: %.1f / sec", currentCoreSpawnRate_);
    ImGui::Text("Current Outer Spawn Rate: %.1f / sec", currentOuterSpawnRate_);
    ImGui::Text("Current Active Particle Estimate: %u", activeCount);
    ImGui::Text("Current Pool Usage: %.1f%%", poolUsage * 100.0f);
    if (state && state->isCounterReadbackValid) {
        ImGui::Text("Free Actual / Dead Actual: %u / %u", state->actualFreeListCount, state->actualDeadListCount);
    }
    if (showPoolWarning_ && (skippedEmit > 0 || activeCount >= maxActiveExhaustParticles_ || freeCount == 0)) {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.10f, 1.0f), "警告: 排気Particleプールが詰まり気味です。Spawn Rateを下げるか上限を上げてください。");
    }
    if (ImGui::Button("排気Particle Poolをリセット (Reset Exhaust Particle Pool)")) {
        if (particleSystem_) {
            particleSystem_->ResetParticlePool();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("排気Particleを全消去 (Force Kill Exhaust Particles)")) {
        if (particleSystem_) {
            particleSystem_->ResetParticlePool();
        }
    }
    if (ImGui::Button("GPUカウンタを読込 (Readback Counters)")) {
        if (particleSystem_) {
            particleSystem_->RequestCounterReadback();
        }
    }

    ImGui::SeparatorText("炎パラメータ (Flame Parameters)");
    ImGui::DragFloat("円錐角度 (Cone Angle)", &coneAngleDegrees_, 0.1f, 1.0f, 70.0f, "%.1f deg");
    ImGui::DragFloat("Emitter Cone Height", &emitterConeHeight_, 0.005f, 0.001f, 1.0f, "%.3f");
    ImGui::BeginDisabled();
    ImGui::DragFloat("Core Spawn Rate", &spawnRate_, 1.0f, 0.0f, 2000.0f, "%.0f/sec");
    ImGui::DragFloat("Core Speed", &exhaustSpeed_, 0.1f, 0.0f, 80.0f, "%.1f");
    ImGui::DragFloat("Core Lifetime Min", &lifeTimeMin_, 0.005f, 0.02f, 2.0f, "%.3f");
    ImGui::DragFloat("Core Lifetime Max", &lifeTimeMax_, 0.005f, 0.02f, 2.0f, "%.3f");
    ImGui::DragFloat("Core Start Size", &coreStartSize_, 0.001f, 0.001f, 1.0f, "%.3f");
    ImGui::DragFloat("Core End Size", &coreEndSize_, 0.001f, 0.001f, 1.0f, "%.3f");
    ImGui::EndDisabled();
    ImGui::DragFloat("Outer Spawn Rate", &outerSpawnRate_, 1.0f, 0.0f, 2500.0f, "%.0f/sec");
    ImGui::DragFloat("Outer Speed", &outerExhaustSpeed_, 0.1f, 0.0f, 80.0f, "%.1f");
    ImGui::DragFloat("Outer Lifetime Min", &outerLifeTimeMin_, 0.005f, 0.01f, 2.0f, "%.3f");
    ImGui::DragFloat("Outer Lifetime Max", &outerLifeTimeMax_, 0.005f, 0.01f, 2.0f, "%.3f");
    ImGui::DragFloat("Outer Start Size", &outerStartSize_, 0.001f, 0.001f, 1.0f, "%.3f");
    ImGui::DragFloat("Outer End Size", &outerEndSize_, 0.001f, 0.001f, 1.0f, "%.3f");
    ImGui::DragFloat("Outer Particle Scale", &outerParticleScale_, 0.01f, 0.0f, 3.0f, "%.2f");
    ImGui::DragFloat("Outer Particle Alpha", &outerParticleAlphaScale_, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("明るさ (Brightness)", &brightness_, 0.01f, 0.0f, 4.0f, "%.2f");

    ImGui::SeparatorText("Boost連動 (Boost Link)");
    ImGui::DragFloat("Boost Length Multiplier", &boostLengthMultiplier_, 0.01f, 0.1f, 6.0f, "%.2f");
    ImGui::DragFloat("Boost Speed Multiplier", &boostSpeedMultiplier_, 0.01f, 0.1f, 6.0f, "%.2f");
    ImGui::DragFloat("Boost Spawn Rate Multiplier", &boostSpawnRateMultiplier_, 0.01f, 0.1f, 6.0f, "%.2f");
    ImGui::DragFloat("Boost Brightness Multiplier", &boostBrightnessMultiplier_, 0.01f, 0.1f, 6.0f, "%.2f");
    ImGui::Text("Boost Power: %.2f", smoothedBoostPower_);
    ImGui::Text("Current Length / Speed / Rate / Brightness: %.2f / %.2f / %.2f / %.2f",
        currentLengthMultiplier_, currentSpeedMultiplier_, currentSpawnRateMultiplier_, currentBrightness_);

    ImGui::SeparatorText("外力設定 (Flow / Influence)");
    ImGui::TextUnformatted("排気はデフォルトで風圧フィールド・平面衝突・Rail相対流れの影響を受けません。");
    ImGui::Checkbox("Rail相対流れを受ける (Affected By Rail Flow)", &affectedByRailFlow_);
    ImGui::DragFloat("Rail Flow Scale", &railFlowScale_, 0.01f, 0.0f, 5.0f, "%.2f");
    ImGui::Text("Update Count: %llu", static_cast<unsigned long long>(updateCount_));
    ImGui::End();
#endif
}
