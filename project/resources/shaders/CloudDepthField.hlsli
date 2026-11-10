// Shared geometry for volumetric near banks and distant surface banks.
float DepthHash(float3 p){p=frac(p*.1031);p+=dot(p,p.yzx+33.33);return frac((p.x+p.y)*p.z);}
float DepthNoise(float3 p){float3 c=floor(p),f=frac(p);f=f*f*(3-2*f);return lerp(lerp(lerp(DepthHash(c),DepthHash(c+float3(1,0,0)),f.x),lerp(DepthHash(c+float3(0,1,0)),DepthHash(c+float3(1,1,0)),f.x),f.y),lerp(lerp(DepthHash(c+float3(0,0,1)),DepthHash(c+float3(1,0,1)),f.x),lerp(DepthHash(c+float3(0,1,1)),DepthHash(c+1),f.x),f.y),f.z);}
float DepthEllipsoid(float3 p,float3 r){return (length(p/r)-1)*min(r.x,min(r.y,r.z));}
float DepthUnion(float a,float b,float k){float h=saturate(.5+.5*(b-a)/k);return lerp(b,a,h)-k*h*(1-h);}
float DepthBankDistance(float3 p,float4 center,float3 radius){
 float3 q=p-center.xyz;float seed=center.w;
 float d=DepthEllipsoid(q,radius*float3(.90,.72,1));
 d=DepthUnion(d,DepthEllipsoid(q-float3(-radius.x*.48,radius.y*.25,-radius.z*.10),radius*float3(.65,.68,.70)),1.8);
 d=DepthUnion(d,DepthEllipsoid(q-float3(radius.x*.45,radius.y*.48,radius.z*.15),radius*float3(.65,.62,.65)),1.8);
 d=DepthUnion(d,DepthEllipsoid(q-float3((seed-.5)*radius.x*.6,radius.y*.95,-radius.z*.15),radius*float3(.45,.48,.50)),1.8);
 d=DepthUnion(d,DepthEllipsoid(q-float3(radius.x*.15,-radius.y*.30,radius.z*.50),radius*float3(.65,.50,.55)),1.8);
 float scale=1/max(radius.x,12);float3 n=q*scale;
 return d+(DepthNoise(n*2.6+seed*17)-.5)*5+(DepthNoise(n*8+9)-.5)*2.8+(DepthNoise(n*18+19)-.5)*.45;
}
float3 DepthBankGradient(float3 p,float4 c,float3 r){
 const float e=.6;
 return float3(DepthBankDistance(p+float3(e,0,0),c,r)-DepthBankDistance(p-float3(e,0,0),c,r),DepthBankDistance(p+float3(0,e,0),c,r)-DepthBankDistance(p-float3(0,e,0),c,r),DepthBankDistance(p+float3(0,0,e),c,r)-DepthBankDistance(p-float3(0,0,e),c,r))/(2*e);
}
bool DepthBankRay(float3 eye,float3 ray,float4 center,float3 radius,out float entry,out float exit){
 float3 extent=radius*1.8+4;
 float3 safeRay=(step(0,ray)*2-1)*max(abs(ray),.00001);
 float3 a=(center.xyz-extent-eye)/safeRay,b=(center.xyz+extent-eye)/safeRay;
 float3 lo=min(a,b),hi=max(a,b);entry=max(0,max(lo.x,max(lo.y,lo.z)));exit=min(hi.x,min(hi.y,hi.z));return exit>entry;
}
float DepthFarWeight(float3 eye,float3 center,float range){return range<1?1:smoothstep(range*.70,range*1.05,length(center-eye));}
float4 DepthCumulus(float3 eye,float3 ray,float4 centers[16],float4 radii[16],float range,int steps,float3 sunTravel,float3 sunColor,float3 sky,float3 shadow,float3 lightColor){
 float nearest=1e5,alpha=0;int hitIndex=-1;float3 hit=0;
 float pixelAngle=max(length(ddx(ray)),length(ddy(ray)));
 [loop]for(int i=0;i<16;++i){float weight=radii[i].w*DepthFarWeight(eye,centers[i].xyz,range);if(weight<.001)continue;
  float t,end;if(!DepthBankRay(eye,ray,centers[i],radii[i].xyz,t,end)||t>=nearest)continue;
  [loop]for(int j=0;j<steps;++j){if(t>end||t>=nearest)break;float3 p=eye+ray*t;float d=DepthBankDistance(p,centers[i],radii[i].xyz);
   if(d<.10+pixelAngle*t*.30){nearest=t;hit=p;hitIndex=i;alpha=weight;break;}t+=max(.20,d*.65);
  }
 }
 if(hitIndex<0)return 0;float4 c=centers[hitIndex];float3 r=radii[hitIndex].xyz;
 float3 n=normalize(DepthBankGradient(hit,c,r)+float3(0,.0001,0));
 float3 sun=normalize(-sunTravel);float occlusion=1;
 [loop]for(int j=1;j<=(steps>64?4:2);++j){float at=(steps>64?3:5)+j*5;occlusion=min(occlusion,saturate(DepthBankDistance(hit+sun*at,c,r)/(at*.22)));}
 float form=saturate(dot(n,sun)*.5+.5);float lit=pow(saturate(occlusion*.30+form*.70),2.6);
 float3 color=lerp(shadow*1.05,lightColor*sunColor,lit);
 color=lerp(color,sky,smoothstep(150,330,nearest)*.20);return float4(color,alpha);
}
float3 DepthCirrus(float3 eye,float3 ray,float3 sky,float strength,float3 lightColor){
 if(strength<=0||ray.y<=.025)return sky;
 float t=max(0,(1800-eye.y)/ray.y);float2 p=(eye+ray*t).xz;
 float3 q=float3(p.x*.00014,1.37,p.y*.00030);
 float bands=DepthNoise(q+DepthNoise(q*.4)*2.5)*.7+DepthNoise(q*2.4+7)*.3;
 float alpha=smoothstep(.58,.78,bands)*strength*smoothstep(.025,.12,ray.y);
 return lerp(sky,lightColor,alpha);
}
