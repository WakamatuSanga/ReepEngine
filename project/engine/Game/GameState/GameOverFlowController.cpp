#include "GameOverFlowController.h"
#include "PlayerDeathSequenceController.h"
#include "Engine/Core/DebugUiWorkspace.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Core/FrameTimer.h"
#include "Engine/Core/GameViewport.h"
#include "Engine/Graphics/Sprite/Sprite.h"
#include "Engine/Graphics/Sprite/SpriteCommon.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Input/Input.h"
#include "SceneManager.h"
#include "GameScene.h"
#include "TitleScene.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif
namespace {
GameOverFlowController::Settings tuning;
bool Inside(Vector2 p,int button){const float left=button==0?.18f:.55f;return p.x>=left&&p.x<=left+.27f&&p.y>=.65f&&p.y<=.75f;}
bool Held(const Input& input){return input.MouseDown(Input::MouseLeft)||input.PushKey(DIK_RETURN)||input.PushKey(DIK_SPACE)||input.PushKey(DIK_LEFT)||input.PushKey(DIK_RIGHT)||input.PushKey(DIK_A)||input.PushKey(DIK_D);}
}
struct GameOverFlowController::View {
    PlayerDeathSequenceController* death=nullptr;SpriteCommon* sprites=nullptr;GameViewport* viewport=nullptr;Input* input=nullptr;
    std::array<std::unique_ptr<Sprite>,16> pool;
    std::array<uint32_t,3> textures{};
    std::array<uint32_t,4> titleTextures{};
    bool visible=false,armed=false,requested=false,prepared=false;int selected=0;size_t used=0;
    float backgroundAlpha=.58f;
    double titleElapsed=0,nextFaultAt=-1,faultUntil=0,scheduleOrigin=0;
    unsigned titleVariant=0,faultVariant=0;
    std::minstd_rand random{0x47564f52u};
    void ResetTitle(){titleElapsed=0;nextFaultAt=-1;faultUntil=0;scheduleOrigin=0;titleVariant=faultVariant=0;}
    double NextInterval(const Settings& settings){
        const float low=std::clamp(settings.faultIntervalMin,.4f,8.0f);
        const float high=std::clamp(settings.faultIntervalMax,low,8.0f);
        return std::uniform_real_distribution<double>(low,high)(random);
    }
    void AdvanceTitle(double deltaTime,const Settings& settings){
        titleElapsed+=(std::max)(0.0,deltaTime);
        if(!settings.glitchEnabled){titleVariant=0;nextFaultAt=-1;faultUntil=0;scheduleOrigin=titleElapsed;return;}
        const double intro=std::clamp(settings.introSeconds,0.0f,2.0f);
        if(titleElapsed<intro){
            constexpr std::array<double,6> starts{0,.1,.18,.26,.4,.48};
            constexpr std::array<unsigned,6> variants{1,2,1,3,2,3};
            const double time=titleElapsed*.6/intro;
            size_t phase=0;while(phase+1<starts.size()&&time>=starts[phase+1])++phase;
            titleVariant=variants[phase];return;
        }
        if(nextFaultAt<0)nextFaultAt=(std::max)(intro,scheduleOrigin)+NextInterval(settings);
        // Absolute deadlines select the current event even if a frame skips a
        // boundary. Randomness is sampled only when scheduling/starting events.
        while(titleElapsed>=nextFaultAt){
            faultVariant=1+std::uniform_int_distribution<unsigned>(0,2)(random);
            const float gap=std::clamp(settings.faultIntervalMin,.4f,8.0f);
            faultUntil=nextFaultAt+std::clamp(settings.faultSeconds,.01f,(std::min)(.5f,gap*.5f));
            nextFaultAt+=NextInterval(settings);
        }
        titleVariant=titleElapsed<faultUntil?faultVariant:0;
    }
    void Draw(uint32_t texture,Vector2 position,Vector2 size,Vector4 color,bool screenNoise=false,float textureNoise=0){
        if(used>=pool.size())return;auto& sprite=*pool[used++];sprite.SetTextureIndex(texture);sprite.SetTextureLeftTop({});
        const auto& metadata=TextureManager::GetInstance()->GetMetaData(texture);
        sprite.SetTextureSize({static_cast<float>(metadata.width),static_cast<float>(metadata.height)});
        sprite.SetCanvasSize(viewport->GetClientSize());
        sprite.SetScreenNoise(screenNoise,static_cast<float>(std::fmod(titleElapsed,1024.0)),GetTuning().screenNoiseGrainPixels);
        if(textureNoise>0)sprite.SetTextureNoise(true,static_cast<float>(std::fmod(titleElapsed,1024.0)),textureNoise,GetTuning().screenNoiseGrainPixels);
        sprite.SetPosition(position);sprite.SetSize(size);sprite.SetColor(color);sprite.Update();sprite.Draw();
    }
};
GameOverFlowController::Settings& GameOverFlowController::GetTuning(){return tuning;}
GameOverFlowController::GameOverFlowController():view_(std::make_unique<View>()){}
GameOverFlowController::~GameOverFlowController()=default;
void GameOverFlowController::Initialize(PlayerDeathSequenceController* d,SpriteCommon* s,GameViewport* v,Input* i){view_=std::make_unique<View>();view_->death=d;view_->sprites=s;view_->viewport=v;view_->input=i;}
bool GameOverFlowController::PrepareForPresentation(){
    auto& v=*view_;if(v.prepared)return true;if(!v.sprites||!v.viewport||!v.input)return false;
    const char* assets[]{"resources/human/white.png","resources/ui/gameover-restart.png","resources/ui/gameover-title.png"};
    auto* textures=TextureManager::GetInstance();for(size_t i=0;i<v.textures.size();++i){textures->LoadTexture(assets[i]);v.textures[i]=textures->GetTextureIndexByFilePath(assets[i]);}
    const char* titles[]{"resources/ui/gameover-glitch/00-game-over.png","resources/ui/gameover-glitch/01-corrupt-a.png","resources/ui/gameover-glitch/02-corrupt-b.png","resources/ui/gameover-glitch/03-corrupt-c.png"};
    for(size_t i=0;i<v.titleTextures.size();++i){textures->LoadTexture(titles[i]);v.titleTextures[i]=textures->GetTextureIndexByFilePath(titles[i]);}
    for(auto& sprite:v.pool){sprite=std::make_unique<Sprite>();sprite->Initialize(v.sprites);sprite->SetTextureSampling(true);}
    v.prepared=true;return true;
}
void GameOverFlowController::Finalize(){view_=std::make_unique<View>();}
bool GameOverFlowController::IsVisible()const{return view_->visible;}
bool GameOverFlowController::IsInputArmed()const{return view_->armed;}
void GameOverFlowController::Update(){
    auto& v=*view_;if(!v.death||!v.input||!v.viewport||!v.prepared||v.requested)return;
    const bool ready=v.death->IsFinished()&&v.death->HasExploded();
    if(!ready){v.visible=v.armed=false;v.ResetTitle();return;}
    if(!v.visible){v.visible=true;v.armed=false;v.selected=0;v.ResetTitle();v.AdvanceTitle(0,GetTuning());return;}
    v.AdvanceTitle(v.death->GetPresentationDeltaTime(FrameTimer::GetInstance().GetGameplayDeltaTime()),GetTuning());
    if(v.death->IsPreview())return; // Preview cannot accidentally restart or leave the scene.
    if(!v.armed){if(!Held(*v.input)&&v.input->IsMouseStateValid())v.armed=true;return;}
    const bool focus=v.viewport->GetMode()==GameViewport::Mode::FullscreenGame||v.viewport->IsGameViewFocused();
    const bool hover=v.viewport->IsMouseInGameViewport()&&v.viewport->IsGameViewHovered();
    bool editing=false;
#ifdef USE_IMGUI
    editing=ImGui::GetIO().WantTextInput||ImGui::IsAnyItemActive();
#endif
    if(editing)return;
    const auto point=v.viewport->GetMouseNormalizedInGameViewport();
    int clicked=-1;
    if(hover){for(int i=0;i<2;++i)if(Inside(point,i)){v.selected=i;if(v.input->MouseTrigger(Input::MouseLeft))clicked=i;}}
    if(focus){if(v.input->TriggerKey(DIK_LEFT)||v.input->TriggerKey(DIK_A))v.selected=0;if(v.input->TriggerKey(DIK_RIGHT)||v.input->TriggerKey(DIK_D))v.selected=1;
        if(v.input->TriggerKey(DIK_RETURN)||v.input->TriggerKey(DIK_SPACE))clicked=v.selected;}
    if(clicked<0)return;
    v.requested=true;
    if(clicked==0)SceneManager::GetInstance()->ChangeScene(std::make_unique<GameScene>());
    else SceneManager::GetInstance()->ChangeSceneWithTitleClouds(std::make_unique<TitleScene>());
}
void GameOverFlowController::DrawOverlay(){
    auto& v=*view_;if(!v.visible||!v.prepared||!v.viewport)return;
    const auto& r=v.viewport->GetGameViewportRect();if(r.width<=1||r.height<=1)return;
    auto* dx=v.sprites->GetDxCommon();const auto viewport=dx->GetBackBufferViewport();const auto previous=dx->GetBackBufferScissorRect();
    dx->GetCommandList()->RSSetViewports(1,&viewport);v.sprites->CommonDrawSetting(true);
    const auto client=v.viewport->GetClientSize();
    const float sx=viewport.Width/(std::max)(1.0f,client.x),sy=viewport.Height/(std::max)(1.0f,client.y);
    D3D12_RECT clip{static_cast<LONG>(r.left*sx),static_cast<LONG>(r.top*sy),static_cast<LONG>((r.left+r.width)*sx),static_cast<LONG>((r.top+r.height)*sy)};
    dx->GetCommandList()->RSSetScissorRects(1,&clip);v.used=0;
    v.Draw(v.textures[0],{r.left,r.top},{r.width,r.height},{0,0,0,v.backgroundAlpha});
    const auto& settings=GetTuning();
    const float titleWidth=(std::min)(r.width*std::clamp(settings.titleWidthFraction,.35f,.95f),r.height*2.1f);
    const float titleHeight=titleWidth/3;
    v.Draw(v.titleTextures[v.titleVariant],{r.left+r.width*std::clamp(settings.titleCenterX,.1f,.9f)-titleWidth*.5f,
        r.top+r.height*std::clamp(settings.titleCenterY,.2f,.5f)-titleHeight*.5f},{titleWidth,titleHeight},{1,1,1,1},false,
        settings.titleNoiseEnabled?std::clamp(settings.titleNoiseStrength,0.0f,1.0f):0.0f);
    for(int button=0;button<2;++button){const float x=r.left+r.width*(button==0?.18f:.55f),y=r.top+r.height*.65f,w=r.width*.27f,h=r.height*.10f;
        const bool selected=v.selected==button;
        v.Draw(v.textures[0],{x,y},{w,h},selected?Vector4{.15f,.38f,.48f,.98f}:Vector4{.07f,.12f,.17f,.96f});
        const Vector4 edge=selected?Vector4{.55f,.9f,1,1}:Vector4{.38f,.49f,.57f,1};const float border=(std::max)(1.0f,r.height/360);
        v.Draw(v.textures[0],{x,y},{w,border},edge);v.Draw(v.textures[0],{x,y+h-border},{w,border},edge);
        v.Draw(v.textures[0],{x,y},{border,h},edge);v.Draw(v.textures[0],{x+w-border,y},{border,h},edge);
        const auto& metadata=TextureManager::GetInstance()->GetMetaData(v.textures[1+button]);
        const float fit=(std::min)(w*.78f/static_cast<float>(metadata.width),h*.6f/static_cast<float>(metadata.height));
        const Vector2 size{static_cast<float>(metadata.width)*fit,static_cast<float>(metadata.height)*fit};
        v.Draw(v.textures[1+button],{x+(w-size.x)*.5f,y+(h-size.y)*.5f},size,{1,1,1,1});
    }
    if(settings.screenNoiseEnabled&&settings.screenNoiseStrength>0){
        const float strength=std::clamp(settings.screenNoiseStrength,0.0f,.4f)*(v.titleVariant!=0?1.15f:1.0f);
        v.Draw(v.textures[0],{r.left,r.top},{r.width,r.height},{1,1,1,(std::min)(strength,.45f)},true);
    }
    dx->GetCommandList()->RSSetScissorRects(1,&previous);
}
void GameOverFlowController::DrawImGui(){
#ifdef USE_IMGUI
    if(DebugUiWorkspace::BeginPanel("GameScene内GAME OVER")){
        ImGui::Text("表示 %s / 入力解放 %s / 遷移要求 %s",view_->visible?"はい":"いいえ",view_->armed?"済":"待機",view_->requested?"あり":"なし");
        ImGui::SliderFloat("GAME OVER背景の暗さ",&view_->backgroundAlpha,.2f,.85f);
        auto& settings=GetTuning();
        ImGui::Checkbox("GAME OVER文字化け有効",&settings.glitchEnabled);
        ImGui::SliderFloat("見出しの中央X",&settings.titleCenterX,.1f,.9f);
        ImGui::SliderFloat("見出しの中央Y",&settings.titleCenterY,.2f,.5f);
        ImGui::SliderFloat("見出しの幅（画面比）",&settings.titleWidthFraction,.35f,.95f);
        ImGui::SliderFloat("出現時の復旧秒",&settings.introSeconds,0,2);
        ImGui::SliderFloat("故障間隔の最短秒",&settings.faultIntervalMin,.4f,8);
        ImGui::SliderFloat("故障間隔の最長秒",&settings.faultIntervalMax,settings.faultIntervalMin,8);
        ImGui::SliderFloat("短い故障の継続秒",&settings.faultSeconds,.01f,.5f);
        ImGui::Text("見出し時計 %.2f秒 / 画像 %u",view_->titleElapsed,view_->titleVariant);
        ImGui::Checkbox("GAME OVER画面全体のノイズ",&settings.screenNoiseEnabled);
        ImGui::SliderFloat("画面ノイズの強さ",&settings.screenNoiseStrength,0,.4f);
        ImGui::SliderFloat("画面ノイズの粒サイズ",&settings.screenNoiseGrainPixels,1,6);
        ImGui::Checkbox("GAME OVER文字のノイズ",&settings.titleNoiseEnabled);
        ImGui::SliderFloat("文字ノイズの強さ",&settings.titleNoiseStrength,0,1);
        ImGui::TextWrapped("文字の不透明な部分だけに細かなノイズを付けます。画面全体の弱いノイズとは個別に調整できます。");
        ImGui::TextWrapped("画面全体の砂嵐はGAME OVER表示中だけ。文字化け中は少し強まり、Pause中はノイズも停止します。");
        ImGui::TextWrapped("出現0.6秒・故障0.1秒が目安、細かな間隔は仮調整です。見出しだけを切り替え、ボタンの表示・操作開始は変えません。Pauseで時計も停止します。");
        ImGui::TextWrapped("製品画面はSprite。爆散後のみ表示し、入力解放を待ってGame View内で決定します。");
    }DebugUiWorkspace::EndPanel();
#endif
}
