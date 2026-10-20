#include "PlayerHudController.h"
#include "Engine/Core/DebugUiWorkspace.h"
#include "Engine/Game/GameState/StageCombatState.h"
#include "Engine/Game/Player/BoostController.h"
#include "Engine/Game/Player/PlayerBulletManager.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif
void PlayerHudController::DrawImGui(){
#ifdef USE_IMGUI
    if(DebugUiWorkspace::BeginPanel("HUD・スコア・ロック弾・スロー回避")){
        ImGui::Checkbox("HUDを表示",&enabled_);ImGui::Checkbox("動きを抑える（短いフェード）",&reducedMotion_);
        ImGui::PushItemWidth(140.0f);
        ImGui::SliderFloat("下地Alpha倍率",&panelScale_,0,2);
        ImGui::TextWrapped("画像の64/255（約25%%）を維持する標準倍率は1です。");
        ImGui::SliderFloat("緑枠Alpha",&frameAlpha_,0,1);ImGui::SliderFloat("文字・ゲージAlpha",&textAlpha_,0,1);
        ImGui::Text("組み上がり 上帯 %.2f / 下帯 %.2f",topReveal_,bottomReveal_);
        ImGui::TextWrapped("文字中は非表示。横切りで上帯、下からの入場で下帯。最後0.2秒は安定表示。");
        if(combat_)combat_->DrawImGui();
        ImGui::TextWrapped("ロック弾 %d / 5（実発射で消費、敵1体の撃破で1回復）",bullets_?bullets_->GetLockAmmo():0);
        if(boost_)boost_->DrawEvadeSettingsImGui();
        ImGui::TextWrapped("Spaceは照準切替。Pauseキーで進行・実秒待ちを停止します。");
        ImGui::PopItemWidth();
    }DebugUiWorkspace::EndPanel();
#endif
}
