#include "Engine/Core/GpuTimestampProfiler.h"
#include "ProjectileBeamVisual.h"
#include "Engine/Game/Enemy/EnemyBullet.h"
#include "Engine/Graphics/Primitive/BeamBatchRenderer.h"
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

float CoreBloomOpacity(RenderStyleComparison::CoreBloomStrength strength) {
    switch (strength) {
    case RenderStyleComparison::CoreBloomStrength::Weak: return .10f;
    case RenderStyleComparison::CoreBloomStrength::Strong: return .22f;
    case RenderStyleComparison::CoreBloomStrength::None: return 0.f;
    }
    return 0.f;
}
}
struct ProjectileBeamVisual::State {
    Object3dCommon* common=nullptr;
    const Camera* camera=nullptr;
    BeamBatchRenderer renderer,solidRenderer,bodyRenderer;
    std::vector<BeamBatchRenderer::Vertex> vertices;
    std::vector<BeamBatchRenderer::Vertex> cores,bodies,solids;
    size_t shotCount=0;
    bool ready=false;
};
ProjectileBeamVisual::ProjectileBeamVisual():state_(std::make_unique<State>()){}
ProjectileBeamVisual::~ProjectileBeamVisual()=default;
bool ProjectileBeamVisual::Initialize(Object3dCommon* common,const Camera* camera){
    auto& s=*state_;s.common=common;s.camera=camera;s.vertices.reserve(4096*12);s.cores.reserve(4096*36);s.bodies.reserve(4096*36);s.solids.reserve(4096*72);
    s.ready=common&&camera&&s.renderer.Initialize(common->GetDxCommon(),true,true,4096*12)&&s.solidRenderer.Initialize(common->GetDxCommon(),true,true,4096*72)&&s.bodyRenderer.Initialize(common->GetDxCommon(),true,true,4096*36);return s.ready;
}
bool ProjectileBeamVisual::IsReady()const{return state_->ready;}
void ProjectileBeamVisual::Begin(){auto& s=*state_;s.vertices.clear();s.cores.clear();s.bodies.clear();s.shotCount=0;}
void ProjectileBeamVisual::Add(const EnemyBullet& bullet,bool player){
    auto& s=*state_;if(!s.ready||!bullet.IsActive()||bullet.IsDead()||s.shotCount>=4096)return;
    ++s.shotCount;const auto& laser=RenderStyleComparison::GetNormalBeamSettings();
    const auto& settings=RenderStyleComparison::GetSettings().presentation;
    const auto head=bullet.GetPosition();const float speed=Length(bullet.GetVelocity());
    const Vector3 forward=Unit(bullet.HasVisualForwardOverride()?bullet.GetVisualForwardOverride():bullet.GetVelocity(),{0,0,1});
    const auto& m=s.camera->GetViewWorldMatrix();const Vector3 right{m.m[0][0],m.m[0][1],m.m[0][2]};
    const auto side=Unit(Cross(forward,Sub(s.camera->GetViewTranslate(),head)),right);
    const float requested=std::clamp((player?settings.playerBeamLength:settings.enemyBeamLength)*(laser.solidPrism?laser.lengthScale:1),.05f,3.f);
    // The moving head is the existing collider center. No tail extends behind the muzzle at birth.
    const float length=(std::min)(requested,(std::max)(.035f,bullet.GetElapsedTime()*speed+.035f));
    const auto tail=Sub(head,Mul(forward,length));
    const float width=std::clamp((player?settings.playerBeamWidth:settings.enemyBeamWidth)*(laser.solidPrism?laser.widthScale:1),.005f,.3f);
    if(laser.solidPrism){
        // A least-aligned world axis gives a finite square section at every angle.
        // Only the auxiliary halo/trail faces the camera.
        const Vector3 axis=std::fabs(forward.y)<.9f?Vector3{0,1,0}:Vector3{1,0,0};
        const auto x=Unit(Cross(axis,forward),{1,0,0}),y=Unit(Cross(forward,x),{0,1,0});
        auto prism=[&](std::vector<BeamBatchRenderer::Vertex>& vertices,float w,float inset,float layer){
            const auto a=Sum(tail,Mul(forward,inset)),b=Sub(head,Mul(forward,inset));
            Vector3 corners[8];for(int z=0;z<2;++z)for(int v=0;v<2;++v)for(int u=0;u<2;++u)corners[z*4+v*2+u]=Sum(z?b:a,Sum(Mul(x,(u?.5f:-.5f)*w),Mul(y,(v?.5f:-.5f)*w)));
            const int faces[6][4]={{0,2,3,1},{4,5,7,6},{0,4,6,2},{1,3,7,5},{0,1,5,4},{2,6,7,3}};
            const Vector3 normals[6]={Mul(forward,-1),forward,Mul(x,-1),x,Mul(y,-1),y};
            for(int face=0;face<6;++face){const auto& f=faces[face];const auto n=normals[face];
                auto vertex=[&](int index,Vector2 oldUv){
                    Vector2 uv=oldUv;
                    if(laser.energyProfile){
                        const float u=static_cast<float>((index>>2)&1);
                        const float v=static_cast<float>((index>>(face<4?1:0))&1);
                        uv=face<2?Vector2{static_cast<float>(index&1),static_cast<float>((index>>1)&1)}:Vector2{u,v};
                    }
                    return BeamBatchRenderer::Vertex{corners[index],uv,n,layer+(laser.energyProfile&&face<2?2.f:0.f)};
                };
                vertices.insert(vertices.end(),{vertex(f[0],{0,0}),vertex(f[1],{1,0}),vertex(f[2],{1,1}),vertex(f[0],{0,0}),vertex(f[2],{1,1}),vertex(f[3],{0,1})});
            }
        };
        const float coreWidth=laser.energyProfile?std::clamp(player?laser.coreWidth:laser.enemyCoreWidth,.08f,.38f):.34f;
        const float coreSize = width * coreWidth;
        const float coreInset = length * .025f;
        prism(s.cores, coreSize, coreInset, 1);
        auto glow=[&](Vector3 a,Vector3 b,float w,float v=0){auto h=Mul(side,w*.5f);s.vertices.insert(s.vertices.end(),{{Sub(a,h),{0,v}},{Sum(a,h),{0,v+1}},{Sum(b,h),{1,v+1}},{Sub(a,h),{0,v}},{Sum(b,h),{1,v+1}},{Sub(b,h),{1,v}}});};
        if (settings.beamVariant == RenderStyleComparison::BeamVariant::CoreBloom) {
            if (settings.coreBloomStrength != RenderStyleComparison::CoreBloomStrength::None) {
                const auto inset = Mul(forward, coreInset);
                const float haloScale = settings.coreBloomStrength == RenderStyleComparison::CoreBloomStrength::Strong ? 6.f : 4.5f;
                glow(Sum(tail, inset), Sub(head, inset), coreSize * haloScale);
            }
            return;
        }
        prism(s.bodies,width,0,0);
        glow(tail,head,width*(laser.energyProfile?2.6f:1.8f));
        const float traveled=bullet.GetElapsedTime()*speed+.035f;
        const float trail=(std::min)(requested*std::clamp(laser.trailRatio,0.f,.45f),(std::max)(0.f,traveled-length));
        if(trail>.001f)glow(Sub(tail,Mul(forward,trail)),tail,width*(laser.energyProfile?1.f:.75f),laser.energyProfile?2.f:0.f);
        return;
    }
    const bool opaque=RenderStyleComparison::GetNormalBeamSettings().opaqueCore;
    auto quad=[&](float w,float v){const auto half=Mul(side,w*.5f);const auto a=Sub(tail,half),b=Sum(tail,half),c=Sum(head,half),d=Sub(head,half);
        s.vertices.insert(s.vertices.end(),{{a,{0,v}},{b,{0,v+1}},{c,{1,v+1}},{a,{0,v}},{c,{1,v+1}},{d,{1,v}}});};
    quad(width*2.1f,opaque?2.f:0.f);quad(width,0);
}
void ProjectileBeamVisual::Draw(bool player){
    auto& s=*state_;if(!s.ready||(s.vertices.empty()&&s.cores.empty()))return;
    auto* dx=s.common->GetDxCommon();auto& profiler=dx->GetGpuProfiler();
    const auto token=profiler.Begin(dx->GetCommandList(),GpuTimestampProfiler::Stage::NormalShots,{});
    const auto& b=RenderStyleComparison::GetNormalBeamSettings();
    if(b.solidPrism&&b.energyProfile){
        auto linear=[](float value){value=std::clamp(value,0.f,1.f);return value<=.04045f?value/12.92f:std::pow((value+.055f)/1.055f,2.4f);};
        const auto hue=player?b.playerColorSRGB:b.enemyColorSRGB;
        const Vector4 colorCore{linear(hue.x),linear(hue.y),linear(hue.z),b.coreBrightness};
        const float white=player?b.coreWhiteness:b.enemyCoreWhiteness;
        const float coreWidth=player?b.coreWidth:b.enemyCoreWidth;
        const float glowBudget=1/std::sqrt((std::max)(1.f,static_cast<float>(s.shotCount)/16.f));
        const auto& settings = RenderStyleComparison::GetSettings().presentation;
        const bool coreBloom = settings.beamVariant == RenderStyleComparison::BeamVariant::CoreBloom;
        const float haloOpacity = coreBloom ? CoreBloomOpacity(settings.coreBloomStrength) : b.haloOpacity;
        s.renderer.Draw(s.vertices,s.camera,b.brightness,haloOpacity*glowBudget,0,2.5f,.3f,0,player?14u:15u,.3f,&colorCore);
        if (!coreBloom) {
            s.bodyRenderer.Draw(s.bodies,s.camera,b.brightness,b.opacity,white,coreWidth,.3f,0,player?16u:17u,.3f,&colorCore);
        }
        s.solidRenderer.Draw(s.cores,s.camera,b.brightness,1,white,coreWidth,.3f,0,player?10u:11u,.3f,&colorCore);
    }else if(b.solidPrism){
        s.solids.clear();s.solids.insert(s.solids.end(),s.cores.begin(),s.cores.end());s.solids.insert(s.solids.end(),s.bodies.begin(),s.bodies.end());
        s.solidRenderer.Draw(s.solids,s.camera,b.brightness,b.opacity,b.coreWhiteness,2.5f,.3f,0,player?8u:9u);
        const float glowBudget=1/std::sqrt((std::max)(1.f,static_cast<float>(s.shotCount)/16.f));
        s.renderer.Draw(s.vertices,s.camera,b.brightness,b.haloOpacity*glowBudget,0,2.5f,.3f,0,player?12u:13u);
    }else s.renderer.Draw(s.vertices,s.camera,b.brightness,b.opacity,b.opaqueCore?b.haloOpacity:0,2.5f,.3f,0,b.opaqueCore?(player?6u:7u):(player?4u:5u));
    profiler.End(dx->GetCommandList(),token);
    s.common->CommonDrawSetting(Object3dCommon::BlendMode::kNormal);
}
