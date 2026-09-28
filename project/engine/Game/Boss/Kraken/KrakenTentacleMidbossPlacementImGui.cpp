#include "KrakenTentacleMidbossControllerInternal.h"

#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void KrakenTentacleMidbossController::DrawPlacementImGui() {
    if (impl_) {
        impl_->DrawPlacementImGui();
    }
}

void KrakenTentacleMidbossController::Impl::DrawPlacementImGui() {
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("配置調整##Placement")) {
        return;
    }
    ImGui::TextWrapped("出現時のカメラを基準に配置します。描画用の揺れやカメラ移行は含みません。");
    ImGui::TextWrapped("待機中は次の更新で反映します。登場中・攻撃中は次の待機時、非表示・撃破後は次の登場時に反映します。");
    ImGui::TextWrapped("調整値はRestart後も保持します。位置の単位はワールド単位です。");
    const auto edit = [this](const char* label, float& value, float speed,
                            float minimum, float maximum) {
        const float previous = value;
        if (ImGui::DragFloat(label, &value, speed, minimum, maximum, "%.3f",
                ImGuiSliderFlags_AlwaysClamp)) {
            if (!std::isfinite(value) || value < minimum || value > maximum) {
                value = previous;
            } else if (value != previous) {
                placementChangePending = true;
            }
        }
    };
    edit("前後位置", placementSettings.forwardOffset, 0.1f, -1000.0f, 1000.0f);
    ImGui::TextWrapped("増やすと基準カメラの前方（奥）、減らすと後方（手前）へ移動します。");
    edit("左右位置", placementSettings.rightOffset, 0.1f, -1000.0f, 1000.0f);
    ImGui::TextWrapped("増やすと基準カメラの右、減らすと左へ移動します。");
    edit("上下位置", placementSettings.upOffset, 0.1f, -1000.0f, 1000.0f);
    ImGui::TextWrapped("増やすと基準カメラの上、減らすと下へ移動します。");
    edit("全体の大きさ（倍率）", placementSettings.uniformScale, 0.01f, 0.01f, 100.0f);
    ImGui::TextWrapped("増やすと拡大、減らすと縮小します。攻撃到達距離にも影響します。");
    if (ImGui::Button("配置を初期値に戻す")) {
        ApplyRecommendedPlacementSettings();
    }
    ImGui::Text("配置の反映待ち: %s", placementChangePending ? "はい" : "いいえ");
    ImGui::TextWrapped("到達性は「クラーケン触手中ボス実行時デバッグ」の「配置・画面・攻撃到達診断」で確認できます。");

    // Preserve the existing manual placement tools without allowing an encounter bypass.
    if (ImGui::TreeNode("手動配置の詳細##ManualPlacement")) {
        ImGui::TextWrapped("ウェーブ4自動制御中・登場中・攻撃中は手動配置を操作できません。");
        ImGui::BeginDisabled(waveEncounterControlActive || entranceActive || IsAttackState());
        ImGui::DragFloat3("ワールド位置##WorldPosition",
            &worldPosition.x, 0.1f, -10000.0f, 10000.0f, "%.3f");
        Vector3 rotationDegrees = {
            worldRotation.x * 180.0f / std::numbers::pi_v<float>,
            worldRotation.y * 180.0f / std::numbers::pi_v<float>,
            worldRotation.z * 180.0f / std::numbers::pi_v<float> };
        if (ImGui::DragFloat3("ワールド回転（度）##WorldRotation",
                &rotationDegrees.x, 0.5f, -360.0f, 360.0f, "%.2f")) {
            worldRotation = {
                rotationDegrees.x * std::numbers::pi_v<float> / 180.0f,
                rotationDegrees.y * std::numbers::pi_v<float> / 180.0f,
                rotationDegrees.z * std::numbers::pi_v<float> / 180.0f };
        }
        float facingDegrees = placementSettings.modelFacingYawOffset * 180.0f /
            std::numbers::pi_v<float>;
        if (ImGui::DragFloat("モデル正面の左右回転補正（度）",
                &facingDegrees, 0.5f, -180.0f, 180.0f, "%.2f")) {
            placementSettings.modelFacingYawOffset = facingDegrees *
                std::numbers::pi_v<float> / 180.0f;
        }
        if (ImGui::Button("プレイヤー方向へ向け直す##FacePlayer")) {
            pendingCommand = KrakenTentacleMidbossPendingCommand::FacePlayer;
        }
        if (ImGui::Button("現在カメラ前方へ配置##PlaceInFrontOfCamera")) {
            pendingCommand = KrakenTentacleMidbossPendingCommand::PlaceInFrontOfCamera;
        }
        ImGui::EndDisabled();
        ImGui::TreePop();
    }
#endif
}
