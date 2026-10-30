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
