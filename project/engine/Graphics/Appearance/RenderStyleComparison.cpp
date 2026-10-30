#include "RenderStyleComparison.h"
#include "Engine/Core/DebugUiWorkspace.h"
#include "Engine/Graphics/Camera/Camera.h"
#include <algorithm>
#include <cmath>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace RenderStyleComparison {
namespace {
Settings settings;
bool gameDraw=false,sunCaptured=false;
Vector3 referenceSun{-.52f,-.78f,.35f};
Vector4 Color(Vector3 c,float w=0) { return {c.x,c.y,c.z,w}; }
Vector3 Unit(Vector3 v) {const float n=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);return n>.0001f?Vector3{v.x/n,v.y/n,v.z/n}:Vector3{0,-1,0};}
}
Settings& GetSettings(){return settings;}
CandidateSettings MakeEmphasizedCandidate(){
    CandidateSettings r;auto& s=r.aircraft;
    s.strength=1;s.softness=.01f;s.shadowFloor=.22f;s.shadowColor={.18f,.28f,.46f};
    s.highlightStrength=.06f;s.highlightWidth=.06f;s.rimStrength=.42f;s.rimPower=2;
    s.shadowThreshold=.18f;s.lightThreshold=.85f;s.middleBrightness=.50f;s.emphasizedBands=true;
    r.skyStrength=.80f;r.skyZenith={.025f,.18f,.58f};r.skyHorizon={.30f,.64f,1};
    r.cloudStrength=.88f;r.cloudShadow={.40f,.57f,.82f};r.cloudHighlight={1.04f,1.035f,1.02f};r.cloudContrast=1.8f;
    return r;
}
CandidateSettings MakeTargetCandidate(){
    auto r=MakeEmphasizedCandidate();auto& s=r.aircraft;
    s.bandExponent=1;s.shadowThreshold=.24f;s.lightThreshold=.64f;s.middleBrightness=.74f;
    s.shadowFloor=.38f;s.shadowColor={.28f,.44f,.68f};s.softness=.025f;
    s.rimStrength=.12f;s.rimPower=3;s.highlightStrength=.045f;s.highlightWidth=.04f;
    r.skyStrength=1;r.skyZenith={.003f,.025f,.55f};r.skyHorizon={.05f,.63f,1};
    r.cloudStrength=.92f;r.cloudShadow={.32f,.58f,.86f};r.cloudHighlight={1.07f,1.055f,1.01f};r.cloudContrast=1.2f;
    return r;
}
CloudCompositionSettings MakeRefinedCloudSettings() {
    CloudCompositionSettings c;
    c.baseHeight=-34;c.towerHeight=46;c.towerCoverage=.70f;
    c.shadowColor={.22f,.46f,.74f};c.lightColor={1,.99f,.94f};
    c.seaBelow=40;c.seaThickness=40;c.seaWidth=180;c.seaDepth=260;
    return c;
}
CloudCompositionSettings& GetCloudSettings() {
    return settings.presentation.cloudVariant==CloudVariant::Refined
        ? settings.presentation.refinedClouds : settings.presentation.clouds;
}
bool IsRefinedClouds() {
    return IsTargetBackground()&&settings.presentation.stage>=3
        &&settings.presentation.cloudVariant==CloudVariant::Refined;
}
CandidateSettings& GetCandidateSettings(){return settings.candidate==Candidate::Initial?settings.initial:settings.candidate==Candidate::Emphasized?settings.emphasized:settings.target;}
bool IsTargetPresentation(){return settings.mode!=Mode::Original&&settings.candidate==Candidate::CloudSeaTarget;}
bool IsTargetBackground(){return IsTargetPresentation()&&settings.mode==Mode::AircraftAndBackground;}
float PlayerDisplayScale(){return IsTargetPresentation()?std::clamp(settings.presentation.playerScale,.9f,1.2f):1;}
float AircraftOutlinePixels(Target t){return gameDraw&&IsTargetPresentation()&&((t==Target::Player&&settings.player)||(t==Target::NormalEnemy&&settings.normalEnemies))?std::clamp(settings.presentation.outlinePixels,0.f,2.f):0;}
void CaptureSunFromCamera(const Camera& c){
    // Light TRAVELS down-left and forward; -direction points up-right toward the sun.
    const auto& m=c.GetWorldMatrix();const Vector3 local=Unit({-.52f,-.78f,.35f});
    settings.presentation.sunTravelDirection=Unit({local.x*m.m[0][0]+local.y*m.m[1][0]+local.z*m.m[2][0],local.x*m.m[0][1]+local.y*m.m[1][1]+local.z*m.m[2][1],local.x*m.m[0][2]+local.y*m.m[1][2]+local.z*m.m[2][2]});referenceSun=settings.presentation.sunTravelDirection;sunCaptured=true;
}
void InitializeGameplay(const Camera& c){if(!sunCaptured)CaptureSunFromCamera(c);}
GameDrawScope::GameDrawScope():previous_(gameDraw){gameDraw=true;}
GameDrawScope::~GameDrawScope(){gameDraw=previous_;}
SurfaceData ResolveSurface(Target t){
    if(!gameDraw||settings.mode==Mode::Original)return {};
    const bool aircraft=(t==Target::Player&&settings.player)||(t==Target::NormalEnemy&&settings.normalEnemies);
    const bool background=t==Target::BackgroundModel&&settings.backgroundModels&&settings.mode==Mode::AircraftAndBackground;
    if(!aircraft&&!background)return {};
    const auto& bank=GetCandidateSettings();const auto& s=aircraft?bank.aircraft:bank.background;
    const float d=static_cast<float>(settings.diagnostic),low=std::clamp(s.shadowThreshold,.05f,.9f);
    SurfaceData r{{std::clamp(s.strength,0.f,1.f),s.levels==2?2.f:3.f,std::clamp(s.softness,.01f,.4f),std::clamp(s.shadowFloor,.1f,1.f)},Color(s.shadowColor,d>0?static_cast<float>(t):0),{s.highlightStrength,s.highlightWidth,s.rimStrength,s.rimPower},Color(s.rimColor,d),{low,std::clamp(s.lightThreshold,low+.02f,.995f),s.middleBrightness,s.emphasizedBands?s.bandExponent:0}};
    if(IsTargetPresentation()){r.sun=Color(Unit(settings.presentation.sunTravelDirection),1);r.sunColor=Color(settings.presentation.sunColor,1);}
    return r;
}
BackgroundData ResolveBackground(){
    if(!gameDraw||settings.mode!=Mode::AircraftAndBackground)return {};
    const auto& s=GetCandidateSettings();BackgroundData r{{s.skyStrength,0,0,0},Color(s.skyZenith),Color(s.skyHorizon),Color(s.cloudShadow,s.cloudStrength),Color(s.cloudHighlight,s.cloudContrast)};
    if(IsTargetBackground()){
        r.sun=Color(Unit(settings.presentation.sunTravelDirection),1);r.sunColor=Color(settings.presentation.sunColor,1);
        const auto& p=settings.presentation;const auto& c=GetCloudSettings();
        r.cumulus={p.stage>=3&&c.enableCumulus?c.strength:0,c.tileSize,c.towerHeight,c.seaDensity};
        r.cumulusExtra={c.worldOffset.x,c.worldOffset.y,c.baseHeight,c.scale};
        r.cumulusDistribution={c.towerCoverage,c.enableSea?1.f:0.f,c.seaDensity,p.stage>=3?1.f:0.f};
        r.seaShape={c.seaBelow,c.seaThickness,c.seaWidth,c.seaDepth};
        r.cumulusShadow=Color(c.shadowColor);r.cumulusLight=Color(c.lightColor);
        if(IsRefinedClouds()) {
            r.cloudRelief={c.macroRelief,c.mediumRelief,c.fineRelief,1};
            r.cloudLayout={c.layoutVariation,c.bankFill,0,0};
            r.seaRefinement={c.seaNoiseMultiplier,c.seaCoreWidth,c.seaShadowLift,1};
            r.cloudShadow=Color(c.seaShadowColor,1);r.cloudHighlight=Color(c.seaLightColor,1);
        }
    }return r;
}
#ifdef USE_IMGUI
namespace {
void DrawCloudControls(){
    auto& p=settings.presentation;
        if(ImGui::CollapsingHeader("積雲・下方の雲海（今回追加）",ImGuiTreeNodeFlags_DefaultOpen)){
            if(!IsTargetBackground()||p.stage<3){
                ImGui::TextWrapped("この項目は目標C・段階3へ即時反映します。");
                if(ImGui::Button("積雲・雲海を表示して調整")){settings.mode=Mode::AircraftAndBackground;settings.candidate=Candidate::CloudSeaTarget;p.stage=3;settings.diagnostic=Diagnostic::None;}
            }
            int variant=static_cast<int>(p.cloudVariant);
            const char* variants[]{"前回：2026-10-28", "改良：大きな起伏・非対称配置"};
            if(ImGui::Combo("雲の比較候補",&variant,variants,2))p.cloudVariant=static_cast<CloudVariant>(variant);
            if(ImGui::Button("改良雲Cを表示")){settings.mode=Mode::AircraftAndBackground;settings.candidate=Candidate::CloudSeaTarget;p.stage=3;p.cloudVariant=CloudVariant::Refined;settings.diagnostic=Diagnostic::None;}
            ImGui::SameLine();if(ImGui::Button("前回の雲Cを表示")){settings.mode=Mode::AircraftAndBackground;settings.candidate=Candidate::CloudSeaTarget;p.stage=3;p.cloudVariant=CloudVariant::Previous;settings.diagnostic=Diagnostic::None;}
            auto& c=GetCloudSettings();const bool refined=p.cloudVariant==CloudVariant::Refined;
            if(ImGui::Button("雲構成だけ初期値へ"))c=refined?MakeRefinedCloudSettings():CloudCompositionSettings{};
            ImGui::TextWrapped("変更は実際の描画へ即時反映。Restartでも調整値を保持し、アプリ終了で既定値へ戻ります。元の雲JSONは変更しません。");
            if(ImGui::TreeNodeEx("遠景の積雲",ImGuiTreeNodeFlags_DefaultOpen)){
                ImGui::Checkbox("積雲を表示",&c.enableCumulus);ImGui::SliderFloat("積雲の濃さ",&c.strength,0,1);
                ImGui::SliderFloat("塔の高さ",&c.towerHeight,15,100);ImGui::SliderFloat("塊の大きさ",&c.scale,.5f,1.5f);
                ImGui::SliderFloat("塊の間隔",&c.tileSize,100,240);ImGui::SliderFloat("背の高い雲の割合",&c.towerCoverage,0,1);
                ImGui::SliderFloat("積雲の基準高度（world Y）",&c.baseHeight,-90,30);
                ImGui::DragFloat2("積雲の配置（world X/Z）",&c.worldOffset.x,1,-300,300);
                ImGui::ColorEdit3("積雲の影色",&c.shadowColor.x);ImGui::ColorEdit3("積雲の明部色",&c.lightColor.x);
                if(refined){
                    ImGui::SliderFloat("積雲の大きな起伏",&c.macroRelief,0,12);
                    ImGui::SliderFloat("積雲の中くらいの起伏",&c.mediumRelief,0,5);
                    ImGui::SliderFloat("積雲の細部の強さ",&c.fineRelief,0,1);
                    ImGui::SliderFloat("高さ・幅・奥行きのばらつき",&c.layoutVariation,0,1);
                    ImGui::SliderFloat("低い雲のつながり",&c.bankFill,0,1);
                }
                ImGui::TreePop();
            }
            if(ImGui::TreeNodeEx("下方の雲海",ImGuiTreeNodeFlags_DefaultOpen)){
                ImGui::Checkbox("雲海を表示",&c.enableSea);ImGui::SliderFloat("カメラから下への距離",&c.seaBelow,10,120);
                ImGui::SliderFloat("雲海の厚み",&c.seaThickness,10,100);ImGui::SliderFloat("雲海の横幅",&c.seaWidth,60,400);
                ImGui::SliderFloat("雲海の奥行き",&c.seaDepth,80,500);ImGui::SliderFloat("雲海の濃さ",&c.seaDensity,0,1);
                if(refined){
                    ImGui::ColorEdit3("雲海の青い影色",&c.seaShadowColor.x);ImGui::ColorEdit3("雲海の白い明部色",&c.seaLightColor.x);
                    ImGui::SliderFloat("雲海の塊の細かさ",&c.seaNoiseMultiplier,.75f,2);
                    ImGui::SliderFloat("雲海の輪郭の柔らかさ",&c.seaCoreWidth,.03f,.16f);
                    ImGui::SliderFloat("雲海の影の明るさ",&c.seaShadowLift,.1f,.65f);
                }else{
                    ImGui::ColorEdit3("雲海の影色（色倍率）",&settings.target.cloudShadow.x);ImGui::ColorEdit3("雲海の明部色（色倍率）",&settings.target.cloudHighlight.x);
                }
                ImGui::TreePop();
            }
            if(ImGui::TreeNode("雲海の画質・負荷")){
                ImGui::SliderFloat("近景雲の解像度倍率",&c.resolution,.125f,.5f);ImGui::SliderFloat("近景雲のViewステップ倍率",&c.viewSteps,.25f,1);
                ImGui::TextWrapped("既存の固定解像度比較中はそのサンプリング設定を優先します。");ImGui::TreePop();
            }
        }
}
}
#endif
void DrawCloudImGui(){
#ifdef USE_IMGUI
    if(DebugUiWorkspace::BeginPanel("積雲・下方の雲海")){
        ImGui::TextWrapped("今回追加した立体積雲と下方の雲海を調整します。カメラ・レール・Flowの位相は変更しません。");
        if(ImGui::Button("現行Aへ戻す（全表示設定）")){settings.mode=Mode::Original;settings.diagnostic=Diagnostic::None;}
        DrawCloudControls();
    }DebugUiWorkspace::EndPanel();
#endif
}
void DrawImGui(){
#ifdef USE_IMGUI
    if(DebugUiWorkspace::BeginPanel("画風比較")){
        int mode=static_cast<int>(settings.mode),candidate=static_cast<int>(settings.candidate);
        const char* modes[]{"A：現行の設定へ戻す","B：機体（目標時は表示倍率・弾・排気も）","C：機体＋背景"};
        const char* banks[]{"初回候補","強調候補（前回）","雲海の目標候補"};
        if(ImGui::Combo("比較モード",&mode,modes,3)){settings.mode=static_cast<Mode>(mode);settings.diagnostic=Diagnostic::None;}
        if(ImGui::Combo("候補",&candidate,banks,3)){settings.candidate=static_cast<Candidate>(candidate);settings.diagnostic=Diagnostic::None;}
        if(ImGui::Button("現行Aへ設定を復元")){settings.mode=Mode::Original;settings.diagnostic=Diagnostic::None;}
        ImGui::SameLine();if(ImGui::Button("目標Cを試す")){settings.mode=Mode::AircraftAndBackground;settings.candidate=Candidate::CloudSeaTarget;settings.diagnostic=Diagnostic::None;}
        ImGui::TextWrapped("Aでは画風・太陽・自機表示倍率・通常弾・遠景構成を戻します。元JSON/画像は変更せず、比較はセッション内のみ。初回／強調バンクを保持。既存の排気・残像は巻き戻さず寿命まで連続して入れ替わります。");
        if(ImGui::Button("初回値を復元"))settings.initial={};ImGui::SameLine();
        if(ImGui::Button("強調値を復元"))settings.emphasized=MakeEmphasizedCandidate();
        if(ImGui::Button("目標値を復元")){settings.target=MakeTargetCandidate();settings.presentation={};settings.presentation.sunTravelDirection=referenceSun;}
        ImGui::Checkbox("対象：自機",&settings.player);ImGui::SameLine();ImGui::Checkbox("対象：通常敵",&settings.normalEnemies);
        auto& p=settings.presentation;
        if(settings.candidate==Candidate::CloudSeaTarget){
            ImGui::SliderInt("段階（1:機体/配色 2:通常弾 3:積雲）",&p.stage,1,3);
            if(ImGui::CollapsingHeader("機体・太陽・通常弾・排気")){
            ImGui::SliderFloat("自機の表示倍率",&p.playerScale,.9f,1.2f);ImGui::SliderFloat("輪郭 px @実描画解像度",&p.outlinePixels,0,2);
            ImGui::DragFloat3("太陽光のワールド進行方向",&p.sunTravelDirection.x,.01f,-1,1);ImGui::ColorEdit3("共通太陽色",&p.sunColor.x);
            ImGui::TextWrapped("光の進行方向の反対側が太陽。初期通常カメラから一度だけ設定。レール・カメラ回転に追従しません。");
            ImGui::SliderFloat("自機通常弾 長さ",&p.playerBeamLength,.1f,2);ImGui::SliderFloat("自機通常弾 幅",&p.playerBeamWidth,.01f,.15f);
            ImGui::SliderFloat("敵通常弾 長さ",&p.enemyBeamLength,.1f,2);ImGui::SliderFloat("敵通常弾 幅",&p.enemyBeamWidth,.01f,.15f);

            ImGui::SliderFloat("排気の外側粒子 サイズ倍率",&p.outerFlameScale,.25f,1);
            ImGui::SliderFloat("排気の外側粒子 不透明度倍率",&p.outerFlameAlpha,.1f,1);
            }
        }
        DrawCloudControls();
        auto& b=GetCandidateSettings();
        if(ImGui::TreeNode("機体の色面")){auto& s=b.aircraft;ImGui::SliderFloat("適用率",&s.strength,0,1);ImGui::ColorEdit3("影色",&s.shadowColor.x);ImGui::SliderFloat("影明度",&s.shadowFloor,.1f,1);ImGui::SliderFloat("中間明度",&s.middleBrightness,.15f,.9f);ImGui::SliderFloat("リム強度",&s.rimStrength,0,.5f);ImGui::TreePop();}
        if(ImGui::TreeNode("空と雲の色")){ImGui::ColorEdit3("上空",&b.skyZenith.x);ImGui::ColorEdit3("地平線",&b.skyHorizon.x);ImGui::ColorEdit3("雲影",&b.cloudShadow.x);ImGui::ColorEdit3("雲明部",&b.cloudHighlight.x);ImGui::TreePop();}
        if(ImGui::TreeNode("適用診断")){int d=static_cast<int>(settings.diagnostic);const char* names[]{"通常","自機:緑／通常敵:紫","N dot L"};if(ImGui::Combo("診断",&d,names,3))settings.diagnostic=static_cast<Diagnostic>(d);ImGui::TreePop();}
        ImGui::TextWrapped("HUD・中ボス・スキニング・タイトル・ロック弾・排気・粒子は機体画風の対象外。現在の起動既定はAです。");
    }DebugUiWorkspace::EndPanel();
#endif
}
}
