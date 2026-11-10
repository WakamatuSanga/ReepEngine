// Solid normal-projectile energy layers. Existing exhaust/beam modes never use this PSO.
struct BeamConstants {float4x4 viewProjection;float4 params;float4 qualityParams;float4 projectileColorCore;float4 projectileCamera;};
ConstantBuffer<BeamConstants> gBeam:register(b0);
struct Input {float4 position:SV_POSITION;float2 uv:TEXCOORD0;float nearFade:TEXCOORD1;float3 normal:TEXCOORD2;nointerpolation float layer:TEXCOORD3;float3 worldPosition:TEXCOORD4;};
float4 main(Input input):SV_TARGET0 {
 uint mode=uint(gBeam.params.w+.5);
 if(mode==10u||mode==11u||mode==16u||mode==17u){
  bool cap=input.layer>=2,core=fmod(input.layer,2)>.5;
  float edge=cap?max(abs(input.uv.x*2-1),abs(input.uv.y*2-1)):abs(input.uv.y*2-1);
  float nearFade=saturate(input.nearFade);
  if(nearFade<=.001)discard;
  float3 hue=gBeam.projectileColorCore.rgb;
  if(core){
   float3 warmWhite=float3(1,.96,.82);
   float3 emission=lerp(hue,warmWhite,saturate(gBeam.params.y))*gBeam.projectileColorCore.a;
   return float4(emission*(1-.06*edge),nearFade);
  }
  float radial=edge;
  if(!cap){
   // Canonical side UVs recover the actual square prism basis without per-shot
   // constants. Project the camera ray onto its central axis, so two visible
   // faces share one luminous center instead of becoming two outlined strips.
   float2 a=ddx(input.uv),b=ddy(input.uv);
   float3 dx=ddx(input.worldPosition),dy=ddy(input.worldPosition);
   float determinant=a.x*b.y-a.y*b.x;
   float inverse=(determinant<0?-1:1)/max(abs(determinant),1e-9);
   float3 along=(dx*b.y-dy*a.y)*inverse;
   float3 across=(dy*a.x-dx*b.x)*inverse;
   float halfWidth=max(length(across)*.5,1e-5);
   float3 axis=along/max(length(along),1e-5);
   float3 onAxis=input.worldPosition-across*(input.uv.y-.5)-input.normal*halfWidth;
   float3 ray=normalize(input.worldPosition-gBeam.projectileCamera.xyz);
   float3 transverse=cross(axis,ray);
   radial=abs(dot(input.worldPosition-onAxis,transverse))/max(length(transverse)*halfWidth,.00001);
  }
  float band=pow(saturate(1-radial/1.35),.8);
  if(cap)band*=1-smoothstep(.76,1,edge);
  float ends=cap?1:smoothstep(0,.045,input.uv.x)*(1-smoothstep(.955,1,input.uv.x));
  float alpha=saturate(gBeam.qualityParams.x)*band*ends*nearFade;
  if(alpha<=.001)discard;
  float energy=.48+.70*exp2(-radial*radial*2.6);
  return float4(hue*gBeam.params.x*energy,alpha);
 }
 bool player=uint(gBeam.params.w+.5)==8u;
 float3 hue=player?float3(1,.82,.025):float3(1,.025,.24);
 if(input.layer>.5)return float4(lerp(hue,1,saturate(gBeam.params.y))*(gBeam.params.x+.22),1);
 float faceLight=.80+.20*saturate(dot(normalize(input.normal),normalize(float3(-.35,.8,-.45)))*.5+.5);
 float edge=1-smoothstep(.02,.12,min(input.uv.y,1-input.uv.y));
 float3 color=lerp(hue*faceLight,lerp(hue,1,.25),edge*.08)*gBeam.params.x;
 float coreWindow=1-smoothstep(.12,.24,abs(input.uv.y-.5));
 return float4(color,saturate(gBeam.qualityParams.x)*lerp(1,.45,coreWindow));
}
