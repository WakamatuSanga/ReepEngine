#include "VolumetricCloudPass.h"

#include <algorithm>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void VolumetricCloudPass::UpdateQualityConstantBuffer()
{
    constantData_->cloudBottomShapingExtra = { std::clamp(cloudBottomDensity_, 0.0f, 2.0f), 0.0f, 0.0f, 0.0f };
    constantData_->cloudBottomUndulation = { cloudBottomUndulationStrength_, cloudBottomUndulationScale_, std::clamp(cloudDetailNoiseNearBottom_, 0.0f, 1.0f), std::clamp(cloudBoundarySoftness_, 0.0f, 1.0f) };
    constantData_->volumeEdgeFade = { enableVolumeEdgeFade_ ? 1.0f : 0.0f, (std::max)(volumeEdgeFadeDistance_, 0.001f), 0.0f, 0.0f };
    constantData_->farCloudLayer = { enableFarCloudLayer_ ? 1.0f : 0.0f, (std::max)(farCloudDistance_, 1.0f), farCloudHeight_, farCloudScale_ };
    constantData_->farCloudLayerExtra = { std::clamp(farCloudAlpha_, 0.0f, 1.0f), farCloudFlowSpeed_, farCloudUseProceduralNoise_ ? 1.0f : 0.0f, 0.0f };
    constantData_->farCloudColor = { 0.92f, 0.95f, 1.0f, 1.0f };
    constantData_->cloudSeaLayer = { enableCloudSeaLayer_ ? 1.0f : 0.0f, cloudSeaDistance_, cloudSeaHeight_, std::clamp(cloudSeaAlpha_, 0.0f, 1.0f) };
    constantData_->cloudSeaShape = { (std::max)(cloudSeaWidth_, 1.0f), (std::max)(cloudSeaDepth_, 1.0f), (std::max)(cloudSeaNoiseScale_, 0.0001f), std::clamp(cloudSeaSoftness_, 0.0f, 1.0f) };
    constantData_->cloudSeaFlow = { cloudSeaFlowSpeed_, cloudSeaUseCameraRelative_ ? 1.0f : 0.0f, 0.0f, 0.0f };
    constantData_->cloudSeaColor = cloudSeaColor_;
}


