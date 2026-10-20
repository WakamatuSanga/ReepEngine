#include "VolumetricCloudPass.h"

#include <algorithm>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void VolumetricCloudPass::UpdateQualityConstantBuffer()
{
    constantData_->cloudBottomShapingExtra = { std::clamp(settings_.cloudBottomDensity_, 0.0f, 2.0f), 0.0f, 0.0f, 0.0f };
    constantData_->cloudBottomUndulation = { settings_.cloudBottomUndulationStrength_, settings_.cloudBottomUndulationScale_, std::clamp(settings_.cloudDetailNoiseNearBottom_, 0.0f, 1.0f), std::clamp(settings_.cloudBoundarySoftness_, 0.0f, 1.0f) };
    constantData_->volumeEdgeFade = { settings_.enableVolumeEdgeFade_ ? 1.0f : 0.0f, (std::max)(settings_.volumeEdgeFadeDistance_, 0.001f), 0.0f, 0.0f };
    constantData_->farCloudLayer = { settings_.enableFarCloudLayer_ ? 1.0f : 0.0f, (std::max)(settings_.farCloudDistance_, 1.0f), settings_.farCloudHeight_, settings_.farCloudScale_ };
    constantData_->farCloudLayerExtra = { std::clamp(settings_.farCloudAlpha_, 0.0f, 1.0f), settings_.farCloudFlowSpeed_, settings_.farCloudUseProceduralNoise_ ? 1.0f : 0.0f, 0.0f };
    constantData_->farCloudColor = { 0.92f, 0.95f, 1.0f, 1.0f };
    constantData_->cloudSeaLayer = { settings_.enableCloudSeaLayer_ ? 1.0f : 0.0f, settings_.cloudSeaDistance_, settings_.cloudSeaHeight_, std::clamp(settings_.cloudSeaAlpha_, 0.0f, 1.0f) };
    constantData_->cloudSeaShape = { (std::max)(settings_.cloudSeaWidth_, 1.0f), (std::max)(settings_.cloudSeaDepth_, 1.0f), (std::max)(settings_.cloudSeaNoiseScale_, 0.0001f), std::clamp(settings_.cloudSeaSoftness_, 0.0f, 1.0f) };
    constantData_->cloudSeaFlow = { settings_.cloudSeaFlowSpeed_, settings_.cloudSeaUseCameraRelative_ ? 1.0f : 0.0f, 0.0f, 0.0f };
    constantData_->cloudSeaColor = settings_.cloudSeaColor_;
}

void VolumetricCloudPass::ResetCloudDefaults() {
    if (settings_.ResetDefaults()) hasValidCloudBuffer_ = false;
}

