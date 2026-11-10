// Distant world-space cumulus. Analytic ellipsoid intersections, not a painted
// fullscreen plate. The existing nearby cloud volume continues to carry Flow.
float CloudHash(float3 p) {
    p=frac(p*.1031);p+=dot(p,p.yzx+33.33);return frac((p.x+p.y)*p.z);
}
float CloudNoise(float3 p) {
    float3 cell=floor(p),f=frac(p);f=f*f*(3-2*f);
    return lerp(lerp(lerp(CloudHash(cell),CloudHash(cell+float3(1,0,0)),f.x),lerp(CloudHash(cell+float3(0,1,0)),CloudHash(cell+float3(1,1,0)),f.x),f.y),
                lerp(lerp(CloudHash(cell+float3(0,0,1)),CloudHash(cell+float3(1,0,1)),f.x),lerp(CloudHash(cell+float3(0,1,1)),CloudHash(cell+1),f.x),f.y),f.z);
}
float3 CloudHash3(float3 p) {
    p=frac(p*float3(.1031,.1030,.0973));p+=dot(p,p.yxz+33.33);return frac((p.xxy+p.yxx)*p.zyx);
}
float CloudCellDistance(float3 p) {
    float3 cell=floor(p),f=frac(p);float nearest=100,second=100;
    [unroll] for(int z=-1;z<=1;++z) [unroll] for(int y=-1;y<=1;++y) [unroll] for(int x=-1;x<=1;++x){
        float3 neighbor=float3(x,y,z);float3 v=neighbor+CloudHash3(cell+neighbor)-f;
        float d=dot(v,v);second=min(second,max(nearest,d));nearest=min(nearest,d);
    }
    float a=sqrt(nearest),b=sqrt(second),h=saturate(.5+.5*(b-a)/.12);return lerp(b,a,h)-.12*h*(1-h);
}
float CloudSmoothUnion(float a,float b,float width) {
    float h=saturate(.5+.5*(b-a)/width);return lerp(b,a,h)-width*h*(1-h);
}
float CloudDistance(float3 samplePosition,float3 origin,float seed,float height,float coverage) {
    float result=10000;float tower=step(1-saturate(coverage),seed);
    [unroll] for(int i=0;i<7;++i) {
        float3 offset=0,radii=0;
        if(i==0){offset=float3(-18,0,-6);radii=float3(36,18,34);}
        if(i==1){offset=float3(18,-5,9);radii=float3(35,19,33);}
        if(i==2){offset=float3(-2,height*.40,0);radii=float3(27,26,27);}
        if(i==3){offset=float3(-7,height*.86,1);radii=float3(20,24,21);}
        if(i==4){offset=float3(15,height*.62,6);radii=float3(21,21,21);}
        if(i==5){offset=float3(-21,height*.73,-4);radii=float3(15,18,16);}
        if(i==6){offset=float3(4,height*1.10,-3);radii=float3(13,16,14);}
        if(i>=2&&tower<.5)continue;
        radii*=lerp(.9,1.1,frac(seed*float(i+17)*7.13));
        float d=(length((samplePosition-origin-offset)/radii)-1)*min(radii.x,min(radii.y,radii.z));
        result=CloudSmoothUnion(result,d,5.5);
    }
    // World-space surface structure survives camera turns; no camera-facing cards.
    if(result<7){
        result+=(CloudCellDistance(samplePosition*.135)-.50)*3.3;
    }
    return result+(CloudNoise(samplePosition*.065)-.5)*2.3;
}
float CloudWorldDistance(float3 samplePosition,float3 origin,float seed,float height,float coverage,float scale) {
    return CloudDistance(origin+(samplePosition-origin)/scale,origin,seed,height,coverage)*scale;
}
float4 DistantCumulus(float3 eye,float3 ray,float4 composition,float3 sunTravel,float3 sunColor,float3 sky,float4 placement,float towerCoverage,float3 shadowColor,float3 lightColor) {
    float pixelAngle=max(length(ddx(ray)),length(ddy(ray)));
    if(composition.x<=0)return float4(0,0,0,0);
    float tileSize=max(composition.y,100),height=composition.z;
    float scale=clamp(placement.w,.5,1.5);float2 offset=placement.xy;
    float2 baseCell=floor((eye.xz-offset)/tileSize);
    float nearest=1e6,alpha=0,hitSeed=0;float3 hit=0,hitOrigin=0;
    [loop] for(int z=-2;z<=2;++z) [loop] for(int x=-2;x<=2;++x) {
        float2 cell=baseCell+float2(x,z);
        float seed=CloudHash(float3(cell,3.37));
        float2 centerXZ=offset+(cell+.5)*tileSize+(float2(CloudHash(float3(cell,4.7)),CloudHash(float3(cell,9.1)))-.5)*tileSize*.16;
        float dist=length(centerXZ-eye.xz);
        float fade=smoothstep(130,185,dist)*(1-smoothstep(tileSize*1.65,tileSize*2.15,dist));
        if(fade<.001)continue;
        float3 origin=float3(centerXZ.x,placement.z+(seed-.5)*12*scale,centerXZ.y);
        float radius=max(78,height*.80+38)*scale;
        float3 oc=eye-origin-float3(0,height*.32,0)*scale;float b=dot(oc,ray),disc=b*b-dot(oc,oc)+radius*radius;
        if(disc<=0)continue;
        float start=max(1,-b-sqrt(disc)),end=-b+sqrt(disc);
        if(start>=nearest||end<1)continue;
        float t=start;
        [loop] for(int step=0;step<48;++step) {
            if(t>=end||t>=nearest)break;
            float3 samplePosition=eye+ray*t;float distance=CloudWorldDistance(samplePosition,origin,seed,height,towerCoverage,scale);
            float tolerance=.15+pixelAngle*t*.35;
            if(distance<tolerance){nearest=t;hit=samplePosition;hitOrigin=origin;hitSeed=seed;alpha=fade;break;}
            t+=max(.20,distance*.48);
        }
    }
    if(alpha<=0)return float4(0,0,0,0);
    float e=.35*scale;
    float3 n=normalize(float3(CloudWorldDistance(hit+float3(e,0,0),hitOrigin,hitSeed,height,towerCoverage,scale)-CloudWorldDistance(hit-float3(e,0,0),hitOrigin,hitSeed,height,towerCoverage,scale),
        CloudWorldDistance(hit+float3(0,e,0),hitOrigin,hitSeed,height,towerCoverage,scale)-CloudWorldDistance(hit-float3(0,e,0),hitOrigin,hitSeed,height,towerCoverage,scale),
        CloudWorldDistance(hit+float3(0,0,e),hitOrigin,hitSeed,height,towerCoverage,scale)-CloudWorldDistance(hit-float3(0,0,e),hitOrigin,hitSeed,height,towerCoverage,scale)));
    float structure=CloudNoise(hit*.55)*.6+CloudNoise(hit*1.2+9)*.4;
    float light=saturate(dot(n,normalize(-sunTravel))*.62+.35+(structure-.5)*.20);
    float3 middle=lerp(shadowColor,lightColor,.42)*float3(.90,1.08,1.15);
    float3 color=lerp(shadowColor,middle,smoothstep(.06,.60,light));
    color=lerp(color,lightColor*sunColor,smoothstep(.49,.87,light));
    color=lerp(color,sky,smoothstep(240,400,nearest)*.22);
    return float4(color,saturate(alpha*composition.x));
}


