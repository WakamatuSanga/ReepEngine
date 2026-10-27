#include "PlayerDeathSequenceController.h"
#include "Engine/Core/DebugUiWorkspace.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Animation/Skeleton.h"
#include "Engine/Game/Boss/Kraken/KrakenTentacleChainUtility.h"
#include "Engine/Game/Boss/Kraken/KrakenTentacleWaveEncounterController.h"
#include "Engine/Game/Camera/CameraShakeController.h"
#include "Engine/Game/Effect/CombatEffectController.h"
#include "Engine/Game/Effect/GpuParticleEffectPlayer.h"
#include "Engine/Game/Enemy/Enemy.h"
#include "Engine/Game/Enemy/EnemyBullet.h"
#include "Engine/Game/Player/Player.h"
#include "Engine/Game/Player/PlayerJetExhaustBeamRenderer.h"
#include "Engine/Game/Player/BoostController.h"
#include "Engine/Game/Player/PlayerDamageFeedbackController.h"
#include "Engine/Game/RailShooter/EnemyWaveManager.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Model/GltfSkeletonLoader.h"
#include "Engine/Graphics/Model/GltfSkinnedModel.h"
#include "Engine/Graphics/Model/ModelManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Graphics/Object3d/Object3d.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include <algorithm>
#include <array>
#include <cmath>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
PlayerDeathSequenceController::Settings tuning;
constexpr float kPi = 3.14159265f;
float Smooth(float t) { t=std::clamp(t,0.0f,1.0f); return t*t*(3-2*t); }
Vector3 Add(Vector3 a,Vector3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vector3 Sub(Vector3 a,Vector3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vector3 Mul(Vector3 a,float s){return {a.x*s,a.y*s,a.z*s};}
float Dot(Vector3 a,Vector3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
float Length(Vector3 v){return std::sqrt(Dot(v,v));}
Vector3 Unit(Vector3 v,Vector3 fallback={0,0,1}){const float n=Length(v);return n>.0001f?Mul(v,1/n):fallback;}
Vector3 Lerp(Vector3 a,Vector3 b,float t){return Add(a,Mul(Sub(b,a),t));}
Vector3 Position(const Matrix4x4& m){return {m.m[3][0],m.m[3][1],m.m[3][2]};}
Matrix4x4 FromTo(Vector3 a,Vector3 b){
    a=Unit(a);b=Unit(b);const float c=std::clamp(Dot(a,b),-1.0f,1.0f);
    auto axis=MatrixMath::Cross(a,b);float s=Length(axis);
    if(s<.0001f){if(c>0)return MatrixMath::MakeIdentity4x4();axis=Unit(MatrixMath::Cross(a,{1,0,0}),{0,1,0});s=0;}
    else axis=Mul(axis,1/s);
    const float v[3]{axis.x,axis.y,axis.z};auto r=MatrixMath::MakeIdentity4x4();
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)r.m[i][j]=(i==j?c:0)+(1-c)*v[i]*v[j];
    r.m[0][1]+=s*axis.z;r.m[0][2]-=s*axis.y;r.m[1][0]-=s*axis.z;
    r.m[1][2]+=s*axis.x;r.m[2][0]+=s*axis.y;r.m[2][1]-=s*axis.x;return r;
}
}
struct PlayerDeathSequenceController::Flight {
    Player* player=nullptr; Camera* camera=nullptr; BoostController* boost=nullptr;
    PlayerDamageFeedbackController* health=nullptr; GpuParticleEffectPlayer* effects=nullptr;
    CombatEffectController* combat=nullptr; EnemyWaveManager* waves=nullptr;
    KrakenTentacleWaveEncounterController* encounter=nullptr;
    Object3dCommon* objects=nullptr; ModelCommon* models=nullptr;
    Settings settings; Camera originalCamera; Transform originalPose{};
    Vector3 forward{0,0,1},right{1,0,0},up{0,1,0},center{},enemyPosition{},muzzle{};
    Route route=Route::RearEnemy; std::string wave;
    std::unique_ptr<Enemy> enemy; std::unique_ptr<EnemyBullet> bullet;
    std::unique_ptr<PlayerJetExhaustBeamRenderer> bulletGlow;
    std::vector<PlayerJetExhaustBeamRenderer::Vertex> bulletGlowVertices;
    std::unique_ptr<Skeleton> skeleton; std::unique_ptr<GltfSkinnedModel> tentacles;
    std::unique_ptr<Object3d> tentacleObject;
    std::vector<KrakenTentacleChain> chains;
    std::vector<Matrix4x4> bindMatrices, bindLocalMatrices;
    std::array<std::unique_ptr<Object3d>,24> debris;
    Model* debrisModel=nullptr;
    GpuParticleEffectPlayer::FollowHandle smoke=0;
    bool prepared=false,captured=false,preview=false,paused=false,fired=false,hit=false,exploded=false;
    float unit=1;
    float speed=1,fireTime=0,hitTime=0,explosionTime=0,finishTime=0;
    float CameraSeconds()const{return route==Route::KrakenGrip?settings.bossCameraSeconds:settings.cameraSeconds;}
    Vector3 PlaceWorld(float x,float y,float z)const{return Add(center,Add(Mul(right,x),Add(Mul(up,y),Mul(forward,z))));}
    Vector3 Place(float x,float y,float z)const{return PlaceWorld(x*unit,y*unit,z*unit);}
    Vector3 TentaclePoint(float u,float side,float rise,float approach,float squeeze)const{
        const float gap=settings.tentacleGripWidth*(1-squeeze*settings.squeezeAmount);
        const float x=side*(settings.tentacleStartSpread*std::pow(1-u,1.4f)
            + (3.2f+(gap-3.2f)*approach)*u+2*settings.tentacleCurl*approach*std::sin(kPi*u));
        const float y=settings.tentacleRootY+(-.35f-settings.tentacleRootY)*u
            +6*settings.tentacleCurl*approach*std::sin(kPi*u)-(1-rise)*9;
        return {x,y,.3f*std::sin(kPi*u)};
    }
    void UpdateTentacles(float time){
        const float t=time-CameraSeconds();
        const float rise=Smooth(t/settings.tentacleRiseSeconds);
        const float approach=Smooth((t-settings.tentacleRiseSeconds)/settings.tentacleApproachSeconds);
        const float squeeze=Smooth((t-settings.tentacleRiseSeconds-settings.tentacleApproachSeconds-settings.gripHoldSeconds)/settings.squeezeSeconds);
        const float scale = settings.tentacleScale;
        for (size_t ci = 0; ci < chains.size(); ++ci) {
            const auto& chain = chains[ci];
            const size_t count = chain.joints.size();
            const bool visible = ci == 0 || ci == 3;
            std::array<Vector3, 10> directions{};
            Vector3 rootPosition{0, -1000, 0};
            if (visible) {
                const float side = ci == 0 ? -1.0f : 1.0f;
                rootPosition = TentaclePoint(1, side, rise, approach, squeeze);
                // Anchor the tip near the hull, then derive the root from the actual
                // scaled bone lengths. The large base continues below the frame.
                for (size_t n = 0; n + 1 < count; ++n) {
                    const float u = (static_cast<float>(n) + .5f) / static_cast<float>(count - 1);
                    directions[n] = Unit(Sub(TentaclePoint(u + .005f, side, rise, approach, squeeze),
                        TentaclePoint(u - .005f, side, rise, approach, squeeze)));
                    const float length = Length(Sub(Position(bindMatrices[chain.joints[n + 1]]),
                        Position(bindMatrices[chain.joints[n]]))) * scale;
                    rootPosition = Sub(rootPosition, Mul(directions[n], length));
                }
            }
            for (size_t n = 0; n < count; ++n) {
                const auto index = chain.joints[n];
                Matrix4x4 pose;
                if (n == 0) {
                    pose = bindMatrices[index];
                    pose.m[3][0] = pose.m[3][1] = pose.m[3][2] = 0;
                    pose = MatrixMath::Multipty(pose, MatrixMath::MakeScale({scale, scale, scale}));
                    pose.m[3][0] = rootPosition.x; pose.m[3][1] = rootPosition.y; pose.m[3][2] = rootPosition.z;
                } else {
                    // Forward kinematics: every child keeps its bind-local offset
                    // and inherits the parent's scale/rotation. No independent
                    // joint translations that compress or separate the mesh rings.
                    pose = MatrixMath::Multipty(bindLocalMatrices[index],
                        skeleton->joints[chain.joints[n - 1]].skeletonSpaceMatrix);
                }
                if (visible && n + 1 < count) {
                    const auto child = MatrixMath::Multipty(bindLocalMatrices[chain.joints[n + 1]], pose);
                    const auto pivot = Position(pose);
                    const auto source = Sub(Position(child), pivot);
                    pose.m[3][0] = pose.m[3][1] = pose.m[3][2] = 0;
                    pose = MatrixMath::Multipty(pose, FromTo(source, directions[n]));
                    pose.m[3][0] = pivot.x; pose.m[3][1] = pivot.y; pose.m[3][2] = pivot.z;
                }
                auto& joint = skeleton->joints[index];
                joint.skeletonSpaceMatrix = pose; joint.worldMatrix = pose; joint.worldTranslate = Position(pose);
            }
        }
        tentacles->UpdateSkinning();
        // The isolated model uses the captured progression frame, never combat AI/colliders.
        auto frame=MatrixMath::MakeIdentity4x4();const Vector3 axes[]{right,up,forward};
        for(int i=0;i<3;++i){frame.m[i][0]=axes[i].x;frame.m[i][1]=axes[i].y;frame.m[i][2]=axes[i].z;}
        const float yaw=std::asin(std::clamp(-frame.m[0][2],-1.0f,1.0f));
        Vector3 euler{0,yaw,0};
        if(std::fabs(std::cos(yaw))>.0001f){euler.x=std::atan2(frame.m[1][2],frame.m[2][2]);euler.z=std::atan2(frame.m[0][1],frame.m[0][0]);}
        else euler.x=std::atan2(-frame.m[2][1],frame.m[1][1]);
        tentacleObject->SetRotate(euler);tentacleObject->SetScale({unit,unit,unit});tentacleObject->SetTranslate(center);tentacleObject->Update();
    }
};
PlayerDeathSequenceController::Settings& PlayerDeathSequenceController::GetTuning(){return tuning;}
PlayerDeathSequenceController::PlayerDeathSequenceController():flight_(std::make_unique<Flight>()){}
PlayerDeathSequenceController::~PlayerDeathSequenceController()=default;
void PlayerDeathSequenceController::Initialize(DirectXCommon*,SpriteCommon*,CameraShakeController* shake){cameraShake_=shake;}
void PlayerDeathSequenceController::BindFlight(Player* p,Camera* c,BoostController* b,PlayerDamageFeedbackController* h,
    GpuParticleEffectPlayer* fx,CombatEffectController* combat,EnemyWaveManager* waves,
    KrakenTentacleWaveEncounterController* encounter,ModelCommon* models,Object3dCommon* objects){
    auto& f=*flight_;f.player=p;f.camera=c;f.boost=b;f.health=h;f.effects=fx;f.combat=combat;f.waves=waves;f.encounter=encounter;f.models=models;f.objects=objects;
}
bool PlayerDeathSequenceController::PrepareForPresentation(){
    auto& f=*flight_;if(f.prepared)return true;
    if(!f.effects||!f.objects||!f.models||!f.camera||!f.effects->PrepareDamageEffect())return false;
    f.enemy=std::make_unique<Enemy>();f.enemy->Initialize(f.objects,f.camera,"death-presentation-only");
    f.enemy->SetScale({.65f,.65f,.65f});f.enemy->SetVelocity({});
    f.bullet=std::make_unique<EnemyBullet>();if(!f.bullet->Initialize(f.objects,f.camera))return false;
    f.bullet->SetLifeTime(60);f.bullet->SetVelocity({});
    f.bulletGlow=std::make_unique<PlayerJetExhaustBeamRenderer>();
    if(!f.bulletGlow->Initialize(f.objects->GetDxCommon()))return false;
    f.bulletGlowVertices.reserve(6);
    const std::string asset="resources/midboss/kraken_midboss_tentacles.gltf";
    f.skeleton=GltfSkeletonLoader::LoadFromFile(asset);std::string error;
    if(!f.skeleton||!DetectKrakenTentacleChains(*f.skeleton,f.chains,error)||f.chains.size()!=4)return false;
    for(const auto& chain:f.chains)if(chain.joints.size()<2||chain.joints.size()>10)return false;
    UpdateSkeletonWorldTransforms(*f.skeleton);
    for(const auto& joint:f.skeleton->joints){f.bindMatrices.push_back(joint.skeletonSpaceMatrix);f.bindLocalMatrices.push_back(joint.localMatrix);}
    f.tentacles=std::make_unique<GltfSkinnedModel>();if(!f.tentacles->Initialize(f.models,f.skeleton.get(),asset))return false;
    f.tentacles->SetUseComputeOutputVertices(true);
    f.tentacleObject=std::make_unique<Object3d>();f.tentacleObject->Initialize(f.objects);
    f.tentacleObject->SetModel(f.tentacles->GetModel());f.tentacleObject->SetCamera(f.camera);f.tentacleObject->SetEnvironmentMapEnabled(false);
    f.debrisModel=ModelManager::GetInstance()->CreateBox("player-death-fragment");
    if(!f.debrisModel||!f.tentacleObject->IsValid())return false;
    f.debrisModel->GetMaterialData()->color={.32f,.36f,.4f,1};
    f.debrisModel->SetTextureIndex(TextureManager::GetInstance()->GetTextureIndexByFilePath("resources/human/white.png"));
    for(auto& fragment:f.debris){fragment=std::make_unique<Object3d>();fragment->Initialize(f.objects);
        fragment->SetModel(f.debrisModel);fragment->SetCamera(f.camera);fragment->SetEnvironmentMapEnabled(false);if(!fragment->IsValid())return false;}
    f.prepared=true;return true;
}
void PlayerDeathSequenceController::Finalize(){Reset();flight_=std::make_unique<Flight>();cameraShake_=nullptr;}
void PlayerDeathSequenceController::StartDeathSequence(){
    const auto* encounter=flight_->encounter;
    Start(false,encounter&&encounter->IsControllingKraken()?Route::KrakenGrip:Route::RearEnemy);
}
void PlayerDeathSequenceController::StartPreview(Route route){
    if((IsActiveOrFinished()&&!flight_->preview)||(flight_->health&&flight_->health->IsDead()))return;
    Reset();Start(true,route);
}
void PlayerDeathSequenceController::Start(bool preview,Route route){
    auto& f=*flight_;if(IsActiveOrFinished()||!f.prepared||!f.player||!f.camera||(!preview&&f.health&&!f.health->IsDead()))return;
    f.originalPose=f.player->GetVisualTransform();f.center=f.originalPose.translate;
    Vector3 minimum{},maximum{};
    const float width=f.player->GetVisualModelBounds(minimum,maximum)?(maximum.x-minimum.x)*f.originalPose.scale.x:.4f;
    f.unit=std::clamp(width/3.8f,.01f,10.0f); // Cinematic units track aircraft dimensions, never alter its model Scale.
    if(cameraShake_)cameraShake_->Reset(f.camera);f.originalCamera=*f.camera;
    f.forward=Unit(f.player->GetBaseForward());const auto& basis=f.originalCamera.GetWorldMatrix();
    f.right=Unit(MatrixMath::Cross({0,1,0},f.forward),{basis.m[0][0],basis.m[0][1],basis.m[0][2]});
    f.up=Unit(MatrixMath::Cross(f.forward,f.right),{0,1,0});
    f.settings=GetTuning();f.route=route;f.wave=f.waves?f.waves->GetCurrentWaveId():"(none)";
    f.enemyPosition=f.Place(f.settings.enemySide,f.settings.enemyHeight,-f.settings.enemyDistance);
    f.muzzle=Add(f.enemyPosition,Mul(Unit(Sub(f.center,f.enemyPosition)),.7f*f.unit));
    const float appear=f.settings.cameraSeconds+f.settings.enemyAppearDelay;
    f.fireTime=appear+f.settings.enemyRiseSeconds+f.settings.fireDelay;
    f.hitTime=f.fireTime+Length(Sub(f.center,f.muzzle))/((std::max)(1.0f,f.settings.bulletSpeed)*f.unit);
    f.explosionTime=route==Route::RearEnemy?f.hitTime+f.settings.hitToExplosion:
        f.CameraSeconds()+f.settings.tentacleRiseSeconds+f.settings.tentacleApproachSeconds+f.settings.gripHoldSeconds+f.settings.squeezeSeconds;
    f.finishTime=f.explosionTime+(std::max)(.3f,f.settings.resultDelay);
    f.captured=true;f.preview=preview;f.paused=f.fired=f.hit=f.exploded=false;
    if(f.boost)f.boost->Cancel();f.player->SetGameViewInputActive(false);f.player->SetDisplayOpacity(1);f.player->SetDamageFeedbackAlpha(1);
    if(cameraShake_)cameraShake_->SetGameplayEventsEnabled(true);
    elapsedTime_=0;isPlaying_=true;isFinished_=false;ApplyPresentation();
}
void PlayerDeathSequenceController::Update(float raw){
    auto& f=*flight_;if(!IsActiveOrFinished())return;
    const float dt=GetPresentationDeltaTime(raw);if(dt<=0)return;
    if(cameraShake_)cameraShake_->BeginFrame(f.camera);
    elapsedTime_=(std::min)(elapsedTime_+dt,f.finishTime+(std::max)(3.0f,f.settings.debrisLifetime));
    if(f.route==Route::RearEnemy){
        if(!f.fired&&elapsedTime_>=f.fireTime){f.fired=true;if(f.effects)f.effects->PlayGpuParticleEffectAt("resources/effects/gpu/hit_sparks.json",f.muzzle,f.unit*.5f,1,.3f,.6f);}
        if(!f.hit&&elapsedTime_>=f.hitTime){f.hit=true;if(f.combat)f.combat->PlayEnemyBulletHitPlayer(f.center,f.unit);}
    }
    if(!f.exploded&&elapsedTime_>=f.explosionTime)Explode();
    if(elapsedTime_>=f.finishTime){isPlaying_=false;isFinished_=true;}
    ApplyPresentation();
    if(cameraShake_){cameraShake_->UpdateAndApply(dt,f.camera,true);f.camera->Update();f.player->RefreshCameraPresentation(0);}
}
void PlayerDeathSequenceController::ApplyPresentation(){
    auto& f=*flight_;const auto& s=f.settings;const float blend=Smooth(elapsedTime_/(std::max)(.1f,f.CameraSeconds()));
    const auto start=Sub(f.originalCamera.GetTranslate(),f.center);
    const float angle0=std::atan2(Dot(start,f.right),Dot(start,f.forward));
    const float targetAngle=f.route==Route::KrakenGrip?s.bossCameraAzimuth:0;
    const float angle=angle0+std::remainder(targetAngle-angle0,2*kPi)*blend;
    const float initialRadius=Length(Sub(start,Mul(f.up,Dot(start,f.up))));
    const float radius=initialRadius+((f.route==Route::KrakenGrip?s.bossCameraDistance:s.cameraDistance)*f.unit-initialRadius)*blend;
    const float height=Dot(start,f.up)+((f.route==Route::KrakenGrip?s.bossCameraHeight:s.cameraHeight)*f.unit-Dot(start,f.up))*blend;
    const auto cameraPosition=f.PlaceWorld(std::sin(angle)*radius,height,std::cos(angle)*radius);
    const auto focus=f.route==Route::KrakenGrip?f.Place(0,s.bossCameraFocusY,0):f.center;
    const auto direction=Sub(focus,cameraPosition);
    const Vector3 look{std::atan2(-direction.y,std::hypot(direction.x,direction.z)),std::atan2(direction.x,direction.z),0};
    auto rotation=f.originalCamera.GetRotate();
    rotation.x+=std::remainder(look.x-rotation.x,2*kPi)*blend;
    rotation.y+=std::remainder(look.y-rotation.y,2*kPi)*blend;rotation.z*=1-blend;
    f.camera->SetTranslate(cameraPosition);f.camera->SetRotate(rotation);
    f.camera->SetFovY(f.originalCamera.GetFovY()+(s.cameraFov-f.originalCamera.GetFovY())*blend);
    f.camera->SetViewTranslationOffset({});f.camera->SetViewRotationOffset({});f.camera->Update();
    // Settle the captured attitude without inheriting the player's whole roll.
    auto pose=f.originalPose;const float yaw=std::atan2(f.forward.x,f.forward.z);
    pose.rotate.x+=(std::atan2(-f.forward.y,std::hypot(f.forward.x,f.forward.z))-pose.rotate.x)*blend;
    pose.rotate.y+=std::remainder(yaw-pose.rotate.y,2*kPi)*blend;pose.rotate.z*=1-blend;
    float squeeze=0;
    if(f.route==Route::KrakenGrip){
        const float begin=f.CameraSeconds()+s.tentacleRiseSeconds+s.tentacleApproachSeconds+s.gripHoldSeconds;
        squeeze=Smooth((elapsedTime_-begin)/s.squeezeSeconds)*s.squeezeAmount;f.UpdateTentacles(elapsedTime_);
    }
    f.player->SetDisplayScale({1-squeeze*.55f,1-squeeze*.75f,1+squeeze*.10f});
    f.player->SetDisplayPose(Sub(pose.translate,f.player->GetWorldPosition()),pose.rotate);
    if(f.route==Route::RearEnemy){
        const float rise=Smooth((elapsedTime_-s.cameraSeconds-s.enemyAppearDelay)/s.enemyRiseSeconds);
        f.enemy->SetPosition(Add(f.enemyPosition,Mul(f.up,-4*(1-rise)*f.unit)));
        f.enemy->SetScale({s.enemyScale*f.unit,s.enemyScale*f.unit,s.enemyScale*f.unit});f.enemy->LookAt(f.center);f.enemy->Update(0);
        if(f.fired&&!f.hit){const float u=std::clamp((elapsedTime_-f.fireTime)/(f.hitTime-f.fireTime),0.0f,1.0f);
            const float bulletSize=s.bulletScale*f.unit;f.bullet->SetScale({bulletSize,bulletSize,bulletSize});f.bullet->SetPosition(Lerp(f.muzzle,f.center,u));f.bullet->SetVisualForwardOverride(Unit(Sub(f.center,f.muzzle)));f.bullet->Update(0);}
    }
    if(f.exploded){const float age=elapsedTime_-f.explosionTime;const int count=std::clamp(s.debrisCount,0,24);
        for(int i=0;i<count;++i){const float a=i*2.399963f;const float speed=2.8f+static_cast<float>(i%4)*.5f;
            const auto v=Add(Mul(f.right,std::cos(a)*speed),Add(Mul(f.up,std::sin(a)*speed),Mul(f.forward,static_cast<float>(i%3)-1)));
            auto position=Add(f.center,Mul(v,age*f.unit));position.y-=1.5f*age*age*f.unit;
            auto& fragment=*f.debris[i];fragment.SetTranslate(position);fragment.SetRotate({a+age*5,a*.5f+age*3,age*4});
            const float fade=1-Smooth(age/(std::max)(.2f,s.debrisLifetime));fragment.SetScale({.10f*fade*f.unit,.035f*fade*f.unit,.22f*fade*f.unit});fragment.Update();}
    }
}
void PlayerDeathSequenceController::Explode(){
    auto& f=*flight_;f.exploded=true;
    if(cameraShake_)cameraShake_->SetGameplayEventsEnabled(false);
    if(f.combat)f.combat->PlayPlayerDeathExplosion(f.center,f.settings.explosionScale*f.unit,f.settings.explosionBrightness,f.settings.explosionAmount,f.settings.explosionLifetime);
    if(cameraShake_&&f.settings.shakeStrength>0){cameraShake_->SetGameplayEventsEnabled(true);cameraShake_->RequestGameplayShake(CameraShakeController::GameplayEvent::Explosion,f.settings.shakeStrength);}
    if(f.effects){GpuParticleEffectPlayer::DamageSource source;source.position=f.center;source.direction=f.up;
        source.scale=.8f*f.unit;source.intensity=.5f;source.smokeMix=.9f;source.duration=.6f;f.smoke=f.effects->StartDamageEffect(source);}
}
void PlayerDeathSequenceController::Draw(){
    auto& f=*flight_;if(!f.captured)return;
    if(f.route==Route::RearEnemy){if(elapsedTime_>=f.settings.cameraSeconds+f.settings.enemyAppearDelay)f.enemy->Draw();if(f.fired&&!f.hit){
            f.bullet->Draw();
            const auto head=f.bullet->GetPosition();const auto direction=Unit(Sub(f.center,f.muzzle));
            const float traveled=Length(Sub(head,f.muzzle));
            const auto tail=Sub(head,Mul(direction,(std::min)(f.settings.bulletTrailLength*f.unit,traveled+.15f*f.unit)));
            const auto normal=Unit(Sub(f.camera->GetViewTranslate(),head));
            const auto side=Mul(Unit(MatrixMath::Cross(direction,normal),f.up),f.settings.bulletGlowWidth*f.unit);
            const auto a=Add(tail,side),b=Sub(tail,side),c=Add(head,side),d=Sub(head,side);
            auto& vertices=f.bulletGlowVertices;vertices.clear();
            vertices.push_back({a,{0,0}});vertices.push_back({c,{1,0}});vertices.push_back({d,{1,1}});
            vertices.push_back({a,{0,0}});vertices.push_back({d,{1,1}});vertices.push_back({b,{0,1}});
            f.bulletGlow->Draw(vertices,f.camera,1.8f,.85f,0,1.5f,1,elapsedTime_,3u);
        }}
    else if(elapsedTime_>=f.CameraSeconds()){f.tentacles->DispatchComputeSkinning(f.objects->GetDxCommon()->GetCommandList());f.objects->CommonDrawSetting();f.tentacleObject->Draw();}
    if(f.exploded&&elapsedTime_-f.explosionTime<f.settings.debrisLifetime){f.objects->CommonDrawSetting();for(int i=0;i<std::clamp(f.settings.debrisCount,0,24);++i)f.debris[i]->Draw();}
}
void PlayerDeathSequenceController::Reset(){
    auto& f=*flight_;if(f.effects&&f.smoke)f.effects->EndDamageEffect(f.smoke);f.smoke=0;
    if(f.captured){if(cameraShake_)cameraShake_->Reset(f.camera);if(f.camera)*f.camera=f.originalCamera;
        if(f.player){f.player->ClearDisplayPose();f.player->RefreshCameraPresentation(0);}
        if(f.effects){if(f.fired)f.effects->StopEffect("resources/effects/gpu/hit_sparks.json");if(f.exploded)f.effects->StopEffect("resources/effects/gpu/death_explosion.json");}}
    f.captured=f.preview=f.paused=f.fired=f.hit=f.exploded=false;isPlaying_=isFinished_=false;elapsedTime_=0;
}
bool PlayerDeathSequenceController::IsPreview()const{return flight_->preview;}
void PlayerDeathSequenceController::SetPreviewPaused(bool p){if(flight_->preview)flight_->paused=p;}
float PlayerDeathSequenceController::GetDuration()const{return flight_->finishTime;}
float PlayerDeathSequenceController::GetExplosionTime()const{return flight_->explosionTime;}
float PlayerDeathSequenceController::GetPresentationDeltaTime(float raw)const{return flight_->paused?0:std::clamp(raw,0.0f,1.0f/15.0f)*(flight_->preview?flight_->speed:1);}
float PlayerDeathSequenceController::GetHudVisibility()const{return !IsActiveOrFinished()?1:1-Smooth((elapsedTime_-flight_->explosionTime)/.3f);}
PlayerDeathSequenceController::Route PlayerDeathSequenceController::GetRoute()const{return flight_->route;}
const std::string& PlayerDeathSequenceController::GetCapturedWaveId()const{return flight_->wave;}
bool PlayerDeathSequenceController::IsAircraftVisible()const{return !flight_->exploded;}
bool PlayerDeathSequenceController::HasFired()const{return flight_->fired;}
bool PlayerDeathSequenceController::HasHit()const{return flight_->hit;}
bool PlayerDeathSequenceController::HasExploded()const{return flight_->exploded;}
void PlayerDeathSequenceController::DrawImGui(){
#ifdef USE_IMGUI
    if(DebugUiWorkspace::BeginPanel("プレイヤー死亡演出")){
        ImGui::Text("固定ルート: %s / Wave: %s",GetRoute()==Route::RearEnemy?"後方敵の射撃":"触手2本の把持",flight_->wave.c_str());
        ImGui::Text("経過 %.2f / 爆散 %.2f / UI %.2f",elapsedTime_,GetExplosionTime(),GetDuration());
        ImGui::TextWrapped("Boost非依存の通常フレーム時間、最大1/15秒、Pauseで停止。SYSTEM ERRORは保持。距離・規模は機体幅3.8を基準とする演出単位です。設定は次回開始へ適用します。");
        const bool canPreview=(!IsActiveOrFinished()||flight_->preview)&&(!flight_->health||!flight_->health->IsDead());
        ImGui::BeginDisabled(!canPreview);if(ImGui::Button("後方敵ルートを確認"))StartPreview(Route::RearEnemy);ImGui::SameLine();if(ImGui::Button("触手ルートを確認"))StartPreview(Route::KrakenGrip);ImGui::EndDisabled();
        if(flight_->preview){if(ImGui::Button("演出確認を終了・復元"))Reset();ImGui::Checkbox("演出確認の一時停止",&flight_->paused);ImGui::SliderFloat("確認用の再生倍率",&flight_->speed,.1f,1);}
        auto& s=GetTuning();ImGui::PushItemWidth(150);
        ImGui::SliderFloat("カメラ移動秒",&s.cameraSeconds,.3f,2);ImGui::SliderFloat("正面カメラ距離",&s.cameraDistance,7,18);ImGui::SliderFloat("カメラ高さ",&s.cameraHeight,-1,4);ImGui::SliderFloat("カメラ画角",&s.cameraFov,.6f,1.3f);
        if(ImGui::CollapsingHeader("後方敵・射撃")){ImGui::SliderFloat("敵出現まで",&s.enemyAppearDelay,0,1);ImGui::SliderFloat("敵の上昇秒",&s.enemyRiseSeconds,.2f,1);ImGui::SliderFloat("出現後の射撃待ち",&s.fireDelay,.1f,1.5f);ImGui::SliderFloat("後方距離",&s.enemyDistance,4,15);ImGui::SliderFloat("敵の横位置",&s.enemySide,1,6);ImGui::SliderFloat("敵の高さ",&s.enemyHeight,-1,4);ImGui::SliderFloat("敵の表示規模",&s.enemyScale,.2f,1.5f);ImGui::SliderFloat("演出弾速",&s.bulletSpeed,5,40);ImGui::SliderFloat("演出弾の大きさ",&s.bulletScale,.2f,1.2f);ImGui::SliderFloat("演出弾の発光幅",&s.bulletGlowWidth,.05f,.3f);ImGui::SliderFloat("発光の長さ",&s.bulletTrailLength,.3f,2);ImGui::SliderFloat("命中から爆散",&s.hitToExplosion,.03f,.4f);}
        if(ImGui::CollapsingHeader("触手2本・把持")){ImGui::SliderFloat("中ボス用カメラ移動秒",&s.bossCameraSeconds,.2f,1.2f);ImGui::SliderFloat("中ボス用カメラ横角度",&s.bossCameraAzimuth,-.8f,.8f);ImGui::SliderFloat("中ボス用カメラ高さ",&s.bossCameraHeight,0,4);ImGui::SliderFloat("中ボス用カメラ距離",&s.bossCameraDistance,10,18);ImGui::SliderFloat("中ボス用の注視高さ",&s.bossCameraFocusY,-3,1);ImGui::SliderFloat("せり上がり秒",&s.tentacleRiseSeconds,.3f,2);ImGui::SliderFloat("接近秒",&s.tentacleApproachSeconds,.3f,2);ImGui::SliderFloat("掴みの間",&s.gripHoldSeconds,.1f,1);ImGui::SliderFloat("締め付け秒",&s.squeezeSeconds,.3f,2);ImGui::SliderFloat("開始の左右幅",&s.tentacleStartSpread,3,7);ImGui::SliderFloat("曲線基準の根元高さ",&s.tentacleRootY,-18,-9);ImGui::SliderFloat("触手全体の大きさ",&s.tentacleScale,1.2f,2.2f);ImGui::SliderFloat("掴む左右位置",&s.tentacleGripWidth,.8f,3);ImGui::SliderFloat("曲がり量",&s.tentacleCurl,0,2);ImGui::SliderFloat("締め付け量",&s.squeezeAmount,.2f,.85f);}
        if(ImGui::CollapsingHeader("爆散・表示")){ImGui::SliderFloat("爆発の大きさ",&s.explosionScale,.5f,3);ImGui::SliderFloat("爆発の明るさ",&s.explosionBrightness,.3f,2);ImGui::SliderFloat("爆発粒子量",&s.explosionAmount,.25f,2);ImGui::SliderFloat("爆発寿命倍率",&s.explosionLifetime,.5f,2);ImGui::SliderInt("破片量（上限24）",&s.debrisCount,0,24);ImGui::SliderFloat("破片寿命",&s.debrisLifetime,.5f,3);ImGui::SliderFloat("回転シェイク倍率",&s.shakeStrength,0,2);ImGui::SliderFloat("爆散後UIまで",&s.resultDelay,.3f,2);}
        ImGui::PopItemWidth();
    }DebugUiWorkspace::EndPanel();
#endif
}