void VolumetricCloudPass::ApplyUserPreferredCloudPreset()
{
    useLowResolutionCloud_ = true;
    enableCloudComposite_ = true;
    enableDepthAwareUpsample_ = true;
    enableGameplayObjectPreserve_ = true;
    preservePlayerFromLowResCloud_ = true;
    preserveEnemyFromLowResCloud_ = true;
    preserveBulletFromLowResCloud_ = true;
    enableCloudDepthTest_ = true;
    enableGameplayObjectMask_ = false;
    cloudOverGameplayObjectStrength_ = 0.2f;
    foregroundCloudAlphaReduction_ = 0.8f;
    cloudCompositeDebugMode_ = 0;
    cloudResolutionScale_ = 0.25f;
    cloudRenderInterval_ = 1;
    viewStepScale_ = 0.5f;
    lightStepScale_ = 0.5f;
    cloudFlowDirectionMode_ = CloudFlowDirectionMode::TowardCamera;
    invertCloudFlowDirection_ = false;
    cloudBaseFlowSpeed_ = 10.0f;
    useCameraRelativeCloudVolume_ = true;
    cloudNearDistance_ = -5.0f;
    cloudFarDistance_ = 200.0f;
    cloudBehindCameraDistance_ = 8.0f;
    cloudHeightOffset_ = 0.7f;
    cloudVolumeWidth_ = 564.0f;
    cloudVolumeHeight_ = 90.0f;
    cloudVolumeDepth_ = 1000.0f;
    keepCameraBelowClouds_ = true;
    cameraToCloudBottom_ = 36.2f;
    cloudLayerThickness_ = 13.0f;
    cloudBottomFade_ = 0.1f;
    cloudTopFade_ = 19.6f;
    enableCloudBottomShaping_ = true;
    cloudBottomFlattenStrength_ = 0.0f;
    cloudBottomSmoothness_ = 0.30f;
    cloudBottomNoiseSuppression_ = 0.40f;
    cloudBottomDensity_ = 0.64f;
    cloudBottomUndulationStrength_ = 24.0f;
    cloudBottomUndulationScale_ = 0.052f;
    cloudBoundarySoftness_ = 0.86f;
    cloudDetailNoiseNearBottom_ = 1.0f;
    enableVolumeEdgeFade_ = true;
    volumeEdgeFadeDistance_ = 20.0f;
    enableCloudSeaLayer_ = true;
    cloudSeaUseCameraRelative_ = true;
    cloudSeaDistance_ = 180.0f;
    cloudSeaHeight_ = 22.0f;
    cloudSeaWidth_ = 360.0f;
    cloudSeaDepth_ = 320.0f;
    cloudSeaAlpha_ = 0.42f;
    cloudSeaFlowSpeed_ = 8.9f;
    cloudSeaNoiseScale_ = 0.045f;
    cloudSeaSoftness_ = 0.38f;
    cloudSeaColor_ = { 230.0f / 255.0f, 242.0f / 255.0f, 1.0f, 1.0f };
    recreateCloudBufferRequested_ = true;
}
void VolumetricCloudPass::DrawQualityImGuiControls()
{
#ifdef USE_IMGUI
    auto setDebugView = [&](DebugViewMode mode) { debugViewMode_ = mode; };
    const char* currentSource = "Final";
    switch (debugViewMode_) {
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
    ImGui::TextWrapped("現在の理想に近い、雲だけ低解像度で軽くしつつPlayer / Enemy / Bulletをくっきり残す推奨プリセットです。");
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
    ImGui::Checkbox("Gameplay Object Preserve", &enableGameplayObjectPreserve_);
    ImGui::Checkbox("Playerを低解像度雲から保護 (Preserve Player From LowRes Cloud)", &preservePlayerFromLowResCloud_);
    ImGui::Checkbox("Enemyを低解像度雲から保護 (Preserve Enemy From LowRes Cloud)", &preserveEnemyFromLowResCloud_);
    ImGui::Checkbox("Bulletを低解像度雲から保護 (Preserve Bullet From LowRes Cloud)", &preserveBulletFromLowResCloud_);
    ImGui::SliderFloat("オブジェクト上に残す雲の強さ (Cloud Over Gameplay Object Strength)", &cloudOverGameplayObjectStrength_, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("前景雲Alpha減衰 (Foreground Cloud Alpha Reduction)", &foregroundCloudAlphaReduction_, 0.0f, 1.0f, "%.2f");
    ImGui::Checkbox("Cloud Depth Testを使う (Enable Cloud Depth Test)", &enableCloudDepthTest_);
    ImGui::Checkbox("Gameplay Object Maskを使う (Enable Gameplay Object Mask)", &enableGameplayObjectMask_);
    ImGui::TextDisabled("Mask RTは今後用です。現在はDepth Testで前景の雲Alphaを抑制します。");
    const char* compositeDebugNames[] = { "Off", "Cloud Alpha", "Preserve Mask", "Preserve Result" };
    ImGui::Combo("Cloud Composite Debug", &cloudCompositeDebugMode_, compositeDebugNames, IM_ARRAYSIZE(compositeDebugNames));
    ImGui::TextWrapped("PlayerやEnemyをくっきり見せたい場合はGameModeRenderScaleを1.0にし、雲だけ軽くしたい場合はCloud Resolution Scaleを下げてください。");

    ImGui::SeparatorText("遠景雲レイヤー (Far Cloud Layer)");
    ImGui::TextWrapped("Far Cloud Layerは遠くの雲を軽く見せる背景状の雲です。円形artifactを避けるため、画面中心距離ではなくワールド平面ノイズで描きます。");
    ImGui::Checkbox("遠景雲を使う (Enable Far Cloud Layer)", &enableFarCloudLayer_);
    ImGui::DragFloat("遠景雲距離 (Far Cloud Distance)", &farCloudDistance_, 1.0f, 10.0f, 1000.0f, "%.1f");
    ImGui::DragFloat("遠景雲高さ (Far Cloud Height)", &farCloudHeight_, 0.5f, -100.0f, 200.0f, "%.1f");
    ImGui::DragFloat("遠景雲スケール (Far Cloud Scale)", &farCloudScale_, 0.001f, 0.001f, 0.1f, "%.3f");
    ImGui::SliderFloat("遠景雲透明度 (Far Cloud Alpha)", &farCloudAlpha_, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("遠景雲流速 (Far Cloud Flow Speed)", &farCloudFlowSpeed_, 0.01f, 0.0f, 5.0f, "%.2f");
    ImGui::InputText("遠景雲テクスチャパス (Far Cloud Texture Path)", farCloudTexturePath_, IM_ARRAYSIZE(farCloudTexturePath_));
    ImGui::Checkbox("遠景雲を手続きノイズで描く (Far Cloud Use Procedural Noise)", &farCloudUseProceduralNoise_);

    ImGui::SeparatorText("雲海レイヤー (Cloud Sea Layer)");
    ImGui::TextWrapped("Cloud Sea Layerは奥に広がる軽量な雲海です。Player / Enemy / Bulletの解像度には影響せず、空部分にだけ合成します。");
    ImGui::Checkbox("雲海を使う (Enable Cloud Sea Layer)", &enableCloudSeaLayer_);
    ImGui::Checkbox("カメラ相対雲海 (Cloud Sea Use Camera Relative)", &cloudSeaUseCameraRelative_);
    ImGui::DragFloat("雲海距離 (Cloud Sea Distance)", &cloudSeaDistance_, 1.0f, 10.0f, 1000.0f, "%.1f");
    ImGui::DragFloat("雲海高さ (Cloud Sea Height)", &cloudSeaHeight_, 0.5f, -100.0f, 200.0f, "%.1f");
    ImGui::DragFloat("雲海幅 (Cloud Sea Width)", &cloudSeaWidth_, 1.0f, 10.0f, 2000.0f, "%.1f");
    ImGui::DragFloat("雲海奥行き (Cloud Sea Depth)", &cloudSeaDepth_, 1.0f, 10.0f, 2000.0f, "%.1f");
    ImGui::SliderFloat("雲海透明度 (Cloud Sea Alpha)", &cloudSeaAlpha_, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("雲海流速 (Cloud Sea Flow Speed)", &cloudSeaFlowSpeed_, 0.1f, 0.0f, 30.0f, "%.1f");
    ImGui::DragFloat("雲海ノイズスケール (Cloud Sea Noise Scale)", &cloudSeaNoiseScale_, 0.001f, 0.001f, 0.1f, "%.3f");
    ImGui::SliderFloat("雲海の柔らかさ (Cloud Sea Softness)", &cloudSeaSoftness_, 0.0f, 1.0f, "%.2f");
    ImGui::ColorEdit4("雲海色 (Cloud Sea Color)", &cloudSeaColor_.x);
#endif
}