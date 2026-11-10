#include "CloudSceneryLayout.h"
#include "Engine/Graphics/Appearance/RenderStyleComparison.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Game/Camera/RailShooterCameraRig.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace {
float Hash(int station,int lane,int salt){uint32_t n=static_cast<uint32_t>(station)*0x9e3779b9u+static_cast<uint32_t>(lane)*0x85ebca6bu+static_cast<uint32_t>(salt)*0xc2b2ae35u;n^=n>>16;n*=0x7feb352du;n^=n>>15;n*=0x846ca68bu;n^=n>>16;return static_cast<float>(n&0xffffff)/16777216.f;}
float Smooth(float a,float b,float x){float t=std::clamp((x-a)/(b-a),0.f,1.f);return t*t*(3-2*t);}
}
CloudSceneryLayout CloudSceneryLayout::Build(const Camera* camera,const RailShooterCameraRig* rig,const RenderStyleComparison::CloudCompositionSettings& c,const Vector3& integratedFlow){
    CloudSceneryLayout result;if(!camera||!c.depthLayout)return result;
    const auto fallback=camera->GetWorldMatrix();const float baseDistance=rig?rig->GetSceneryDistance():fallback.m[3][2];
    const auto reference=rig?rig->SampleSceneryFrame(baseDistance):fallback;
    // Rail/straight cameras already advance in world space. A stationary gameplay
    // camera uses the one existing integrated Flow, never a second time clock.
    const Vector3 shift=(!rig||!rig->IsCameraRigActive())?integratedFlow:Vector3{};
    const float distance=baseDistance-(shift.x*reference.m[2][0]+shift.y*reference.m[2][1]+shift.z*reference.m[2][2]);
    const float spacing=std::clamp(c.bankSpacing,36.f,90.f);const int cell=static_cast<int>(std::floor(distance/spacing));
    result.referenceY=reference.m[3][1];
    for(int row=0;row<8;++row){const int station=cell+row-2;const float at=(static_cast<float>(station)+.5f)*spacing;
        auto frame=rig?rig->SampleSceneryFrame(at):MatrixMath::MakeIdentity4x4();if(!rig){frame.m[3][0]=0;frame.m[3][1]=result.referenceY;frame.m[3][2]=at;}
        for(int lane=0;lane<2;++lane){const float seed=Hash(station,lane,7),size=std::clamp(c.bankScale,.65f,1.35f),rx=(17+Hash(station,lane,1)*8)*size,ry=(20+Hash(station,lane,2)*17)*size,rz=(18+Hash(station,lane,3)*9)*size;
            const float side=(lane?1.f:-1.f)*(std::clamp(c.corridorHalfWidth,9.f,24.f)+rx+Hash(station,lane,4)*5),along=(Hash(station,lane,5)-.5f)*spacing*.16f;
            const float ahead=at-distance;const float fade=Smooth(-100,-60,ahead)*(1-Smooth(spacing*4.3f,spacing*5.4f,ahead));const int i=row*2+lane;
            result.centers[i]={shift.x+frame.m[3][0]+frame.m[0][0]*side+frame.m[2][0]*along,shift.y+frame.m[3][1]-9+(seed-.5f)*8,shift.z+frame.m[3][2]+frame.m[0][2]*side+frame.m[2][2]*along,seed};
            result.radii[i]={rx,ry,rz,fade};
        }
    }return result;
}
