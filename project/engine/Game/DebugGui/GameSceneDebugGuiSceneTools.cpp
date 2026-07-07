#include "GameSceneDebugGui.h"
#include "GameScene.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Editor/SkinningEditor.h"
#include "Engine/Graphics/Cloud/CloudVolume.h"
#include "Engine/Graphics/Cloud/VolumetricCloudPass.h"
#include "Engine/Graphics/Effect/PrimitiveEffectSystem.h"
#include "Engine/Graphics/Model/GltfSkinnedModel.h"
#include "Engine/Graphics/Model/Model.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Particle/ParticleManager.h"
#include "Engine/Graphics/Sprite/Sprite.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif
namespace {
    struct RadialBlurPreset {
        const char* name;
        uint32_t enabled;
        float strength;
        std::array<float, 2> center;
        uint32_t sampleCount;
    };
    struct DissolvePreset {
        const char* name;
        uint32_t enabled;
        float threshold;
        float edgeWidth;
        std::array<float, 4> edgeColor;
    };
    struct OutlinePreset {
        const char* name;
        uint32_t outlineMode;
        uint32_t hybridColorSource;
        float hybridColorWeight;
        float hybridDepthWeight;
        float hybridNormalWeight;
        float outlineStrength;
        float outlineThickness;
        float outlineThreshold;
        float outlineSoftness;
        float outlineDepthThreshold;
        float outlineDepthStrength;
        float outlineNormalThreshold;
        float outlineNormalStrength;
        std::array<float, 4> outlineColor;
    };
    constexpr OutlinePreset kOutlinePresets[] = {
        { "Balanced", 4u, 2u, 1.00f, 1.00f, 1.00f, 2.40f, 1.10f, 0.050f, 0.025f, 0.0020f, 10.0f, 0.10f, 4.0f, { 0.02f, 0.02f, 0.02f, 1.0f } },
        { "Color Emphasis", 4u, 2u, 1.35f, 0.45f, 1.00f, 2.80f, 1.15f, 0.055f, 0.025f, 0.0020f, 10.0f, 0.10f, 4.0f, { 0.03f, 0.03f, 0.03f, 1.0f } },
        { "Depth Emphasis", 4u, 1u, 0.55f, 1.45f, 1.00f, 2.60f, 1.20f, 0.060f, 0.030f, 0.0015f, 14.0f, 0.10f, 4.0f, { 0.01f, 0.01f, 0.01f, 1.0f } },
        { "Soft Outline", 4u, 1u, 0.85f, 0.75f, 1.00f, 1.70f, 1.60f, 0.035f, 0.100f, 0.0020f, 10.0f, 0.10f, 4.0f, { 0.08f, 0.08f, 0.08f, 1.0f } },
        { "FinalHybrid Balanced", 6u, 2u, 1.00f, 1.00f, 1.00f, 2.40f, 1.10f, 0.050f, 0.025f, 0.0015f, 14.0f, 0.10f, 4.0f, { 0.02f, 0.02f, 0.02f, 1.0f } },
        { "FinalHybrid Color Emphasis", 6u, 2u, 1.45f, 0.55f, 0.65f, 2.80f, 1.15f, 0.055f, 0.025f, 0.0025f, 10.0f, 0.12f, 3.5f, { 0.02f, 0.02f, 0.02f, 1.0f } },
        { "FinalHybrid Depth Emphasis", 6u, 1u, 0.55f, 1.55f, 0.70f, 2.60f, 1.20f, 0.060f, 0.030f, 0.0010f, 18.0f, 0.12f, 3.5f, { 0.02f, 0.02f, 0.02f, 1.0f } },
        { "FinalHybrid Normal Emphasis", 6u, 2u, 0.65f, 0.75f, 1.65f, 2.50f, 1.15f, 0.050f, 0.025f, 0.0015f, 12.0f, 0.08f, 5.5f, { 0.02f, 0.02f, 0.02f, 1.0f } },
    };
    constexpr RadialBlurPreset kRadialBlurPresets[] = {
        { "Weak", 1u, 0.010f, { 0.5f, 0.5f }, 6u },
        { "Medium", 1u, 0.020f, { 0.5f, 0.5f }, 8u },
        { "Strong", 1u, 0.040f, { 0.5f, 0.5f }, 12u },
        { "Dramatic", 1u, 0.060f, { 0.5f, 0.5f }, 16u },
    };
    constexpr DissolvePreset kDissolvePresets[] = {
        { "Weak", 1u, 0.20f, 0.02f, { 1.0f, 0.6f, 0.2f, 1.0f } },
        { "Medium", 1u, 0.45f, 0.04f, { 1.0f, 0.5f, 0.1f, 1.0f } },
        { "Strong", 1u, 0.65f, 0.06f, { 1.0f, 0.4f, 0.0f, 1.0f } },
        { "Dramatic", 1u, 0.82f, 0.08f, { 0.4f, 0.9f, 1.0f, 1.0f } },
    };
    CloudVolume::Parameters MakeRecommendedCloudParameters() {
        CloudVolume::Parameters parameters{};
        parameters.center = { 0.0f, 4.5f, 8.0f };
        parameters.halfExtents = { 12.0f, 4.5f, 12.0f };
        parameters.density = 0.85f;
        parameters.absorption = 1.15f;
        parameters.windDirection = { 1.0f, 0.0f, 0.25f };
        parameters.windSpeed = 0.20f;
        parameters.sunDirection = { 0.35f, -1.0f, 0.15f };
        parameters.lightAbsorption = 0.75f;
        parameters.color = { 0.98f, 0.99f, 1.00f, 1.00f };
        parameters.noiseScale = 0.12f;
        parameters.detailNoiseScale = 0.42f;
        parameters.detailWeight = 0.20f;
        parameters.edgeFade = 0.30f;
        parameters.ambientLighting = 0.18f;
        parameters.sunIntensity = 1.05f;
        parameters.viewStepCount = 72;
        parameters.lightStepCount = 8;
        return parameters;
    }
}
void GameSceneDebugGui::DrawSceneToolWindows(DirectXCommon* dxCommon, VolumetricCloudPass* volumetricCloudPass) {
#ifdef USE_IMGUI
    if (!scene_ || !dxCommon) {
        return;
    }
    auto particleManager = ParticleManager::GetInstance();
    auto texManager = TextureManager::GetInstance();
    auto& postEffectParams = dxCommon->GetPostEffectParameters();
    auto& hitEffectParams = particleManager->GetHitEffectParams();
    auto& fireballEffectParams = particleManager->GetFireballEffectParams();
    auto& windEffectParams = particleManager->GetWindEffectParams();
    ImGui::SetNextWindowSize(ImVec2(500, 200), ImGuiCond_Once);
    ImGui::Begin("DebugText");
    Vector2 spritePos = scene_->debugSprite_->GetPosition();
    ImGui::DragFloat2("Sprite Pos", &spritePos.x, 1.0f, -9999.0f, 9999.0f, "%4.1f");
    scene_->debugSprite_->SetPosition(spritePos);
    ImGui::End();
    auto DrawEffectParamsUI = [](const char* label, ParticleManager::EffectParams& params) {
        std::string prefix = label;
        int spawnCount = static_cast<int>(params.spawnCount);
        if (ImGui::DragInt((prefix + " Spawn Count").c_str(), &spawnCount, 1.0f, 1, 100)) {
            if (spawnCount < 1) {
                spawnCount = 1;
            }
            params.spawnCount = static_cast<uint32_t>(spawnCount);
        }
        ImGui::DragFloat2((prefix + " Scale X").c_str(), &params.scaleXRange.x, 0.01f, 0.01f, 4.0f, "%.2f");
        ImGui::DragFloat2((prefix + " Scale Y").c_str(), &params.scaleYRange.x, 0.01f, 0.01f, 6.0f, "%.2f");
        ImGui::DragFloat2((prefix + " Lifetime").c_str(), &params.lifeTimeRange.x, 0.01f, 0.01f, 3.0f, "%.2f");
        ImGui::DragFloat2((prefix + " Speed").c_str(), &params.speedRange.x, 0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat2((prefix + " Rotate Z").c_str(), &params.rotateZRange.x, 0.01f, -6.29f, 6.29f, "%.2f");
        Vector3 colorMin = { params.colorRRange.x, params.colorGRange.x, params.colorBRange.x };
        Vector3 colorMax = { params.colorRRange.y, params.colorGRange.y, params.colorBRange.y };
        if (ImGui::ColorEdit3((prefix + " Color Min").c_str(), &colorMin.x)) {
            params.colorRRange.x = colorMin.x;
            params.colorGRange.x = colorMin.y;
            params.colorBRange.x = colorMin.z;
        }
        if (ImGui::ColorEdit3((prefix + " Color Max").c_str(), &colorMax.x)) {
            params.colorRRange.y = colorMax.x;
            params.colorGRange.y = colorMax.y;
            params.colorBRange.y = colorMax.z;
        }
        };
    ImGui::Begin("Game Scene Menu");
    ImGui::Text("Press [T] to return to Title");
    ImGui::SeparatorText("Camera");
    Vector3 camTrans = scene_->camera_->GetTranslate();
    if (ImGui::DragFloat3("Cam Pos", &camTrans.x, 0.1f)) scene_->camera_->SetTranslate(camTrans);
    ImGui::SeparatorText("Skybox");
    ImGui::Checkbox("Show Skybox", &scene_->isSkyboxVisible_);
    ImGui::Checkbox("Follow Camera", &scene_->isSkyboxFollowCamera_);
    ImGui::DragFloat3("Skybox Scale", &scene_->skyboxScale_.x, 1.0f, 1.0f, 1000.0f, "%.1f");
    ImGui::TextWrapped("DDS: %s", scene_->skyboxTexturePath_.c_str());
    ImGui::Text("TextureIndex: %u", scene_->skyboxTextureIndex_);
    if (scene_->cloudVolume_) {
        auto& cloudParams = scene_->cloudVolume_->GetParameters();
        ImGui::SeparatorText("ボリューメトリック雲 (Volumetric Cloud)");
        if (volumetricCloudPass) {
            bool isCloudPassEnabled = volumetricCloudPass->IsEnabled();
            if (ImGui::Checkbox("雲描画を有効化 (Cloud Pass Enabled)", &isCloudPassEnabled)) {
                volumetricCloudPass->SetEnabled(isCloudPassEnabled);
                scene_->cloudProjectedBounds_ = volumetricCloudPass->BuildProjectedBounds(scene_->camera_.get(), scene_->cloudVolume_.get());
            }
            const char* cloudForceModeNames[] = {
                "None",
                "Force Skip",
                "Force Fullscreen",
                "Force Scissor",
                "Force Max Quality",
                "Force Aggressive LOD"
            };
            int cloudForceMode = static_cast<int>(volumetricCloudPass->GetForceMode());
            if (ImGui::Combo("雲の強制モード (Cloud Force Mode)", &cloudForceMode, cloudForceModeNames, IM_ARRAYSIZE(cloudForceModeNames))) {
                volumetricCloudPass->SetForceMode(static_cast<VolumetricCloudPass::ForceMode>(cloudForceMode));
                scene_->cloudProjectedBounds_ = volumetricCloudPass->BuildProjectedBounds(scene_->camera_.get(), scene_->cloudVolume_.get());
            }
        }
        if (ImGui::Button("雲プリセットを初期化 (Reset Cloud Preset)")) {
            cloudParams = MakeRecommendedCloudParameters();
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("見えやすい初期値へ戻します");
        ImGui::DragFloat3("雲の中心 (Cloud Center)", &cloudParams.center.x, 0.1f);
        ImGui::DragFloat3("雲の半径範囲 (Cloud HalfExtents)", &cloudParams.halfExtents.x, 0.1f, 0.1f, 100.0f, "%.2f");
        ImGui::SliderFloat("雲の濃さ (Cloud Density)", &cloudParams.density, 0.0f, 2.0f, "%.3f");
        ImGui::SliderFloat("雲の吸収量 (Cloud Absorption)", &cloudParams.absorption, 0.01f, 8.0f, "%.2f");
        ImGui::DragFloat3("雲の流れる方向 (Cloud Wind Dir)", &cloudParams.windDirection.x, 0.01f, -1.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("雲の流れる速さ (Cloud Wind Speed)", &cloudParams.windSpeed, 0.0f, 5.0f, "%.2f");
        ImGui::DragFloat3("太陽方向 (Cloud Sun Dir)", &cloudParams.sunDirection.x, 0.01f, -1.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("光の吸収量 (Cloud Light Absorption)", &cloudParams.lightAbsorption, 0.0f, 8.0f, "%.2f");
        ImGui::ColorEdit4("雲の色 (A = density scale)", &cloudParams.color.x);
        ImGui::SliderFloat("雲ノイズ倍率 (Cloud Noise Scale)", &cloudParams.noiseScale, 0.01f, 2.0f, "%.3f");
        ImGui::SliderFloat("細部ノイズ倍率 (Cloud Detail Noise)", &cloudParams.detailNoiseScale, 0.01f, 4.0f, "%.3f");
        ImGui::SliderFloat("細部ノイズの強さ (Cloud Detail Weight)", &cloudParams.detailWeight, 0.0f, 1.5f, "%.2f");
        ImGui::SliderFloat("雲の端のぼかし (Cloud Edge Fade)", &cloudParams.edgeFade, 0.01f, 1.0f, "%.3f");
        ImGui::SliderFloat("環境光の強さ (Cloud Ambient)", &cloudParams.ambientLighting, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("太陽光の強さ (Cloud Sun Intensity)", &cloudParams.sunIntensity, 0.0f, 4.0f, "%.2f");
        int cloudViewStepCount = static_cast<int>(cloudParams.viewStepCount);
        if (ImGui::SliderInt("視線方向ステップ数 (Cloud View Steps)", &cloudViewStepCount, 1, 256)) {
            cloudParams.viewStepCount = static_cast<uint32_t>(cloudViewStepCount);
        }
        int cloudLightStepCount = static_cast<int>(cloudParams.lightStepCount);
        if (ImGui::SliderInt("光方向ステップ数 (Cloud Light Steps)", &cloudLightStepCount, 1, 32)) {
            cloudParams.lightStepCount = static_cast<uint32_t>(cloudLightStepCount);
        }
        if (volumetricCloudPass) {
            const char* cloudDebugViewNames[] = {
                "Final",
                "Alpha only",
                "Density only",
                "Light only"
            };
            int cloudDebugView = static_cast<int>(volumetricCloudPass->GetDebugViewMode());
            if (ImGui::Combo("雲のデバッグ表示 (Cloud Debug View)", &cloudDebugView, cloudDebugViewNames, IM_ARRAYSIZE(cloudDebugViewNames))) {
                volumetricCloudPass->SetDebugViewMode(
                    static_cast<VolumetricCloudPass::DebugViewMode>(cloudDebugView));
            }
            volumetricCloudPass->DrawImGui();
        }
        ImGui::SeparatorText("雲の最適化診断 (Cloud Optimization Debug)");
        auto DrawCloudFlag = [](const char* label, bool value, const ImVec4& trueColor, const ImVec4& falseColor) {
            ImGui::TextUnformatted(label);
            ImGui::SameLine(220.0f);
            ImGui::TextColored(value ? trueColor : falseColor, value ? "true" : "false");
        };
        DrawCloudFlag("雲が表示範囲内 (Cloud Visible)", scene_->cloudProjectedBounds_.isVisible, ImVec4(0.30f, 1.00f, 0.35f, 1.0f), ImVec4(1.00f, 0.35f, 0.35f, 1.0f));
        DrawCloudFlag("雲描画をスキップ (Cloud Pass Skipped)", scene_->cloudProjectedBounds_.isPassSkipped, ImVec4(1.00f, 0.35f, 0.35f, 1.0f), ImVec4(0.30f, 1.00f, 0.35f, 1.0f));
        DrawCloudFlag("全画面へフォールバック (Fullscreen Fallback)", scene_->cloudProjectedBounds_.isFullScreenFallback, ImVec4(1.00f, 0.80f, 0.25f, 1.0f), ImVec4(0.45f, 0.85f, 1.00f, 1.0f));
        DrawCloudFlag("全画面シザー使用 (Use Fullscreen Scissor)", scene_->cloudProjectedBounds_.useFullScreenScissor, ImVec4(1.00f, 0.80f, 0.25f, 1.0f), ImVec4(0.30f, 1.00f, 0.35f, 1.0f));
        DrawCloudFlag("カメラが雲の中 (Camera Inside Cloud)", scene_->cloudProjectedBounds_.isCameraInsideCloud, ImVec4(1.00f, 0.80f, 0.25f, 1.0f), ImVec4(0.45f, 0.85f, 1.00f, 1.0f));
        DrawCloudFlag("Near Planeと交差 (Near Plane Crossing)", scene_->cloudProjectedBounds_.isNearPlaneCrossing, ImVec4(1.00f, 0.80f, 0.25f, 1.0f), ImVec4(0.45f, 0.85f, 1.00f, 1.0f));
        const LONG scissorWidth = scene_->cloudProjectedBounds_.scissorRect.right - scene_->cloudProjectedBounds_.scissorRect.left;
        const LONG scissorHeight = scene_->cloudProjectedBounds_.scissorRect.bottom - scene_->cloudProjectedBounds_.scissorRect.top;
        ImGui::Text("シザー矩形 (Scissor Rect): L=%ld T=%ld R=%ld B=%ld", scene_->cloudProjectedBounds_.scissorRect.left, scene_->cloudProjectedBounds_.scissorRect.top, scene_->cloudProjectedBounds_.scissorRect.right, scene_->cloudProjectedBounds_.scissorRect.bottom);
        ImGui::Text("シザーサイズ (Scissor Size): %ld x %ld", scissorWidth, scissorHeight);
        ImGui::TextColored(
            (scene_->cloudProjectedBounds_.scissorAreaRatio >= 0.90f) ? ImVec4(1.00f, 0.45f, 0.35f, 1.0f) : ImVec4(0.35f, 1.00f, 0.45f, 1.0f),
            "シザー面積比 (Scissor Area Ratio): %.3f (%.1f%%)",
            scene_->cloudProjectedBounds_.scissorAreaRatio,
            scene_->cloudProjectedBounds_.scissorAreaRatio * 100.0f);
        ImGui::Text("現在の視線ステップ倍率 (Current ViewStep Scale): %.3f", scene_->cloudProjectedBounds_.currentViewStepScale);
        ImGui::Text("現在の光ステップ倍率 (Current LightStep Scale): %.3f", scene_->cloudProjectedBounds_.currentLightStepScale);
        ImGui::Text("推定視線ステップ数 (Estimated View Steps): %u", scene_->cloudProjectedBounds_.estimatedViewSteps);
        ImGui::Text("推定光ステップ数 (Estimated Light Steps): %u", scene_->cloudProjectedBounds_.estimatedLightSteps);
    }
    ImGui::SeparatorText("Environment Map");
    ImGui::Checkbox("Reflect Sphere", &scene_->isSphereEnvironmentMapEnabled_);
    ImGui::SliderFloat("Reflect Strength", &scene_->sphereEnvironmentMapIntensity_, 0.0f, 1.0f, "%.2f");
    ImGui::TextWrapped("Cubemap DDS: %s", scene_->skyboxTexturePath_.c_str());
    ImGui::Text("Cubemap TextureIndex: %u", scene_->skyboxTextureIndex_);
    ImGui::SeparatorText("Object Dissolve");
    ImGui::TextWrapped("Applies only to the sphere object for assignment verification.");
    ImGui::Checkbox("Enable Object Dissolve", &scene_->isObjectDissolveEnabled_);
    ImGui::SliderFloat("Object Dissolve Threshold", &scene_->objectDissolveThreshold_, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Object Dissolve Edge Width", &scene_->objectDissolveEdgeWidth_, 0.001f, 0.2f, "%.3f");
    ImGui::SliderFloat("Object Dissolve Edge Glow", &scene_->objectDissolveEdgeGlowStrength_, 0.0f, 4.0f, "%.2f");
    ImGui::SliderFloat("Object Dissolve Edge Noise", &scene_->objectDissolveEdgeNoiseStrength_, 0.0f, 1.0f, "%.2f");
    ImGui::ColorEdit4("Object Dissolve Edge Color", scene_->objectDissolveEdgeColor_.data());
    const char* objectDissolveMaskTextureNames[] = { "noise0", "noise1" };
    const char* objectDissolveMaskTexturePaths[] = {
        "resources/postEffect/noise0.png",
        "resources/postEffect/noise1.png"
    };
    if (ImGui::Combo("Object Dissolve Mask", &scene_->currentObjectDissolveMaskTexture_, objectDissolveMaskTextureNames, IM_ARRAYSIZE(objectDissolveMaskTextureNames))) {
        scene_->objectDissolveMaskTexturePath_ = objectDissolveMaskTexturePaths[scene_->currentObjectDissolveMaskTexture_];
        scene_->object3dSphere_->SetDissolveMaskTexture(scene_->objectDissolveMaskTexturePath_);
    }
    ImGui::TextWrapped("Mask Path: %s", scene_->objectDissolveMaskTexturePath_.c_str());
    ImGui::SeparatorText("Object Random Noise");
    ImGui::TextWrapped("Applies only to the sphere object for shader random verification.");
    ImGui::Checkbox("Enable Object Random", &scene_->isObjectRandomEnabled_);
    ImGui::Checkbox("Preview Object Random", &scene_->isObjectRandomPreview_);
    ImGui::SliderFloat("Object Random Intensity", &scene_->objectRandomIntensity_, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Object Random Time", &scene_->objectRandomTime_, 0.0f, 100.0f, "%.2f");
    ImGui::SeparatorText("Post Effects");
    auto DrawPostEffectUI = [](const char* label, uint32_t& enabled, float& intensity) {
        bool isEnabled = enabled != 0;
        if (ImGui::Checkbox(label, &isEnabled)) {
            enabled = isEnabled ? 1u : 0u;
        }
        std::string sliderLabel = std::string(label) + " Strength";
        ImGui::SliderFloat(sliderLabel.c_str(), &intensity, 0.0f, 1.0f, "%.2f");
        };
    auto ApplyRadialBlurPreset = [&](const RadialBlurPreset& preset) {
        postEffectParams.radialBlurEnabled = preset.enabled;
        postEffectParams.radialBlurStrength = preset.strength;
        postEffectParams.radialBlurCenter = preset.center;
        postEffectParams.radialBlurSampleCount = preset.sampleCount;
    };
    auto ApplyDissolvePreset = [&](const DissolvePreset& preset) {
        postEffectParams.dissolveEnabled = preset.enabled;
        postEffectParams.dissolveThreshold = preset.threshold;
        postEffectParams.dissolveEdgeWidth = preset.edgeWidth;
        postEffectParams.dissolveEdgeColor = preset.edgeColor;
    };
    auto ApplyOutlinePreset = [&](const OutlinePreset& preset) {
        postEffectParams.outlineMode = preset.outlineMode;
        postEffectParams.hybridColorSource = preset.hybridColorSource;
        postEffectParams.hybridColorWeight = preset.hybridColorWeight;
        postEffectParams.hybridDepthWeight = preset.hybridDepthWeight;
        postEffectParams.hybridNormalWeight = preset.hybridNormalWeight;
        postEffectParams.outlineIntensity = preset.outlineStrength;
        postEffectParams.outlineThickness = preset.outlineThickness;
        postEffectParams.outlineThreshold = preset.outlineThreshold;
        postEffectParams.outlineSoftness = preset.outlineSoftness;
        postEffectParams.outlineDepthThreshold = preset.outlineDepthThreshold;
        postEffectParams.outlineDepthStrength = preset.outlineDepthStrength;
        postEffectParams.outlineNormalThreshold = preset.outlineNormalThreshold;
        postEffectParams.outlineNormalStrength = preset.outlineNormalStrength;
        postEffectParams.outlineColor = preset.outlineColor;
    };
    bool gaussianEnabled = postEffectParams.gaussianEnabled != 0;
    if (ImGui::Checkbox("Gaussian", &gaussianEnabled)) {
        postEffectParams.gaussianEnabled = gaussianEnabled ? 1u : 0u;
    }
    ImGui::SliderFloat("Gaussian Strength", &postEffectParams.gaussianIntensity, 0.0f, 4.0f, "%.2f");
    bool radialBlurEnabled = postEffectParams.radialBlurEnabled != 0;
    if (ImGui::Checkbox("RadialBlur", &radialBlurEnabled)) {
        postEffectParams.radialBlurEnabled = radialBlurEnabled ? 1u : 0u;
    }
    static int radialBlurPresetIndex = 1;
    const char* radialBlurPresetNames[] = { "Weak", "Medium", "Strong", "Dramatic" };
    if (ImGui::Combo("RadialBlur Preset", &radialBlurPresetIndex, radialBlurPresetNames, IM_ARRAYSIZE(radialBlurPresetNames))) {
        ApplyRadialBlurPreset(kRadialBlurPresets[radialBlurPresetIndex]);
    }
    ImGui::SliderFloat("RadialBlur Strength", &postEffectParams.radialBlurStrength, 0.0f, 0.2f, "%.3f");
    ImGui::SliderFloat2("RadialBlur Center", postEffectParams.radialBlurCenter.data(), 0.0f, 1.0f, "%.2f");
    int radialBlurSampleCount = static_cast<int>(postEffectParams.radialBlurSampleCount);
    if (ImGui::SliderInt("RadialBlur Sample Count", &radialBlurSampleCount, 1, 32)) {
        postEffectParams.radialBlurSampleCount = static_cast<uint32_t>(radialBlurSampleCount);
    }
    const char* dissolveNoiseTextureNames[] = { "noise0", "noise1" };
    const char* dissolveNoiseTexturePaths[] = {
        "resources/postEffect/noise0.png",
        "resources/postEffect/noise1.png"
    };
    if (ImGui::Combo("Dissolve Noise Texture", &scene_->currentDissolveNoiseTexture_, dissolveNoiseTextureNames, IM_ARRAYSIZE(dissolveNoiseTextureNames))) {
        texManager->LoadTexture(dissolveNoiseTexturePaths[scene_->currentDissolveNoiseTexture_]);
        dxCommon->SetDissolveNoiseTextureIndex(
            texManager->GetTextureIndexByFilePath(dissolveNoiseTexturePaths[scene_->currentDissolveNoiseTexture_]));
    }
    bool dissolveEnabled = postEffectParams.dissolveEnabled != 0;
    if (ImGui::Checkbox("Dissolve", &dissolveEnabled)) {
        postEffectParams.dissolveEnabled = dissolveEnabled ? 1u : 0u;
    }
    static int dissolvePresetIndex = 1;
    const char* dissolvePresetNames[] = { "Weak", "Medium", "Strong", "Dramatic" };
    if (ImGui::Combo("Dissolve Preset", &dissolvePresetIndex, dissolvePresetNames, IM_ARRAYSIZE(dissolvePresetNames))) {
        ApplyDissolvePreset(kDissolvePresets[dissolvePresetIndex]);
    }
    ImGui::SliderFloat("Dissolve Threshold", &postEffectParams.dissolveThreshold, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Dissolve Edge Width", &postEffectParams.dissolveEdgeWidth, 0.001f, 0.2f, "%.3f");
    ImGui::ColorEdit4("Dissolve Edge Color", postEffectParams.dissolveEdgeColor.data());
    bool outlineEnabled = postEffectParams.outlineMode != 0;
    if (ImGui::Checkbox("Outline", &outlineEnabled)) {
        if (!outlineEnabled) {
            postEffectParams.outlineMode = 0;
        } else if (postEffectParams.outlineMode == 0) {
            postEffectParams.outlineMode = 1;
        }
    }
    const char* outlineModeNames[] = { "Off", "ColorDiff8", "Sobel", "Depth", "Hybrid", "Normal", "FinalHybrid" };
    int outlineMode = static_cast<int>(postEffectParams.outlineMode);
    if (ImGui::Combo("Outline Mode", &outlineMode, outlineModeNames, IM_ARRAYSIZE(outlineModeNames))) {
        postEffectParams.outlineMode = static_cast<uint32_t>(outlineMode);
    }
    static int outlinePresetIndex = 0;
    const char* outlinePresetNames[] = {
        "Balanced",
        "Color Emphasis",
        "Depth Emphasis",
        "Soft Outline",
        "FinalHybrid Balanced",
        "FinalHybrid Color Emphasis",
        "FinalHybrid Depth Emphasis",
        "FinalHybrid Normal Emphasis"
    };
    if (ImGui::Combo("Outline Preset", &outlinePresetIndex, outlinePresetNames, IM_ARRAYSIZE(outlinePresetNames))) {
        ApplyOutlinePreset(kOutlinePresets[outlinePresetIndex]);
    }
    ImGui::SliderFloat("Outline Strength", &postEffectParams.outlineIntensity, 0.0f, 10.0f, "%.2f");
    ImGui::SliderFloat("Outline Thickness", &postEffectParams.outlineThickness, 0.5f, 4.0f, "%.2f");
    ImGui::SliderFloat("Outline Threshold", &postEffectParams.outlineThreshold, 0.0f, 1.5f, "%.3f");
    ImGui::SliderFloat("Outline Softness", &postEffectParams.outlineSoftness, 0.001f, 1.0f, "%.3f");
    ImGui::SliderFloat("Outline Depth Threshold", &postEffectParams.outlineDepthThreshold, 0.0001f, 0.05f, "%.4f");
    ImGui::SliderFloat("Outline Depth Strength", &postEffectParams.outlineDepthStrength, 0.0f, 50.0f, "%.2f");
    ImGui::SliderFloat("Outline Normal Threshold", &postEffectParams.outlineNormalThreshold, 0.0f, 2.0f, "%.3f");
    ImGui::SliderFloat("Outline Normal Strength", &postEffectParams.outlineNormalStrength, 0.0f, 20.0f, "%.2f");
    if (postEffectParams.outlineMode == 4 || postEffectParams.outlineMode == 6) {
        const char* hybridColorSourceNames[] = { "ColorDiff8", "Sobel" };
        int hybridColorSourceIndex = (postEffectParams.hybridColorSource == 1u) ? 0 : 1;
        if (ImGui::Combo("Hybrid Color Source", &hybridColorSourceIndex, hybridColorSourceNames, IM_ARRAYSIZE(hybridColorSourceNames))) {
            postEffectParams.hybridColorSource = (hybridColorSourceIndex == 0) ? 1u : 2u;
        }
        ImGui::SliderFloat("Hybrid Color Weight", &postEffectParams.hybridColorWeight, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("Hybrid Depth Weight", &postEffectParams.hybridDepthWeight, 0.0f, 2.0f, "%.2f");
        if (postEffectParams.outlineMode == 6) {
            ImGui::SliderFloat("Hybrid Normal Weight", &postEffectParams.hybridNormalWeight, 0.0f, 2.0f, "%.2f");
        }
    }
    ImGui::ColorEdit4("Outline Color", postEffectParams.outlineColor.data());
    DrawPostEffectUI("Grayscale", postEffectParams.grayscaleEnabled, postEffectParams.grayscaleIntensity);
    DrawPostEffectUI("Sepia", postEffectParams.sepiaEnabled, postEffectParams.sepiaIntensity);
    DrawPostEffectUI("Invert", postEffectParams.invertEnabled, postEffectParams.invertIntensity);
    DrawPostEffectUI("Vignette", postEffectParams.vignetteEnabled, postEffectParams.vignetteIntensity);
    DrawPostEffectUI("Smoothing", postEffectParams.smoothingEnabled, postEffectParams.smoothingIntensity);
    ImGui::SeparatorText("Primitive Preview");
    ImGui::Checkbox("Show Primitive Preview", &scene_->isPrimitivePreviewVisible_);
    ImGui::Text("Front Row : Plane / Circle / Ring / Triangle");
    ImGui::Text("Back Row  : Box / Cylinder / Cone / Torus");
    ImGui::Text("Ring uses gradationLine.png (AddressV = CLAMP)");
    ImGui::SeparatorText("Particle Texture");
    const char* particleTextureNames[] = { "uvChecker", "Circle2", "Fence" };
    const char* particleTexturePaths[] = {
        "resources/obj/axis/uvChecker.png",
        "resources/particle/circle2.png",
        "resources/obj/fence/fence.png"
    };
    if (ImGui::Combo("Particle Texture", &scene_->currentParticleTexture_, particleTextureNames, IM_ARRAYSIZE(particleTextureNames))) {
        scene_->particleTexturePath_ = particleTexturePaths[scene_->currentParticleTexture_];
        particleManager->SetTexture(scene_->particleTexturePath_);
    }
    ImGui::TextWrapped("Particle Texture Path: %s", scene_->particleTexturePath_.c_str());
    ImGui::SeparatorText("Hit Effect");
    ImGui::TextWrapped("Main submission target: plane billboard particles stretched into hit streaks.");
    if (ImGui::Button("Emit Hit")) {
        particleManager->Emit("Hit", scene_->object3dSphere_->GetTransform().translate, hitEffectParams.spawnCount);
    }
    ImGui::Text("Hit Trigger: [Space] / [H]");
    ImGui::SeparatorText("Hit Params");
    DrawEffectParamsUI("Hit", hitEffectParams);
    if (ImGui::CollapsingHeader("Other Effects (Optional)")) {
        if (ImGui::Button("Emit Fireball")) {
            particleManager->Emit("Fireball", scene_->object3dSphere_->GetTransform().translate, fireballEffectParams.spawnCount);
        }
        ImGui::SameLine();
        if (ImGui::Button("Emit Wind")) {
            particleManager->Emit("Wind", scene_->object3dSphere_->GetTransform().translate, windEffectParams.spawnCount);
        }
        ImGui::SeparatorText("Fireball Params");
        DrawEffectParamsUI("Fireball", fireballEffectParams);
        ImGui::SeparatorText("Wind Params");
        DrawEffectParamsUI("Wind", windEffectParams);
    }
    ImGui::SeparatorText("Particle Smoke Test");
    if (ImGui::Button("Emit Basic Particle")) {
        particleManager->Emit(scene_->particleTexturePath_, scene_->object3dSphere_->GetTransform().translate, 1);
    }
    ImGui::Text("Trigger: [P]");
    ImGui::SeparatorText("Target Object Selection");
    ImGui::Combo("Target", &scene_->targetObjectIndex_, "Fence\0Sphere\0");
    Object3d* targetObj = (scene_->targetObjectIndex_ == 0) ? scene_->object3d_.get() : scene_->object3dSphere_.get();
    ImGui::SeparatorText("Model Transform");
    Transform& tf = targetObj->GetTransform();
    ImGui::DragFloat3("Pos", &tf.translate.x, 0.1f);
    ImGui::DragFloat3("Rot", &tf.rotate.x, 0.01f);
    ImGui::DragFloat3("Scl", &tf.scale.x, 0.1f);
    ImGui::SeparatorText("Model Texture");
    const char* modelTextureNames[] = { "uvChecker", "FenceTexture", "MonsterBall" };
    if (ImGui::Combo("Texture", &scene_->currentModelTexture_, modelTextureNames, IM_ARRAYSIZE(modelTextureNames))) {
        Model* targetModel = (scene_->targetObjectIndex_ == 0) ? scene_->modelFence_ : scene_->modelSphere_;
        if (targetModel) {
            if (scene_->currentModelTexture_ == 0) targetModel->SetTextureIndex(scene_->texIndexUvChecker_);
            else if (scene_->currentModelTexture_ == 1) targetModel->SetTextureIndex(scene_->texIndexFence_);
            else if (scene_->currentModelTexture_ == 2) targetModel->SetTextureIndex(scene_->texIndexMonsterBall_);
        }
    }
    ImGui::SeparatorText("Lighting & Material");
    auto* lightData = targetObj->GetDirectionalLightData();
    if (lightData) {
        if (ImGui::SliderFloat3("LightDir", &lightData->direction.x, -1.0f, 1.0f)) {
            float len = std::sqrt(lightData->direction.x * lightData->direction.x +
                lightData->direction.y * lightData->direction.y +
                lightData->direction.z * lightData->direction.z);
            if (len > 0.0f) {
                lightData->direction.x /= len;
                lightData->direction.y /= len;
                lightData->direction.z /= len;
            }
        }
        ImGui::ColorEdit3("LightColor", &lightData->color.x);
        ImGui::DragFloat("Intensity", &lightData->intensity, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Shininess", &lightData->shininess, 1.0f, 1.0f, 256.0f, "%.1f");
    }
    ImGui::SeparatorText("Blend Mode");
    ImGui::Combo("Blend", &scene_->currentBlendMode_, scene_->blendModeNames_, IM_ARRAYSIZE(scene_->blendModeNames_));
    ImGui::End();
    if (scene_->primitiveEffectSystem_) {
        scene_->primitiveEffectSystem_->DrawImGui();
    }
    ImGui::SetNextWindowSize(ImVec2(320, 520), ImGuiCond_Once);
    ImGui::Begin("Scene Visibility");
    auto setAllVisibility = [this](bool isVisible) {
        scene_->isSkyboxVisible_ = isVisible;
        scene_->isFenceVisible_ = isVisible;
        scene_->isSphereVisible_ = isVisible;
        scene_->isAnimatedCubeVisible_ = isVisible;
        scene_->isSkinnedModelVisible_ = isVisible;
        scene_->isPrimitivePreviewVisible_ = isVisible;
        if (scene_->primitiveEffectSystem_) {
            scene_->primitiveEffectSystem_->SetVisible(isVisible);
        }
        scene_->isParticleVisible_ = isVisible;
        scene_->isVolumetricCloudVisible_ = isVisible;
        scene_->isDebugSpriteVisible_ = isVisible;
        };
    if (ImGui::Button("Show All")) {
        setAllVisibility(true);
    }
    ImGui::SameLine();
    if (ImGui::Button("Hide All")) {
        setAllVisibility(false);
    }
    ImGui::SeparatorText("Models");
    ImGui::Checkbox("Skybox", &scene_->isSkyboxVisible_);
    ImGui::Checkbox("Fence", &scene_->isFenceVisible_);
    ImGui::Checkbox("Sphere", &scene_->isSphereVisible_);
    ImGui::Checkbox("AnimatedCube", &scene_->isAnimatedCubeVisible_);
    ImGui::Checkbox("Active Skinned Model", &scene_->isSkinnedModelVisible_);
    const Skeleton* visibleSkinningTarget = scene_->skinningEditor_ ? scene_->skinningEditor_->GetTargetSkeleton() : nullptr;
    GltfSkinnedModel* activeSkinnedModel = nullptr;
    const char* activeSkinnedModelName = "None";
    if (visibleSkinningTarget == scene_->simpleSkinSkeleton_.get()) {
        activeSkinnedModel = scene_->simpleSkinSkinnedModel_.get();
        activeSkinnedModelName = "simpleSkin";
    } else if (visibleSkinningTarget == scene_->walkSkeleton_.get()) {
        activeSkinnedModel = scene_->walkSkinnedModel_.get();
        activeSkinnedModelName = "walk.gltf";
    } else if (visibleSkinningTarget == scene_->sneakWalkSkeleton_.get()) {
        activeSkinnedModel = scene_->sneakWalkSkinnedModel_.get();
        activeSkinnedModelName = "sneakWalk.gltf";
    }
    ImGui::Checkbox("Primitive Preview", &scene_->isPrimitivePreviewVisible_);
    ImGui::SeparatorText("Particles / Effects");
    ImGui::Checkbox("ParticleManager", &scene_->isParticleVisible_);
    if (scene_->primitiveEffectSystem_) {
        scene_->primitiveEffectSystem_->DrawVisibilityImGui();
    }
    ImGui::Checkbox("ボリューメトリック雲 (Volumetric Cloud)", &scene_->isVolumetricCloudVisible_);
    ImGui::SeparatorText("Debug");
    ImGui::Checkbox("Debug Sprite", &scene_->isDebugSpriteVisible_);
    ImGui::End();
    ImGui::SetNextWindowSize(ImVec2(360, 260), ImGuiCond_Once);
    ImGui::Begin("Skinning Debug");
    ImGui::Text("Active Target: %s", activeSkinnedModelName);
    ImGui::Text("CPU Skinning Path: Enabled");
    if (activeSkinnedModel) {
        bool useComputeOutputVertices = activeSkinnedModel->IsUsingComputeOutputVertices();
        if (ImGui::Checkbox("GPU Skinning Output VBV", &useComputeOutputVertices)) {
            activeSkinnedModel->SetUseComputeOutputVertices(useComputeOutputVertices);
        }
        ImGui::SameLine();
        ImGui::TextDisabled(useComputeOutputVertices ? "ON" : "OFF");
        ImGui::SeparatorText("Resource Counts");
        ImGui::Text("Vertex Count    : %u", activeSkinnedModel->GetVertexCount());
        ImGui::Text("Influence Count : %u", activeSkinnedModel->GetInfluenceCount());
        ImGui::Text("Palette Count   : %u", activeSkinnedModel->GetPaletteCount());
        ImGui::Text("Dispatch Groups : %u", activeSkinnedModel->GetDispatchThreadGroupCount());
        ImGui::Text("Threads / Group : 1024");
        ImGui::SeparatorText("Resource State");
        ImGui::Text("Compute Resources: %s", activeSkinnedModel->HasComputeSkinningResources() ? "Ready" : "Missing");
        ImGui::TextDisabled("CPU/GPU max vertex delta: N/A (readback not implemented)");
        ImGui::TextWrapped("ON compares the compute output path visually. OFF keeps the CPU-updated vertex buffer path.");
    } else {
        ImGui::TextDisabled("Select a skinned target in Skinning Editor.");
    }
    ImGui::End();
#else
    (void)dxCommon;
    (void)volumetricCloudPass;
#endif
}
