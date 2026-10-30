#include "Engine/Core/GpuTimestampProfiler.h"
#include "ProjectileBeamVisual.h"
#include "Engine/Game/Enemy/EnemyBullet.h"
#include "Engine/Game/Player/PlayerJetExhaustBeamRenderer.h"
#include "Engine/Graphics/Appearance/RenderStyleComparison.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Object3d/Object3dCommon.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
Vector3 Sum(Vector3 a,Vector3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vector3 Mul(Vector3 a,float b){return {a.x*b,a.y*b,a.z*b};}
Vector3 Sub(Vector3 a,Vector3 b){return Sum(a,Mul(b,-1));}
float Length(Vector3 a){return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);}
Vector3 Cross(Vector3 a,Vector3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Vector3 Unit(Vector3 a,Vector3 fallback){const float n=Length(a);return n>.00001f?Mul(a,1/n):fallback;}
}
struct ProjectileBeamVisual::State {
    Object3dCommon* common=nullptr;
    const Camera* camera=nullptr;
    PlayerJetExhaustBeamRenderer renderer;
    std::vector<PlayerJetExhaustBeamRenderer::Vertex> vertices;
    bool ready=false;
};
ProjectileBeamVisual::ProjectileBeamVisual():state_(std::make_unique<State>()){}
ProjectileBeamVisual::~ProjectileBeamVisual()=default;
bool ProjectileBeamVisual::Initialize(Object3dCommon* common,const Camera* camera){
    auto& s=*state_;s.common=common;s.camera=camera;s.vertices.reserve(768);
    s.ready=common&&camera&&s.renderer.Initialize(common->GetDxCommon(),true);return s.ready;
}
bool ProjectileBeamVisual::IsReady()const{return state_->ready;}
void ProjectileBeamVisual::Begin(){state_->vertices.clear();}
void ProjectileBeamVisual::Add(const EnemyBullet& bullet,bool player){
    auto& s=*state_;if(!s.ready||!bullet.IsActive()||bullet.IsDead()||s.vertices.size()>=4096*12)return;
    const auto& settings=RenderStyleComparison::GetSettings().presentation;
    const auto head=bullet.GetPosition();const float speed=Length(bullet.GetVelocity());
    const Vector3 forward=Unit(bullet.HasVisualForwardOverride()?bullet.GetVisualForwardOverride():bullet.GetVelocity(),{0,0,1});
    const auto& m=s.camera->GetViewWorldMatrix();const Vector3 right{m.m[0][0],m.m[0][1],m.m[0][2]};
    const auto side=Unit(Cross(forward,Sub(s.camera->GetViewTranslate(),head)),right);
    const float requested=std::clamp(player?settings.playerBeamLength:settings.enemyBeamLength,.05f,2.f);
    // The moving head is the existing collider center. No tail extends behind the muzzle at birth.
    const float length=(std::min)(requested,(std::max)(.035f,bullet.GetElapsedTime()*speed+.035f));
    const auto tail=Sub(head,Mul(forward,length));
    const float width=std::clamp(player?settings.playerBeamWidth:settings.enemyBeamWidth,.005f,.2f);
    auto quad=[&](float w){const auto half=Mul(side,w*.5f);const auto a=Sub(tail,half),b=Sum(tail,half),c=Sum(head,half),d=Sub(head,half);
        s.vertices.insert(s.vertices.end(),{{a,{0,0}},{b,{0,1}},{c,{1,1}},{a,{0,0}},{c,{1,1}},{d,{1,0}}});};
    quad(width*2.1f);quad(width);
}
void ProjectileBeamVisual::Draw(bool player){
    auto& s=*state_;if(!s.ready||s.vertices.empty())return;
    auto* dx=s.common->GetDxCommon();auto& profiler=dx->GetGpuProfiler();
    const auto token=profiler.Begin(dx->GetCommandList(),GpuTimestampProfiler::Stage::NormalShots,{});
    s.renderer.Draw(s.vertices,s.camera,1.25f,.90f,0,2.5f,.3f,0,player?4u:5u);
    profiler.End(dx->GetCommandList(),token);
    s.common->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
}
