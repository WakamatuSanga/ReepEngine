#pragma once
#include "Engine/math/Matrix4x4.h"
#include <cstdint>

class Camera;
namespace RenderStyleComparison {
enum class Mode { Original, Aircraft, AircraftAndBackground };
enum class Target : uint32_t { None, Player, NormalEnemy, BackgroundModel };
enum class Candidate { Initial, Emphasized, CloudSeaTarget };
enum class CloudVariant { Previous, Refined, Adjusted, Depth };
enum class BeamVariant { Previous, Adjusted, Depth };
enum class Diagnostic { None, TargetColor, LightDirection };
struct SurfaceSettings {
    float strength=.65f; int levels=3; float softness=.08f, shadowFloor=.35f;
    Vector3 shadowColor{.65f,.75f,.90f};
    float highlightStrength=.18f, highlightWidth=.15f, rimStrength=.10f, rimPower=3;
    Vector3 rimColor{.70f,.85f,1};
    float shadowThreshold=.32f, lightThreshold=.68f, middleBrightness=.5f;
    bool emphasizedBands=false;
    float bandExponent=8;
};
struct CandidateSettings {
    SurfaceSettings aircraft;
    SurfaceSettings background{.35f,3,.20f,.55f,{.72f,.80f,.92f},.05f,.22f,.02f,3,{.75f,.85f,1}};
    float skyStrength=.22f;
    Vector3 skyZenith{.12f,.40f,.77f},skyHorizon{.56f,.76f,.94f};
    float cloudStrength=.25f;
    Vector3 cloudShadow{.72f,.82f,1},cloudHighlight{1.02f,1.015f,1};
    float cloudContrast=0;
};
struct CloudCompositionSettings {
    bool enableCumulus=true,enableSea=true;
    float strength=1,tileSize=155,towerHeight=46,baseHeight=-31,scale=1,towerCoverage=.82f;
    Vector2 worldOffset{};
    Vector3 shadowColor{.13f,.34f,.65f},lightColor{1,.99f,.92f};
    float seaBelow=34,seaThickness=44,seaWidth=150,seaDepth=205,seaDensity=.62f;
    float resolution=.25f,viewSteps=1;
    float macroRelief=5.5f,mediumRelief=1.8f,fineRelief=.20f;
    float layoutVariation=.9f,bankFill=.65f;
    float seaNoiseMultiplier=1.4f,seaCoreWidth=.065f,seaShadowLift=.30f;
    Vector3 seaShadowColor{.22f,.47f,.76f},seaLightColor{1,.99f,.94f};
    int farTraceSteps=48;
    bool depthLayout=false,volumetricBanks=true;
    float bankSpacing=52, corridorHalfWidth=13, bankScale=1, bankDensity=1;
    float nearVolumeRange=105,highCloudStrength=.26f;
    int depthFarSteps=48;
    int seaShapeMode=1;
    float seaShadeContrast=1;
};
CloudCompositionSettings MakeRefinedCloudSettings();
CloudCompositionSettings MakeAdjustedCloudSettings();
CloudCompositionSettings MakeDepthCloudSettings();
struct NormalBeamSettings {
    bool opaqueCore=false;
    float opacity=.90f,brightness=1.25f,haloOpacity=.10f;
    bool solidPrism=false;
    float widthScale=1.75f,lengthScale=1.15f,coreWhiteness=.72f,trailRatio=.30f;
};
struct TargetSettings {
    int stage=3; // 1: aircraft/light/color, 2: normal shots, 3: distant composition
    float playerScale=1.10f, outlinePixels=1.0f;
    Vector3 sunTravelDirection{-.52f,-.78f,.35f},sunColor{1,.97f,.91f};
    float playerBeamLength=.65f, playerBeamWidth=.045f;
    float enemyBeamLength=.55f, enemyBeamWidth=.060f;
    float outerFlameScale=.62f,outerFlameAlpha=.90f;
    CloudCompositionSettings clouds;
    CloudVariant cloudVariant=CloudVariant::Previous;
    CloudCompositionSettings refinedClouds=MakeRefinedCloudSettings();
    CloudCompositionSettings adjustedClouds=MakeAdjustedCloudSettings();
    CloudCompositionSettings depthClouds=MakeDepthCloudSettings();
    BeamVariant beamVariant=BeamVariant::Previous;
    NormalBeamSettings previousBeams;
    NormalBeamSettings adjustedBeams{true,.98f,1.55f,.10f};
    NormalBeamSettings depthBeams{true,.90f,1.30f,.16f,true,1.75f,1.15f,.72f,.30f};
};
CandidateSettings MakeEmphasizedCandidate();
CandidateSettings MakeTargetCandidate();
struct Settings {
    Mode mode=Mode::Original;
    Candidate candidate=Candidate::Initial;
    Diagnostic diagnostic=Diagnostic::None;
    bool player=true,normalEnemies=true,backgroundModels=true;
    CandidateSettings initial,emphasized=MakeEmphasizedCandidate(),target=MakeTargetCandidate();
    TargetSettings presentation;
};
struct SurfaceData { Vector4 control{},shadow{},highlight{},rim{},bands{},sun{},sunColor{}; };
struct BackgroundData { Vector4 control{},zenith{},horizon{},cloudShadow{},cloudHighlight{},sun{},sunColor{},cumulus{},cumulusExtra{},cumulusDistribution{},seaShape{},cumulusShadow{},cumulusLight{},cloudRelief{},cloudLayout{},seaRefinement{},seaContour{},depthControl{},depthStyle{}; };
Settings& GetSettings();
CandidateSettings& GetCandidateSettings();
CloudCompositionSettings& GetCloudSettings();
NormalBeamSettings& GetNormalBeamSettings();
bool IsRefinedClouds();
bool IsTargetPresentation();
bool IsTargetBackground();
float PlayerDisplayScale();
float AircraftOutlinePixels(Target target);
// Capture the configured ordinary view once; rail/camera rotation never recaptures it.
void InitializeGameplay(const Camera& reference);
void CaptureSunFromCamera(const Camera& reference);
SurfaceData ResolveSurface(Target target);
BackgroundData ResolveBackground();
void DrawImGui();
void DrawCloudImGui();
class GameDrawScope {
public:
    GameDrawScope(); ~GameDrawScope();
    GameDrawScope(const GameDrawScope&)=delete;
    GameDrawScope& operator=(const GameDrawScope&)=delete;
private: bool previous_;
};
}
