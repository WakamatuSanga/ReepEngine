struct BeamConstants
{
    float4x4 viewProjection;
    float4 params;        // x: brightness, y: flicker strength, z: time, w: mode
    float4 qualityParams; // x: alpha scale, y: edge softness, z: tip fade power, w: nozzle clip Z
    float4 projectileColorCore; // Linear RGB hue, core emission
    float4 projectileCamera;
};

ConstantBuffer<BeamConstants> gBeam : register(b0);

struct PixelShaderInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float beamNearFade : TEXCOORD1;
};

float4 MakeOutput(float4 color) : SV_TARGET0
{
    return color;
}

float4 main(PixelShaderInput input) : SV_TARGET0
{
    float brightness = max(gBeam.params.x, 0.0f);
    float flickerStrength = saturate(gBeam.params.y);
    float time = gBeam.params.z;
    uint mode = uint(gBeam.params.w + 0.5f);
    float alphaScale = saturate(gBeam.qualityParams.x);
    float edgeSoftness = max(gBeam.qualityParams.y, 0.5f);
    float tipFadePower = max(gBeam.qualityParams.z, 0.2f);

    if(mode==14u || mode==15u){
        bool trail=input.uv.y>1.5;
        float u=saturate(input.uv.x),v=input.uv.y-(trail?2:0),edge=abs(v*2-1);
        float band=pow(saturate(1-edge),trail?2.3:3.0);
        float cap=trail?pow(u,1.35):smoothstep(0,.09,u)*(1-smoothstep(.90,1,u));
        // Tint the background locally. Pure additive red over blue otherwise reads
        // as purple, even when the normal projectile's source color is red.
        float alpha=band*cap*saturate(alphaScale*3.2)*(trail?.84:1)*pow(saturate(input.beamNearFade),2);
        if(alpha<=.001)discard;
        return MakeOutput(float4(gBeam.projectileColorCore.rgb*brightness,alpha));
    }
    if(mode==12u || mode==13u) {
        float u=saturate(input.uv.x),edge=abs(input.uv.y*2-1);
        float alpha=pow(saturate(1-edge),2.5)*smoothstep(0,.12,u)*(1-smoothstep(.85,1,u))*alphaScale;
        if(alpha<=.001)discard;
        float3 hue=mode==12u?float3(1,.82,.025):float3(1,.025,.24);
        return MakeOutput(float4(hue*brightness,alpha));
    }
    // Modes 6/7 belong only to short normal projectiles. UV +2 selects the
    // existing halo quad without changing its dimensions or projectile physics.
    if(mode==6u || mode==7u) {
        bool halo=input.uv.y>1.5;
        float v=input.uv.y-(halo?2:0),u=saturate(input.uv.x);
        float edge=abs(v*2-1);
        float cap=smoothstep(0,.10,u)*(1-smoothstep(.90,1,u));
        float band=halo?pow(saturate(1-edge),2.5):(1-smoothstep(.65,1,edge));
        float alpha=band*cap*alphaScale*(halo?flickerStrength:1);
        if(alpha<=.01)discard;
        float3 hue=mode==6u?float3(1,.82,.025):float3(1,.025,.24);
        return MakeOutput(float4(hue*brightness,alpha));
    }
    if(mode==4u || mode==5u) {
        float u=saturate(input.uv.x),edge=abs(input.uv.y*2-1);
        float band=pow(saturate(1-edge),edgeSoftness);
        float cap=smoothstep(0,.16,u)*(1-smoothstep(.84,1,u));
        float alpha=band*cap*alphaScale;
        if(alpha<=.01)discard;
        float3 hue=mode==4u?float3(1,.82,.025):float3(1,.025,.24);
        return MakeOutput(float4(hue*brightness,alpha));
    }
    if (mode == 1u)
    {
        float2 centered = input.uv * 2.0f - 1.0f;
        float radial = saturate(1.0f - dot(centered, centered));
        float alpha = pow(radial, 2.8f) * alphaScale;
        if (alpha <= 0.01f)
        {
            discard;
        }
        float flicker = 1.0f + sin(time * 43.0f) * flickerStrength * 0.04f;
        return MakeOutput(float4(float3(1.0f, 0.94f, 0.58f) * brightness * flicker, alpha * 0.62f));
    }

    if (mode == 2u)
    {
        float u = saturate(input.uv.x);
        float edge = abs(input.uv.y * 2.0f - 1.0f);
        float band = pow(saturate(1.0f - edge), 1.45f);
        float angularFlicker = 1.0f + sin(u * 37.0f + time * 18.0f) * flickerStrength * 0.08f;
        float alpha = band * alphaScale;
        if (alpha <= 0.01f)
        {
            discard;
        }
        float3 rimColor = lerp(float3(0.45f, 0.82f, 1.0f), float3(1.0f, 1.0f, 1.0f), band);
        return MakeOutput(float4(rimColor * brightness * angularFlicker, alpha));
    }

    if (mode == 3u)
    {
        float u = saturate(input.uv.x);
        float edge = abs(input.uv.y * 2.0f - 1.0f);
        float band = pow(saturate(1.0f - edge), edgeSoftness);
        float pulse = 1.0f + sin(time * 58.0f + u * 11.0f) * flickerStrength * 0.08f;
        float alpha = band * alphaScale;
        if (alpha <= 0.01f)
        {
            discard;
        }
        float3 laserColor = lerp(float3(1.0f, 0.08f, 0.05f), float3(1.0f, 0.92f, 0.88f), pow(band, 2.2f));
        return MakeOutput(float4(laserColor * brightness * pulse, alpha));
    }
    float u = saturate(input.uv.x);
    float v = abs(input.uv.y * 2.0f - 1.0f);
    float center = saturate(1.0f - v);
    float core = pow(center, edgeSoftness);
    float edgeHeat = pow(center, 1.2f);
    float tipFade = pow(saturate(1.0f - smoothstep(0.0f, 1.0f, u)), tipFadePower);
    // Soften the finite nozzle edge; side and tip fades already reach zero.
    float rootFade = smoothstep(0.0f, 0.1f, u);
    // Only the camera-facing extension fades. Squaring compensates for its
    // inverse-depth-squared projected area; it reaches zero at the near plane.
    float nearFade = saturate(input.beamNearFade);

    float3 c0 = float3(1.0f, 0.95f, 0.65f);
    float3 c1 = float3(1.0f, 0.45f, 0.05f);
    float3 c2 = float3(0.65f, 0.06f, 0.02f);
    float3 color = lerp(c0, c1, smoothstep(0.0f, 0.45f, u));
    color = lerp(color, c2, smoothstep(0.45f, 1.0f, u));

    float flicker = 1.0f + sin(time * 37.0f + u * 9.0f) * flickerStrength * 0.06f;
    float alpha = saturate(lerp(edgeHeat * 0.22f, core, 0.80f) * tipFade * rootFade * nearFade * nearFade * alphaScale);
    if (alpha <= 0.01f)
    {
        discard;
    }

    return MakeOutput(float4(color * brightness * flicker, alpha));
}