// Refinement is opt-in. Keep the previous distance field and placement intact.
float RefinedCloudDistance(float3 samplePosition,float3 origin,float seed,float height,float coverage,float scale,float4 relief,float4 layout) {
    float variation=saturate(layout.x);
    float width=lerp(1,lerp(.76,1.16,frac(seed*13.37+.17)),variation);
    float depth=lerp(1,lerp(.72,1.36,frac(seed*9.71+.41)),variation);
    float towerHeight=height*lerp(1,lerp(.60,1.28,frac(seed*13.7)),variation);
    float angle=(frac(seed*5.83)-.5)*.8*variation;
    float2x2 turn=float2x2(cos(angle),-sin(angle),sin(angle),cos(angle));
    float3 q=(samplePosition-origin)/scale;
    q.xz=mul(q.xz,turn)/float2(width,depth);
    float result=10000;bool tower=frac(seed*3.17+.2)>=1-saturate(coverage);float fill=saturate(layout.y);
    [unroll] for(int i=0;i<7;++i) {
        float3 center=0,radii=0;
        if(i==0){center=float3(-24,0,-8);radii=float3(38+fill*8,19,32);}
        if(i==1){center=float3(25,-5,8);radii=float3(31+fill*8,20,31);}
        if(i==2){center=float3(-30,tower?towerHeight*.40:11,-15);radii=float3(26,22,28);}
        if(i==3){center=float3(1,tower?towerHeight*.83:14,0);radii=float3(24,22,25);}
        if(i==4){center=float3(28,tower?towerHeight*.60:8,6);radii=float3(25,20,25);}
        if(i==5){center=float3(-16,towerHeight*.90+13,5);radii=float3(20,20,21);}
        if(i==6){center=float3(6,towerHeight*1.20+8,-5);radii=float3(18,18,18);}
        if(i>=5&&!tower)continue;
        radii*=lerp(.92,1.08,frac(seed*float(i+11)*3.71));
        float d=(length((q-center)/radii)-1)*min(radii.x,min(radii.y,radii.z));
        result=CloudSmoothUnion(result,d,3.5);
    }
    // Three separated scales; broad lobes dominate, fine detail stays subordinate.
    result+=(CloudNoise(q*.047+origin*.004)-.5)*relief.x;
    if(result<9)result+=(CloudCellDistance(q*.065)-.5)*relief.y*lerp(.35,1,CloudNoise(q*.024+origin*.017));
    result+=(CloudNoise(q*.34+9)-.5)*relief.z;
    return result*min(1,min(width,depth))*scale;
}
float4 RefinedDistantCumulus(float3 eye,float3 ray,float4 composition,float3 sunTravel,float3 sunColor,float3 sky,float4 placement,float coverage,float3 shadowColor,float3 lightColor,float4 relief,float4 layout) {
    float pixelAngle=max(length(ddx(ray)),length(ddy(ray)));
    float tileSize=max(composition.y,100),height=composition.z,scale=clamp(placement.w,.5,1.5);
    float2 baseCell=floor((eye.xz-placement.xy)/tileSize);
    float nearest=1e6,alpha=0,hitSeed=0;float3 hit=0,hitOrigin=0;
    [loop] for(int z=-2;z<=2;++z) [loop] for(int x=-2;x<=2;++x) {
        float2 cell=baseCell+float2(x,z);float seed=CloudHash(float3(cell,3.37));
        float jitter=lerp(.16,.46,saturate(layout.x));
        float2 centerXZ=placement.xy+(cell+.5)*tileSize+(float2(CloudHash(float3(cell,4.7)),CloudHash(float3(cell,9.1)))-.5)*tileSize*jitter;
        float dist=length(centerXZ-eye.xz);
        // Cells entering the 5x5 neighborhood are outside the fully faded range.
        float fade=smoothstep(130,185,dist)*(1-smoothstep(tileSize*1.65,tileSize*2.15,dist));
        if(fade<.001)continue;
        float3 origin=float3(centerXZ.x,placement.z+(seed-.5)*18*scale,centerXZ.y);
        float variation=saturate(layout.x);
        float width=lerp(1,lerp(.76,1.16,frac(seed*13.37+.17)),variation);
        float depth=lerp(1,lerp(.72,1.36,frac(seed*9.71+.41)),variation);
        float towerHeight=height*lerp(1,lerp(.60,1.28,frac(seed*13.7)),variation);
        float angle=(frac(seed*5.83)-.5)*.8*variation;
        float2x2 turn=float2x2(cos(angle),-sin(angle),sin(angle),cos(angle));
        float3 o=(eye-origin)/scale,direction=ray/scale;
        o.xz=mul(o.xz,turn)/float2(width,depth);direction.xz=mul(direction.xz,turn)/float2(width,depth);
        float3 safeDirection=(step(0,direction)*2-1)*max(abs(direction),.00001);
        float padding=.5*(relief.x+relief.y+relief.z)+2;
        float3 a=(float3(-76,-28,-48)-padding-o)/safeDirection;
        float3 b=(float3(76,max(38,towerHeight*1.2+28),48)+padding-o)/safeDirection;
        float3 tMin=min(a,b),tMax=max(a,b);
        float t=max(1,max(tMin.x,max(tMin.y,tMin.z))),end=min(tMax.x,min(tMax.y,tMax.z));
        if(end<=t)continue;
        if(t>=nearest||end<1)continue;
        [loop] for(int step=0;step<clamp((int)layout.z,48,96);++step) {
            if(t>=end||t>=nearest)break;
            float3 samplePosition=eye+ray*t;
            float d=RefinedCloudDistance(samplePosition,origin,seed,height,coverage,scale,relief,layout);
            if(d<.15+pixelAngle*t*.35){nearest=t;hit=samplePosition;hitOrigin=origin;hitSeed=seed;alpha=fade;break;}
            t+=max(.20,d*.48);
        }
    }
    if(alpha<=0)return 0;
    float e=.75*scale;
    float3 n=normalize(float3(
        RefinedCloudDistance(hit+float3(e,0,0),hitOrigin,hitSeed,height,coverage,scale,relief,layout)-RefinedCloudDistance(hit-float3(e,0,0),hitOrigin,hitSeed,height,coverage,scale,relief,layout),
        RefinedCloudDistance(hit+float3(0,e,0),hitOrigin,hitSeed,height,coverage,scale,relief,layout)-RefinedCloudDistance(hit-float3(0,e,0),hitOrigin,hitSeed,height,coverage,scale,relief,layout),
        RefinedCloudDistance(hit+float3(0,0,e),hitOrigin,hitSeed,height,coverage,scale,relief,layout)-RefinedCloudDistance(hit-float3(0,0,e),hitOrigin,hitSeed,height,coverage,scale,relief,layout)));
    float3 lightDirection=normalize(-sunTravel);float occlusion=1;
    [unroll] for(int j=1;j<=4;++j){float h=(3+j*5)*scale;
        float d=RefinedCloudDistance(hit+lightDirection*h,hitOrigin,hitSeed,height,coverage,scale,relief,layout);
        occlusion=min(occlusion,saturate(d/(h*.22)));}
    float light=saturate(dot(n,lightDirection)*.55+.42)*lerp(.38,1,occlusion);
    float3 middle=lerp(shadowColor,lightColor,.56);
    float3 color=lerp(shadowColor,middle,smoothstep(.02,.60,light));
    color=lerp(color,lightColor*sunColor,smoothstep(.40,.73,light));
    color=lerp(color,sky,smoothstep(250,420,nearest)*.08);
    return float4(color,saturate(alpha*composition.x));
}