void VolumetricCloudPass::ApplyUserPreferredCloudPreset() {
    if (settings_.ApplyUserPreferredPreset()) hasValidCloudBuffer_ = false;
    externalFlowMultiplier_ = 1.0f;
    currentCloudFlowSpeed_ = settings_.cloudBaseFlowSpeed_;
}
void VolumetricCloudPass::DrawQualityImGuiControls()
{
#ifdef USE_IMGUI
    auto setDebugView = [&](DebugViewMode mode) { settings_.debugViewMode_ = mode; };
    const char* currentSource = "Final";
    switch (settings_.debugViewMode_) {
    case DebugViewMode::FarCloudOnly: currentSource = "Far Cloud Only"; break;
    case DebugViewMode::VolumetricOnly: currentSource = "Volumetric Only"; break;
    case DebugViewMode::NoiseDebug: currentSource = "Cloud UV / Noise Debug"; break;
    case DebugViewMode::CloudSeaOnly: currentSource = "Cloud Sea Only"; break;
    case DebugViewMode::AlphaOnly: currentSource = "Alpha Only"; break;
    case DebugViewMode::DensityOnly: currentSource = "Density Only"; break;
    case DebugViewMode::LightOnly: currentSource = "Light Only"; break;
    default: break;
    }

    ImGui::SeparatorText("雲Artifact診断 (Cloud Artifact Debug)");
    if (ImGui::Button("ユーザー推奨 雲設定 (User Preferred Cloud)")) { ApplyUserPreferredCloudPreset(); }
    ImGui::TextWrapped("雲の見え方をレールシューティング向けに調整し、Player / Enemy / Bulletが雲に埋もれにくくなる推奨プリセットです。");
    ImGui::Text("Current Cloud Source: %s", currentSource);
    if (ImGui::Button("雲Alpha表示 (Show Cloud Alpha)")) { setDebugView(DebugViewMode::AlphaOnly); }
    ImGui::SameLine();
    if (ImGui::Button("雲Density表示 (Show Cloud Density)")) { setDebugView(DebugViewMode::DensityOnly); }
    if (ImGui::Button("Far Cloudのみ (Show Far Cloud Only)")) { setDebugView(DebugViewMode::FarCloudOnly); }
    ImGui::SameLine();
    if (ImGui::Button("Volumetricのみ (Show Volumetric Only)")) { setDebugView(DebugViewMode::VolumetricOnly); }
    if (ImGui::Button("Cloud Seaのみ (Show Cloud Sea Only)")) { setDebugView(DebugViewMode::CloudSeaOnly); }
    ImGui::SameLine();
    if (ImGui::Button("Noise/UV表示 (Show Cloud UV / Noise Debug)")) { setDebugView(DebugViewMode::NoiseDebug); }
    if (ImGui::Button("通常表示へ戻す (Final Cloud View)")) { setDebugView(DebugViewMode::Final); }
    ImGui::TextWrapped("丸い波動が見える時は Far Cloud / Cloud Sea / Volumetric / Noise を切り替えて、どの層から出ているか確認します。");
    ImGui::SeparatorText("ゲームオブジェクト保護 (Gameplay Object Preserve)");
    ImGui::TextWrapped("低解像度CloudがPlayer / Enemy / Bulletの上ににじむ量を抑えます。厳密なMaskではなくScene Depthを使った軽量保護です。");
    ImGui::Checkbox("Gameplay Object Preserve", &settings_.enableGameplayObjectPreserve_);
    ImGui::Checkbox("Playerを低解像度雲から保護 (Preserve Player From LowRes Cloud)", &settings_.preservePlayerFromLowResCloud_);
    ImGui::Checkbox("Enemyを低解像度雲から保護 (Preserve Enemy From LowRes Cloud)", &settings_.preserveEnemyFromLowResCloud_);
    ImGui::Checkbox("Bulletを低解像度雲から保護 (Preserve Bullet From LowRes Cloud)", &settings_.preserveBulletFromLowResCloud_);
    ImGui::SliderFloat("オブジェクト上に残す雲の強さ (Cloud Over Gameplay Object Strength)", &settings_.cloudOverGameplayObjectStrength_, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("前景雲Alpha減衰 (Foreground Cloud Alpha Reduction)", &settings_.foregroundCloudAlphaReduction_, 0.0f, 1.0f, "%.2f");
    ImGui::Checkbox("Cloud Depth Testを使う (Enable Cloud Depth Test)", &settings_.enableCloudDepthTest_);
    ImGui::Checkbox("Gameplay Object Maskを使う (Enable Gameplay Object Mask)", &settings_.enableGameplayObjectMask_);
    ImGui::TextDisabled("Mask RTは今後用です。現在はDepth Testで前景の雲Alphaを抑制します。");
    const char* compositeDebugNames[] = { "Off", "Cloud Alpha", "Preserve Mask", "Preserve Result" };
    ImGui::Combo("Cloud Composite Debug", &settings_.cloudCompositeDebugMode_, compositeDebugNames, IM_ARRAYSIZE(compositeDebugNames));
    ImGui::TextWrapped("PlayerやEnemyをくっきり見せたい場合はGameModeRenderScaleを1.0にし、雲だけ軽くしたい場合はCloud Resolution Scaleを下げてください。");

    ImGui::SeparatorText("遠景雲レイヤー (Far Cloud Layer)");
    ImGui::TextWrapped("Far Cloud Layerは遠くの雲を軽く見せる背景状の雲です。円形artifactを避けるため、画面中心距離ではなくワールド平面ノイズで描きます。");
    ImGui::Checkbox("遠景雲を使う (Enable Far Cloud Layer)", &settings_.enableFarCloudLayer_);
    ImGui::DragFloat("遠景雲距離 (Far Cloud Distance)", &settings_.farCloudDistance_, 1.0f, 10.0f, 1000.0f, "%.1f");
    ImGui::DragFloat("遠景雲高さ (Far Cloud Height)", &settings_.farCloudHeight_, 0.5f, -100.0f, 200.0f, "%.1f");
    ImGui::DragFloat("遠景雲スケール (Far Cloud Scale)", &settings_.farCloudScale_, 0.001f, 0.001f, 0.1f, "%.3f");
    ImGui::SliderFloat("遠景雲透明度 (Far Cloud Alpha)", &settings_.farCloudAlpha_, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("遠景雲流速 (Far Cloud Flow Speed)", &settings_.farCloudFlowSpeed_, 0.01f, 0.0f, 5.0f, "%.2f");
    ImGui::Checkbox("遠景雲を手続きノイズで描く (Far Cloud Use Procedural Noise)", &settings_.farCloudUseProceduralNoise_);

    ImGui::SeparatorText("雲海レイヤー (Cloud Sea Layer)");
    ImGui::TextWrapped("Cloud Sea Layerは奥に広がる軽量な雲海です。Player / Enemy / Bulletの解像度には影響せず、空部分にだけ合成します。");
    ImGui::Checkbox("雲海を使う (Enable Cloud Sea Layer)", &settings_.enableCloudSeaLayer_);
    ImGui::Checkbox("カメラ相対雲海 (Cloud Sea Use Camera Relative)", &settings_.cloudSeaUseCameraRelative_);
    ImGui::DragFloat("雲海距離 (Cloud Sea Distance)", &settings_.cloudSeaDistance_, 1.0f, 10.0f, 1000.0f, "%.1f");
    ImGui::DragFloat("雲海高さ (Cloud Sea Height)", &settings_.cloudSeaHeight_, 0.5f, -100.0f, 200.0f, "%.1f");
    ImGui::DragFloat("雲海幅 (Cloud Sea Width)", &settings_.cloudSeaWidth_, 1.0f, 10.0f, 2000.0f, "%.1f");
    ImGui::DragFloat("雲海奥行き (Cloud Sea Depth)", &settings_.cloudSeaDepth_, 1.0f, 10.0f, 2000.0f, "%.1f");
    ImGui::SliderFloat("雲海透明度 (Cloud Sea Alpha)", &settings_.cloudSeaAlpha_, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("雲海流速 (Cloud Sea Flow Speed)", &settings_.cloudSeaFlowSpeed_, 0.1f, 0.0f, 30.0f, "%.1f");
    ImGui::DragFloat("雲海ノイズスケール (Cloud Sea Noise Scale)", &settings_.cloudSeaNoiseScale_, 0.001f, 0.001f, 0.1f, "%.3f");
    ImGui::SliderFloat("雲海の柔らかさ (Cloud Sea Softness)", &settings_.cloudSeaSoftness_, 0.0f, 1.0f, "%.2f");
    ImGui::ColorEdit4("雲海色 (Cloud Sea Color)", &settings_.cloudSeaColor_.x);
#endif
}
