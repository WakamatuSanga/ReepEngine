#include "PlayerHudController.h"
#include "Engine/Game/Player/BoostController.h"
#include "Engine/Game/Player/PlayerBulletManager.h"
#include "Engine/Game/Player/PlayerDamageFeedbackController.h"
#include "Engine/Game/GameState/StageCombatState.h"
#include "Engine/Game/GameState/StageStartSequenceController.h"
#include "Engine/Game/GameState/PlayerDeathSequenceController.h"
#include "Engine/Game/RailShooter/EnemyWaveManager.h"
#include "Engine/Game/Camera/RailShooterCameraRig.h"
#include "Engine/Graphics/Sprite/Sprite.h"
#include "Engine/Graphics/Sprite/SpriteCommon.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
namespace { float Smooth(float t) { t=std::clamp(t,0.0f,1.0f); return t*t*(3-2*t); } }
PlayerHudController::PlayerHudController() = default;
PlayerHudController::~PlayerHudController() = default;
void PlayerHudController::Initialize(SpriteCommon* s, PlayerDamageFeedbackController* d, PlayerBulletManager* b, BoostController* boost) {
    sprites_=s; damage_=d; bullets_=b; boost_=boost;
    const std::string root="resources/ui/hud/";
    // Positions and padded cell sizes copied from the supplied layout.json.
    struct Asset { const char* name; const char* file; Vector2 position; Vector2 size; };
    const Asset assets[]{
        {"score-label","parts/score-label.png",{44.0f,38.0f},{94.0f,30.0f}},
        {"progress-label","parts/progress-label.png",{646.0f,35.0f},{147.0f,30.0f}},
        {"lock-label","parts/lock-label.png",{45.0f,925.0f},{75.0f,30.0f}},
        {"hp-label","parts/hp-label.png",{1490.0f,929.0f},{42.0f,31.0f}},
        {"evade-label","parts/evade-label.png",{522.0f,928.0f},{121.0f,29.0f}},
        {"evade-state-ready","parts/evade-state-ready.png",{662.0f,928.0f},{93.0f,29.0f}},
        {"evade-state-wait","parts/evade-state-wait.png",{660.0f,928.0f},{77.0f,29.0f}},
        {"evade-state-safe","parts/evade-state-safe.png",{662.0f,928.0f},{74.0f,30.0f}},
        {"progress-track","parts/progress-track.png",{643.0f,84.0f},{749.0f,28.0f}},
        {"progress-fill","parts/progress-fill.png",{643.0f,84.0f},{749.0f,28.0f}},
        {"evade-track","parts/evade-track.png",{519.0f,971.0f},{374.0f,28.0f}},
        {"evade-fill-ready","parts/evade-fill-ready.png",{519.0f,971.0f},{374.0f,28.0f}},
        {"evade-fill-active","parts/evade-fill-active.png",{519.0f,971.0f},{374.0f,28.0f}},
        {"hp-track","parts/hp-track.png",{1488.0f,1007.0f},{390.0f,36.0f}},
        {"hp-fill","parts/hp-fill.png",{1488.0f,1007.0f},{390.0f,36.0f}},
        {"ammo-slot-on","parts/ammo-slot-on.png",{42.0f,968.0f},{51.0f,36.0f}},
        {"ammo-slot-off","parts/ammo-slot-off.png",{42.0f,968.0f},{51.0f,36.0f}},
        {"missile-icon","parts/missile-icon.png",{314.0f,960.0f},{31.0f,52.0f}},
        {"digits-score/0","parts/digits-score/0.png",{0.0f,0.0f},{46.0f,85.0f}},
        {"digits-score/1","parts/digits-score/1.png",{0.0f,0.0f},{46.0f,85.0f}},
        {"digits-score/2","parts/digits-score/2.png",{0.0f,0.0f},{46.0f,85.0f}},
        {"digits-score/3","parts/digits-score/3.png",{0.0f,0.0f},{46.0f,85.0f}},
        {"digits-score/4","parts/digits-score/4.png",{0.0f,0.0f},{46.0f,85.0f}},
        {"digits-score/5","parts/digits-score/5.png",{0.0f,0.0f},{46.0f,85.0f}},
        {"digits-score/6","parts/digits-score/6.png",{0.0f,0.0f},{46.0f,85.0f}},
        {"digits-score/7","parts/digits-score/7.png",{0.0f,0.0f},{46.0f,85.0f}},
        {"digits-score/8","parts/digits-score/8.png",{0.0f,0.0f},{46.0f,85.0f}},
        {"digits-score/9","parts/digits-score/9.png",{0.0f,0.0f},{46.0f,85.0f}},
        {"digits-score/slash","parts/digits-score/slash.png",{0.0f,0.0f},{46.0f,85.0f}},
        {"digits-small/0","parts/digits-small/0.png",{0.0f,0.0f},{35.0f,63.0f}},
        {"digits-small/1","parts/digits-small/1.png",{0.0f,0.0f},{35.0f,63.0f}},
        {"digits-small/2","parts/digits-small/2.png",{0.0f,0.0f},{35.0f,63.0f}},
        {"digits-small/3","parts/digits-small/3.png",{0.0f,0.0f},{35.0f,63.0f}},
        {"digits-small/4","parts/digits-small/4.png",{0.0f,0.0f},{35.0f,63.0f}},
        {"digits-small/5","parts/digits-small/5.png",{0.0f,0.0f},{35.0f,63.0f}},
        {"digits-small/6","parts/digits-small/6.png",{0.0f,0.0f},{35.0f,63.0f}},
        {"digits-small/7","parts/digits-small/7.png",{0.0f,0.0f},{35.0f,63.0f}},
        {"digits-small/8","parts/digits-small/8.png",{0.0f,0.0f},{35.0f,63.0f}},
        {"digits-small/9","parts/digits-small/9.png",{0.0f,0.0f},{35.0f,63.0f}},
        {"digits-small/slash","parts/digits-small/slash.png",{0.0f,0.0f},{35.0f,63.0f}},
    };
    auto* textures=TextureManager::GetInstance();
    for (const auto& asset:assets) {
        const std::string path=root+asset.file;
        textures->LoadTexture(path);
        parts_[asset.name]={textures->GetTextureIndexByFilePath(path),asset.position,asset.size};
    }
    textures->LoadTexture(root+"hud-frames.png");
    framesTexture_=textures->GetTextureIndexByFilePath(root+"hud-frames.png");
    textures->LoadTexture("resources/human/white.png");
    whiteTexture_=textures->GetTextureIndexByFilePath("resources/human/white.png");
    textures->LoadTexture("resources/ui/system-error.png");
    errorTexture_=textures->GetTextureIndexByFilePath("resources/ui/system-error.png");
    const auto& errorMetadata=textures->GetMetaData(errorTexture_);
    errorTextureSize_={static_cast<float>(errorMetadata.width),static_cast<float>(errorMetadata.height)};
    for(auto& sprite:pool_) {sprite=std::make_unique<Sprite>();sprite->Initialize(sprites_);sprite->SetTextureSampling(true);}
    Reset();
}
void PlayerHudController::BindState(StageCombatState* c,EnemyWaveManager* w,RailShooterCameraRig* r){combat_=c;waves_=w;rig_=r;}
void PlayerHudController::Finalize(){for(auto& sprite:pool_)sprite.reset();parts_.clear();sequence_=nullptr;death_=nullptr;combat_=nullptr;waves_=nullptr;rig_=nullptr;}
void PlayerHudController::Reset(){topReveal_=bottomReveal_=0;ammoPulse_=0;previousAmmo_=bullets_?bullets_->GetLockAmmo():5;}
void PlayerHudController::Update(float dt){
    topReveal_=sequence_?sequence_->GetHudTopReveal():1;
    bottomReveal_=sequence_?sequence_->GetHudBottomReveal():1;
    ammoPulse_=(std::max)(0.0f,ammoPulse_-std::clamp(dt,0.0f,.1f));
    const int ammo=bullets_?bullets_->GetLockAmmo():0;
    if(ammo!=previousAmmo_){previousAmmo_=ammo;ammoPulse_=.20f;}
}
Vector2 PlayerHudController::Map(Vector2 p,int group) const {
    float x=left_+p.x*scale_;
    if(group==1)x=left_+width_*.5f+(p.x-960)*scale_;
    if(group==3)x=left_+width_+(p.x-1920)*scale_;
    const float y=group>=2?top_+height_+(p.y-1080)*scale_:top_+p.y*scale_;
    return {x+jitter_*scale_,y};
}
void PlayerHudController::DrawItem(uint32_t texture,Vector2 position,Vector2 size,int group,float alpha,Vector2 origin,Vector2 uvSize,bool frame){
    if(used_>=pool_.size() || alpha<=0 || size.x<=0 || size.y<=0)return;
    auto& sprite=*pool_[used_++];sprite.SetTextureIndex(texture);
    sprite.SetTextureLeftTop(origin);sprite.SetTextureSize(uvSize.x>0?uvSize:size);
    sprite.SetCanvasSize({sprites_->GetDxCommon()->GetBackBufferViewport().Width,sprites_->GetDxCommon()->GetBackBufferViewport().Height});
    sprite.SetPosition(Map(position,group));sprite.SetSize({size.x*scale_,size.y*scale_});
    sprite.SetTextureAlphaScale(frame?panelScale_:1,frame?frameAlpha_:1);
    const bool error=death_&&death_->IsActiveOrFinished();
    sprite.SetTextureAlertTint(error);
    const auto color=error?errorColor_:Vector3{1,1,1};
    sprite.SetColor({color.x,color.y,color.z,alpha*groupAlpha_*(frame?1:textAlpha_)*(error?death_->GetHudVisibility()*errorAlpha_:1)});sprite.Update();sprite.Draw();
}
void PlayerHudController::DrawPart(const std::string& name,int group,float alpha,float fill){
    const auto it=parts_.find(name);if(it==parts_.end() || fill<=0)return;
    const auto& p=it->second;fill=std::clamp(fill,0.0f,1.0f);
    DrawItem(p.texture,p.position,{p.size.x*fill,p.size.y},group,alpha,{}, {p.size.x*fill,p.size.y});
}
void PlayerHudController::DrawNumber(const std::string& text,Vector2 baseline,bool smallDigits,int group,bool right){
    const float advance=smallDigits?21.9628125f:31.846078f;
    if(right)baseline.x-=advance*static_cast<float>(text.size());
    Vector2 origin{baseline.x-5.2173913f,baseline.y-(smallDigits?41.6973913f:58.1133913f)};
    for(char c:text){if(c!=' '){std::string name=smallDigits?"digits-small/":"digits-score/";name+=c=='/'?"slash":std::string(1,c);const auto& p=parts_.at(name);DrawItem(p.texture,origin,p.size,group,1);}origin.x+=advance;}
}
void PlayerHudController::DrawGroup(int group){
    const float reveal=group<2?topReveal_:bottomReveal_;
    if(reveal<=0)return;
    const Vector2 framePos[]{{0,0},{600,0},{0,900},{1440,900}};
    const Vector2 frameSize[]{{600,160},{850,160},{940,180},{480,180}};
    jitter_=!reducedMotion_&&reveal<1?std::sin(reveal*47)*2.5f*(1-reveal):0;
    groupAlpha_=Smooth(reveal);
    if(!reducedMotion_&&reveal<1){const float notch=std::fabs(reveal-.23f)<.025f||std::fabs(reveal-.53f)<.025f?.82f:1;groupAlpha_*=notch;}
    auto* dx=sprites_->GetDxCommon();const auto pos=Map(framePos[group],group);
    const float scan=reducedMotion_?1:std::clamp(reveal*1.25f,0.0f,1.0f);
    D3D12_RECT clip{static_cast<LONG>((std::max)(left_,pos.x)),static_cast<LONG>((std::max)(top_,pos.y)),
        static_cast<LONG>((std::min)(left_+width_,pos.x+frameSize[group].x*scale_*scan)),
        static_cast<LONG>((std::min)(top_+height_,pos.y+frameSize[group].y*scale_))};
    dx->GetCommandList()->RSSetScissorRects(1,&clip);
    DrawItem(framesTexture_,framePos[group],frameSize[group],group,1,framePos[group],frameSize[group],true);
    if(group==0){DrawPart("score-label",0,1);std::ostringstream score;score<<std::setw(5)<<std::setfill('0')<<(combat_?combat_->GetScore():0);DrawNumber(score.str(),{44.347826f,112.173913f},false,0);}
    if(group==1){DrawPart("progress-label",1,1);DrawPart("progress-track",1,1);float progress=waves_?waves_->GetStageProgress():0;if(rig_&&rig_->HasStageRailProgress())progress=waves_&&waves_->IsStageComplete()?1:(std::min)(rig_->GetStageRailProgress(),.99f);DrawPart("progress-fill",1,1,progress);}
    if(group==2){
        DrawPart("lock-label",2,1);const int ammo=bullets_?bullets_->GetLockAmmo():0;
        for(int i=0;i<5;++i){const auto& p=parts_.at(i<ammo?"ammo-slot-on":"ammo-slot-off");DrawItem(p.texture,{42+i*53.906087f,968},p.size,2,1);}
        DrawPart("missile-icon",2,1);DrawNumber(std::to_string(ammo)+" / 5",{355.61739f,999.13043f},true,2);
        DrawPart("evade-label",2,1);const bool active=boost_&&boost_->IsEvadeActive(),waiting=boost_&&boost_->IsCoolingDown();
        DrawPart(active?"evade-state-safe":waiting?"evade-state-wait":"evade-state-ready",2,1);
        if(waiting)DrawNumber(std::to_string(static_cast<int>(std::ceil(boost_->GetCooldownRemaining()))),{865,955},true,2);
        DrawPart("evade-track",2,1);DrawPart(active?"evade-fill-active":"evade-fill-ready",2,1,boost_?boost_->GetBoostGaugeRate():1);
        if(ammoPulse_>0){const auto& p=parts_.at("ammo-slot-on");DrawItem(p.texture,{42+std::clamp(ammo,0,4)*53.906087f,968},p.size,2,ammoPulse_*1.2f);}
    }
    if(group==3){DrawPart("hp-label",3,1);const int hp=damage_?damage_->GetHp():0,maxHp=damage_?damage_->GetMaxHp():0;DrawNumber(std::to_string(hp)+" / "+std::to_string(maxHp),{1875.65217f,954.78261f},true,3,true);DrawPart("hp-track",3,1);DrawPart("hp-fill",3,1,maxHp>0?static_cast<float>(hp)/maxHp:0);}
    DrawSystemErrors(group);
    if(!reducedMotion_&&reveal<1){
        // Two thin, local scan/electric fragments. No full-screen post effect or fabricated values.
        DrawItem(whiteTexture_,{framePos[group].x+12,framePos[group].y+15+reveal*(frameSize[group].y-30)}, {frameSize[group].x*.48f,1.5f},group,.20f,{}, {1,1});
    }
}
void PlayerHudController::DrawSystemErrors(int group){
    if(!death_||!death_->IsActiveOrFinished()||death_->GetHudVisibility()<=0)return;
    const Vector2 origins[]{{0,0},{600,0},{0,900},{1440,900}};
    const int count=std::clamp(errorCount_,0,32);
    const int rows=count/4+(group<count%4?1:0);
    const float time=death_->GetElapsedTime();
    for(int i=0;i<rows;++i){
        const float phase=time*errorBlinkSpeed_+i*.31f+group*.63f;
        const float width=(group==1?300.0f:230.0f)*std::clamp(errorTextScale_,.5f,1.5f)*(i%3==1?.78f:1);
        const float shift=reducedMotion_?0:std::sin(phase*9)*errorJitter_;
        const float crop=!reducedMotion_&&std::fmod(phase,1.0f)<.12f?.72f:1;
        const float alpha=reducedMotion_?.85f:.65f+.35f*std::fabs(std::sin(phase*3.14159f));
        DrawItem(errorTexture_,{origins[group].x+16+static_cast<float>((i*53)%97)+shift,origins[group].y+12+i*20.0f},
            {width*crop,22*errorTextScale_},group,alpha,{}, {errorTextureSize_.x*crop,errorTextureSize_.y});
    }
}
void PlayerHudController::Draw(){auto v=sprites_->GetDxCommon()->GetBackBufferViewport();DrawInRect(0,0,v.Width,v.Height);}
void PlayerHudController::DrawInRect(float l,float t,float w,float h){
    if(!enabled_||!sprites_||w<=1||h<=1)return;
    if(death_&&death_->IsActiveOrFinished()&&death_->GetHudVisibility()<=0)return;
    left_=l;top_=t;width_=w;height_=h;scale_=(std::min)(w/1920,h/1080);used_=0;
    auto* dx=sprites_->GetDxCommon();const auto viewport=dx->GetBackBufferViewport();const auto scissor=dx->GetBackBufferScissorRect();
    dx->GetCommandList()->RSSetViewports(1,&viewport);sprites_->CommonDrawSetting(true);
    for(int group=0;group<4;++group)DrawGroup(group);
    dx->GetCommandList()->RSSetScissorRects(1,&scissor);
}
