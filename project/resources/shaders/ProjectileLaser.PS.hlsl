// Solid normal-projectile energy layers. Existing exhaust/beam modes never use this PSO.
struct BeamConstants {float4x4 viewProjection;float4 params;float4 qualityParams;};
ConstantBuffer<BeamConstants> gBeam:register(b0);
struct Input {float4 position:SV_POSITION;float2 uv:TEXCOORD0;float nearFade:TEXCOORD1;float3 normal:TEXCOORD2;nointerpolation float layer:TEXCOORD3;};
float4 main(Input input):SV_TARGET0 {
 bool player=uint(gBeam.params.w+.5)==8u;
 float3 hue=player?float3(1,.82,.025):float3(1,.025,.24);
 if(input.layer>.5)return float4(lerp(hue,1,saturate(gBeam.params.y))*(gBeam.params.x+.22),1);
 float faceLight=.80+.20*saturate(dot(normalize(input.normal),normalize(float3(-.35,.8,-.45)))*.5+.5);
 float edge=1-smoothstep(.02,.12,min(input.uv.y,1-input.uv.y));
 float3 color=lerp(hue*faceLight,lerp(hue,1,.25),edge*.08)*gBeam.params.x;
 float coreWindow=1-smoothstep(.12,.24,abs(input.uv.y-.5));
 return float4(color,saturate(gBeam.qualityParams.x)*lerp(1,.45,coreWindow));
}
